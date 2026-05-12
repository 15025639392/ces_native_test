#include <jni.h>

#include <android/log.h>
#include <curl/curl.h>
#include <GLES3/gl3.h>

#include <Cesium3DTilesSelection/BoundingVolume.h>
#include <Cesium3DTilesSelection/EllipsoidTilesetLoader.h>
#include <Cesium3DTilesSelection/IPrepareRendererResources.h>
#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TileContent.h>
#include <Cesium3DTilesSelection/Tileset.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <Cesium3DTilesSelection/TilesetOptions.h>
#include <Cesium3DTilesSelection/ViewState.h>
#include <Cesium3DTilesSelection/ViewUpdateResult.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/ITaskProcessor.h>
#include <CesiumGeometry/QuadtreeTileID.h>
#include <CesiumGeospatial/BoundingRegion.h>
#include <CesiumGeospatial/Cartographic.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumGltf/AccessorView.h>
#include <CesiumGltf/ImageAsset.h>
#include <CesiumGltf/Mesh.h>
#include <CesiumGltf/MeshPrimitive.h>
#include <CesiumGltf/Model.h>
#include <CesiumRasterOverlays/RasterOverlayTile.h>
#include <CesiumUtility/IntrusivePointer.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include <stb_image.h>

namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;

float levelColor(uint32_t level, int channel) {
    const float t = static_cast<float>((level * (channel == 0 ? 37 : channel == 1 ? 53 : 71)) % 255) / 255.0f;
    return 0.35f + t * 0.55f;
}

struct CameraState {
    double longitudeDegrees = 116.397389;
    double latitudeDegrees = 39.908722;
    double zoom = 15.0;
    bool autoOrbit = false;
};

struct EcefPosition {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct SelectedTile {
    double west = 0.0;
    double south = 0.0;
    double east = 0.0;
    double north = 0.0;
    uint32_t level = 0;
    uint32_t x = 0;
    uint32_t y = 0;
};

struct ImageRgba {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;
};

struct GpuPrimitive {
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;
    GLuint texture = 0;
    glm::dvec3 originEcef = glm::dvec3(0.0);
    std::vector<float> cpuVertexData;
    std::vector<uint32_t> cpuIndexData;
    std::shared_ptr<const ImageRgba> image;
    GLsizei indexCount = 0;
    GLenum indexType = GL_UNSIGNED_SHORT;
};

enum class RenderResourceKind : uint32_t {
    LoadThread = 0x4c4f4144u,
    MainThreadGpu = 0x47505552u,
};

struct RenderResourceHeader {
    RenderResourceKind kind;
};

struct LoadTileResources {
    RenderResourceHeader header{RenderResourceKind::LoadThread};
    std::shared_ptr<const ImageRgba> image;
    uint32_t imageryZ = 0;
    uint32_t imageryX = 0;
    uint32_t imageryY = 0;
};

struct GpuTileResources {
    RenderResourceHeader header{RenderResourceKind::MainThreadGpu};
    std::vector<GpuPrimitive> primitives;
    size_t bytes = 0;
};

struct ProgramLocations {
    GLint projection = -1;
    GLint originEye = -1;
    GLint right = -1;
    GLint up = -1;
    GLint backward = -1;
    GLint texture = -1;
};

struct BatchResources {
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;
    GLuint textureArray = 0;
    glm::dvec3 originEcef = glm::dvec3(0.0);
    GLsizei indexCount = 0;
    size_t primitiveCount = 0;
    size_t textureLayers = 0;
    bool valid = false;
};

class InlineTaskProcessor final : public CesiumAsync::ITaskProcessor {
public:
    void startTask(std::function<void()> f) override {
        std::thread(std::move(f)).detach();
    }
};

class MinimalPrepareRendererResources final
    : public Cesium3DTilesSelection::IPrepareRendererResources {
public:
    struct Marker {};

    CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources>
	    prepareInLoadThread(
	        const CesiumAsync::AsyncSystem& asyncSystem,
	        Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
	        const glm::dmat4&,
	        const std::any&) override {
	        std::unique_ptr<LoadTileResources> loadResources;
	        if (tileLoadResult.state == Cesium3DTilesSelection::TileLoadResultState::Success) {
	            loadResources = prepareTileImageInLoadThread(tileLoadResult);
	        }
	        return asyncSystem.createResolvedFuture(
	            Cesium3DTilesSelection::TileLoadResultAndRenderResources{
	                std::move(tileLoadResult),
	                loadResources.release()});
	    }

	    void* prepareInMainThread(
	        Cesium3DTilesSelection::Tile& tile,
	        void* pLoadThreadResult) override {
	        std::unique_ptr<LoadTileResources> loadResources;
	        if (isLoadThreadResource(pLoadThreadResult)) {
	            loadResources.reset(reinterpret_cast<LoadTileResources*>(pLoadThreadResult));
	        }

	        const Cesium3DTilesSelection::TileRenderContent* renderContent =
	            tile.getContent().getRenderContent();
	        if (!renderContent) {
	            return new GpuTileResources();
	        }

	        auto resources = std::make_unique<GpuTileResources>();
	        const LoadTileResources* loadResourcesPtr = loadResources.get();
	        const CesiumGltf::Model& model = renderContent->getModel();
	        model.forEachNodeInScene(
	            -1,
	            [resources = resources.get(), &tile, loadResourcesPtr](
	                const CesiumGltf::Model& gltf,
	                const CesiumGltf::Node& node,
	                const glm::dmat4& nodeTransform) {
                const CesiumGltf::Mesh* mesh =
                    CesiumGltf::Model::getSafe(&gltf.meshes, node.mesh);
                if (!mesh) return;

                const glm::dmat4 transform = tile.getTransform() * nodeTransform;
                for (const CesiumGltf::MeshPrimitive& primitive : mesh->primitives) {
	                    if (primitive.mode != CesiumGltf::MeshPrimitive::Mode::TRIANGLES) {
	                        continue;
	                    }
	                    appendPrimitive(gltf, primitive, transform, tile, loadResourcesPtr, *resources);
	                }
	            });

	        return resources.release();
	    }

    void free(
        Cesium3DTilesSelection::Tile&,
	        void* pLoadThreadResult,
	        void* pMainThreadResult) noexcept override {
	        if (isLoadThreadResource(pLoadThreadResult)) {
	            delete reinterpret_cast<LoadTileResources*>(pLoadThreadResult);
	        } else {
	            delete reinterpret_cast<Marker*>(pLoadThreadResult);
	        }
	        auto* resources = reinterpret_cast<GpuTileResources*>(pMainThreadResult);
        if (resources) {
            for (const GpuPrimitive& primitive : resources->primitives) {
                if (primitive.vertexBuffer != 0) glDeleteBuffers(1, &primitive.vertexBuffer);
                if (primitive.indexBuffer != 0) glDeleteBuffers(1, &primitive.indexBuffer);
                if (primitive.texture != 0) glDeleteTextures(1, &primitive.texture);
            }
        }
        delete resources;
    }

    void* prepareRasterInLoadThread(
        CesiumGltf::ImageAsset&,
        const std::any&) override {
        return new Marker();
    }

    void* prepareRasterInMainThread(
        CesiumRasterOverlays::RasterOverlayTile&,
        void* pLoadThreadResult) override {
        delete reinterpret_cast<Marker*>(pLoadThreadResult);
        return new Marker();
    }

    void freeRaster(
        const CesiumRasterOverlays::RasterOverlayTile&,
        void* pLoadThreadResult,
        void* pMainThreadResult) noexcept override {
        delete reinterpret_cast<Marker*>(pLoadThreadResult);
        delete reinterpret_cast<Marker*>(pMainThreadResult);
    }

    void attachRasterInMainThread(
        const Cesium3DTilesSelection::Tile&,
        int32_t,
        const CesiumRasterOverlays::RasterOverlayTile&,
        void*,
        const glm::dvec2&,
        const glm::dvec2&) override {}

    void detachRasterInMainThread(
        const Cesium3DTilesSelection::Tile&,
        int32_t,
        const CesiumRasterOverlays::RasterOverlayTile&,
        void*) noexcept override {}

private:
    static bool isLoadThreadResource(void* resource) {
        if (!resource) return false;
        const auto* header = reinterpret_cast<const RenderResourceHeader*>(resource);
        return header->kind == RenderResourceKind::LoadThread;
    }

public:
    static bool isGpuResource(const void* resource) {
        if (!resource) return false;
        const auto* header = reinterpret_cast<const RenderResourceHeader*>(resource);
        return header->kind == RenderResourceKind::MainThreadGpu;
    }

private:
    static std::unique_ptr<LoadTileResources> prepareTileImageInLoadThread(
        const Cesium3DTilesSelection::TileLoadResult& tileLoadResult) {
        const Cesium3DTilesSelection::BoundingVolume* boundingVolume = nullptr;
        if (tileLoadResult.updatedBoundingVolume) {
            boundingVolume = &*tileLoadResult.updatedBoundingVolume;
        } else if (tileLoadResult.initialBoundingVolume) {
            boundingVolume = &*tileLoadResult.initialBoundingVolume;
        }
        if (!boundingVolume) return nullptr;

        uint32_t z = 0;
        uint32_t x = 0;
        uint32_t y = 0;
        if (!satelliteTileId(*boundingVolume, z, x, y)) return nullptr;

        auto image = imageForSatelliteTile(z, x, y);
        if (!image || image->pixels.empty()) return nullptr;

        auto resources = std::make_unique<LoadTileResources>();
        resources->image = std::move(image);
        resources->imageryZ = z;
        resources->imageryX = x;
        resources->imageryY = y;
        return resources;
    }

    static void appendPrimitive(
	        const CesiumGltf::Model& model,
	        const CesiumGltf::MeshPrimitive& primitive,
	        const glm::dmat4& transform,
	        const Cesium3DTilesSelection::Tile& tile,
	        const LoadTileResources* loadResources,
	        GpuTileResources& resources) {
        static int logBudget = 12;
        int32_t positionAccessor = -1;
        for (const auto& entry : primitive.attributes) {
            if (entry.first == "POSITION") {
                positionAccessor = entry.second;
                break;
            }
        }
        if (positionAccessor < 0) {
            if (logBudget-- > 0) {
                std::string keys;
                for (const auto& entry : primitive.attributes) {
                    if (!keys.empty()) keys += ",";
                    keys += entry.first;
                }
                __android_log_print(ANDROID_LOG_WARN, "CesiumBridge", "skip primitive: no POSITION attrs=%s", keys.c_str());
            }
            return;
        }

        CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC3<float>> positions(
            model,
            positionAccessor);
        if (positions.status() != CesiumGltf::AccessorViewStatus::Valid ||
            positions.size() == 0) {
            if (logBudget-- > 0) {
                __android_log_print(
                    ANDROID_LOG_WARN,
                    "CesiumBridge",
                    "skip primitive: position status=%d size=%lld accessor=%d",
                    static_cast<int>(positions.status()),
                    static_cast<long long>(positions.size()),
                    positionAccessor);
            }
            return;
        }

        const CesiumGeospatial::BoundingRegion* region =
            Cesium3DTilesSelection::getBoundingRegionFromBoundingVolume(tile.getBoundingVolume());
        if (!region) {
            if (logBudget-- > 0) __android_log_print(ANDROID_LOG_WARN, "CesiumBridge", "skip primitive: no bounding region");
            return;
        }

        const CesiumGeospatial::GlobeRectangle& rectangle = region->getRectangle();
        const double west = rectangle.getWest();
        const double south = rectangle.getSouth();
        const double lonSpan = std::max(rectangle.getEast() - west, 0.00000001);
	        const double latSpan = std::max(rectangle.getNorth() - south, 0.00000001);
	
	        GpuPrimitive gpu;
	        bool hasOrigin = false;
	        std::vector<float> vertexData;
	        vertexData.reserve(static_cast<size_t>(positions.size()) * 5);
	        for (int64_t i = 0; i < positions.size(); ++i) {
	            const auto& p = positions[i];
	            const glm::dvec3 ecef = glm::dvec3(transform * glm::dvec4(p.value[0], p.value[1], p.value[2], 1.0));
	            if (!hasOrigin) {
	                gpu.originEcef = ecef;
	                hasOrigin = true;
	            }
	            const std::optional<CesiumGeospatial::Cartographic> cartographic =
	                CesiumGeospatial::Ellipsoid::WGS84.cartesianToCartographic(ecef);
	            if (!cartographic) {
                if (logBudget-- > 0) __android_log_print(ANDROID_LOG_WARN, "CesiumBridge", "skip primitive: cartographic failed");
                return;
            }
	
	            const double longitude = cartographic->longitude;
	            const double latitude = cartographic->latitude;
	            const glm::dvec3 relative = ecef - gpu.originEcef;
	            vertexData.push_back(static_cast<float>(relative.x));
	            vertexData.push_back(static_cast<float>(relative.y));
	            vertexData.push_back(static_cast<float>(relative.z));
	            vertexData.push_back(static_cast<float>((longitude - west) / lonSpan));
	            vertexData.push_back(static_cast<float>((latitude - south) / latSpan));
	        }
	
	        uploadIndices(model, primitive, gpu);
        if (gpu.indexCount == 0 || gpu.indexBuffer == 0) {
            if (logBudget-- > 0) {
                __android_log_print(
                    ANDROID_LOG_WARN,
                    "CesiumBridge",
                    "skip primitive: no indices accessor=%d",
                    primitive.indices);
            }
            return;
        }

        glGenBuffers(1, &gpu.vertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, gpu.vertexBuffer);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(vertexData.size() * sizeof(float)),
            vertexData.data(),
            GL_STATIC_DRAW);
	        glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	        gpu.cpuVertexData = std::move(vertexData);
	        if (loadResources) {
	            gpu.image = loadResources->image;
	        }
		        gpu.texture = createTileTexture(tile, loadResources);
		        resources.bytes += gpu.cpuVertexData.size() * sizeof(float);
	        if (loadResources && loadResources->image) {
	            resources.bytes += static_cast<size_t>(loadResources->image->width) *
	                static_cast<size_t>(loadResources->image->height) * 4;
	        } else {
	            resources.bytes += 8 * 8 * 4;
	        }
	        resources.primitives.push_back(gpu);
	    }

    static void uploadIndices(
        const CesiumGltf::Model& model,
        const CesiumGltf::MeshPrimitive& primitive,
        GpuPrimitive& gpu) {
        const CesiumGltf::Accessor* accessor =
            CesiumGltf::Model::getSafe(&model.accessors, primitive.indices);
        if (!accessor || accessor->type != CesiumGltf::Accessor::Type::SCALAR) return;

        if (accessor->componentType == CesiumGltf::Accessor::ComponentType::UNSIGNED_SHORT) {
            CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::SCALAR<uint16_t>> indices(
                model,
                primitive.indices);
            if (indices.status() != CesiumGltf::AccessorViewStatus::Valid) return;

	            std::vector<uint16_t> data;
	            data.reserve(static_cast<size_t>(indices.size()));
	            gpu.cpuIndexData.reserve(static_cast<size_t>(indices.size()));
	            for (int64_t i = 0; i < indices.size(); ++i) {
	                const uint16_t value = indices[i].value[0];
	                data.push_back(value);
	                gpu.cpuIndexData.push_back(value);
	            }
	            uploadIndexBuffer(data, GL_UNSIGNED_SHORT, gpu);
	            return;
        }

        if (accessor->componentType == CesiumGltf::Accessor::ComponentType::UNSIGNED_INT) {
            CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::SCALAR<uint32_t>> indices(
                model,
                primitive.indices);
            if (indices.status() != CesiumGltf::AccessorViewStatus::Valid) return;

	            std::vector<uint32_t> data;
	            data.reserve(static_cast<size_t>(indices.size()));
	            gpu.cpuIndexData.reserve(static_cast<size_t>(indices.size()));
	            for (int64_t i = 0; i < indices.size(); ++i) {
	                const uint32_t value = indices[i].value[0];
	                data.push_back(value);
	                gpu.cpuIndexData.push_back(value);
	            }
	            uploadIndexBuffer(data, GL_UNSIGNED_INT, gpu);
	        }
    }

    template <typename T>
    static void uploadIndexBuffer(
        const std::vector<T>& data,
        GLenum indexType,
        GpuPrimitive& gpu) {
        if (data.empty()) return;
        glGenBuffers(1, &gpu.indexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.indexBuffer);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(data.size() * sizeof(T)),
            data.data(),
            GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        gpu.indexCount = static_cast<GLsizei>(data.size());
        gpu.indexType = indexType;
    }

    static GLuint createTileTexture(
        const Cesium3DTilesSelection::Tile& tile,
        const LoadTileResources* loadResources) {
	        uint32_t level = 0;
	        uint32_t x = 0;
	        uint32_t y = 0;
	        if (const auto* id = std::get_if<CesiumGeometry::QuadtreeTileID>(&tile.getTileID())) {
	            level = id->level;
	            x = id->x;
	            y = id->y;
	        }

	        if (loadResources && loadResources->image && !loadResources->image->pixels.empty()) {
	            const ImageRgba& satellite = *loadResources->image;
	            GLuint texture = 0;
	            glGenTextures(1, &texture);
	            glBindTexture(GL_TEXTURE_2D, texture);
	            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(
	                GL_TEXTURE_2D,
	                0,
	                GL_RGBA,
	                satellite.width,
	                satellite.height,
	                0,
	                GL_RGBA,
	                GL_UNSIGNED_BYTE,
	                satellite.pixels.data());
	            glBindTexture(GL_TEXTURE_2D, 0);
	            return texture;
	        }

        const uint8_t r = static_cast<uint8_t>(levelColor(level, 0) * 255.0f);
        const uint8_t g = static_cast<uint8_t>(levelColor(level + x, 1) * 255.0f);
        const uint8_t b = static_cast<uint8_t>(levelColor(level + y, 2) * 255.0f);
        std::array<uint8_t, 8 * 8 * 4> pixels{};
        for (size_t row = 0; row < 8; ++row) {
            for (size_t col = 0; col < 8; ++col) {
                const bool bright = ((row / 2 + col / 2) % 2) == 0;
                const size_t offset = (row * 8 + col) * 4;
                pixels[offset] = bright ? r : static_cast<uint8_t>(r * 0.55f);
                pixels[offset + 1] = bright ? g : static_cast<uint8_t>(g * 0.55f);
                pixels[offset + 2] = bright ? b : static_cast<uint8_t>(b * 0.55f);
                pixels[offset + 3] = 255;
            }
        }

        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            8,
            8,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        return texture;
    }

    static std::shared_ptr<const ImageRgba> imageForSatelliteTile(
        uint32_t z,
        uint32_t x,
        uint32_t y) {
        const std::string key = satelliteTileKey(z, x, y);
        {
            std::lock_guard<std::mutex> lock(imageCacheMutex());
            const auto found = imageCache().find(key);
            if (found != imageCache().end()) return found->second;
        }

        std::vector<uint8_t> encoded;
        if (!downloadUrl(satelliteTileUrl(z, x, y), encoded)) {
            return nullptr;
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* decoded = stbi_load_from_memory(
            encoded.data(),
            static_cast<int>(encoded.size()),
            &width,
            &height,
            &channels,
            4);
        if (!decoded || width <= 0 || height <= 0) {
            if (decoded) stbi_image_free(decoded);
            return nullptr;
        }

        auto image = std::make_shared<ImageRgba>();
        image->width = width;
        image->height = height;
        image->pixels.assign(decoded, decoded + width * height * 4);
        stbi_image_free(decoded);

        {
            std::lock_guard<std::mutex> lock(imageCacheMutex());
            imageCache()[key] = image;
        }
        return image;
    }

    static bool satelliteTileId(
        const Cesium3DTilesSelection::BoundingVolume& boundingVolume,
        uint32_t& z,
        uint32_t& x,
        uint32_t& y) {
        const CesiumGeospatial::BoundingRegion* region =
            Cesium3DTilesSelection::getBoundingRegionFromBoundingVolume(boundingVolume);
        if (!region) return false;

        const CesiumGeospatial::GlobeRectangle& rectangle = region->getRectangle();
        const double west = rectangle.getWest();
        const double east = rectangle.getEast();
        const double south = rectangle.getSouth();
        const double north = rectangle.getNorth();
        const double lonSpan = std::max(east - west, 2.0 * kPi / std::pow(2.0, 18.0));

        z = static_cast<uint32_t>(glm::clamp(
            std::floor(std::log2((2.0 * kPi) / lonSpan)) + 1.0,
            0.0,
            18.0));
        const double lon = (west + east) * 0.5 * 180.0 / kPi;
        const double lat = (south + north) * 0.5 * 180.0 / kPi;
        const double n = std::pow(2.0, static_cast<double>(z));
        const double latRad = glm::clamp(lat, -85.05112878, 85.05112878) * kPi / 180.0;
        x = static_cast<uint32_t>(glm::clamp(std::floor((lon + 180.0) / 360.0 * n), 0.0, n - 1.0));
        y = static_cast<uint32_t>(glm::clamp(
            std::floor((1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / kPi) * 0.5 * n),
            0.0,
            n - 1.0));
        return true;
    }

    static std::string satelliteTileUrl(uint32_t z, uint32_t x, uint32_t y) {
        const uint32_t server = ((x + y) % 4) + 1;
        std::ostringstream stream;
        stream << "https://webst0" << server
               << ".is.autonavi.com/appmaptile?style=6&x=" << x
               << "&y=" << y
               << "&z=" << z;
        return stream.str();
    }

    static std::string satelliteTileKey(uint32_t z, uint32_t x, uint32_t y) {
        std::ostringstream stream;
        stream << "gaode-satellite/" << z << "/" << x << "/" << y;
        return stream.str();
    }

    static std::unordered_map<std::string, std::shared_ptr<const ImageRgba>>& imageCache() {
        static std::unordered_map<std::string, std::shared_ptr<const ImageRgba>> cache;
        return cache;
    }

    static std::mutex& imageCacheMutex() {
        static std::mutex mutex;
        return mutex;
    }

    static bool downloadUrl(const std::string& url, std::vector<uint8_t>& bytes) {
        static std::once_flag curlInitFlag;
        std::call_once(curlInitFlag, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });

        CURL* curl = curl_easy_init();
        if (!curl) return false;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "CesiumNativeAndroidPoC/0.1");
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 2500L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 4500L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeCurlBytes);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bytes);

        const CURLcode result = curl_easy_perform(curl);
        long responseCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        curl_easy_cleanup(curl);
        return result == CURLE_OK && responseCode >= 200 && responseCode < 300 && !bytes.empty();
    }

    static size_t writeCurlBytes(char* ptr, size_t size, size_t nmemb, void* userdata) {
        const size_t byteCount = size * nmemb;
        auto* bytes = reinterpret_cast<std::vector<uint8_t>*>(userdata);
        bytes->insert(bytes->end(), reinterpret_cast<uint8_t*>(ptr), reinterpret_cast<uint8_t*>(ptr) + byteCount);
        return byteCount;
    }
};

GLuint compileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

GLuint createProgram() {
    constexpr const char* vertexShader = R"(
        attribute vec3 a_position;
        attribute vec2 a_texcoord;
        uniform mat4 u_projection;
        uniform vec3 u_originEye;
        uniform vec3 u_right;
        uniform vec3 u_up;
        uniform vec3 u_backward;
        varying vec2 v_texcoord;
        void main() {
            vec3 rel = a_position + u_originEye;
            vec3 camera = vec3(dot(rel, u_right), dot(rel, u_up), dot(rel, u_backward));
            v_texcoord = a_texcoord;
            gl_Position = u_projection * vec4(camera, 1.0);
        }
    )";
    constexpr const char* fragmentShader = R"(
        precision mediump float;
        uniform sampler2D u_texture;
        varying vec2 v_texcoord;
        void main() {
            gl_FragColor = texture2D(u_texture, v_texcoord);
        }
    )";

    const GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShader);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShader);
    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glBindAttribLocation(program, 0, "a_position");
    glBindAttribLocation(program, 1, "a_texcoord");
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

GLuint createBatchProgram() {
    constexpr const char* vertexShader = R"(#version 300 es
        in vec3 a_position;
        in vec2 a_texcoord;
        in float a_layer;
        uniform mat4 u_projection;
        uniform vec3 u_originEye;
        uniform vec3 u_right;
        uniform vec3 u_up;
        uniform vec3 u_backward;
        out vec2 v_texcoord;
        flat out int v_layer;
        void main() {
            vec3 rel = a_position + u_originEye;
            vec3 camera = vec3(dot(rel, u_right), dot(rel, u_up), dot(rel, u_backward));
            v_texcoord = a_texcoord;
            v_layer = int(a_layer + 0.5);
            gl_Position = u_projection * vec4(camera, 1.0);
        }
    )";
    constexpr const char* fragmentShader = R"(#version 300 es
        precision mediump float;
        precision mediump sampler2DArray;
        uniform sampler2DArray u_textureArray;
        in vec2 v_texcoord;
        flat in int v_layer;
        out vec4 fragColor;
        void main() {
            fragColor = texture(u_textureArray, vec3(v_texcoord, float(v_layer)));
        }
    )";

    const GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShader);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShader);
    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glBindAttribLocation(program, 0, "a_position");
    glBindAttribLocation(program, 1, "a_texcoord");
    glBindAttribLocation(program, 2, "a_layer");
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

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
              {}},
          _tileset(Cesium3DTilesSelection::EllipsoidTilesetLoader::createTileset(
              _externals,
              createTilesetOptions())) {
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
	            _camera.autoOrbit != camera.autoOrbit;
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

	    void renderFrame(int width, int height, double deltaSeconds) {
	        const auto start = std::chrono::steady_clock::now();
	        onSurfaceChanged(width, height);

        CameraState camera;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            camera = _camera;
        }

	        const auto selectionStart = std::chrono::steady_clock::now();
	        const bool selectionUpdated = updateTileSelection(camera, deltaSeconds);
	        if (selectionUpdated) {
	            rebuildSelectedBatch();
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
    double lastDeltaSeconds() const { return _lastDeltaSeconds; }
    int32_t selectedTiles() const { return static_cast<int32_t>(_selectedTiles.size()); }
    int32_t loadedTiles() const { return _tileset ? _tileset->getNumberOfTilesLoaded() : 0; }
    double drawMs() const { return _drawMs; }
    double gpuMemoryMb() const {
        size_t bytes = 0;
        for (const GpuTileResources* resources : _selectedResources) {
            if (resources) bytes += resources->bytes;
        }
        return static_cast<double>(bytes) / 1024.0 / 1024.0;
    }
    const EcefPosition& ecef() const { return _ecef; }

private:
    static Cesium3DTilesSelection::TilesetOptions createTilesetOptions() {
        Cesium3DTilesSelection::TilesetOptions options;
	        options.maximumScreenSpaceError = 8.0;
	        options.enableFrustumCulling = true;
	        options.enableFogCulling = false;
	        options.enableOcclusionCulling = false;
	        options.ellipsoid = CesiumGeospatial::Ellipsoid::WGS84;
	        return options;
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
        const glm::dvec3 surface =
            CesiumGeospatial::Ellipsoid::WGS84.cartographicToCartesian(surfaceCartographic);
        const glm::dvec3 surfaceNormal =
            CesiumGeospatial::Ellipsoid::WGS84.geodeticSurfaceNormal(surfaceCartographic);
        const glm::dvec3 direction = glm::normalize(surface - eye);
        glm::dvec3 east = glm::normalize(glm::cross(glm::dvec3(0.0, 0.0, 1.0), surfaceNormal));
        if (glm::length(east) < 0.000001) {
            east = glm::dvec3(1.0, 0.0, 0.0);
        }
        const glm::dvec3 up = glm::normalize(glm::cross(east, direction));

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

	    static bool isBatchablePrimitive(const GpuPrimitive& primitive) {
	        return primitive.image &&
	            primitive.image->width == 256 &&
	            primitive.image->height == 256 &&
	            !primitive.image->pixels.empty() &&
	            !primitive.cpuVertexData.empty() &&
	            !primitive.cpuIndexData.empty();
	    }

	    void freeSelectedBatch() {
	        if (_selectedBatch.vertexBuffer != 0) glDeleteBuffers(1, &_selectedBatch.vertexBuffer);
	        if (_selectedBatch.indexBuffer != 0) glDeleteBuffers(1, &_selectedBatch.indexBuffer);
	        if (_selectedBatch.textureArray != 0) glDeleteTextures(1, &_selectedBatch.textureArray);
	        _selectedBatch = BatchResources();
	        _batchedPrimitives.clear();
	    }

	    void rebuildSelectedBatch() {
	        freeSelectedBatch();
	        if (_batchProgram == 0) return;

	        GLint maxLayers = 0;
	        glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
	        if (maxLayers <= 0) return;

	        std::vector<const GpuPrimitive*> primitives;
	        primitives.reserve(_selectedResources.size());
	        for (const GpuTileResources* resources : _selectedResources) {
	            if (!resources) continue;
	            for (const GpuPrimitive& primitive : resources->primitives) {
	                if (isBatchablePrimitive(primitive)) {
	                    primitives.push_back(&primitive);
	                    if (static_cast<GLint>(primitives.size()) >= maxLayers) break;
	                }
	            }
	            if (static_cast<GLint>(primitives.size()) >= maxLayers) break;
	        }
	        if (primitives.empty()) return;

	        _selectedBatch.originEcef = primitives.front()->originEcef;
	        std::vector<float> vertices;
	        std::vector<uint32_t> indices;
	        for (size_t layer = 0; layer < primitives.size(); ++layer) {
	            const GpuPrimitive& primitive = *primitives[layer];
	            const uint32_t baseVertex = static_cast<uint32_t>(vertices.size() / 6);
	            const glm::dvec3 primitiveOffset = primitive.originEcef - _selectedBatch.originEcef;

	            for (size_t i = 0; i + 4 < primitive.cpuVertexData.size(); i += 5) {
	                vertices.push_back(static_cast<float>(static_cast<double>(primitive.cpuVertexData[i]) + primitiveOffset.x));
	                vertices.push_back(static_cast<float>(static_cast<double>(primitive.cpuVertexData[i + 1]) + primitiveOffset.y));
	                vertices.push_back(static_cast<float>(static_cast<double>(primitive.cpuVertexData[i + 2]) + primitiveOffset.z));
	                vertices.push_back(primitive.cpuVertexData[i + 3]);
	                vertices.push_back(primitive.cpuVertexData[i + 4]);
	                vertices.push_back(static_cast<float>(layer));
	            }

	            for (uint32_t index : primitive.cpuIndexData) {
	                indices.push_back(baseVertex + index);
	            }
	            _batchedPrimitives.insert(&primitive);
	        }
	        if (vertices.empty() || indices.empty()) {
	            freeSelectedBatch();
	            return;
	        }

	        glGenTextures(1, &_selectedBatch.textureArray);
	        glBindTexture(GL_TEXTURE_2D_ARRAY, _selectedBatch.textureArray);
	        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	        glTexImage3D(
	            GL_TEXTURE_2D_ARRAY,
	            0,
	            GL_RGBA,
	            256,
	            256,
	            static_cast<GLsizei>(primitives.size()),
	            0,
	            GL_RGBA,
	            GL_UNSIGNED_BYTE,
	            nullptr);
	        for (size_t layer = 0; layer < primitives.size(); ++layer) {
	            const ImageRgba& image = *primitives[layer]->image;
	            glTexSubImage3D(
	                GL_TEXTURE_2D_ARRAY,
	                0,
	                0,
	                0,
	                static_cast<GLint>(layer),
	                256,
	                256,
	                1,
	                GL_RGBA,
	                GL_UNSIGNED_BYTE,
	                image.pixels.data());
	        }
	        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	        glGenBuffers(1, &_selectedBatch.vertexBuffer);
	        glBindBuffer(GL_ARRAY_BUFFER, _selectedBatch.vertexBuffer);
	        glBufferData(
	            GL_ARRAY_BUFFER,
	            static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
	            vertices.data(),
	            GL_STATIC_DRAW);
	        glBindBuffer(GL_ARRAY_BUFFER, 0);

	        glGenBuffers(1, &_selectedBatch.indexBuffer);
	        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _selectedBatch.indexBuffer);
	        glBufferData(
	            GL_ELEMENT_ARRAY_BUFFER,
	            static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
	            indices.data(),
	            GL_STATIC_DRAW);
	        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	        _selectedBatch.indexCount = static_cast<GLsizei>(indices.size());
	        _selectedBatch.primitiveCount = primitives.size();
	        _selectedBatch.textureLayers = primitives.size();
	        _selectedBatch.valid = true;
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
	        const glm::dvec3 surface =
	            CesiumGeospatial::Ellipsoid::WGS84.cartographicToCartesian(surfaceCartographic);
	        const glm::dvec3 surfaceNormal =
	            CesiumGeospatial::Ellipsoid::WGS84.geodeticSurfaceNormal(surfaceCartographic);
	        const glm::dvec3 forward = glm::normalize(surface - eye);
	        glm::dvec3 right = glm::normalize(glm::cross(glm::dvec3(0.0, 0.0, 1.0), surfaceNormal));
	        if (glm::length(right) < 0.000001) {
	            right = glm::dvec3(1.0, 0.0, 0.0);
	        }
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
	    bool _cameraDirty = false;
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
    jboolean autoOrbit) {
    if (auto* bridge = fromHandle(handle)) {
        bridge->updateCamera({longitude, latitude, zoom, autoOrbit == JNI_TRUE});
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
