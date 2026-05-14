#include <jni.h>

#include "batch_renderer.h"
#include "cesium_types.h"
#include "gl_resources.h"
#include "inline_task_processor.h"
#include "prepare_renderer_resources.h"

#include <android/log.h>
#include <GLES3/gl3.h>

#include <Cesium3DTilesSelection/EllipsoidTilesetLoader.h>
#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TileContent.h>
#include <Cesium3DTilesSelection/Tileset.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <Cesium3DTilesSelection/TilesetOptions.h>
#include <Cesium3DTilesSelection/ViewState.h>
#include <Cesium3DTilesSelection/ViewUpdateResult.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumGeometry/QuadtreeTileID.h>
#include <CesiumGeospatial/BoundingRegion.h>
#include <CesiumGeospatial/Cartographic.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumUtility/IntrusivePointer.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

namespace {

using namespace cesium_poc;

class CesiumBridge {
public:
    CesiumBridge()
        : _taskProcessor(std::make_shared<InlineTaskProcessor>()),
          _prepareRendererResources(std::make_shared<MinimalPrepareRendererResources>()),
          _externals{
              nullptr,
              _prepareRendererResources,
              CesiumAsync::AsyncSystem(_taskProcessor),
              nullptr,
              spdlog::default_logger(),
              nullptr,
              Cesium3DTilesSelection::TilesetSharedAssetSystem::getDefault(),
              {}} {
        recreateTileset(_maximumScreenSpaceError);
        updateCamera(_camera);
    }

	    ~CesiumBridge() {
	        freeSelectedBatch();
	        if (_program != 0) {
	            glDeleteProgram(_program);
	        }
	        if (_batchProgram != 0) {
	            glDeleteProgram(_batchProgram);
	        }
	    }

	    void updateCamera(const CameraState& camera) {
	        std::lock_guard<std::mutex> lock(_mutex);
	        const bool changed =
	            std::abs(_camera.longitudeDegrees - camera.longitudeDegrees) > 0.0000001 ||
	            std::abs(_camera.latitudeDegrees - camera.latitudeDegrees) > 0.0000001 ||
	            std::abs(_camera.zoom - camera.zoom) > 0.0001 ||
	            _camera.autoOrbit != camera.autoOrbit ||
	            std::abs(_camera.bearingDegrees - camera.bearingDegrees) > 0.0001 ||
	            std::abs(_camera.pitchDegrees - camera.pitchDegrees) > 0.0001;
	        _camera = camera;
        const CesiumGeospatial::Cartographic cartographic =
            CesiumGeospatial::Cartographic::fromDegrees(
                camera.longitudeDegrees,
                camera.latitudeDegrees,
                cameraHeightMeters(camera.zoom));
        const glm::dvec3 ecef =
            CesiumGeospatial::Ellipsoid::WGS84.cartographicToCartesian(cartographic);
        _ecef = {ecef.x, ecef.y, ecef.z};
	        if (changed) {
	            _cameraDirty = true;
	            _selectionSettled = false;
	            _stableSelectionFrames = 0;
	            ++_cameraUpdates;
	        }
	    }

	    void onSurfaceCreated() {
	        _program = createProgram();
	        _locations.projection = glGetUniformLocation(_program, "u_projection");
	        _locations.originEye = glGetUniformLocation(_program, "u_originEye");
	        _locations.right = glGetUniformLocation(_program, "u_right");
	        _locations.up = glGetUniformLocation(_program, "u_up");
	        _locations.backward = glGetUniformLocation(_program, "u_backward");
	        _locations.texture = glGetUniformLocation(_program, "u_texture");
	        _batchProgram = createBatchProgram();
	        _batchLocations.projection = glGetUniformLocation(_batchProgram, "u_projection");
	        _batchLocations.originEye = glGetUniformLocation(_batchProgram, "u_originEye");
	        _batchLocations.right = glGetUniformLocation(_batchProgram, "u_right");
	        _batchLocations.up = glGetUniformLocation(_batchProgram, "u_up");
	        _batchLocations.backward = glGetUniformLocation(_batchProgram, "u_backward");
	        _batchLocations.texture = glGetUniformLocation(_batchProgram, "u_textureArray");
	        glEnable(GL_DEPTH_TEST);
	        glDepthFunc(GL_LEQUAL);
	        glEnable(GL_BLEND);
	        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	    }

	    void onSurfaceChanged(int width, int height) {
	        const int newWidth = std::max(width, 1);
	        const int newHeight = std::max(height, 1);
	        if (_width == newWidth && _height == newHeight) return;
	        _width = newWidth;
	        _height = newHeight;
	        glViewport(0, 0, _width, _height);
	        _cameraDirty = true;
	        _selectionSettled = false;
	        _stableSelectionFrames = 0;
	    }

    void setMaximumScreenSpaceError(double maximumScreenSpaceError) {
        std::lock_guard<std::mutex> lock(_mutex);
        const double clamped = std::max(0.5, maximumScreenSpaceError);
        if (std::abs(_maximumScreenSpaceError - clamped) < 0.0001) return;
        _maximumScreenSpaceError = clamped;
        _tilesetRebuildRequested = true;
        _cameraDirty = true;
        _selectionSettled = false;
        _stableSelectionFrames = 0;
    }

	    void renderFrame(int width, int height, double deltaSeconds) {
	        const auto start = std::chrono::steady_clock::now();
	        onSurfaceChanged(width, height);
        applyPendingTilesetOptions();

        CameraState camera;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            camera = _camera;
        }

	        const auto selectionStart = std::chrono::steady_clock::now();
	        const bool selectionUpdated = updateTileSelection(camera, deltaSeconds);
	        if (selectionUpdated) {
	            if (_selectionSettled) {
	                rebuildSelectedBatch();
	            } else {
	                freeSelectedBatch();
	            }
	        }
	        const auto drawStart = std::chrono::steady_clock::now();

	        glClearColor(0.015f, 0.025f, 0.04f, 1.0f);
	        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	        drawSelectedTiles(camera);
	        const auto drawEnd = std::chrono::steady_clock::now();

	        _lastDeltaSeconds = deltaSeconds;
	        _cameraDirty = false;
	        ++_frameUpdates;
	        _selectionMs = std::chrono::duration<double, std::milli>(drawStart - selectionStart).count();
	        _gpuDrawMs = std::chrono::duration<double, std::milli>(drawEnd - drawStart).count();
	        _selectionSkipped = !selectionUpdated;
	        const auto elapsed = drawEnd - start;
	        _drawMs = std::chrono::duration<double, std::milli>(elapsed).count();
	        if ((_frameUpdates % 60) == 0) {
	            __android_log_print(
	                ANDROID_LOG_INFO,
	                "CesiumBridge",
	                "frame total=%.3fms selection=%.3fms draw=%.3fms selectionSkipped=%d",
	                _drawMs,
	                _selectionMs,
	                _gpuDrawMs,
	                _selectionSkipped ? 1 : 0);
	        }
	    }

	    void clearMemory() {
	        _selectedTiles.clear();
	        _selectedResources.clear();
	        freeSelectedBatch();
	        _cameraDirty = true;
	        _selectionSettled = false;
	        _stableSelectionFrames = 0;
	        ++_memoryClears;
	    }

    jlong cameraUpdates() const { return _cameraUpdates; }
    jlong memoryClears() const { return _memoryClears; }
    jlong frameUpdates() const { return _frameUpdates; }
    bool cameraDirty() const { return _cameraDirty; }
    jlong recommendedFrameIntervalNanos() const {
        if (_cameraDirty || _tilesetRebuildRequested || _selectedResources.empty()) {
            return 16'666'667LL;
        }
        if (!_selectionSettled || _workerQueueLength > 0 || _mainQueueLength > 0) {
            return 33'333'333LL;
        }
        return 200'000'000LL;
    }
    double lastDeltaSeconds() const { return _lastDeltaSeconds; }
    int32_t selectedTiles() const { return static_cast<int32_t>(_selectedTiles.size()); }
    int32_t loadedTiles() const { return _tileset ? _tileset->getNumberOfTilesLoaded() : 0; }
    double drawMs() const { return _drawMs; }
    double gpuMemoryMb() const {
        size_t bytes = 0;
        std::unordered_set<const GpuTexture*> countedTextures;
        for (const GpuTileResources* resources : _selectedResources) {
            if (!resources) continue;
            bytes += resources->bytes;
            for (const GpuPrimitive& primitive : resources->primitives) {
                const GpuTexture* texture = primitive.textureResource.get();
                if (texture && countedTextures.insert(texture).second) {
                    bytes += texture->bytes;
                }
            }
        }
        return static_cast<double>(bytes) / 1024.0 / 1024.0;
    }
    const EcefPosition& ecef() const { return _ecef; }

private:
    static Cesium3DTilesSelection::TilesetOptions createTilesetOptions(double maximumScreenSpaceError) {
        Cesium3DTilesSelection::TilesetOptions options;
	        options.maximumScreenSpaceError = maximumScreenSpaceError;
	        options.enableFrustumCulling = true;
	        options.enableFogCulling = false;
	        options.enableOcclusionCulling = false;
	        options.ellipsoid = CesiumGeospatial::Ellipsoid::WGS84;
	        return options;
	    }

    void applyPendingTilesetOptions() {
        double maximumScreenSpaceError = 4.0;
        bool rebuildRequested = false;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            maximumScreenSpaceError = _maximumScreenSpaceError;
            rebuildRequested = _tilesetRebuildRequested;
            _tilesetRebuildRequested = false;
        }
        if (!rebuildRequested) return;
        recreateTileset(maximumScreenSpaceError);
    }

    void recreateTileset(double maximumScreenSpaceError) {
        _selectedTiles.clear();
        _selectedResources.clear();
        freeSelectedBatch();
        _batchedPrimitives.clear();
        _tileset = Cesium3DTilesSelection::EllipsoidTilesetLoader::createTileset(
            _externals,
            createTilesetOptions(maximumScreenSpaceError));
        _tilesVisited = 0;
        _workerQueueLength = 0;
        _mainQueueLength = 0;
        _cameraDirty = true;
        _selectionSkipped = false;
        _selectionSettled = false;
        _stableSelectionFrames = 0;
        _lastSettledLoadedTiles = 0;
        _lastSettledSelectedResources = 0;
    }

    static double cameraHeightMeters(double zoom) {
        return 20000000.0 / std::pow(2.0, zoom - 3.0);
    }

	    bool updateTileSelection(const CameraState& camera, double deltaSeconds) {
	        if (!_tileset) return false;
	        if (!_cameraDirty && _selectionSettled && !_selectedResources.empty()) {
	            _tileset->getAsyncSystem().dispatchMainThreadTasks();
	            return false;
	        }
	        _tileset->getAsyncSystem().dispatchMainThreadTasks();

        const CesiumGeospatial::Cartographic eyeCartographic =
            CesiumGeospatial::Cartographic::fromDegrees(
                camera.longitudeDegrees,
                camera.latitudeDegrees,
                cameraHeightMeters(camera.zoom));
        const CesiumGeospatial::Cartographic surfaceCartographic =
            CesiumGeospatial::Cartographic::fromDegrees(
                camera.longitudeDegrees,
                camera.latitudeDegrees,
                0.0);
        const glm::dvec3 eye =
            CesiumGeospatial::Ellipsoid::WGS84.cartographicToCartesian(eyeCartographic);
        const glm::dvec3 surfaceNormal =
            CesiumGeospatial::Ellipsoid::WGS84.geodeticSurfaceNormal(surfaceCartographic);
        glm::dvec3 east = glm::normalize(glm::cross(glm::dvec3(0.0, 0.0, 1.0), surfaceNormal));
        if (glm::length(east) < 0.000001) {
            east = glm::dvec3(1.0, 0.0, 0.0);
        }
        const glm::dvec3 north = glm::normalize(glm::cross(surfaceNormal, east));
        const double bearingRadians = camera.bearingDegrees * kPi / 180.0;
        const double pitchRadians = camera.pitchDegrees * kPi / 180.0;
        const glm::dvec3 horizontalForward =
            glm::normalize(north * std::cos(bearingRadians) + east * std::sin(bearingRadians));
        const glm::dvec3 direction =
            glm::normalize((-surfaceNormal) * std::cos(pitchRadians) + horizontalForward * std::sin(pitchRadians));
        const glm::dvec3 right = glm::normalize(glm::cross(horizontalForward, surfaceNormal));
        const glm::dvec3 up = glm::normalize(glm::cross(right, direction));

        const double aspect = static_cast<double>(_width) / static_cast<double>(_height);
        const double verticalFov = kPi / 3.0;
        const double horizontalFov = 2.0 * std::atan(std::tan(verticalFov * 0.5) * aspect);
        Cesium3DTilesSelection::ViewState view(
            eye,
            direction,
            up,
            glm::dvec2(static_cast<double>(_width), static_cast<double>(_height)),
            horizontalFov,
            verticalFov,
            CesiumGeospatial::Ellipsoid::WGS84);

        const Cesium3DTilesSelection::ViewUpdateResult& result =
            _tileset->updateViewGroup(
                _tileset->getDefaultViewGroup(),
                std::vector<Cesium3DTilesSelection::ViewState>{view},
                static_cast<float>(deltaSeconds));
        _tileset->loadTiles();
        _tileset->getAsyncSystem().dispatchMainThreadTasks();

        _selectedTiles.clear();
        _selectedTiles.reserve(result.tilesToRenderThisFrame.size());
        _selectedResources.clear();
        _selectedResources.reserve(result.tilesToRenderThisFrame.size());
        size_t selectedPrimitiveCount = 0;
        for (const Cesium3DTilesSelection::Tile::ConstPointer& tile : result.tilesToRenderThisFrame) {
            const Cesium3DTilesSelection::TileRenderContent* renderContent =
                tile->getContent().getRenderContent();
	            if (renderContent) {
	                const void* rawResources = renderContent->getRenderResources();
	                const auto* resources = MinimalPrepareRendererResources::isGpuResource(rawResources)
	                    ? reinterpret_cast<const GpuTileResources*>(rawResources)
	                    : nullptr;
	                _selectedResources.push_back(resources);
	                if (resources) selectedPrimitiveCount += resources->primitives.size();
	            }

            const CesiumGeospatial::BoundingRegion* region =
                Cesium3DTilesSelection::getBoundingRegionFromBoundingVolume(tile->getBoundingVolume());
            if (!region) continue;

            const CesiumGeospatial::GlobeRectangle& rectangle = region->getRectangle();
            SelectedTile selected;
            selected.west = rectangle.getWest();
            selected.south = rectangle.getSouth();
            selected.east = rectangle.getEast();
            selected.north = rectangle.getNorth();
            if (const auto* id = std::get_if<CesiumGeometry::QuadtreeTileID>(&tile->getTileID())) {
                selected.level = id->level;
                selected.x = id->x;
                selected.y = id->y;
            }
            _selectedTiles.push_back(selected);
        }
	        _tilesVisited = result.tilesVisited;
	        _workerQueueLength = result.workerThreadTileLoadQueueLength;
	        _mainQueueLength = result.mainThreadTileLoadQueueLength;
	        updateSelectionSettledState(camera);
	        if ((_frameUpdates % 60) == 0) {
            __android_log_print(
                ANDROID_LOG_INFO,
                "CesiumBridge",
                "tiles render=%zu loaded=%d visited=%u culled=%u workerQ=%d mainQ=%d maxDepth=%u",
                result.tilesToRenderThisFrame.size(),
                _tileset->getNumberOfTilesLoaded(),
                result.tilesVisited,
                result.tilesCulled,
                result.workerThreadTileLoadQueueLength,
                result.mainThreadTileLoadQueueLength,
                result.maxDepthVisited);
	            __android_log_print(
	                ANDROID_LOG_INFO,
	                "CesiumBridge",
	                "gpu resources=%zu primitives=%zu batched=%zu layers=%zu bytes=%.2fMB selection=%.3fms draw=%.3fms skipped=%d",
	                _selectedResources.size(),
	                selectedPrimitiveCount,
	                _selectedBatch.primitiveCount,
	                _selectedBatch.textureLayers,
	                gpuMemoryMb(),
	                _selectionMs,
	                _gpuDrawMs,
	                _selectionSkipped ? 1 : 0);
	        }
	        return true;
	    }

	    void updateSelectionSettledState(const CameraState& camera) {
	        const int32_t currentLoadedTiles = this->loadedTiles();
	        const size_t selectedResources = _selectedResources.size();
	        const size_t minimumResources =
	            camera.zoom >= 15.0 ? 64 :
	            camera.zoom >= 13.0 ? 64 :
	            camera.zoom >= 10.0 ? 24 :
	            4;
	        const bool queuesEmpty = _workerQueueLength == 0 && _mainQueueLength == 0;
	        const bool enoughDetail = selectedResources >= minimumResources;
	        const bool unchanged =
	            currentLoadedTiles == _lastSettledLoadedTiles &&
	            selectedResources == _lastSettledSelectedResources;

	        if (queuesEmpty && enoughDetail && unchanged) {
	            ++_stableSelectionFrames;
	        } else {
	            _stableSelectionFrames = 0;
	        }

	        _lastSettledLoadedTiles = currentLoadedTiles;
	        _lastSettledSelectedResources = selectedResources;
	        _selectionSettled = _stableSelectionFrames >= 30;
	    }

	    void freeSelectedBatch() {
	        freeBatchResources(_selectedBatch, _batchedPrimitives);
	    }

	    void rebuildSelectedBatch() {
	        rebuildSelectedBatchResources(
	            _selectedResources,
	            _batchProgram,
	            _selectedBatch,
	            _batchedPrimitives);
	    }

	    void drawSelectedTiles(const CameraState& camera) {
	        if (_program == 0) return;
	
	        const double height = cameraHeightMeters(camera.zoom);
	        const CesiumGeospatial::Cartographic eyeCartographic =
	            CesiumGeospatial::Cartographic::fromDegrees(
	                camera.longitudeDegrees,
	                camera.latitudeDegrees,
	                height);
	        const CesiumGeospatial::Cartographic surfaceCartographic =
	            CesiumGeospatial::Cartographic::fromDegrees(
	                camera.longitudeDegrees,
	                camera.latitudeDegrees,
	                0.0);
	        const glm::dvec3 eye =
	            CesiumGeospatial::Ellipsoid::WGS84.cartographicToCartesian(eyeCartographic);
	        const glm::dvec3 surfaceNormal =
	            CesiumGeospatial::Ellipsoid::WGS84.geodeticSurfaceNormal(surfaceCartographic);
	        glm::dvec3 east = glm::normalize(glm::cross(glm::dvec3(0.0, 0.0, 1.0), surfaceNormal));
	        if (glm::length(east) < 0.000001) {
	            east = glm::dvec3(1.0, 0.0, 0.0);
	        }
	        const glm::dvec3 north = glm::normalize(glm::cross(surfaceNormal, east));
	        const double bearingRadians = camera.bearingDegrees * kPi / 180.0;
	        const double pitchRadians = camera.pitchDegrees * kPi / 180.0;
	        const glm::dvec3 horizontalForward =
	            glm::normalize(north * std::cos(bearingRadians) + east * std::sin(bearingRadians));
	        const glm::dvec3 forward =
	            glm::normalize((-surfaceNormal) * std::cos(pitchRadians) + horizontalForward * std::sin(pitchRadians));
	        const glm::dvec3 right = glm::normalize(glm::cross(horizontalForward, surfaceNormal));
	        const glm::dvec3 up = glm::normalize(glm::cross(right, forward));
	        const glm::dvec3 backward = -forward;

	        const float aspect = static_cast<float>(_width) / static_cast<float>(_height);
	        const float verticalFov = static_cast<float>(kPi / 3.0);
	        const float nearPlane = static_cast<float>(std::max(1.0, height * 0.002));
	        const float farPlane = static_cast<float>(std::max(100000.0, height + 13000000.0));
	        const glm::mat4 projection = glm::perspective(verticalFov, aspect, nearPlane, farPlane);
		
	        if (_selectedBatch.valid && _selectedBatch.indexCount > 0) {
	            const glm::dvec3 batchOriginEye = _selectedBatch.originEcef - eye;
	            glUseProgram(_batchProgram);
	            glUniformMatrix4fv(_batchLocations.projection, 1, GL_FALSE, &projection[0][0]);
	            glUniform3f(
	                _batchLocations.originEye,
	                static_cast<float>(batchOriginEye.x),
	                static_cast<float>(batchOriginEye.y),
	                static_cast<float>(batchOriginEye.z));
	            glUniform3f(_batchLocations.right, static_cast<float>(right.x), static_cast<float>(right.y), static_cast<float>(right.z));
	            glUniform3f(_batchLocations.up, static_cast<float>(up.x), static_cast<float>(up.y), static_cast<float>(up.z));
	            glUniform3f(_batchLocations.backward, static_cast<float>(backward.x), static_cast<float>(backward.y), static_cast<float>(backward.z));
	            glUniform1i(_batchLocations.texture, 0);
	            glActiveTexture(GL_TEXTURE0);
	            glBindTexture(GL_TEXTURE_2D_ARRAY, _selectedBatch.textureArray);
	            glBindBuffer(GL_ARRAY_BUFFER, _selectedBatch.vertexBuffer);
	            glEnableVertexAttribArray(0);
	            glEnableVertexAttribArray(1);
	            glEnableVertexAttribArray(2);
	            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
	            glVertexAttribPointer(
	                1,
	                2,
	                GL_FLOAT,
	                GL_FALSE,
	                6 * sizeof(float),
	                reinterpret_cast<const void*>(3 * sizeof(float)));
	            glVertexAttribPointer(
	                2,
	                1,
	                GL_FLOAT,
	                GL_FALSE,
	                6 * sizeof(float),
	                reinterpret_cast<const void*>(5 * sizeof(float)));
	            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _selectedBatch.indexBuffer);
	            glDrawElements(GL_TRIANGLES, _selectedBatch.indexCount, GL_UNSIGNED_INT, nullptr);
	            glDisableVertexAttribArray(2);
	            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	        }

	        glUseProgram(_program);
	        glUniformMatrix4fv(_locations.projection, 1, GL_FALSE, &projection[0][0]);
	        glUniform3f(_locations.right, static_cast<float>(right.x), static_cast<float>(right.y), static_cast<float>(right.z));
	        glUniform3f(_locations.up, static_cast<float>(up.x), static_cast<float>(up.y), static_cast<float>(up.z));
	        glUniform3f(_locations.backward, static_cast<float>(backward.x), static_cast<float>(backward.y), static_cast<float>(backward.z));
	        glUniform1i(_locations.texture, 0);
	        glEnableVertexAttribArray(0);
	        glEnableVertexAttribArray(1);
	
	        for (const GpuTileResources* resources : _selectedResources) {
	            if (!resources) continue;
	            for (const GpuPrimitive& primitive : resources->primitives) {
	                if (_batchedPrimitives.find(&primitive) != _batchedPrimitives.end()) {
	                    continue;
	                }
	                if (primitive.vertexBuffer == 0 || primitive.indexBuffer == 0 || primitive.texture == 0) {
	                    continue;
	                }
	                const glm::dvec3 originEye = primitive.originEcef - eye;
	                glUniform3f(
	                    _locations.originEye,
	                    static_cast<float>(originEye.x),
	                    static_cast<float>(originEye.y),
	                    static_cast<float>(originEye.z));
	                glActiveTexture(GL_TEXTURE0);
	                glBindTexture(GL_TEXTURE_2D, primitive.texture);
	                glBindBuffer(GL_ARRAY_BUFFER, primitive.vertexBuffer);
	                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
	                glVertexAttribPointer(
	                    1,
	                    2,
	                    GL_FLOAT,
	                    GL_FALSE,
	                    5 * sizeof(float),
	                    reinterpret_cast<const void*>(3 * sizeof(float)));
	                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, primitive.indexBuffer);
	                glDrawElements(GL_TRIANGLES, primitive.indexCount, primitive.indexType, nullptr);
	            }
	        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	        glBindBuffer(GL_ARRAY_BUFFER, 0);
	        glBindTexture(GL_TEXTURE_2D, 0);
	        glDisableVertexAttribArray(0);
	        glDisableVertexAttribArray(1);
	    }

    std::mutex _mutex;
    std::shared_ptr<InlineTaskProcessor> _taskProcessor;
    std::shared_ptr<MinimalPrepareRendererResources> _prepareRendererResources;
    Cesium3DTilesSelection::TilesetExternals _externals;
    std::unique_ptr<Cesium3DTilesSelection::Tileset> _tileset;
    CameraState _camera;
    EcefPosition _ecef;
	    std::vector<SelectedTile> _selectedTiles;
	    std::vector<const GpuTileResources*> _selectedResources;
	    std::unordered_set<const GpuPrimitive*> _batchedPrimitives;
	    BatchResources _selectedBatch;
	    GLuint _program = 0;
	    GLuint _batchProgram = 0;
	    ProgramLocations _locations;
	    ProgramLocations _batchLocations;
	    int _width = 1;
	    int _height = 1;
    uint32_t _tilesVisited = 0;
    int32_t _workerQueueLength = 0;
    int32_t _mainQueueLength = 0;
    jlong _cameraUpdates = 0;
    jlong _memoryClears = 0;
    jlong _frameUpdates = 0;
    double _maximumScreenSpaceError = 4.0;
	    bool _cameraDirty = false;
	    bool _tilesetRebuildRequested = false;
	    bool _selectionSkipped = false;
	    bool _selectionSettled = false;
	    int32_t _stableSelectionFrames = 0;
	    int32_t _lastSettledLoadedTiles = 0;
	    size_t _lastSettledSelectedResources = 0;
	    double _lastDeltaSeconds = 0.0;
	    double _drawMs = 0.0;
	    double _selectionMs = 0.0;
	    double _gpuDrawMs = 0.0;
	};

CesiumBridge* fromHandle(jlong handle) {
    return reinterpret_cast<CesiumBridge*>(static_cast<intptr_t>(handle));
}

jstring newString(JNIEnv* env, const std::string& value) {
    return env->NewStringUTF(value.c_str());
}

} // namespace

extern "C" size_t fwrite_unlocked(const void* buffer, size_t size, size_t count, FILE* stream) {
    return std::fwrite(buffer, size, count, stream);
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeCreate(
    JNIEnv*,
    jobject) {
    auto bridge = std::make_unique<CesiumBridge>();
    return static_cast<jlong>(reinterpret_cast<intptr_t>(bridge.release()));
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeDestroy(
    JNIEnv*,
    jobject,
    jlong handle) {
    delete fromHandle(handle);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeUpdateCamera(
    JNIEnv*,
    jobject,
    jlong handle,
    jdouble longitude,
    jdouble latitude,
    jdouble zoom,
    jboolean autoOrbit,
    jdouble bearing,
    jdouble pitch) {
    if (auto* bridge = fromHandle(handle)) {
        bridge->updateCamera({longitude, latitude, zoom, autoOrbit == JNI_TRUE, bearing, pitch});
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeOnSurfaceCreated(
    JNIEnv*,
    jobject,
    jlong handle) {
    if (auto* bridge = fromHandle(handle)) {
        bridge->onSurfaceCreated();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeOnSurfaceChanged(
    JNIEnv*,
    jobject,
    jlong handle,
    jint width,
    jint height) {
    if (auto* bridge = fromHandle(handle)) {
        bridge->onSurfaceChanged(width, height);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeRenderFrame(
    JNIEnv*,
    jobject,
    jlong handle,
    jint width,
    jint height,
    jdouble deltaSeconds) {
    if (auto* bridge = fromHandle(handle)) {
        bridge->renderFrame(width, height, deltaSeconds);
    }
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeRecommendedFrameIntervalNanos(
    JNIEnv*,
    jobject,
    jlong handle) {
    if (auto* bridge = fromHandle(handle)) {
        return bridge->recommendedFrameIntervalNanos();
    }
    return 16'666'667LL;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeSetMaximumScreenSpaceError(
    JNIEnv*,
    jobject,
    jlong handle,
    jdouble maximumScreenSpaceError) {
    if (auto* bridge = fromHandle(handle)) {
        bridge->setMaximumScreenSpaceError(maximumScreenSpaceError);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeClearMemory(
    JNIEnv*,
    jobject,
    jlong handle) {
    if (auto* bridge = fromHandle(handle)) {
        bridge->clearMemory();
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeIsCesiumNativeLinked(
    JNIEnv*,
    jobject) {
    return JNI_TRUE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeBackendName(
    JNIEnv* env,
    jobject) {
    return newString(env, "cesium-native-gpu");
}

extern "C" JNIEXPORT jdoubleArray JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeCameraEcef(
    JNIEnv* env,
    jobject,
    jlong handle) {
    jdouble values[3] = {0.0, 0.0, 0.0};
    if (auto* bridge = fromHandle(handle)) {
        const auto& ecef = bridge->ecef();
        values[0] = ecef.x;
        values[1] = ecef.y;
        values[2] = ecef.z;
    }
    jdoubleArray array = env->NewDoubleArray(3);
    env->SetDoubleArrayRegion(array, 0, 3, values);
    return array;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeCameraUpdates(JNIEnv*, jobject, jlong handle) {
    if (auto* bridge = fromHandle(handle)) return bridge->cameraUpdates();
    return 0;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeMemoryClears(JNIEnv*, jobject, jlong handle) {
    if (auto* bridge = fromHandle(handle)) return bridge->memoryClears();
    return 0;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeFrameUpdates(JNIEnv*, jobject, jlong handle) {
    if (auto* bridge = fromHandle(handle)) return bridge->frameUpdates();
    return 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeCameraDirty(JNIEnv*, jobject, jlong handle) {
    if (auto* bridge = fromHandle(handle)) return bridge->cameraDirty() ? JNI_TRUE : JNI_FALSE;
    return JNI_FALSE;
}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeLastDeltaSeconds(JNIEnv*, jobject, jlong handle) {
    if (auto* bridge = fromHandle(handle)) return bridge->lastDeltaSeconds();
    return 0.0;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeSelectedTiles(JNIEnv*, jobject, jlong handle) {
    if (auto* bridge = fromHandle(handle)) return bridge->selectedTiles();
    return 0;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeLoadedTiles(JNIEnv*, jobject, jlong handle) {
    if (auto* bridge = fromHandle(handle)) return bridge->loadedTiles();
    return 0;
}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeDrawMs(JNIEnv*, jobject, jlong handle) {
    if (auto* bridge = fromHandle(handle)) return bridge->drawMs();
    return 0.0;
}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeGpuMemoryMb(JNIEnv*, jobject, jlong handle) {
    if (auto* bridge = fromHandle(handle)) return bridge->gpuMemoryMb();
    return 0.0;
}
