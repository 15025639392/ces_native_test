#include <jni.h>

#include "cesium_types.h"
#include "background_task_processor.h"
#include "curl_asset_accessor.h"
#include "gl_resources.h"
#include "imagery_source_config.h"
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
#include <CesiumGeometry/IntersectionTests.h>
#include <CesiumGeometry/QuadtreeTileID.h>
#include <CesiumGeometry/Ray.h>
#include <CesiumGeospatial/BoundingRegion.h>
#include <CesiumGeospatial/Cartographic.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumGeospatial/WebMercatorProjection.h>
#include <CesiumRasterOverlays/RasterOverlay.h>
#include <CesiumRasterOverlays/UrlTemplateRasterOverlay.h>
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
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace {

using namespace cesium_poc;

std::string xyzUrlTemplateToCesiumTemplate(std::string urlTemplate) {
    size_t pos = 0;
    while ((pos = urlTemplate.find("{y}", pos)) != std::string::npos) {
        urlTemplate.replace(pos, 3, "{reverseY}");
        pos += 10;
    }
    pos = 0;
    while ((pos = urlTemplate.find("{s}", pos)) != std::string::npos) {
        urlTemplate.replace(pos, 3, "1");
        pos += 1;
    }
    return urlTemplate;
}

double clampDouble(double value, double minimum, double maximum) {
    return std::min(std::max(value, minimum), maximum);
}

double wrapLongitudeDegrees(double value) {
    while (value > 180.0) value -= 360.0;
    while (value < -180.0) value += 360.0;
    return value;
}

double wrapDegrees(double value) {
    while (value >= 360.0) value -= 360.0;
    while (value < 0.0) value += 360.0;
    return value;
}

double horizonDistanceMeters(double altitudeMeters) {
    constexpr double meanEarthRadiusMeters = 6'371'000.0;
    const double h = std::max(altitudeMeters, 0.0);
    return std::sqrt(h * (2.0 * meanEarthRadiusMeters + h));
}

double angularDistanceDegrees(double lonA, double latA, double lonB, double latB) {
    const double latARadians = latA * kPi / 180.0;
    const double latBRadians = latB * kPi / 180.0;
    const double deltaLat = (latB - latA) * kPi / 180.0;
    const double deltaLon = wrapLongitudeDegrees(lonB - lonA) * kPi / 180.0;
    const double sinHalfLat = std::sin(deltaLat * 0.5);
    const double sinHalfLon = std::sin(deltaLon * 0.5);
    const double a =
        sinHalfLat * sinHalfLat +
        std::cos(latARadians) * std::cos(latBRadians) * sinHalfLon * sinHalfLon;
    return 2.0 * std::asin(std::min(1.0, std::sqrt(a))) * 180.0 / kPi;
}

CameraState normalizeGlobeCameraCoordinates(CameraState camera) {
    while (camera.latitudeDegrees > 90.0) {
        camera.latitudeDegrees = 180.0 - camera.latitudeDegrees;
        camera.longitudeDegrees += 180.0;
        camera.bearingDegrees += 180.0;
    }
    while (camera.latitudeDegrees < -90.0) {
        camera.latitudeDegrees = -180.0 - camera.latitudeDegrees;
        camera.longitudeDegrees += 180.0;
        camera.bearingDegrees += 180.0;
    }
    constexpr double poleEpsilonDegrees = 0.000001;
    camera.latitudeDegrees = clampDouble(
        camera.latitudeDegrees,
        -90.0 + poleEpsilonDegrees,
        90.0 - poleEpsilonDegrees);
    return camera;
}

CameraState sanitizeCamera(CameraState camera) {
    camera = normalizeGlobeCameraCoordinates(camera);
    camera.longitudeDegrees = wrapLongitudeDegrees(camera.longitudeDegrees);
    camera.altitudeMeters = clampDouble(camera.altitudeMeters, 500.0, 20'000'000.0);
    camera.bearingDegrees = wrapDegrees(camera.bearingDegrees);
    camera.pitchDegrees = clampDouble(camera.pitchDegrees, 0.0, 85.0);
    return camera;
}

struct NativeCameraFrame {
    glm::dvec3 eye = glm::dvec3(0.0);
    glm::dvec3 direction = glm::dvec3(0.0, 0.0, -1.0);
    glm::dvec3 right = glm::dvec3(1.0, 0.0, 0.0);
    glm::dvec3 up = glm::dvec3(0.0, 1.0, 0.0);
};

struct LocalTangentFrame {
    glm::dvec3 east = glm::dvec3(1.0, 0.0, 0.0);
    glm::dvec3 north = glm::dvec3(0.0, 1.0, 0.0);
    glm::dvec3 up = glm::dvec3(0.0, 0.0, 1.0);
};

LocalTangentFrame buildLocalTangentFrame(double longitudeDegrees, double latitudeDegrees) {
    const CesiumGeospatial::Cartographic surfaceCartographic =
        CesiumGeospatial::Cartographic::fromDegrees(longitudeDegrees, latitudeDegrees, 0.0);
    const glm::dvec3 up =
        CesiumGeospatial::Ellipsoid::WGS84.geodeticSurfaceNormal(surfaceCartographic);
    const double longitudeRadians = longitudeDegrees * kPi / 180.0;
    glm::dvec3 east(-std::sin(longitudeRadians), std::cos(longitudeRadians), 0.0);
    if (glm::length(east) < 0.000001) {
        east = glm::dvec3(1.0, 0.0, 0.0);
    } else {
        east = glm::normalize(east);
    }
    const glm::dvec3 north = glm::normalize(glm::cross(up, east));
    return {east, north, up};
}

NativeCameraFrame buildCameraFrame(const CameraState& camera) {
    const CesiumGeospatial::Cartographic eyeCartographic =
        CesiumGeospatial::Cartographic::fromDegrees(
            camera.longitudeDegrees,
            camera.latitudeDegrees,
            camera.altitudeMeters);
    const glm::dvec3 eye =
        CesiumGeospatial::Ellipsoid::WGS84.cartographicToCartesian(eyeCartographic);
    const LocalTangentFrame local =
        buildLocalTangentFrame(camera.longitudeDegrees, camera.latitudeDegrees);
    const double bearingRadians = camera.bearingDegrees * kPi / 180.0;
    const double pitchRadians = camera.pitchDegrees * kPi / 180.0;
    const glm::dvec3 horizontalForward =
        glm::normalize(local.north * std::cos(bearingRadians) + local.east * std::sin(bearingRadians));
    const glm::dvec3 direction =
        glm::normalize((-local.up) * std::cos(pitchRadians) + horizontalForward * std::sin(pitchRadians));
    const glm::dvec3 right = glm::normalize(glm::cross(horizontalForward, local.up));
    const glm::dvec3 up = glm::normalize(glm::cross(right, direction));
    return {eye, direction, right, up};
}

std::optional<CesiumGeospatial::Cartographic> pickEllipsoid(
    const CameraState& camera,
    int width,
    int height,
    double screenX,
    double screenY) {
    const NativeCameraFrame frame = buildCameraFrame(camera);
    const double viewportWidth = static_cast<double>(std::max(width, 1));
    const double viewportHeight = static_cast<double>(std::max(height, 1));
    const double aspect = viewportWidth / viewportHeight;
    const double verticalFov = kPi / 3.0;
    const double tanVertical = std::tan(verticalFov * 0.5);
    const double ndcX = (2.0 * screenX / viewportWidth) - 1.0;
    const double ndcY = 1.0 - (2.0 * screenY / viewportHeight);
    const glm::dvec3 direction = glm::normalize(
        frame.direction +
        frame.right * (ndcX * tanVertical * aspect) +
        frame.up * (ndcY * tanVertical));
    const CesiumGeometry::Ray ray(frame.eye, direction);
    const std::optional<glm::dvec2> interval =
        CesiumGeometry::IntersectionTests::rayEllipsoid(
            ray,
            CesiumGeospatial::Ellipsoid::WGS84.getRadii());
    if (!interval) return std::nullopt;
    const double distance = interval->x >= 0.0 ? interval->x : interval->y;
    if (distance < 0.0) return std::nullopt;
    const glm::dvec3 hit = ray.pointFromDistance(distance);
    return CesiumGeospatial::Ellipsoid::WGS84.cartesianToCartographic(hit);
}

CameraState shiftCameraByMeters(const CameraState& camera, double eastMeters, double northMeters) {
    const double latRadians = camera.latitudeDegrees * kPi / 180.0;
    const double metersPerDegreeLatitude = 110'540.0;
    const double metersPerDegreeLongitude = std::max(111'320.0 * std::cos(latRadians), 1'000.0);
    CameraState next = camera;
    next.longitudeDegrees = camera.longitudeDegrees + eastMeters / metersPerDegreeLongitude;
    next.latitudeDegrees = camera.latitudeDegrees + northMeters / metersPerDegreeLatitude;
    return sanitizeCamera(next);
}

CameraState shiftCameraByScreenPick(
    const CameraState& camera,
    int width,
    int height,
    double previousX,
    double previousY,
    double currentX,
    double currentY) {
    const std::optional<CesiumGeospatial::Cartographic> previousPick =
        pickEllipsoid(camera, width, height, previousX, previousY);
    const std::optional<CesiumGeospatial::Cartographic> currentPick =
        pickEllipsoid(camera, width, height, currentX, currentY);
    if (!previousPick || !currentPick) {
        const double metersPerPixel =
            2.0 * camera.altitudeMeters * std::tan((kPi / 3.0) * 0.5) / static_cast<double>(std::max(height, 1));
        const double eastMeters = (currentX - previousX) * metersPerPixel;
        const double northMeters = -(currentY - previousY) * metersPerPixel;
        return shiftCameraByMeters(camera, -eastMeters, -northMeters);
    }

    const glm::dvec3 previousCartesian =
        CesiumGeospatial::Ellipsoid::WGS84.cartographicToCartesian(*previousPick);
    const glm::dvec3 currentCartesian =
        CesiumGeospatial::Ellipsoid::WGS84.cartographicToCartesian(*currentPick);
    const glm::dvec3 surfaceDelta = currentCartesian - previousCartesian;
    const LocalTangentFrame local =
        buildLocalTangentFrame(camera.longitudeDegrees, camera.latitudeDegrees);
    const double eastMeters = glm::dot(surfaceDelta, local.east);
    const double northMeters = glm::dot(surfaceDelta, local.north);
    return shiftCameraByMeters(camera, -eastMeters, -northMeters);
}

CameraState keepFocusStable(
    const CameraState& oldCamera,
    const CameraState& newCamera,
    int width,
    int height,
    double focusX,
    double focusY) {
    const std::optional<CesiumGeospatial::Cartographic> oldPick =
        pickEllipsoid(oldCamera, width, height, focusX, focusY);
    const std::optional<CesiumGeospatial::Cartographic> newPick =
        pickEllipsoid(newCamera, width, height, focusX, focusY);
    if (!oldPick || !newPick) return newCamera;

    const glm::dvec3 oldCartesian =
        CesiumGeospatial::Ellipsoid::WGS84.cartographicToCartesian(*oldPick);
    const glm::dvec3 newCartesian =
        CesiumGeospatial::Ellipsoid::WGS84.cartographicToCartesian(*newPick);
    const glm::dvec3 focusDelta = oldCartesian - newCartesian;
    const LocalTangentFrame local =
        buildLocalTangentFrame(newCamera.longitudeDegrees, newCamera.latitudeDegrees);
    const double eastMeters = glm::dot(focusDelta, local.east);
    const double northMeters = glm::dot(focusDelta, local.north);
    return shiftCameraByMeters(newCamera, eastMeters, northMeters);
}

class GlobeCameraController {
public:
    CameraState pan(
        const CameraState& camera,
        int width,
        int height,
        double currentX,
        double currentY,
        double distanceX,
        double distanceY,
        double sensitivity) const {
        const double previousX = currentX + distanceX * sensitivity;
        const double previousY = currentY + distanceY * sensitivity;
        CameraState next = shiftCameraByScreenPick(camera, width, height, previousX, previousY, currentX, currentY);
        next.autoOrbit = false;
        return next;
    }

    CameraState zoom(
        const CameraState& camera,
        int width,
        int height,
        double scale,
        double focusX,
        double focusY) const {
        if (scale <= 0.0) return camera;
        CameraState next = camera;
        next.altitudeMeters = camera.altitudeMeters * scale;
        next.autoOrbit = false;
        return keepFocusStable(camera, sanitizeCamera(next), width, height, focusX, focusY);
    }

    CameraState zoomFromCenter(const CameraState& camera, double scale) const {
        if (scale <= 0.0) return camera;
        CameraState next = camera;
        next.altitudeMeters = camera.altitudeMeters * scale;
        next.autoOrbit = false;
        return sanitizeCamera(next);
    }

    CameraState rotate(
        const CameraState& camera,
        int width,
        int height,
        double bearingDeltaDegrees,
        double focusX,
        double focusY) const {
        CameraState next = camera;
        next.bearingDegrees += bearingDeltaDegrees;
        next.autoOrbit = false;
        return keepFocusStable(camera, sanitizeCamera(next), width, height, focusX, focusY);
    }

    CameraState tilt(
        const CameraState& camera,
        int width,
        int height,
        double pitchDeltaDegrees,
        double focusX,
        double focusY) const {
        CameraState next = camera;
        next.pitchDegrees += pitchDeltaDegrees;
        next.autoOrbit = false;
        return keepFocusStable(camera, sanitizeCamera(next), width, height, focusX, focusY);
    }

    CameraState orbit(const CameraState& camera, double deltaSeconds, double degreesPerSecond) const {
        if (!camera.autoOrbit) return camera;
        CameraState next = camera;
        next.longitudeDegrees += deltaSeconds * degreesPerSecond;
        return sanitizeCamera(next);
    }
};

class CesiumBridge {
public:
    CesiumBridge()
        : _taskProcessor(std::make_shared<BackgroundTaskProcessor>(8)),
          _prepareRendererResources(std::make_shared<MinimalPrepareRendererResources>()),
          _assetAccessor(createCurlAssetAccessor()),
          _externals{
              _assetAccessor,
              _prepareRendererResources,
              CesiumAsync::AsyncSystem(_taskProcessor),
              nullptr,
              spdlog::default_logger(),
              nullptr,
              Cesium3DTilesSelection::TilesetSharedAssetSystem::getDefault(),
              {}} {
        updateCamera(_camera);
    }

    ~CesiumBridge() {
        if (_program != 0) {
            glDeleteProgram(_program);
        }
        if (_textureArrayProgram != 0) {
            glDeleteProgram(_textureArrayProgram);
        }
        freeTextureArrayBatch();
        if (_fallbackTexture != 0) {
            glDeleteTextures(1, &_fallbackTexture);
        }
    }

    void updateCamera(const CameraState& camera) {
	        std::lock_guard<std::mutex> lock(_mutex);
            const CameraState sanitizedCamera = sanitizeCamera(camera);
	        const bool changed =
	            std::abs(_camera.longitudeDegrees - sanitizedCamera.longitudeDegrees) > 0.0000001 ||
	            std::abs(_camera.latitudeDegrees - sanitizedCamera.latitudeDegrees) > 0.0000001 ||
	            std::abs(_camera.altitudeMeters - sanitizedCamera.altitudeMeters) > 0.0001 ||
	            _camera.autoOrbit != sanitizedCamera.autoOrbit ||
	            std::abs(_camera.bearingDegrees - sanitizedCamera.bearingDegrees) > 0.0001 ||
	            std::abs(_camera.pitchDegrees - sanitizedCamera.pitchDegrees) > 0.0001;
	        _camera = sanitizedCamera;
        const CesiumGeospatial::Cartographic cartographic =
            CesiumGeospatial::Cartographic::fromDegrees(
                sanitizedCamera.longitudeDegrees,
                sanitizedCamera.latitudeDegrees,
                sanitizedCamera.altitudeMeters);
        const glm::dvec3 ecef =
            CesiumGeospatial::Ellipsoid::WGS84.cartographicToCartesian(cartographic);
        _ecef = {ecef.x, ecef.y, ecef.z};
	        if (changed) {
	            _cameraDirty = true;
	            _selectionSettled = false;
	            _stableSelectionFrames = 0;
                _lastCameraUpdateFrame = _frameUpdates;
	            ++_cameraUpdates;
	        }
	    }

    void setCameraMoving(bool moving) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_cameraMoving == moving) return;
        _cameraMoving = moving;
        if (moving) {
            _lastCameraUpdateFrame = _frameUpdates;
        }
        applyInteractiveTilesetOptionsLocked();
        _cameraDirty = true;
        _selectionSettled = false;
        _stableSelectionFrames = 0;
    }

    CameraState currentCamera() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _camera;
    }

    CameraState panCamera(double currentX, double currentY, double distanceX, double distanceY, double sensitivity) {
        CameraState camera;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            camera = _camera;
            if (_cameraMoving &&
                !camera.autoOrbit &&
                _frameUpdates > _lastCameraUpdateFrame + 24) {
                _cameraMoving = false;
                applyInteractiveTilesetOptionsLocked();
                _cameraDirty = true;
                _selectionSettled = false;
                _stableSelectionFrames = 0;
            }
        }
        CameraState next =
            _cameraController.pan(camera, _width, _height, currentX, currentY, distanceX, distanceY, sensitivity);
        setCameraMoving(true);
        updateCamera(next);
        return currentCamera();
    }

    CameraState scaleCamera(double scale, double focusX, double focusY) {
        CameraState camera;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            camera = _camera;
        }
        CameraState next = _cameraController.zoom(camera, _width, _height, scale, focusX, focusY);
        setCameraMoving(true);
        updateCamera(next);
        return currentCamera();
    }

    CameraState scaleCameraFromCenter(double scale) {
        CameraState camera;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            camera = _camera;
        }
        CameraState next = _cameraController.zoomFromCenter(camera, scale);
        setCameraMoving(true);
        updateCamera(next);
        return currentCamera();
    }

    CameraState rotateCamera(double bearingDeltaDegrees, double focusX, double focusY) {
        CameraState camera;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            camera = _camera;
        }
        camera = _cameraController.rotate(camera, _width, _height, bearingDeltaDegrees, focusX, focusY);
        setCameraMoving(true);
        updateCamera(camera);
        return currentCamera();
    }

    CameraState tiltCamera(double pitchDeltaDegrees, double focusX, double focusY) {
        CameraState camera;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            camera = _camera;
        }
        camera = _cameraController.tilt(camera, _width, _height, pitchDeltaDegrees, focusX, focusY);
        setCameraMoving(true);
        updateCamera(camera);
        return currentCamera();
    }

    CameraState orbitCamera(double deltaSeconds, double degreesPerSecond) {
        CameraState camera;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            camera = _camera;
        }
        camera = _cameraController.orbit(camera, deltaSeconds, degreesPerSecond);
        if (!camera.autoOrbit) return camera;
        setCameraMoving(true);
        updateCamera(camera);
        return currentCamera();
    }

	    void onSurfaceCreated() {
	        _program = createProgram();
            _textureArrayProgram = createTextureArrayProgram();
	        _locations.projection = glGetUniformLocation(_program, "u_projection");
	        _locations.originEye = glGetUniformLocation(_program, "u_originEye");
	        _locations.right = glGetUniformLocation(_program, "u_right");
	        _locations.up = glGetUniformLocation(_program, "u_up");
	        _locations.backward = glGetUniformLocation(_program, "u_backward");
	        _locations.texture = glGetUniformLocation(_program, "u_texture");
	        _locations.uvTranslation = glGetUniformLocation(_program, "u_uvTranslation");
	        _locations.uvScale = glGetUniformLocation(_program, "u_uvScale");
            _locations.discardOutsideUv = glGetUniformLocation(_program, "u_discardOutsideUv");
            _textureArrayLocations.projection = glGetUniformLocation(_textureArrayProgram, "u_projection");
            _textureArrayLocations.originEye = glGetUniformLocation(_textureArrayProgram, "u_originEye");
            _textureArrayLocations.right = glGetUniformLocation(_textureArrayProgram, "u_right");
            _textureArrayLocations.up = glGetUniformLocation(_textureArrayProgram, "u_up");
            _textureArrayLocations.backward = glGetUniformLocation(_textureArrayProgram, "u_backward");
            _textureArrayLocations.texture = glGetUniformLocation(_textureArrayProgram, "u_textureArray");
	        glEnable(GL_DEPTH_TEST);
	        glDepthFunc(GL_LEQUAL);
            glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (_fallbackTexture == 0) {
            const uint8_t pixels[4] = {126, 136, 118, 255};
            glGenTextures(1, &_fallbackTexture);
            glBindTexture(GL_TEXTURE_2D, _fallbackTexture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGBA,
                1,
                1,
                0,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        if (!_tileset) {
            recreateTileset(_maximumScreenSpaceError);
        }
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
        if (_tileset) {
            _tileset->getOptions().maximumScreenSpaceError = clamped;
            applyInteractiveTilesetOptionsLocked();
        }
        _cameraDirty = true;
        _selectionSettled = false;
        _stableSelectionFrames = 0;
    }

    void setImageryUrlTemplate(const std::string& urlTemplate) {
        if (satelliteImageryUrlTemplate() == urlTemplate) return;
        setSatelliteImageryUrlTemplate(urlTemplate);
        std::lock_guard<std::mutex> lock(_mutex);
        _tilesetRebuildRequested = true;
        _cameraDirty = true;
        _selectionSettled = false;
        _stableSelectionFrames = 0;
    }

	    void renderFrame(int width, int height, double deltaSeconds) {
	        const auto start = std::chrono::steady_clock::now();
	        onSurfaceChanged(width, height);
        applyPendingTilesetOptions();
        applyAdaptiveFrameBudget();

        CameraState camera;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            camera = _camera;
        }

	        const auto selectionStart = std::chrono::steady_clock::now();
	        const bool selectionUpdated = updateTileSelection(camera, deltaSeconds);
	        if (selectionUpdated) {
	            if (_selectionSettled) {
	                // Stable overlay attachments are drawn directly from cesium-native raster resources.
	            } else {
	            }
	        }
	        const auto drawStart = std::chrono::steady_clock::now();
            processRendererUploads();

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
	                "frame total=%.3fms selection=%.3fms draw=%.3fms pressure=%.2f pendingGeometryUploads=%zu pendingRasterUploads=%zu selectionSkipped=%d moving=%d mainDispatchSkipped=%d loadTilesSkipped=%d",
	                _drawMs,
	                _selectionMs,
	                _gpuDrawMs,
                    _framePressure,
                    _prepareRendererResources->pendingGeometryUploads(),
                    _prepareRendererResources->pendingRasterUploads(),
	                _selectionSkipped ? 1 : 0,
                    _cameraMoving ? 1 : 0,
                    _mainThreadDispatchSkipped ? 1 : 0,
                    _loadTilesSkipped ? 1 : 0);
	        }
	    }

	    void clearMemory() {
        _selectedTiles.clear();
        _selectedResources.clear();
        _selectedPrimitiveSet.clear();
        _continuityResources.clear();
        _uploadPriorityResources.clear();
        _selectedDrawablePrimitiveCount = 0;
        _selectedTexturedPrimitiveCount = 0;
        _selectedCameraAltitudeMeters = _camera.altitudeMeters;
        _selectedCameraLongitudeDegrees = _camera.longitudeDegrees;
        _selectedCameraLatitudeDegrees = _camera.latitudeDegrees;
        _selectedAverageTileLevel = 0.0;
        _batchSelectionGeneration = 0;
        ++_selectionGeneration;
        _consecutiveSelectionPreservationFrames = 0;
        _continuityResourceFramesRemaining = 0;
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
        if (_prepareRendererResources->pendingGeometryUploads() > 0 ||
            _prepareRendererResources->pendingRasterUploads() > 0 ||
            _workerQueueLength > 0 ||
            _mainQueueLength > 0) {
            return 16'666'667LL;
        }
        if (!_selectionSettled) {
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
                for (const RasterAttachment& attachment : primitive.rasterAttachments) {
                    const GpuTexture* texture = attachment.textureResource.get();
                    if (texture && countedTextures.insert(texture).second) {
                        bytes += texture->bytes;
                    }
                }
            }
        }
        return static_cast<double>(bytes) / 1024.0 / 1024.0;
    }
    const EcefPosition& ecef() const { return _ecef; }

private:
    struct DrawStats {
        size_t primitives = 0;
        size_t attachments = 0;
        size_t drawCalls = 0;
        size_t skippedByBudget = 0;
        size_t missingOverlayStreams = 0;
    };

    struct DrawCommand {
        const GpuPrimitive* primitive = nullptr;
        const RasterAttachment* attachment = nullptr;
        GLuint vertexBuffer = 0;
        double distanceSq = 0.0;
        bool continuity = false;
    };

    struct TextureArrayBatch {
        GLuint vertexBuffer = 0;
        GLuint indexBuffer = 0;
        GLuint textureArray = 0;
        GLsizei indexCount = 0;
        size_t primitiveCount = 0;
        std::vector<const GpuPrimitive*> primitives;
        int32_t width = 0;
        int32_t height = 0;
        glm::dvec3 originEcef = glm::dvec3(0.0);
        bool ownsTexture = true;
        bool valid = false;
    };

    static size_t countDrawablePrimitives(const std::vector<const GpuTileResources*>& resourcesList) {
        size_t drawable = 0;
        for (const GpuTileResources* resources : resourcesList) {
            if (!resources) continue;
            for (const GpuPrimitive& primitive : resources->primitives) {
                if (primitive.vertexBuffer != 0 &&
                    primitive.indexBuffer != 0) {
                    ++drawable;
                }
            }
        }
        return drawable;
    }

    static size_t countTexturedPrimitives(const std::vector<const GpuTileResources*>& resourcesList) {
        size_t textured = 0;
        for (const GpuTileResources* resources : resourcesList) {
            if (!resources) continue;
            for (const GpuPrimitive& primitive : resources->primitives) {
                if (primitive.vertexBuffer != 0 &&
                    primitive.indexBuffer != 0 &&
                    !primitive.rasterAttachments.empty()) {
                    ++textured;
                }
            }
        }
        return textured;
    }

    static double averageTileLevel(const std::vector<SelectedTile>& tiles) {
        if (tiles.empty()) return 0.0;
        double total = 0.0;
        for (const SelectedTile& tile : tiles) {
            total += static_cast<double>(tile.level);
        }
        return total / static_cast<double>(tiles.size());
    }

    static bool needsConservativeGlobeCulling(const CameraState& camera) {
        const double absLatitude = std::abs(camera.latitudeDegrees);
        if (absLatitude >= 72.0) return true;
        if (camera.altitudeMeters >= 8'000'000.0 && camera.pitchDegrees >= 45.0) return true;
        return false;
    }

    static Cesium3DTilesSelection::TilesetOptions createTilesetOptions(double maximumScreenSpaceError) {
        Cesium3DTilesSelection::TilesetOptions options;
        options.maximumScreenSpaceError = maximumScreenSpaceError;
        options.maximumSimultaneousTileLoads = 32;
        options.preloadSiblings = true;
        options.loadingDescendantLimit = 32;
        options.forbidHoles = true;
	        options.enableFrustumCulling = true;
	        options.enableFogCulling = false;
	        options.enableOcclusionCulling = true;
        options.renderTilesUnderCamera = false;
        options.maximumCachedBytes = 512LL * 1024 * 1024;
        options.mainThreadLoadingTimeLimit = 10.0;
        options.tileCacheUnloadTimeLimit = 0.25;
	        options.ellipsoid = CesiumGeospatial::Ellipsoid::WGS84;
	        return options;
	    }

    void applyInteractiveTilesetOptionsLocked() {
        if (!_tileset) return;
        Cesium3DTilesSelection::TilesetOptions& options = _tileset->getOptions();
        const bool conservativeCulling = needsConservativeGlobeCulling(_camera);
        options.maximumScreenSpaceError = _maximumScreenSpaceError;
        options.enableFrustumCulling = !conservativeCulling;
        options.enableOcclusionCulling = !conservativeCulling;
        options.renderTilesUnderCamera = conservativeCulling;
        options.maximumSimultaneousTileLoads = 32;
        options.loadingDescendantLimit = 32;
        options.mainThreadLoadingTimeLimit =
            _framePressure > 0.75 ? 4.0 :
            _framePressure > 0.35 ? 6.0 :
            8.0;
    }

    void applyAdaptiveFrameBudget() {
        if (!_tileset) return;
        const bool overloaded =
            _drawMs > 16.0 ||
            _selectionMs > 10.0 ||
            _mainQueueLength > 96 ||
            _workerQueueLength > 192;
        const bool comfortable =
            _drawMs > 0.0 &&
            _drawMs < 9.0 &&
            _selectionMs < 6.0 &&
            _mainQueueLength < 32 &&
            _workerQueueLength < 32;

        if (overloaded) {
            _framePressure = std::min(1.0, _framePressure + 0.18);
        } else if (comfortable) {
            _framePressure = std::max(0.0, _framePressure - 0.04);
        } else {
            _framePressure = std::max(0.0, _framePressure - 0.01);
        }

        Cesium3DTilesSelection::TilesetOptions& options = _tileset->getOptions();
        const bool conservativeCulling = needsConservativeGlobeCulling(_camera);
        options.maximumScreenSpaceError = _maximumScreenSpaceError;
        options.enableFrustumCulling = !conservativeCulling;
        options.enableOcclusionCulling = !conservativeCulling;
        options.renderTilesUnderCamera = conservativeCulling;
        options.maximumSimultaneousTileLoads = 32;
        options.loadingDescendantLimit = 32;
        options.mainThreadLoadingTimeLimit =
            _framePressure > 0.75 ? 4.0 :
            _framePressure > 0.35 ? 6.0 :
            8.0;
    }

    void processRendererUploads() {
        _prepareRendererResources->prioritizeVisibleResources(_uploadPriorityResources);
        const size_t pendingGeometryUploads = _prepareRendererResources->pendingGeometryUploads();
        const size_t pendingRasterUploads = _prepareRendererResources->pendingRasterUploads();
        const bool previousFrameOverBudget = _drawMs > 18.0 || _gpuDrawMs > 10.0;
        const size_t maxUploads =
            previousFrameOverBudget ? 6 :
            _framePressure > 0.65 ? 8 :
            pendingGeometryUploads > 128 ? 12 :
            14;
        _geometryUploadsThisFrame = _prepareRendererResources->processPendingGeometryUploads(maxUploads);
        const size_t rasterUploadBudget =
            previousFrameOverBudget ? 6 :
            _framePressure > 0.65 ? 8 :
            pendingRasterUploads > 96 ? 12 :
            pendingRasterUploads > 32 ? 10 :
            10;
        _rasterUploadsThisFrame =
            _prepareRendererResources->processPendingRasterUploads(rasterUploadBudget);
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
            _selectedPrimitiveSet.clear();
            _continuityResources.clear();
            _uploadPriorityResources.clear();
            _selectedDrawablePrimitiveCount = 0;
            _selectedTexturedPrimitiveCount = 0;
            _selectedCameraAltitudeMeters = _camera.altitudeMeters;
            _selectedCameraLongitudeDegrees = _camera.longitudeDegrees;
            _selectedCameraLatitudeDegrees = _camera.latitudeDegrees;
            _selectedAverageTileLevel = 0.0;
            _batchSelectionGeneration = 0;
            ++_selectionGeneration;
            _consecutiveSelectionPreservationFrames = 0;
            _continuityResourceFramesRemaining = 0;
        _tileset = Cesium3DTilesSelection::EllipsoidTilesetLoader::createTileset(
            _externals,
            createTilesetOptions(maximumScreenSpaceError));
        {
            std::lock_guard<std::mutex> lock(_mutex);
            applyInteractiveTilesetOptionsLocked();
        }
        addImageryOverlay(maximumScreenSpaceError);
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

    void addImageryOverlay(double maximumScreenSpaceError) {
        if (!_tileset) return;

        CesiumRasterOverlays::UrlTemplateRasterOverlayOptions templateOptions;
        const CesiumGeospatial::WebMercatorProjection projection(CesiumGeospatial::Ellipsoid::WGS84);
        templateOptions.projection = projection;
        templateOptions.tilingScheme = CesiumGeometry::QuadtreeTilingScheme(
            CesiumGeospatial::WebMercatorProjection::computeMaximumProjectedRectangle(
                CesiumGeospatial::Ellipsoid::WGS84),
            1,
            1);
        templateOptions.coverageRectangle =
            CesiumGeospatial::WebMercatorProjection::computeMaximumProjectedRectangle(
                CesiumGeospatial::Ellipsoid::WGS84);
        templateOptions.tileWidth = 256;
        templateOptions.tileHeight = 256;

        CesiumRasterOverlays::RasterOverlayOptions overlayOptions;
        overlayOptions.maximumSimultaneousTileLoads = 48;
        overlayOptions.maximumTextureSize = 2048;
        overlayOptions.maximumScreenSpaceError = maximumScreenSpaceError;
        overlayOptions.subTileCacheBytes = static_cast<int64_t>(256 * 1024 * 1024);
        overlayOptions.loadErrorCallback =
            [](const CesiumRasterOverlays::RasterOverlayLoadFailureDetails& details) {
                __android_log_print(
                    ANDROID_LOG_WARN,
                    "CesiumBridge",
                    "raster overlay load failed type=%d message=%s",
                    static_cast<int>(details.type),
                    details.message.c_str());
            };

        _tileset->getOverlays().add(new CesiumRasterOverlays::UrlTemplateRasterOverlay(
            "base-imagery",
            xyzUrlTemplateToCesiumTemplate(satelliteImageryUrlTemplate()),
            {},
            templateOptions,
            overlayOptions));
    }

	    bool updateTileSelection(const CameraState& camera, double deltaSeconds) {
	        if (!_tileset) return false;
            bool cameraMoving = false;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                cameraMoving = _cameraMoving;
            }
	        if (!_cameraDirty && _selectionSettled && !_selectedResources.empty()) {
	            dispatchMainThreadTasksForFrame();
	            return false;
	        }
	        dispatchMainThreadTasksForFrame();

        const NativeCameraFrame frame = buildCameraFrame(camera);

        const double aspect = static_cast<double>(_width) / static_cast<double>(_height);
        const double verticalFov = kPi / 3.0;
        const double horizontalFov = 2.0 * std::atan(std::tan(verticalFov * 0.5) * aspect);
        Cesium3DTilesSelection::ViewState view(
            frame.eye,
            frame.direction,
            frame.up,
            glm::dvec2(static_cast<double>(_width), static_cast<double>(_height)),
            horizontalFov,
            verticalFov,
            CesiumGeospatial::Ellipsoid::WGS84);

        const Cesium3DTilesSelection::ViewUpdateResult& result =
            _tileset->updateViewGroup(
                _tileset->getDefaultViewGroup(),
                std::vector<Cesium3DTilesSelection::ViewState>{view},
                static_cast<float>(deltaSeconds));
        _tilesVisited = result.tilesVisited;
        _workerQueueLength = result.workerThreadTileLoadQueueLength;
        _mainQueueLength = result.mainThreadTileLoadQueueLength;
        _tileset->loadTiles();
        _loadTilesSkipped = false;
        dispatchMainThreadTasksForFrame();

        std::vector<SelectedTile> nextSelectedTiles;
        nextSelectedTiles.reserve(result.tilesToRenderThisFrame.size());
        std::vector<const GpuTileResources*> nextSelectedResources;
        nextSelectedResources.reserve(result.tilesToRenderThisFrame.size());
        size_t selectedPrimitiveCount = 0;
        size_t drawablePrimitiveCount = 0;
        size_t texturedPrimitiveCount = 0;
        for (const Cesium3DTilesSelection::Tile::ConstPointer& tile : result.tilesToRenderThisFrame) {
            const Cesium3DTilesSelection::TileRenderContent* renderContent =
                tile->getContent().getRenderContent();
	            if (renderContent) {
	                const void* rawResources = renderContent->getRenderResources();
	                const auto* resources = MinimalPrepareRendererResources::isGpuResource(rawResources)
	                    ? reinterpret_cast<const GpuTileResources*>(rawResources)
	                    : nullptr;
	                nextSelectedResources.push_back(resources);
	                if (resources) {
                        selectedPrimitiveCount += resources->primitives.size();
                        for (const GpuPrimitive& primitive : resources->primitives) {
                            if (primitive.vertexBuffer != 0 &&
                                primitive.indexBuffer != 0) {
                                ++drawablePrimitiveCount;
                                if (!primitive.rasterAttachments.empty()) {
                                    ++texturedPrimitiveCount;
                                }
                            }
                        }
                    }
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
            nextSelectedTiles.push_back(selected);
        }
        _uploadPriorityResources = _selectedResources;
        _uploadPriorityResources.insert(
            _uploadPriorityResources.end(),
            nextSelectedResources.begin(),
            nextSelectedResources.end());
        _uploadPriorityResources.insert(
            _uploadPriorityResources.end(),
            _continuityResources.begin(),
            _continuityResources.end());
        const bool replacementHasDrawableContent = drawablePrimitiveCount > 0;
        const bool replacementExpandsCoverage =
            _selectedDrawablePrimitiveCount == 0 ||
            drawablePrimitiveCount > static_cast<size_t>(
                static_cast<double>(_selectedDrawablePrimitiveCount) * 1.10);
        const bool cameraZoomedOutSinceAcceptedSelection =
            camera.altitudeMeters > _selectedCameraAltitudeMeters * 1.03;
        const double candidateAverageLevel = averageTileLevel(nextSelectedTiles);
        const bool replacementIsCoarserLod =
            _selectedAverageTileLevel > 0.0 &&
            candidateAverageLevel + 0.35 < _selectedAverageTileLevel &&
            !cameraZoomedOutSinceAcceptedSelection;
        const bool replacementIsTooSparse =
            _selectedDrawablePrimitiveCount > 0 &&
            drawablePrimitiveCount < static_cast<size_t>(
                static_cast<double>(_selectedDrawablePrimitiveCount) * 0.98);
        const bool replacementImageryIsTooSparse =
            _selectedTexturedPrimitiveCount > 0 &&
            texturedPrimitiveCount < static_cast<size_t>(
                static_cast<double>(_selectedTexturedPrimitiveCount) * 0.98);
        const bool uploadBacklogActive =
            _prepareRendererResources->pendingGeometryUploads() > 0 ||
            _prepareRendererResources->pendingRasterUploads() > 0 ||
            _workerQueueLength > 0 ||
            _mainQueueLength > 0;
        const bool replacementHasEnoughViewportContent =
            drawablePrimitiveCount >= 128 &&
            texturedPrimitiveCount >= static_cast<size_t>(
                static_cast<double>(drawablePrimitiveCount) * 0.85);
        const bool replacementIsMuchSmallerRenderSet =
            _selectedDrawablePrimitiveCount > 0 &&
            drawablePrimitiveCount > 0 &&
            _selectedDrawablePrimitiveCount > drawablePrimitiveCount * 3;
        const double acceptedCameraDistanceDegrees = angularDistanceDegrees(
            _selectedCameraLongitudeDegrees,
            _selectedCameraLatitudeDegrees,
            camera.longitudeDegrees,
            camera.latitudeDegrees);
        const double viewportMoveThresholdDegrees = std::max(
            0.02,
            camera.altitudeMeters / 6'371'000.0 * 180.0 / kPi * 0.12);
        const bool cameraMovedBeyondAcceptedCoverage =
            _selectedDrawablePrimitiveCount > 0 &&
            acceptedCameraDistanceDegrees > viewportMoveThresholdDegrees;
        const bool replacementHasReadyTargetLod =
            replacementHasDrawableContent &&
            texturedPrimitiveCount > 0 &&
            !replacementIsCoarserLod &&
            drawablePrimitiveCount >= static_cast<size_t>(
                static_cast<double>(std::max<size_t>(_selectedDrawablePrimitiveCount, 1)) * 0.90) &&
            texturedPrimitiveCount >= static_cast<size_t>(
                static_cast<double>(std::max<size_t>(_selectedTexturedPrimitiveCount, 1)) * 0.90);
        const bool shouldKeepSharperCurrentSelection =
            _selectedTexturedPrimitiveCount > 0 &&
            replacementIsCoarserLod &&
            !replacementExpandsCoverage &&
            !cameraMovedBeyondAcceptedCoverage &&
            !cameraZoomedOutSinceAcceptedSelection &&
            (uploadBacklogActive || _framePressure > 0.35);
        const int32_t maxPreservationFrames = uploadBacklogActive ? 24 : 8;
        const bool preservesVisualContinuity =
            !replacementHasReadyTargetLod &&
            (!replacementExpandsCoverage || shouldKeepSharperCurrentSelection) &&
            (!replacementHasEnoughViewportContent || shouldKeepSharperCurrentSelection) &&
            (replacementIsTooSparse || replacementImageryIsTooSparse || replacementIsCoarserLod) &&
            (uploadBacklogActive || _framePressure > 0.35) &&
            _consecutiveSelectionPreservationFrames < maxPreservationFrames;
        const bool acceptsSelection =
            _selectedResources.empty() ||
            (cameraMovedBeyondAcceptedCoverage && replacementHasDrawableContent) ||
            (!shouldKeepSharperCurrentSelection && replacementHasReadyTargetLod) ||
            (!shouldKeepSharperCurrentSelection && replacementExpandsCoverage && replacementHasDrawableContent) ||
            (cameraZoomedOutSinceAcceptedSelection && replacementHasDrawableContent) ||
            (!shouldKeepSharperCurrentSelection &&
                replacementIsMuchSmallerRenderSet &&
                replacementHasEnoughViewportContent &&
                !replacementIsCoarserLod) ||
            (!replacementIsCoarserLod && !uploadBacklogActive && replacementHasDrawableContent) ||
            (!shouldKeepSharperCurrentSelection &&
                !preservesVisualContinuity &&
                replacementHasDrawableContent);

        if (acceptsSelection) {
            std::unordered_set<const GpuTileResources*> nextResourceSet;
            nextResourceSet.reserve(nextSelectedResources.size());
            for (const GpuTileResources* resources : nextSelectedResources) {
                if (resources) nextResourceSet.insert(resources);
            }
            _continuityResources.clear();
            _continuityResources.reserve(_selectedResources.size());
            for (const GpuTileResources* resources : _selectedResources) {
                if (resources && nextResourceSet.find(resources) == nextResourceSet.end()) {
                    _continuityResources.push_back(resources);
                    if (_continuityResources.size() >= 160) {
                        break;
                    }
                }
            }
            _continuityResourceFramesRemaining =
                !_continuityResources.empty() &&
                !replacementHasReadyTargetLod &&
                (uploadBacklogActive || replacementImageryIsTooSparse)
                    ? 6
                    : 0;
            if (_continuityResourceFramesRemaining == 0) {
                _continuityResources.clear();
            }
            _selectedTiles = std::move(nextSelectedTiles);
            _selectedResources = std::move(nextSelectedResources);
            _selectedPrimitiveSet.clear();
            for (const GpuTileResources* resources : _selectedResources) {
                if (!resources) continue;
                for (const GpuPrimitive& primitive : resources->primitives) {
                    _selectedPrimitiveSet.insert(&primitive);
                }
            }
            _selectedDrawablePrimitiveCount = countDrawablePrimitives(_selectedResources);
            _selectedTexturedPrimitiveCount = countTexturedPrimitives(_selectedResources);
            _selectedCameraAltitudeMeters = camera.altitudeMeters;
            _selectedCameraLongitudeDegrees = camera.longitudeDegrees;
            _selectedCameraLatitudeDegrees = camera.latitudeDegrees;
            _selectedAverageTileLevel = candidateAverageLevel;
            ++_selectionGeneration;
            _consecutiveSelectionPreservationFrames = 0;
        } else if (preservesVisualContinuity) {
            ++_resourcePreservationFrames;
            ++_consecutiveSelectionPreservationFrames;
        }
	        updateSelectionSettledState(camera);
	        if ((_frameUpdates % 60) == 0) {
            __android_log_print(
                ANDROID_LOG_INFO,
                "CesiumBridge",
                "tiles render=%zu loaded=%d visited=%u culled=%u workerQ=%d mainQ=%d maxDepth=%u moving=%d pressure=%.2f",
                result.tilesToRenderThisFrame.size(),
                _tileset->getNumberOfTilesLoaded(),
                result.tilesVisited,
                result.tilesCulled,
                result.workerThreadTileLoadQueueLength,
                result.mainThreadTileLoadQueueLength,
                result.maxDepthVisited,
                cameraMoving ? 1 : 0,
                _framePressure);
	            __android_log_print(
	                ANDROID_LOG_INFO,
	                "CesiumBridge",
	                "gpu resources=%zu candidatePrimitives=%zu candidateDrawable=%zu candidateTextured=%zu keptDrawable=%zu keptTextured=%zu accepted=%d preservedNow=%d preserveStreak=%d uploadedGeometry=%zu uploadedRaster=%zu pendingGeometry=%zu pendingRaster=%zu preservedTotal=%lld bytes=%.2fMB selection=%.3fms draw=%.3fms selectionSkipped=%d mainDispatchSkipped=%d loadTilesSkipped=%d",
	                _selectedResources.size(),
	                selectedPrimitiveCount,
                    drawablePrimitiveCount,
                    texturedPrimitiveCount,
                    _selectedDrawablePrimitiveCount,
                    _selectedTexturedPrimitiveCount,
                    acceptsSelection ? 1 : 0,
                    preservesVisualContinuity ? 1 : 0,
                    _consecutiveSelectionPreservationFrames,
                    _geometryUploadsThisFrame,
                    _rasterUploadsThisFrame,
                    _prepareRendererResources->pendingGeometryUploads(),
                    _prepareRendererResources->pendingRasterUploads(),
                    static_cast<long long>(_resourcePreservationFrames),
	                gpuMemoryMb(),
	                _selectionMs,
	                _gpuDrawMs,
	                _selectionSkipped ? 1 : 0,
                    _mainThreadDispatchSkipped ? 1 : 0,
                    _loadTilesSkipped ? 1 : 0);
	        }
	        return true;
	    }

        void dispatchMainThreadTasksForFrame() {
            _tileset->getAsyncSystem().dispatchMainThreadTasks();
            _framesSinceLastMainThreadDispatch = 0;
            _mainThreadDispatchSkipped = false;
        }

	    void updateSelectionSettledState(const CameraState& camera) {
	        const int32_t currentLoadedTiles = this->loadedTiles();
	        const size_t selectedResources = _selectedResources.size();
	        const size_t minimumResources =
	            camera.altitudeMeters <= 5000.0 ? 64 :
	            camera.altitudeMeters <= 50000.0 ? 64 :
	            camera.altitudeMeters <= 500000.0 ? 24 :
	            4;
	        const bool queuesEmpty =
                _workerQueueLength == 0 &&
                _mainQueueLength == 0 &&
                _prepareRendererResources->pendingGeometryUploads() == 0 &&
                _prepareRendererResources->pendingRasterUploads() == 0;
            if (queuesEmpty && _continuityResourceFramesRemaining > 0) {
                _continuityResources.clear();
                _continuityResourceFramesRemaining = 0;
            } else if (_continuityResourceFramesRemaining > 0) {
                --_continuityResourceFramesRemaining;
                if (_continuityResourceFramesRemaining == 0) {
                    _continuityResources.clear();
                }
            }
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

    void freeTextureArrayBatch() {
        for (TextureArrayBatch& batch : _textureAtlasBatches) {
            if (batch.vertexBuffer != 0) glDeleteBuffers(1, &batch.vertexBuffer);
            if (batch.indexBuffer != 0) glDeleteBuffers(1, &batch.indexBuffer);
            if (batch.ownsTexture && batch.textureArray != 0) glDeleteTextures(1, &batch.textureArray);
        }
        _textureAtlasBatches.clear();
        _batchedPrimitives.clear();
        _batchTexturedPrimitiveCount = 0;
        _atlasBatchBuildComplete = false;
        _atlasBatchBuildCooldownFrames = 0;
    }

    const OverlayVertexBuffer* findOverlayVertexBuffer(
        const GpuPrimitive& primitive,
        int32_t overlayTextureCoordinateID) const {
        for (const OverlayVertexBuffer& overlay : primitive.overlayVertexBuffers) {
            if (overlay.overlayTextureCoordinateID == overlayTextureCoordinateID) {
                return &overlay;
            }
        }
        return nullptr;
    }

    void rebuildTextureArrayBatchIfNeeded() {
        const size_t currentTexturedPrimitiveCount = countTexturedPrimitives(_selectedResources);
        if (_batchSelectionGeneration != _selectionGeneration ||
            _batchTexturedPrimitiveCount != currentTexturedPrimitiveCount) {
            _batchSelectionGeneration = _selectionGeneration;
            _batchTexturedPrimitiveCount = currentTexturedPrimitiveCount;
            _atlasBatchBuildComplete = false;
            if (_textureAtlasBatches.size() > 128) {
                freeTextureArrayBatch();
            }
        }
        if (_atlasBatchBuildComplete) {
            return;
        }
        if (_atlasBatchBuildCooldownFrames > 0) {
            --_atlasBatchBuildCooldownFrames;
            return;
        }
        const size_t pendingGeometryUploads = _prepareRendererResources->pendingGeometryUploads();
        const size_t pendingRasterUploads = _prepareRendererResources->pendingRasterUploads();
        const bool queuesBusy =
            _workerQueueLength > 96 ||
            _mainQueueLength > 96 ||
            pendingGeometryUploads > 96 ||
            pendingRasterUploads > 96;
        if (pendingGeometryUploads > 0 || pendingRasterUploads > 0) {
            return;
        }
        if (_workerQueueLength > 128 || _mainQueueLength > 128) {
            return;
        }
        if (_drawMs > 32.0 || _gpuDrawMs > 24.0) {
            return;
        }
        const bool frameHasAtlasBudget =
            _drawMs <= 0.0 ||
            _drawMs < 13.0 ||
            (_drawMs < 25.0 && queuesBusy) ||
            (_drawMs < 18.0 && !queuesBusy);
        if (!frameHasAtlasBudget) {
            return;
        }

        std::unordered_map<GLuint, std::vector<DrawCommand>> uploadAtlasGroups;
        for (const GpuTileResources* resources : _selectedResources) {
            if (!resources) continue;
            for (const GpuPrimitive& primitive : resources->primitives) {
                if (_batchedPrimitives.find(&primitive) != _batchedPrimitives.end()) continue;
                if (primitive.cpuIndexData.empty()) continue;
                for (const RasterAttachment& attachment : primitive.rasterAttachments) {
                    const GpuTexture* texture = attachment.textureResource.get();
                    if (!texture ||
                        texture->id == 0 ||
                        texture->width < 2048 ||
                        texture->height < 2048) {
                        continue;
                    }
                    const OverlayVertexBuffer* overlay =
                        findOverlayVertexBuffer(primitive, attachment.overlayTextureCoordinateID);
                    if (!overlay || overlay->cpuVertexData.empty()) continue;
                    uploadAtlasGroups[texture->id].push_back(
                        DrawCommand{&primitive, &attachment, 0, 0.0, false});
                    break;
                }
            }
        }

        for (const auto& entry : uploadAtlasGroups) {
            const GLuint texture = entry.first;
            const std::vector<DrawCommand>& commands = entry.second;
            if (commands.size() < 8) continue;
            TextureArrayBatch batch;
            batch.textureArray = texture;
            batch.ownsTexture = false;
            batch.originEcef = commands.front().primitive->originEcef;
            std::vector<float> vertices;
            std::vector<uint32_t> indices;
            const size_t commandLimit = std::min<size_t>(
                commands.size(),
                _gpuDrawMs > 8.0 ? 8 :
                _drawMs > 18.0 ? 16 :
                _drawMs > 13.0 ? 24 :
                _drawMs > 8.0 ? 32 :
                queuesBusy ? 48 :
                64);
            vertices.reserve(commandLimit * 4 * 5);
            indices.reserve(commandLimit * 6);
            size_t builtPrimitives = 0;
            for (size_t commandIndex = 0; commandIndex < commandLimit; ++commandIndex) {
                const GpuPrimitive& primitive = *commands[commandIndex].primitive;
                const RasterAttachment& attachment = *commands[commandIndex].attachment;
                const OverlayVertexBuffer* overlay =
                    findOverlayVertexBuffer(primitive, attachment.overlayTextureCoordinateID);
                if (!overlay) continue;
                const uint32_t baseVertex = static_cast<uint32_t>(vertices.size() / 5);
                const glm::dvec3 offset = primitive.originEcef - batch.originEcef;
                for (size_t i = 0; i + 4 < overlay->cpuVertexData.size(); i += 5) {
                    const double u =
                        static_cast<double>(overlay->cpuVertexData[i + 3]) * attachment.scale.x +
                        attachment.translation.x;
                    const double v =
                        static_cast<double>(overlay->cpuVertexData[i + 4]) * attachment.scale.y +
                        attachment.translation.y;
                    vertices.push_back(static_cast<float>(static_cast<double>(overlay->cpuVertexData[i]) + offset.x));
                    vertices.push_back(static_cast<float>(static_cast<double>(overlay->cpuVertexData[i + 1]) + offset.y));
                    vertices.push_back(static_cast<float>(static_cast<double>(overlay->cpuVertexData[i + 2]) + offset.z));
                    vertices.push_back(static_cast<float>(u));
                    vertices.push_back(static_cast<float>(v));
                }
                for (uint32_t index : primitive.cpuIndexData) {
                    indices.push_back(baseVertex + index);
                }
                _batchedPrimitives.insert(&primitive);
                batch.primitives.push_back(&primitive);
                ++builtPrimitives;
            }
            if (vertices.empty() || indices.empty()) continue;
            glGenBuffers(1, &batch.vertexBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, batch.vertexBuffer);
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                vertices.data(),
                GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glGenBuffers(1, &batch.indexBuffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch.indexBuffer);
            glBufferData(
                GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                indices.data(),
                GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            batch.indexCount = static_cast<GLsizei>(indices.size());
            batch.primitiveCount = builtPrimitives;
            batch.valid = true;
            _textureAtlasBatches.push_back(batch);
            _atlasBatchBuildCooldownFrames =
                _drawMs > 18.0 ? 4 :
                _drawMs > 13.0 ? 3 :
                _drawMs > 8.0 ? 2 :
                queuesBusy ? 2 :
                1;
            if ((_frameUpdates % 120) == 0) {
                __android_log_print(
                    ANDROID_LOG_INFO,
                    "CesiumBridge",
                    "atlas batch built texture=%u primitives=%zu/%zu batches=%zu drawMs=%.2f queues worker=%d main=%d pendingGeometry=%zu pendingRaster=%zu",
                    texture,
                    builtPrimitives,
                    commands.size(),
                    _textureAtlasBatches.size(),
                    _drawMs,
                    _workerQueueLength,
                    _mainQueueLength,
                    pendingGeometryUploads,
                    pendingRasterUploads);
            }
            return;
        }
        _atlasBatchBuildComplete = true;
    }

	    void drawSelectedTiles(const CameraState& camera) {
	        if (_program == 0) return;
	
	        const double height = camera.altitudeMeters;
	        const NativeCameraFrame frame = buildCameraFrame(camera);
	        const glm::dvec3 backward = -frame.direction;

	        const float aspect = static_cast<float>(_width) / static_cast<float>(_height);
	        const float verticalFov = static_cast<float>(kPi / 3.0);
            const double nearMeters = clampDouble(height * 0.005, 2.0, 50'000.0);
            const double farMeters = std::max(
                100'000.0,
                std::min(height + 13'000'000.0, horizonDistanceMeters(height) + 750'000.0));
	        const float nearPlane = static_cast<float>(nearMeters);
	        const float farPlane = static_cast<float>(std::max(farMeters, nearMeters + 10'000.0));
	        const glm::mat4 projection = glm::perspective(verticalFov, aspect, nearPlane, farPlane);

            rebuildTextureArrayBatchIfNeeded();

            DrawStats drawStats;
            std::unordered_set<const GpuPrimitive*> activeBatchedPrimitives;
            for (const TextureArrayBatch& batch : _textureAtlasBatches) {
                if (!batch.valid) continue;
                bool batchStillSelected = !batch.primitives.empty();
                for (const GpuPrimitive* primitive : batch.primitives) {
                    if (_selectedPrimitiveSet.find(primitive) == _selectedPrimitiveSet.end()) {
                        batchStillSelected = false;
                        break;
                    }
                }
                if (!batchStillSelected) continue;
                for (const GpuPrimitive* primitive : batch.primitives) {
                    activeBatchedPrimitives.insert(primitive);
                }
                glUseProgram(_program);
                const glm::dvec3 originEye = batch.originEcef - frame.eye;
                glUniformMatrix4fv(_locations.projection, 1, GL_FALSE, &projection[0][0]);
                glUniform3f(
                    _locations.originEye,
                    static_cast<float>(originEye.x),
                    static_cast<float>(originEye.y),
                    static_cast<float>(originEye.z));
                glUniform3f(_locations.right, static_cast<float>(frame.right.x), static_cast<float>(frame.right.y), static_cast<float>(frame.right.z));
                glUniform3f(_locations.up, static_cast<float>(frame.up.x), static_cast<float>(frame.up.y), static_cast<float>(frame.up.z));
                glUniform3f(_locations.backward, static_cast<float>(backward.x), static_cast<float>(backward.y), static_cast<float>(backward.z));
                glUniform1i(_locations.texture, 0);
                glUniform1i(_locations.discardOutsideUv, 1);
                glUniform2f(_locations.uvTranslation, 0.0f, 0.0f);
                glUniform2f(_locations.uvScale, 1.0f, 1.0f);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, batch.textureArray);
                glBindBuffer(GL_ARRAY_BUFFER, batch.vertexBuffer);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch.indexBuffer);
                glEnableVertexAttribArray(0);
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
                glVertexAttribPointer(
                    1,
                    2,
                    GL_FLOAT,
                    GL_FALSE,
                    5 * sizeof(float),
                    reinterpret_cast<const void*>(3 * sizeof(float)));
                glDrawElements(GL_TRIANGLES, batch.indexCount, GL_UNSIGNED_INT, nullptr);
                ++drawStats.drawCalls;
                drawStats.primitives += batch.primitiveCount;
                glBindTexture(GL_TEXTURE_2D, 0);
            }
		
	        glUseProgram(_program);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(1.0f, 1.0f);
	        glUniformMatrix4fv(_locations.projection, 1, GL_FALSE, &projection[0][0]);
	        glUniform3f(_locations.right, static_cast<float>(frame.right.x), static_cast<float>(frame.right.y), static_cast<float>(frame.right.z));
	        glUniform3f(_locations.up, static_cast<float>(frame.up.x), static_cast<float>(frame.up.y), static_cast<float>(frame.up.z));
	        glUniform3f(_locations.backward, static_cast<float>(backward.x), static_cast<float>(backward.y), static_cast<float>(backward.z));
	        glUniform1i(_locations.texture, 0);
	        glEnableVertexAttribArray(0);
	        glEnableVertexAttribArray(1);
        GLuint boundTexture = 0;
        const size_t drawCallBudget = 0;
        std::vector<DrawCommand> drawCommands;
        drawCommands.reserve((_selectedResources.size() + _continuityResources.size()) * 2);
        const bool hasContinuityBackdrop = !_continuityResources.empty();

        auto appendDrawCommands = [&](const std::vector<const GpuTileResources*>& resourcesList, bool continuity) {
	        for (const GpuTileResources* resources : resourcesList) {
	            if (!resources) continue;
	            for (const GpuPrimitive& primitive : resources->primitives) {
                if (!continuity && activeBatchedPrimitives.find(&primitive) != activeBatchedPrimitives.end()) {
                    continue;
                }
                ++drawStats.primitives;
	                if (primitive.vertexBuffer == 0 || primitive.indexBuffer == 0) {
	                    continue;
	                }
                bool pushedTexturedCommand = false;
	                if (!primitive.rasterAttachments.empty()) {
                    drawStats.attachments += primitive.rasterAttachments.size();
	                    for (const RasterAttachment& attachment : primitive.rasterAttachments) {
	                        if (!attachment.textureResource || attachment.textureResource->id == 0) {
	                            continue;
	                        }
	                        GLuint vertexBuffer = 0;
	                        for (const OverlayVertexBuffer& overlay : primitive.overlayVertexBuffers) {
	                            if (overlay.overlayTextureCoordinateID == attachment.overlayTextureCoordinateID) {
	                                vertexBuffer = overlay.vertexBuffer;
	                                break;
	                            }
	                        }
	                        if (vertexBuffer == 0) {
                            ++drawStats.missingOverlayStreams;
	                            continue;
	                        }
                        const glm::dvec3 primitiveToEye = primitive.originEcef - frame.eye;
                        drawCommands.push_back(
                            DrawCommand{
                                &primitive,
                                &attachment,
                                vertexBuffer,
                                glm::dot(primitiveToEye, primitiveToEye),
                                continuity});
                        pushedTexturedCommand = true;
	                    }
	                }
                    if (!pushedTexturedCommand &&
                        _fallbackTexture != 0 &&
                        (continuity || !hasContinuityBackdrop)) {
                        const glm::dvec3 primitiveToEye = primitive.originEcef - frame.eye;
                        drawCommands.push_back(
                            DrawCommand{
                                &primitive,
                                nullptr,
                                primitive.vertexBuffer,
                                glm::dot(primitiveToEye, primitiveToEye),
                                continuity});
                    }
	            }
	        }
        };

        appendDrawCommands(_continuityResources, true);
        appendDrawCommands(_selectedResources, false);

        std::sort(
            drawCommands.begin(),
            drawCommands.end(),
            [](const DrawCommand& a, const DrawCommand& b) {
                if (a.continuity != b.continuity) return a.continuity;
                if (a.distanceSq != b.distanceSq) return a.distanceSq < b.distanceSq;
                if (a.attachment == nullptr || b.attachment == nullptr) {
                    return a.attachment != nullptr;
                }
                return a.attachment->textureResource->id < b.attachment->textureResource->id;
            });

        glActiveTexture(GL_TEXTURE0);
        GLuint boundVertexBuffer = 0;
        GLuint boundIndexBuffer = 0;
        bool depthMaskEnabled = true;
        for (const DrawCommand& command : drawCommands) {
            if (drawCallBudget > 0 && drawStats.drawCalls >= drawCallBudget) {
                ++drawStats.skippedByBudget;
                continue;
            }
            const bool commandDepthMaskEnabled = !command.continuity;
            if (depthMaskEnabled != commandDepthMaskEnabled) {
                glDepthMask(commandDepthMaskEnabled ? GL_TRUE : GL_FALSE);
                depthMaskEnabled = commandDepthMaskEnabled;
            }
            const GpuPrimitive& primitive = *command.primitive;
            const glm::dvec3 originEye = primitive.originEcef - frame.eye;
            glUniform3f(
                _locations.originEye,
                static_cast<float>(originEye.x),
                static_cast<float>(originEye.y),
                static_cast<float>(originEye.z));
            if (boundIndexBuffer != primitive.indexBuffer) {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, primitive.indexBuffer);
                boundIndexBuffer = primitive.indexBuffer;
            }
            if (boundVertexBuffer != command.vertexBuffer) {
                glBindBuffer(GL_ARRAY_BUFFER, command.vertexBuffer);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
                glVertexAttribPointer(
                    1,
                    2,
                    GL_FLOAT,
                    GL_FALSE,
                    5 * sizeof(float),
                    reinterpret_cast<const void*>(3 * sizeof(float)));
                boundVertexBuffer = command.vertexBuffer;
            }
            const GLuint texture = command.attachment && command.attachment->textureResource
                ? command.attachment->textureResource->id
                : _fallbackTexture;
            if (command.attachment) {
                glUniform1i(_locations.discardOutsideUv, 1);
                glUniform2f(
                    _locations.uvTranslation,
                    static_cast<float>(command.attachment->translation.x),
                    static_cast<float>(command.attachment->translation.y));
                glUniform2f(
                    _locations.uvScale,
                    static_cast<float>(command.attachment->scale.x),
                    static_cast<float>(command.attachment->scale.y));
            } else {
                glUniform1i(_locations.discardOutsideUv, 0);
                glUniform2f(_locations.uvTranslation, 0.0f, 0.0f);
                glUniform2f(_locations.uvScale, 1.0f, 1.0f);
            }
            if (boundTexture != texture) {
                glBindTexture(GL_TEXTURE_2D, texture);
                boundTexture = texture;
            }
            glDrawElements(GL_TRIANGLES, primitive.indexCount, primitive.indexType, nullptr);
            ++drawStats.drawCalls;
        }
        if (!depthMaskEnabled) {
            glDepthMask(GL_TRUE);
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	        glBindBuffer(GL_ARRAY_BUFFER, 0);
	        glBindTexture(GL_TEXTURE_2D, 0);
            glDisable(GL_POLYGON_OFFSET_FILL);
	        glDisableVertexAttribArray(0);
	        glDisableVertexAttribArray(1);
        if ((_frameUpdates % 60) == 0) {
            __android_log_print(
                ANDROID_LOG_INFO,
                "CesiumBridge",
                "draw primitives=%zu attachments=%zu drawCalls=%zu skippedByBudget=%zu missingOverlayStreams=%zu",
                drawStats.primitives,
                drawStats.attachments,
                drawStats.drawCalls,
                drawStats.skippedByBudget,
                drawStats.missingOverlayStreams);
            __android_log_print(
                ANDROID_LOG_INFO,
                "CesiumBridge",
                "camera altitude=%.1fm near=%.1fm far=%.1fm depthRatio=%.1f",
                height,
                static_cast<double>(nearPlane),
                static_cast<double>(farPlane),
                static_cast<double>(farPlane / nearPlane));
        }
	    }

    std::mutex _mutex;
    GlobeCameraController _cameraController;
    std::shared_ptr<BackgroundTaskProcessor> _taskProcessor;
    std::shared_ptr<MinimalPrepareRendererResources> _prepareRendererResources;
    std::shared_ptr<CesiumAsync::IAssetAccessor> _assetAccessor;
    Cesium3DTilesSelection::TilesetExternals _externals;
    std::unique_ptr<Cesium3DTilesSelection::Tileset> _tileset;
    CameraState _camera;
    EcefPosition _ecef;
	    std::vector<SelectedTile> _selectedTiles;
	    std::vector<const GpuTileResources*> _selectedResources;
        std::vector<const GpuTileResources*> _continuityResources;
        std::vector<const GpuTileResources*> _uploadPriorityResources;
        size_t _selectedDrawablePrimitiveCount = 0;
        size_t _selectedTexturedPrimitiveCount = 0;
        double _selectedCameraAltitudeMeters = 0.0;
        double _selectedCameraLongitudeDegrees = 0.0;
        double _selectedCameraLatitudeDegrees = 0.0;
        double _selectedAverageTileLevel = 0.0;
	    GLuint _program = 0;
        GLuint _textureArrayProgram = 0;
        GLuint _fallbackTexture = 0;
	    ProgramLocations _locations;
        ProgramLocations _textureArrayLocations;
        std::vector<TextureArrayBatch> _textureAtlasBatches;
        std::unordered_set<const GpuPrimitive*> _batchedPrimitives;
        std::unordered_set<const GpuPrimitive*> _selectedPrimitiveSet;
        uint64_t _selectionGeneration = 1;
        uint64_t _batchSelectionGeneration = 0;
        size_t _batchTexturedPrimitiveCount = 0;
        bool _atlasBatchBuildComplete = false;
        int32_t _atlasBatchBuildCooldownFrames = 0;
	    int _width = 1;
	    int _height = 1;
    uint32_t _tilesVisited = 0;
    int32_t _workerQueueLength = 0;
    int32_t _mainQueueLength = 0;
    jlong _cameraUpdates = 0;
    jlong _memoryClears = 0;
    jlong _frameUpdates = 0;
    jlong _lastCameraUpdateFrame = 0;
    double _maximumScreenSpaceError = 4.0;
        bool _cameraMoving = false;
	    bool _cameraDirty = false;
	    bool _tilesetRebuildRequested = false;
	    bool _selectionSkipped = false;
	    bool _selectionSettled = false;
        int32_t _stableSelectionFrames = 0;
        int32_t _framesSinceLastMainThreadDispatch = 0;
        int32_t _consecutiveSelectionPreservationFrames = 0;
        int32_t _continuityResourceFramesRemaining = 0;
        bool _mainThreadDispatchSkipped = false;
        bool _loadTilesSkipped = false;
        int64_t _resourcePreservationFrames = 0;
	    int32_t _lastSettledLoadedTiles = 0;
	    size_t _lastSettledSelectedResources = 0;
	    double _lastDeltaSeconds = 0.0;
	    double _drawMs = 0.0;
	    double _selectionMs = 0.0;
	    double _gpuDrawMs = 0.0;
        double _framePressure = 0.0;
        size_t _geometryUploadsThisFrame = 0;
        size_t _rasterUploadsThisFrame = 0;
	};

CesiumBridge* fromHandle(jlong handle) {
    return reinterpret_cast<CesiumBridge*>(static_cast<intptr_t>(handle));
}

jstring newString(JNIEnv* env, const std::string& value) {
    return env->NewStringUTF(value.c_str());
}

jdoubleArray newCameraArray(JNIEnv* env, const CameraState& camera) {
    const jdouble values[] = {
        camera.longitudeDegrees,
        camera.latitudeDegrees,
        camera.altitudeMeters,
        camera.autoOrbit ? 1.0 : 0.0,
        camera.bearingDegrees,
        camera.pitchDegrees,
    };
    jdoubleArray result = env->NewDoubleArray(6);
    env->SetDoubleArrayRegion(result, 0, 6, values);
    return result;
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
    jdouble altitudeMeters,
    jboolean autoOrbit,
    jdouble bearing,
    jdouble pitch) {
    if (auto* bridge = fromHandle(handle)) {
        bridge->updateCamera({longitude, latitude, altitudeMeters, autoOrbit == JNI_TRUE, bearing, pitch});
    }
}

extern "C" JNIEXPORT jdoubleArray JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeCameraState(
    JNIEnv* env,
    jobject,
    jlong handle) {
    if (auto* bridge = fromHandle(handle)) {
        return newCameraArray(env, bridge->currentCamera());
    }
    return newCameraArray(env, {});
}

extern "C" JNIEXPORT jdoubleArray JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativePanCamera(
    JNIEnv* env,
    jobject,
    jlong handle,
    jdouble currentX,
    jdouble currentY,
    jdouble distanceX,
    jdouble distanceY,
    jdouble sensitivity) {
    if (auto* bridge = fromHandle(handle)) {
        return newCameraArray(env, bridge->panCamera(currentX, currentY, distanceX, distanceY, sensitivity));
    }
    return newCameraArray(env, {});
}

extern "C" JNIEXPORT jdoubleArray JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeScaleCamera(
    JNIEnv* env,
    jobject,
    jlong handle,
    jdouble scale,
    jdouble focusX,
    jdouble focusY) {
    if (auto* bridge = fromHandle(handle)) {
        return newCameraArray(env, bridge->scaleCamera(scale, focusX, focusY));
    }
    return newCameraArray(env, {});
}

extern "C" JNIEXPORT jdoubleArray JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeScaleCameraFromCenter(
    JNIEnv* env,
    jobject,
    jlong handle,
    jdouble scale) {
    if (auto* bridge = fromHandle(handle)) {
        return newCameraArray(env, bridge->scaleCameraFromCenter(scale));
    }
    return newCameraArray(env, {});
}

extern "C" JNIEXPORT jdoubleArray JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeRotateCamera(
    JNIEnv* env,
    jobject,
    jlong handle,
    jdouble bearingDeltaDegrees,
    jdouble focusX,
    jdouble focusY) {
    if (auto* bridge = fromHandle(handle)) {
        return newCameraArray(env, bridge->rotateCamera(bearingDeltaDegrees, focusX, focusY));
    }
    return newCameraArray(env, {});
}

extern "C" JNIEXPORT jdoubleArray JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeTiltCamera(
    JNIEnv* env,
    jobject,
    jlong handle,
    jdouble pitchDeltaDegrees,
    jdouble focusX,
    jdouble focusY) {
    if (auto* bridge = fromHandle(handle)) {
        return newCameraArray(env, bridge->tiltCamera(pitchDeltaDegrees, focusX, focusY));
    }
    return newCameraArray(env, {});
}

extern "C" JNIEXPORT jdoubleArray JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeOrbitCamera(
    JNIEnv* env,
    jobject,
    jlong handle,
    jdouble deltaSeconds,
    jdouble degreesPerSecond) {
    if (auto* bridge = fromHandle(handle)) {
        return newCameraArray(env, bridge->orbitCamera(deltaSeconds, degreesPerSecond));
    }
    return newCameraArray(env, {});
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeSetCameraMoving(
    JNIEnv*,
    jobject,
    jlong handle,
    jboolean moving) {
    if (auto* bridge = fromHandle(handle)) {
        bridge->setCameraMoving(moving == JNI_TRUE);
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
Java_com_example_cesiumpoc_cesium_1native_1android_1poc_NativeCesiumBridge_nativeSetImageryUrlTemplate(
    JNIEnv* env,
    jobject,
    jlong handle,
    jstring urlTemplate) {
    if (auto* bridge = fromHandle(handle)) {
        std::string templateValue;
        if (urlTemplate != nullptr) {
            const char* chars = env->GetStringUTFChars(urlTemplate, nullptr);
            if (chars != nullptr) {
                templateValue = chars;
                env->ReleaseStringUTFChars(urlTemplate, chars);
            }
        }
        bridge->setImageryUrlTemplate(templateValue);
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
