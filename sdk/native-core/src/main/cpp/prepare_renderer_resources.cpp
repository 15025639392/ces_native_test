#include "prepare_renderer_resources.h"

#include "imagery.h"

#include <android/log.h>
#include <GLES3/gl3.h>

#include <Cesium3DTilesSelection/BoundingVolume.h>
#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TileContent.h>
#include <CesiumGeospatial/BoundingRegion.h>
#include <CesiumGeospatial/Cartographic.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumGeometry/QuadtreeTileID.h>
#include <CesiumGltf/AccessorView.h>
#include <CesiumGltf/ImageAsset.h>
#include <CesiumGltf/Mesh.h>
#include <CesiumGltf/MeshPrimitive.h>
#include <CesiumGltf/Model.h>
#include <CesiumRasterOverlays/RasterOverlayTile.h>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace cesium_poc {
namespace {

std::string imageryTextureKey(const ImageryTileResource& imagery) {
    std::ostringstream stream;
    stream << imagery.z << "/" << imagery.x << "/" << imagery.y;
    return stream.str();
}

std::unordered_map<std::string, std::weak_ptr<GpuTexture>>& imageryTextureCache() {
    static std::unordered_map<std::string, std::weak_ptr<GpuTexture>> cache;
    return cache;
}

std::shared_ptr<GpuTexture> makeTextureResource(GLuint id, size_t bytes) {
    return std::shared_ptr<GpuTexture>(
        new GpuTexture{id, bytes},
        [](GpuTexture* texture) {
            if (texture->id != 0) {
                glDeleteTextures(1, &texture->id);
            }
            delete texture;
        });
}

} // namespace

CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources>
MinimalPrepareRendererResources::prepareInLoadThread(
    const CesiumAsync::AsyncSystem& asyncSystem,
    Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
    const glm::dmat4&,
    const std::any&) {
    std::unique_ptr<LoadTileResources> loadResources;
    if (tileLoadResult.state == Cesium3DTilesSelection::TileLoadResultState::Success) {
        loadResources = prepareTileImageInLoadThread(tileLoadResult);
    }
    return asyncSystem.createResolvedFuture(
        Cesium3DTilesSelection::TileLoadResultAndRenderResources{
            std::move(tileLoadResult),
            loadResources.release()});
}

void* MinimalPrepareRendererResources::prepareInMainThread(
    Cesium3DTilesSelection::Tile& tile,
    void* pLoadThreadResult) {
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

void MinimalPrepareRendererResources::free(
    Cesium3DTilesSelection::Tile&,
    void* pLoadThreadResult,
    void* pMainThreadResult) noexcept {
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
        }
    }
    delete resources;
}

void* MinimalPrepareRendererResources::prepareRasterInLoadThread(
    CesiumGltf::ImageAsset&,
    const std::any&) {
    return new Marker();
}

void* MinimalPrepareRendererResources::prepareRasterInMainThread(
    CesiumRasterOverlays::RasterOverlayTile&,
    void* pLoadThreadResult) {
    delete reinterpret_cast<Marker*>(pLoadThreadResult);
    return new Marker();
}

void MinimalPrepareRendererResources::freeRaster(
    const CesiumRasterOverlays::RasterOverlayTile&,
    void* pLoadThreadResult,
    void* pMainThreadResult) noexcept {
    delete reinterpret_cast<Marker*>(pLoadThreadResult);
    delete reinterpret_cast<Marker*>(pMainThreadResult);
}

void MinimalPrepareRendererResources::attachRasterInMainThread(
    const Cesium3DTilesSelection::Tile&,
    int32_t,
    const CesiumRasterOverlays::RasterOverlayTile&,
    void*,
    const glm::dvec2&,
    const glm::dvec2&) {}

void MinimalPrepareRendererResources::detachRasterInMainThread(
    const Cesium3DTilesSelection::Tile&,
    int32_t,
    const CesiumRasterOverlays::RasterOverlayTile&,
    void*) noexcept {}

bool MinimalPrepareRendererResources::isLoadThreadResource(void* resource) {
    if (!resource) return false;
    const auto* header = reinterpret_cast<const RenderResourceHeader*>(resource);
    return header->kind == RenderResourceKind::LoadThread;
}

bool MinimalPrepareRendererResources::isGpuResource(const void* resource) {
    if (!resource) return false;
    const auto* header = reinterpret_cast<const RenderResourceHeader*>(resource);
    return header->kind == RenderResourceKind::MainThreadGpu;
}

std::unique_ptr<LoadTileResources>
MinimalPrepareRendererResources::prepareTileImageInLoadThread(
    const Cesium3DTilesSelection::TileLoadResult& tileLoadResult) {
    const Cesium3DTilesSelection::BoundingVolume* boundingVolume = nullptr;
    if (tileLoadResult.updatedBoundingVolume) {
        boundingVolume = &*tileLoadResult.updatedBoundingVolume;
    } else if (tileLoadResult.initialBoundingVolume) {
        boundingVolume = &*tileLoadResult.initialBoundingVolume;
    }
    if (!boundingVolume) return nullptr;

    const std::vector<SatelliteTileId> tileIds = satelliteTileIdsForRegion(*boundingVolume);
    if (tileIds.empty()) return nullptr;

    auto resources = std::make_unique<LoadTileResources>();
    resources->imageryTiles.reserve(tileIds.size());
    for (const SatelliteTileId& id : tileIds) {
        uint32_t z = id.z;
        uint32_t x = id.x;
        uint32_t y = id.y;
        std::shared_ptr<const ImageRgba> image;
        while (true) {
            image = imageForSatelliteTile(z, x, y);
            if (image && !image->pixels.empty()) break;
            if (z == 0) break;
            --z;
            x /= 2;
            y /= 2;
        }
        if (image && !image->pixels.empty()) {
            resources->imageryTiles.push_back({std::move(image), z, x, y});
        }
    }
    if (resources->imageryTiles.empty()) return nullptr;
    return resources;
}

void MinimalPrepareRendererResources::appendPrimitive(
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

    const std::vector<ImageryTileResource>* imageryTiles = loadResources && !loadResources->imageryTiles.empty()
        ? &loadResources->imageryTiles
        : nullptr;
    const size_t regionCount = imageryTiles ? imageryTiles->size() : 1;
    std::vector<GpuPrimitive> gpuPrimitives(regionCount);
    std::vector<std::vector<float>> vertexDataByRegion(regionCount);
    for (std::vector<float>& vertexData : vertexDataByRegion) {
        vertexData.reserve(static_cast<size_t>(positions.size()) * 5);
    }

    bool hasOrigin = false;
    for (int64_t i = 0; i < positions.size(); ++i) {
        const auto& p = positions[i];
        const glm::dvec3 ecef = glm::dvec3(transform * glm::dvec4(p.value[0], p.value[1], p.value[2], 1.0));
        if (!hasOrigin) {
            for (GpuPrimitive& gpu : gpuPrimitives) {
                gpu.originEcef = ecef;
            }
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
        const glm::dvec3 relative = ecef - gpuPrimitives.front().originEcef;
        for (size_t region = 0; region < regionCount; ++region) {
            std::vector<float>& vertexData = vertexDataByRegion[region];
            vertexData.push_back(static_cast<float>(relative.x));
            vertexData.push_back(static_cast<float>(relative.y));
            vertexData.push_back(static_cast<float>(relative.z));
            glm::dvec2 uv;
            if (imageryTiles) {
                const ImageryTileResource& imagery = (*imageryTiles)[region];
                uv = unclampedWebMercatorTileUv(
                    longitude,
                    latitude,
                    imagery.z,
                    imagery.x,
                    imagery.y);
            } else {
                uv = glm::dvec2((longitude - west) / lonSpan, (latitude - south) / latSpan);
            }
            vertexData.push_back(static_cast<float>(uv.x));
            vertexData.push_back(static_cast<float>(uv.y));
        }
    }

    std::vector<uint32_t> cpuIndices = readIndices(model, primitive);
    if (cpuIndices.empty()) {
        if (logBudget-- > 0) {
            __android_log_print(
                ANDROID_LOG_WARN,
                "CesiumBridge",
                "skip primitive: no indices accessor=%d",
                primitive.indices);
        }
        return;
    }

    for (size_t region = 0; region < regionCount; ++region) {
        GpuPrimitive& gpu = gpuPrimitives[region];
        uploadIndexBuffer(cpuIndices, gpu);
        std::vector<float>& vertexData = vertexDataByRegion[region];
        glGenBuffers(1, &gpu.vertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, gpu.vertexBuffer);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(vertexData.size() * sizeof(float)),
            vertexData.data(),
            GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        gpu.cpuVertexData = std::move(vertexData);
        if (imageryTiles) {
            gpu.image = (*imageryTiles)[region].image;
        }
        gpu.textureResource = imageryTiles
            ? acquireImageTexture((*imageryTiles)[region])
            : createFallbackTexture(tile);
        gpu.texture = gpu.textureResource ? gpu.textureResource->id : 0;
        resources.bytes += gpu.cpuVertexData.size() * sizeof(float);
        resources.bytes += gpu.cpuIndexData.size() * sizeof(uint32_t);
        resources.primitives.push_back(gpu);
    }
}

std::vector<uint32_t> MinimalPrepareRendererResources::readIndices(
    const CesiumGltf::Model& model,
    const CesiumGltf::MeshPrimitive& primitive) {
    const CesiumGltf::Accessor* accessor =
        CesiumGltf::Model::getSafe(&model.accessors, primitive.indices);
    if (!accessor || accessor->type != CesiumGltf::Accessor::Type::SCALAR) return {};

    if (accessor->componentType == CesiumGltf::Accessor::ComponentType::UNSIGNED_SHORT) {
        CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::SCALAR<uint16_t>> indices(
            model,
            primitive.indices);
        if (indices.status() != CesiumGltf::AccessorViewStatus::Valid) return {};

        std::vector<uint32_t> data;
        data.reserve(static_cast<size_t>(indices.size()));
        for (int64_t i = 0; i < indices.size(); ++i) {
            data.push_back(indices[i].value[0]);
        }
        return data;
    }

    if (accessor->componentType == CesiumGltf::Accessor::ComponentType::UNSIGNED_INT) {
        CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::SCALAR<uint32_t>> indices(
            model,
            primitive.indices);
        if (indices.status() != CesiumGltf::AccessorViewStatus::Valid) return {};

        std::vector<uint32_t> data;
        data.reserve(static_cast<size_t>(indices.size()));
        for (int64_t i = 0; i < indices.size(); ++i) {
            data.push_back(indices[i].value[0]);
        }
        return data;
    }
    return {};
}

void MinimalPrepareRendererResources::uploadIndexBuffer(
    const std::vector<uint32_t>& data,
    GpuPrimitive& gpu) {
    if (data.empty()) return;
    glGenBuffers(1, &gpu.indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.indexBuffer);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(data.size() * sizeof(uint32_t)),
        data.data(),
        GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    gpu.indexCount = static_cast<GLsizei>(data.size());
    gpu.indexType = GL_UNSIGNED_INT;
    gpu.cpuIndexData = data;
}

std::shared_ptr<GpuTexture> MinimalPrepareRendererResources::acquireImageTexture(
    const ImageryTileResource& imagery) {
    const std::string key = imageryTextureKey(imagery);
    auto& cache = imageryTextureCache();
    const auto found = cache.find(key);
    if (found != cache.end()) {
        if (std::shared_ptr<GpuTexture> texture = found->second.lock()) {
            return texture;
        }
        cache.erase(found);
    }

    const ImageRgba& satellite = *imagery.image;
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
    auto resource = makeTextureResource(
        texture,
        static_cast<size_t>(satellite.width) * static_cast<size_t>(satellite.height) * 4);
    cache[key] = resource;
    return resource;
}

std::shared_ptr<GpuTexture> MinimalPrepareRendererResources::createFallbackTexture(
    const Cesium3DTilesSelection::Tile& tile) {
    uint32_t level = 0;
    uint32_t x = 0;
    uint32_t y = 0;
    if (const auto* id = std::get_if<CesiumGeometry::QuadtreeTileID>(&tile.getTileID())) {
        level = id->level;
        x = id->x;
        y = id->y;
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
    return makeTextureResource(texture, 8 * 8 * 4);
}

} // namespace cesium_poc
