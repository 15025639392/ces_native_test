#include "prepare_renderer_resources.h"

#include <android/log.h>
#include <GLES3/gl3.h>

#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TileContent.h>
#include <CesiumGltf/AccessorView.h>
#include <CesiumGltf/ImageAsset.h>
#include <CesiumGltf/Mesh.h>
#include <CesiumGltf/MeshPrimitive.h>
#include <CesiumGltf/Model.h>
#include <CesiumGltfContent/GltfUtilities.h>
#include <CesiumGltfContent/SkirtMeshMetadata.h>
#include <CesiumRasterOverlays/RasterOverlayTile.h>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace cesium_poc {
namespace {

std::shared_ptr<GpuTexture> makeTextureResource(GLuint id, size_t bytes, int32_t width, int32_t height) {
    return std::shared_ptr<GpuTexture>(
        new GpuTexture{id, bytes, width, height},
        [](GpuTexture* texture) {
            if (texture->id != 0) {
                glDeleteTextures(1, &texture->id);
            }
            delete texture;
        });
}

struct RasterTextureResource {
    GLuint texture = 0;
    size_t bytes = 0;
    int32_t width = 0;
    int32_t height = 0;
    std::vector<std::byte> pixelData;
    std::shared_ptr<GpuTexture> textureResource;
    glm::dvec2 atlasTranslation = glm::dvec2(0.0);
    glm::dvec2 atlasScale = glm::dvec2(1.0);
    bool queuedForUpload = false;
    struct PendingAttachment {
        GpuTileResources* resources = nullptr;
        int32_t overlayTextureCoordinateID = -1;
        glm::dvec2 translation = glm::dvec2(0.0);
        glm::dvec2 scale = glm::dvec2(1.0);
    };
    std::vector<PendingAttachment> pendingAttachments;
    uint64_t uploadPriority = 0;
};

bool sameRasterAttachment(
    const RasterAttachment& a,
    const std::shared_ptr<GpuTexture>& texture,
    int32_t overlayTextureCoordinateID,
    const glm::dvec2& translation,
    const glm::dvec2& scale);

GLuint uploadRgbaTexture(
    int32_t width,
    int32_t height,
    const std::vector<std::byte>& pixelData,
    GLuint reusableTexture) {
    if (width <= 0 || height <= 0 || pixelData.empty()) {
        return 0;
    }

    const size_t rowBytes = static_cast<size_t>(width) * 4;
    std::vector<std::byte> glPixelData(pixelData.size());
    for (int32_t y = 0; y < height; ++y) {
        const size_t sourceOffset = static_cast<size_t>(y) * rowBytes;
        const size_t targetOffset = static_cast<size_t>(height - 1 - y) * rowBytes;
        std::memcpy(glPixelData.data() + targetOffset, pixelData.data() + sourceOffset, rowBytes);
    }

    GLuint texture = reusableTexture;
    if (texture == 0) {
        glGenTextures(1, &texture);
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        glPixelData.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    const auto* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (extensions && strstr(extensions, "GL_EXT_texture_filter_anisotropic") != nullptr) {
        constexpr GLenum kTextureMaxAnisotropyExt = 0x84FE;
        constexpr GLenum kMaxTextureMaxAnisotropyExt = 0x84FF;
        GLfloat maxAnisotropy = 1.0f;
        glGetFloatv(kMaxTextureMaxAnisotropyExt, &maxAnisotropy);
        glTexParameterf(GL_TEXTURE_2D, kTextureMaxAnisotropyExt, std::min(maxAnisotropy, 8.0f));
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

std::vector<std::byte> flipRgbaRows(
    int32_t width,
    int32_t height,
    const std::vector<std::byte>& pixelData) {
    const size_t rowBytes = static_cast<size_t>(width) * 4;
    std::vector<std::byte> glPixelData(pixelData.size());
    for (int32_t y = 0; y < height; ++y) {
        const size_t sourceOffset = static_cast<size_t>(y) * rowBytes;
        const size_t targetOffset = static_cast<size_t>(height - 1 - y) * rowBytes;
        std::memcpy(glPixelData.data() + targetOffset, pixelData.data() + sourceOffset, rowBytes);
    }
    return glPixelData;
}

glm::dvec2 combineAtlasTranslation(
    const glm::dvec2& translation,
    const glm::dvec2& atlasTranslation,
    const glm::dvec2& atlasScale) {
    return atlasTranslation + translation * atlasScale;
}

void attachRasterToResources(
    GpuTileResources& resources,
    const std::shared_ptr<GpuTexture>& textureResource,
    int32_t overlayTextureCoordinateID,
    const glm::dvec2& translation,
    const glm::dvec2& scale) {
    if (!textureResource) return;
    RasterAttachment attachment;
    attachment.textureResource = textureResource;
    attachment.overlayTextureCoordinateID = overlayTextureCoordinateID;
    attachment.translation = translation;
    attachment.scale = scale;
    for (GpuPrimitive& primitive : resources.primitives) {
        const bool duplicate = std::any_of(
            primitive.rasterAttachments.begin(),
            primitive.rasterAttachments.end(),
            [&attachment](const RasterAttachment& existing) {
                return sameRasterAttachment(
                    existing,
                    attachment.textureResource,
                    attachment.overlayTextureCoordinateID,
                    attachment.translation,
                    attachment.scale);
            });
        if (duplicate) continue;
        primitive.rasterAttachments.push_back(attachment);
    }
}

std::optional<int32_t> overlayTextureCoordinateID(const std::string& attributeName) {
    constexpr const char* prefix = "_CESIUMOVERLAY_";
    const size_t prefixLength = std::char_traits<char>::length(prefix);
    if (attributeName.rfind(prefix, 0) != 0 || attributeName.size() == prefixLength) {
        return std::nullopt;
    }
    char* end = nullptr;
    const long value = std::strtol(attributeName.c_str() + prefixLength, &end, 10);
    if (end == nullptr || *end != '\0' || value < 0 || value > INT32_MAX) {
        return std::nullopt;
    }
    return static_cast<int32_t>(value);
}

GLuint uploadVertexBuffer(const std::vector<float>& vertexData) {
    GLuint vertexBuffer = 0;
    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertexData.size() * sizeof(float)),
        vertexData.data(),
        GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return vertexBuffer;
}

bool sameRasterAttachment(
    const RasterAttachment& a,
    const std::shared_ptr<GpuTexture>& texture,
    int32_t overlayTextureCoordinateID,
    const glm::dvec2& translation,
    const glm::dvec2& scale) {
    return a.textureResource == texture &&
        a.overlayTextureCoordinateID == overlayTextureCoordinateID &&
        std::abs(a.translation.x - translation.x) < 1e-12 &&
        std::abs(a.translation.y - translation.y) < 1e-12 &&
        std::abs(a.scale.x - scale.x) < 1e-12 &&
        std::abs(a.scale.y - scale.y) < 1e-12;
}

} // namespace

MinimalPrepareRendererResources::~MinimalPrepareRendererResources() {
    std::lock_guard<std::mutex> lock(_uploadMutex);
    for (ReusableTexture& texture : _reusableTextures) {
        if (texture.id != 0) {
            glDeleteTextures(1, &texture.id);
            texture.id = 0;
        }
    }
    _reusableTextures.clear();
}

CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources>
MinimalPrepareRendererResources::prepareInLoadThread(
    const CesiumAsync::AsyncSystem& asyncSystem,
    Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
    const glm::dmat4& transform,
    const std::any&) {
    void* renderResources = nullptr;
    if (const auto* model = std::get_if<CesiumGltf::Model>(&tileLoadResult.contentKind)) {
        auto resources = std::make_unique<GpuTileResources>();
        const glm::dmat4 rootTransform =
            CesiumGltfContent::GltfUtilities::applyGltfUpAxisTransform(*model, transform);
        model->forEachNodeInScene(
            -1,
            [resources = resources.get(), &rootTransform](
                const CesiumGltf::Model& gltf,
                const CesiumGltf::Node& node,
                const glm::dmat4& nodeTransform) {
                const CesiumGltf::Mesh* mesh =
                    CesiumGltf::Model::getSafe(&gltf.meshes, node.mesh);
                if (!mesh) return;

                const glm::dmat4 primitiveTransform = rootTransform * nodeTransform;
                for (const CesiumGltf::MeshPrimitive& primitive : mesh->primitives) {
                    if (primitive.mode != CesiumGltf::MeshPrimitive::Mode::TRIANGLES) {
                        continue;
                    }
                    appendPrimitive(gltf, primitive, primitiveTransform, *resources);
                }
            });
        renderResources = resources.release();
    }
    return asyncSystem.createResolvedFuture(
        Cesium3DTilesSelection::TileLoadResultAndRenderResources{
            std::move(tileLoadResult),
            renderResources});
}

void* MinimalPrepareRendererResources::prepareInMainThread(
    Cesium3DTilesSelection::Tile&,
    void* pLoadThreadResult) {
    auto* result = reinterpret_cast<GpuTileResources*>(pLoadThreadResult);
    if (!result) {
        result = new GpuTileResources();
    }
    queueGeometryUpload(result);
    return result;
}

void MinimalPrepareRendererResources::free(
    Cesium3DTilesSelection::Tile&,
    void* pLoadThreadResult,
    void* pMainThreadResult) noexcept {
    delete reinterpret_cast<GpuTileResources*>(pLoadThreadResult);
    auto* resources = reinterpret_cast<GpuTileResources*>(pMainThreadResult);
    removeGeometryUpload(resources);
    removePendingRasterAttachments(resources);
    if (resources) {
        for (const GpuPrimitive& primitive : resources->primitives) {
            if (primitive.vertexBuffer != 0) glDeleteBuffers(1, &primitive.vertexBuffer);
            if (primitive.indexBuffer != 0) glDeleteBuffers(1, &primitive.indexBuffer);
            for (const OverlayVertexBuffer& overlay : primitive.overlayVertexBuffers) {
                if (overlay.vertexBuffer != 0 && overlay.vertexBuffer != primitive.vertexBuffer) {
                    glDeleteBuffers(1, &overlay.vertexBuffer);
                }
            }
        }
    }
    delete resources;
}

void* MinimalPrepareRendererResources::prepareRasterInLoadThread(
    CesiumGltf::ImageAsset& image,
    const std::any&) {
    if (image.compressedPixelFormat == CesiumGltf::GpuCompressedPixelFormat::NONE &&
        image.bytesPerChannel == 1 &&
        image.channels > 0 &&
        image.channels != 4) {
        image.changeNumberOfChannels(4, std::byte{255});
    }
    if (image.width <= 0 ||
        image.height <= 0 ||
        image.bytesPerChannel != 1 ||
        image.channels != 4 ||
        image.pixelData.empty() ||
        image.compressedPixelFormat != CesiumGltf::GpuCompressedPixelFormat::NONE) {
        return nullptr;
    }
    auto* resource = new RasterTextureResource();
    resource->width = image.width;
    resource->height = image.height;
    resource->bytes = static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * 4;
    resource->pixelData = image.pixelData;
    return resource;
}

void* MinimalPrepareRendererResources::prepareRasterInMainThread(
    CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pLoadThreadResult) {
    (void)rasterTile;
    auto* resource = reinterpret_cast<RasterTextureResource*>(pLoadThreadResult);
    if (!resource ||
        resource->width <= 0 ||
        resource->height <= 0 ||
        resource->pixelData.empty()) {
        return nullptr;
    }
    queueRasterUpload(resource);
    return resource;
}

void MinimalPrepareRendererResources::freeRaster(
    const CesiumRasterOverlays::RasterOverlayTile&,
    void* pLoadThreadResult,
    void* pMainThreadResult) noexcept {
    auto* resource = reinterpret_cast<RasterTextureResource*>(pMainThreadResult);
    if (!resource) {
        resource = reinterpret_cast<RasterTextureResource*>(pLoadThreadResult);
    }
    removeRasterUpload(resource);
    if (resource && resource->texture != 0) {
        glDeleteTextures(1, &resource->texture);
    }
    if (resource && resource->textureResource && resource->textureResource.use_count() == 1) {
        const GLuint texture = resource->textureResource->id;
        resource->textureResource->id = 0;
        cacheReusableTexture(texture, resource->width, resource->height, resource->bytes);
        resource->textureResource.reset();
    }
    delete resource;
}

void MinimalPrepareRendererResources::attachRasterInMainThread(
    const Cesium3DTilesSelection::Tile& tile,
    int32_t overlayTextureCoordinateID,
    const CesiumRasterOverlays::RasterOverlayTile&,
    void* pMainThreadRendererResources,
    const glm::dvec2& translation,
    const glm::dvec2& scale) {
    auto* raster = reinterpret_cast<RasterTextureResource*>(pMainThreadRendererResources);
    if (!raster) return;

    const Cesium3DTilesSelection::TileRenderContent* renderContent =
        tile.getContent().getRenderContent();
    if (!renderContent) return;
    const void* rawResources = renderContent->getRenderResources();
    auto* resources = isGpuResource(rawResources)
        ? const_cast<GpuTileResources*>(reinterpret_cast<const GpuTileResources*>(rawResources))
        : nullptr;
    if (!resources) return;

    if (!raster->textureResource) {
        const bool duplicatePending = std::any_of(
            raster->pendingAttachments.begin(),
            raster->pendingAttachments.end(),
            [resources, overlayTextureCoordinateID, &translation, &scale](
                const RasterTextureResource::PendingAttachment& pending) {
                return pending.resources == resources &&
                    pending.overlayTextureCoordinateID == overlayTextureCoordinateID &&
                    std::abs(pending.translation.x - translation.x) < 1e-12 &&
                    std::abs(pending.translation.y - translation.y) < 1e-12 &&
                    std::abs(pending.scale.x - scale.x) < 1e-12 &&
                    std::abs(pending.scale.y - scale.y) < 1e-12;
            });
        if (!duplicatePending) {
            raster->pendingAttachments.push_back(
                RasterTextureResource::PendingAttachment{
                    resources,
                    overlayTextureCoordinateID,
                    translation,
                    scale});
        }
        return;
    }
    attachRasterToResources(
        *resources,
        raster->textureResource,
        overlayTextureCoordinateID,
        combineAtlasTranslation(translation, raster->atlasTranslation, raster->atlasScale),
        scale * raster->atlasScale);
}

void MinimalPrepareRendererResources::detachRasterInMainThread(
    const Cesium3DTilesSelection::Tile& tile,
    int32_t overlayTextureCoordinateID,
    const CesiumRasterOverlays::RasterOverlayTile&,
    void* pMainThreadRendererResources) noexcept {
    auto* raster = reinterpret_cast<RasterTextureResource*>(pMainThreadRendererResources);
    if (!raster) return;

    const Cesium3DTilesSelection::TileRenderContent* renderContent =
        tile.getContent().getRenderContent();
    if (!renderContent) return;
    const void* rawResources = renderContent->getRenderResources();
    auto* resources = isGpuResource(rawResources)
        ? const_cast<GpuTileResources*>(reinterpret_cast<const GpuTileResources*>(rawResources))
        : nullptr;
    if (!resources) return;

    raster->pendingAttachments.erase(
        std::remove_if(
            raster->pendingAttachments.begin(),
            raster->pendingAttachments.end(),
            [resources, overlayTextureCoordinateID](const RasterTextureResource::PendingAttachment& pending) {
                return pending.resources == resources &&
                    pending.overlayTextureCoordinateID == overlayTextureCoordinateID;
            }),
        raster->pendingAttachments.end());

    if (!raster->textureResource) return;

    for (GpuPrimitive& primitive : resources->primitives) {
        const size_t before = primitive.rasterAttachments.size();
        primitive.rasterAttachments.erase(
            std::remove_if(
                primitive.rasterAttachments.begin(),
                primitive.rasterAttachments.end(),
                [texture = raster->textureResource, overlayTextureCoordinateID](const RasterAttachment& attachment) {
                    return attachment.textureResource == texture &&
                           attachment.overlayTextureCoordinateID == overlayTextureCoordinateID;
                }),
            primitive.rasterAttachments.end());
        (void)before;
    }
}

bool MinimalPrepareRendererResources::isGpuResource(const void* resource) {
    if (!resource) return false;
    const auto* header = reinterpret_cast<const RenderResourceHeader*>(resource);
    return header->kind == RenderResourceKind::MainThreadGpu;
}

void MinimalPrepareRendererResources::prioritizeVisibleResources(
    const std::vector<const GpuTileResources*>& visibleResources) {
    std::lock_guard<std::mutex> lock(_uploadMutex);
    ++_uploadPriorityCounter;
    const uint64_t priority = _uploadPriorityCounter;
    std::unordered_set<const GpuTileResources*> visibleSet;
    visibleSet.reserve(visibleResources.size());
    for (const GpuTileResources* resources : visibleResources) {
        if (resources) {
            visibleSet.insert(resources);
            const_cast<GpuTileResources*>(resources)->uploadPriority = priority;
        }
    }
    for (void* rawRaster : _pendingRasterUploads) {
        auto* raster = reinterpret_cast<RasterTextureResource*>(rawRaster);
        if (!raster) continue;
        for (const RasterTextureResource::PendingAttachment& pending : raster->pendingAttachments) {
            if (pending.resources && visibleSet.find(pending.resources) != visibleSet.end()) {
                raster->uploadPriority = priority;
                break;
            }
        }
    }
}

size_t MinimalPrepareRendererResources::processPendingGeometryUploads(size_t maxPrimitives) {
    if (maxPrimitives == 0) return 0;
    std::lock_guard<std::mutex> lock(_uploadMutex);
    std::stable_sort(
        _pendingGeometryUploads.begin(),
        _pendingGeometryUploads.end(),
        [](const GpuTileResources* a, const GpuTileResources* b) {
            const uint64_t priorityA = a ? a->uploadPriority : 0;
            const uint64_t priorityB = b ? b->uploadPriority : 0;
            return priorityA > priorityB;
        });
    size_t uploaded = 0;
    auto it = _pendingGeometryUploads.begin();
    while (it != _pendingGeometryUploads.end() && uploaded < maxPrimitives) {
        GpuTileResources* resources = *it;
        if (!resources) {
            it = _pendingGeometryUploads.erase(it);
            continue;
        }

        bool complete = true;
        for (GpuPrimitive& primitive : resources->primitives) {
            if (primitive.buffersUploaded) continue;
            uploadPrimitiveBuffers(primitive);
            primitive.buffersUploaded = primitive.vertexBuffer != 0 && primitive.indexBuffer != 0;
            ++uploaded;
            if (uploaded >= maxPrimitives) {
                complete = false;
                break;
            }
        }

        if (complete) {
            resources->queuedForUpload = false;
            it = _pendingGeometryUploads.erase(it);
        } else {
            ++it;
        }
    }
    return uploaded;
}

bool MinimalPrepareRendererResources::uploadRasterToAtlas(
    int32_t width,
    int32_t height,
    const std::vector<std::byte>& pixelData,
    std::shared_ptr<GpuTexture>& textureResource,
    glm::dvec2& atlasTranslation,
    glm::dvec2& atlasScale) {
    constexpr int32_t atlasWidth = 4096;
    constexpr int32_t atlasHeight = 4096;
    constexpr int32_t gutter = 1;
    constexpr int32_t padding = 1;
    const int32_t packedWidth = width + gutter * 2;
    const int32_t packedHeight = height + gutter * 2;
    if (width <= 0 ||
        height <= 0 ||
        packedWidth + padding > atlasWidth ||
        packedHeight + padding > atlasHeight ||
        pixelData.empty()) {
        return false;
    }

    RasterAtlas* targetAtlas = nullptr;
    for (RasterAtlas& atlas : _rasterAtlases) {
        if (atlas.cursorX + packedWidth > atlas.width) {
            atlas.cursorX = 0;
            atlas.cursorY += atlas.rowHeight + padding;
            atlas.rowHeight = 0;
        }
        if (atlas.cursorY + packedHeight <= atlas.height) {
            targetAtlas = &atlas;
            break;
        }
    }

    if (!targetAtlas) {
        RasterAtlas atlas;
        atlas.width = atlasWidth;
        atlas.height = atlasHeight;
        GLuint texture = 0;
        glGenTextures(1, &texture);
        if (texture == 0) return false;
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            atlas.width,
            atlas.height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
        atlas.textureResource = makeTextureResource(
            texture,
            static_cast<size_t>(atlas.width) * static_cast<size_t>(atlas.height) * 4,
            atlas.width,
            atlas.height);
        _rasterAtlases.push_back(std::move(atlas));
        targetAtlas = &_rasterAtlases.back();
    }

    const int32_t x = targetAtlas->cursorX;
    const int32_t y = targetAtlas->cursorY;
    const int32_t innerX = x + gutter;
    const int32_t innerY = y + gutter;
    const std::vector<std::byte> glPixelData = flipRgbaRows(width, height, pixelData);
    glBindTexture(GL_TEXTURE_2D, targetAtlas->textureResource->id);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        innerX,
        innerY,
        width,
        height,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        glPixelData.data());

    std::vector<std::byte> edgeColumn(static_cast<size_t>(height) * 4);
    for (int32_t row = 0; row < height; ++row) {
        std::memcpy(
            edgeColumn.data() + static_cast<size_t>(row) * 4,
            glPixelData.data() + static_cast<size_t>(row) * static_cast<size_t>(width) * 4,
            4);
    }
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        x,
        innerY,
        gutter,
        height,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        edgeColumn.data());
    for (int32_t row = 0; row < height; ++row) {
        std::memcpy(
            edgeColumn.data() + static_cast<size_t>(row) * 4,
            glPixelData.data() +
                (static_cast<size_t>(row) * static_cast<size_t>(width) + static_cast<size_t>(width - 1)) * 4,
            4);
    }
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        innerX + width,
        innerY,
        gutter,
        height,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        edgeColumn.data());

    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        innerX,
        y,
        width,
        gutter,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        glPixelData.data());
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        innerX,
        innerY + height,
        width,
        gutter,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        glPixelData.data() + static_cast<size_t>(height - 1) * static_cast<size_t>(width) * 4);

    const std::byte bottomLeft[4] = {
        glPixelData[0],
        glPixelData[1],
        glPixelData[2],
        glPixelData[3]};
    const size_t bottomRightOffset = static_cast<size_t>(width - 1) * 4;
    const std::byte bottomRight[4] = {
        glPixelData[bottomRightOffset],
        glPixelData[bottomRightOffset + 1],
        glPixelData[bottomRightOffset + 2],
        glPixelData[bottomRightOffset + 3]};
    const size_t topLeftOffset = static_cast<size_t>(height - 1) * static_cast<size_t>(width) * 4;
    const std::byte topLeft[4] = {
        glPixelData[topLeftOffset],
        glPixelData[topLeftOffset + 1],
        glPixelData[topLeftOffset + 2],
        glPixelData[topLeftOffset + 3]};
    const size_t topRightOffset =
        (static_cast<size_t>(height - 1) * static_cast<size_t>(width) + static_cast<size_t>(width - 1)) * 4;
    const std::byte topRight[4] = {
        glPixelData[topRightOffset],
        glPixelData[topRightOffset + 1],
        glPixelData[topRightOffset + 2],
        glPixelData[topRightOffset + 3]};
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, gutter, gutter, GL_RGBA, GL_UNSIGNED_BYTE, bottomLeft);
    glTexSubImage2D(GL_TEXTURE_2D, 0, innerX + width, y, gutter, gutter, GL_RGBA, GL_UNSIGNED_BYTE, bottomRight);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, innerY + height, gutter, gutter, GL_RGBA, GL_UNSIGNED_BYTE, topLeft);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        innerX + width,
        innerY + height,
        gutter,
        gutter,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        topRight);
    glBindTexture(GL_TEXTURE_2D, 0);

    textureResource = targetAtlas->textureResource;
    atlasTranslation = glm::dvec2(
        static_cast<double>(innerX) / static_cast<double>(targetAtlas->width),
        static_cast<double>(innerY) / static_cast<double>(targetAtlas->height));
    atlasScale = glm::dvec2(
        static_cast<double>(width) / static_cast<double>(targetAtlas->width),
        static_cast<double>(height) / static_cast<double>(targetAtlas->height));

    targetAtlas->cursorX += packedWidth + padding;
    targetAtlas->rowHeight = std::max(targetAtlas->rowHeight, packedHeight);
    return true;
}

size_t MinimalPrepareRendererResources::processPendingRasterUploads(size_t maxTextures) {
    if (maxTextures == 0) return 0;
    std::lock_guard<std::mutex> lock(_uploadMutex);
    std::stable_sort(
        _pendingRasterUploads.begin(),
        _pendingRasterUploads.end(),
        [](const void* a, const void* b) {
            const auto* rasterA = reinterpret_cast<const RasterTextureResource*>(a);
            const auto* rasterB = reinterpret_cast<const RasterTextureResource*>(b);
            const uint64_t priorityA = rasterA ? rasterA->uploadPriority : 0;
            const uint64_t priorityB = rasterB ? rasterB->uploadPriority : 0;
            return priorityA > priorityB;
        });
    size_t uploaded = 0;
    auto it = _pendingRasterUploads.begin();
    while (it != _pendingRasterUploads.end() && uploaded < maxTextures) {
        auto* raster = reinterpret_cast<RasterTextureResource*>(*it);
        if (!raster) {
            it = _pendingRasterUploads.erase(it);
            continue;
        }
        if (!raster->textureResource) {
            const bool uploadedToAtlas = uploadRasterToAtlas(
                raster->width,
                raster->height,
                raster->pixelData,
                raster->textureResource,
                raster->atlasTranslation,
                raster->atlasScale);
            if (!uploadedToAtlas) {
                raster->texture = uploadRgbaTexture(
                    raster->width,
                    raster->height,
                    raster->pixelData,
                    acquireReusableTexture(raster->width, raster->height));
            }
            if (raster->textureResource || raster->texture != 0) {
                if (!raster->textureResource) {
                raster->textureResource = makeTextureResource(
                    raster->texture,
                    raster->bytes,
                    raster->width,
                    raster->height);
                raster->texture = 0;
                }
                raster->pixelData.clear();
                raster->pixelData.shrink_to_fit();
                for (const RasterTextureResource::PendingAttachment& pending : raster->pendingAttachments) {
                    if (!pending.resources) continue;
                    attachRasterToResources(
                        *pending.resources,
                        raster->textureResource,
                        pending.overlayTextureCoordinateID,
                        combineAtlasTranslation(
                            pending.translation,
                            raster->atlasTranslation,
                            raster->atlasScale),
                        pending.scale * raster->atlasScale);
                }
                raster->pendingAttachments.clear();
            }
            ++uploaded;
        }
        raster->queuedForUpload = false;
        it = _pendingRasterUploads.erase(it);
    }
    return uploaded;
}

size_t MinimalPrepareRendererResources::pendingGeometryUploads() const {
    std::lock_guard<std::mutex> lock(_uploadMutex);
    size_t pending = 0;
    for (const GpuTileResources* resources : _pendingGeometryUploads) {
        if (!resources) continue;
        for (const GpuPrimitive& primitive : resources->primitives) {
            if (!primitive.buffersUploaded) ++pending;
        }
    }
    return pending;
}

size_t MinimalPrepareRendererResources::pendingRasterUploads() const {
    std::lock_guard<std::mutex> lock(_uploadMutex);
    return _pendingRasterUploads.size();
}

void MinimalPrepareRendererResources::appendPrimitive(
    const CesiumGltf::Model& model,
    const CesiumGltf::MeshPrimitive& primitive,
    const glm::dmat4& transform,
    GpuTileResources& resources) {
    static int logBudget = 12;
    int32_t positionAccessor = -1;
    int32_t overlayUvAccessor = -1;
    std::vector<std::pair<int32_t, int32_t>> overlayAccessors;
    for (const auto& entry : primitive.attributes) {
        if (entry.first == "POSITION") {
            positionAccessor = entry.second;
        } else if (const std::optional<int32_t> id = overlayTextureCoordinateID(entry.first)) {
            overlayAccessors.push_back({*id, entry.second});
            if (*id == 0) {
                overlayUvAccessor = entry.second;
            }
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
    if (overlayAccessors.empty()) {
        if (logBudget-- > 0) {
            std::string keys;
            for (const auto& entry : primitive.attributes) {
                if (!keys.empty()) keys += ",";
                keys += entry.first;
            }
            __android_log_print(
                ANDROID_LOG_WARN,
                "CesiumBridge",
                "skip primitive: no Cesium overlay UV attrs=%s",
                keys.c_str());
        }
        return;
    }
    if (overlayUvAccessor < 0) {
        overlayUvAccessor = overlayAccessors.front().second;
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

    CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC2<float>> overlayUvs(
        model,
        overlayUvAccessor);
    if (overlayUvAccessor >= 0 &&
        (overlayUvs.status() != CesiumGltf::AccessorViewStatus::Valid ||
         overlayUvs.size() != positions.size())) {
        if (logBudget-- > 0) {
            __android_log_print(
                ANDROID_LOG_WARN,
                "CesiumBridge",
                "skip primitive: invalid Cesium overlay UV status=%d size=%lld positions=%lld accessor=%d",
                static_cast<int>(overlayUvs.status()),
                static_cast<long long>(overlayUvs.size()),
                static_cast<long long>(positions.size()),
                overlayUvAccessor);
        }
        return;
    }
    GpuPrimitive gpu;
    std::vector<float> vertexData;
    vertexData.reserve(static_cast<size_t>(positions.size()) * 5);

    const std::optional<CesiumGltfContent::SkirtMeshMetadata> skirtMeshMetadata =
        CesiumGltfContent::SkirtMeshMetadata::parseFromGltfExtras(primitive.extras);
    const bool isQuantizedMeshTerrain = skirtMeshMetadata.has_value();

    bool hasOrigin = false;
    for (int64_t i = 0; i < positions.size(); ++i) {
        const auto& p = positions[i];
        const glm::dvec3 localPosition(p.value[0], p.value[1], p.value[2]);
        const glm::dvec3 ecef = isQuantizedMeshTerrain
            ? skirtMeshMetadata->meshCenter + localPosition
            : glm::dvec3(transform * glm::dvec4(localPosition, 1.0));
        if (!hasOrigin) {
            gpu.originEcef = ecef;
            hasOrigin = true;
        }
        const glm::dvec3 relative = ecef - gpu.originEcef;
        vertexData.push_back(static_cast<float>(relative.x));
        vertexData.push_back(static_cast<float>(relative.y));
        vertexData.push_back(static_cast<float>(relative.z));
        const auto& overlayUv = overlayUvs[i];
        const glm::dvec2 uv(overlayUv.value[0], overlayUv.value[1]);
        vertexData.push_back(static_cast<float>(uv.x));
        vertexData.push_back(static_cast<float>(uv.y));
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

    gpu.cpuVertexData = std::move(vertexData);
    gpu.baseCpuVertexData = gpu.cpuVertexData;
    if (hasOrigin) {
        for (const auto& [overlayID, accessorID] : overlayAccessors) {
            if (accessorID == overlayUvAccessor) {
                gpu.overlayVertexBuffers.push_back(
                    OverlayVertexBuffer{overlayID, 0, gpu.cpuVertexData});
                continue;
            }

            CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC2<float>> extraOverlayUvs(
                model,
                accessorID);
            if (extraOverlayUvs.status() != CesiumGltf::AccessorViewStatus::Valid ||
                extraOverlayUvs.size() != positions.size()) {
                continue;
            }

            std::vector<float> overlayVertexData;
            overlayVertexData.reserve(static_cast<size_t>(positions.size()) * 5);
            for (int64_t i = 0; i < positions.size(); ++i) {
                const auto& p = positions[i];
                const glm::dvec3 localPosition(p.value[0], p.value[1], p.value[2]);
                const glm::dvec3 ecef = isQuantizedMeshTerrain
                    ? skirtMeshMetadata->meshCenter + localPosition
                    : glm::dvec3(transform * glm::dvec4(localPosition, 1.0));
                const glm::dvec3 relative = ecef - gpu.originEcef;
                const auto& uv = extraOverlayUvs[i];
                overlayVertexData.push_back(static_cast<float>(relative.x));
                overlayVertexData.push_back(static_cast<float>(relative.y));
                overlayVertexData.push_back(static_cast<float>(relative.z));
                overlayVertexData.push_back(uv.value[0]);
                overlayVertexData.push_back(uv.value[1]);
            }
            gpu.overlayVertexBuffers.push_back(
                OverlayVertexBuffer{
                    overlayID,
                    0,
                    std::move(overlayVertexData)});
        }
    }
    gpu.cpuIndexData = cpuIndices;
    gpu.indexCount = static_cast<GLsizei>(cpuIndices.size());
    gpu.indexType = GL_UNSIGNED_INT;
    resources.bytes += gpu.cpuVertexData.size() * sizeof(float);
    resources.bytes += gpu.cpuIndexData.size() * sizeof(uint32_t);
    resources.primitives.push_back(gpu);
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

void MinimalPrepareRendererResources::uploadPrimitiveBuffers(GpuPrimitive& gpu) {
    if (gpu.buffersUploaded) return;
    if (gpu.indexBuffer == 0 && !gpu.cpuIndexData.empty()) {
        uploadIndexBuffer(gpu.cpuIndexData, gpu);
    }
    if (gpu.vertexBuffer == 0 && !gpu.cpuVertexData.empty()) {
        gpu.vertexBuffer = uploadVertexBuffer(gpu.cpuVertexData);
    }
    for (OverlayVertexBuffer& overlay : gpu.overlayVertexBuffers) {
        if (overlay.vertexBuffer != 0) continue;
        if (overlay.cpuVertexData == gpu.cpuVertexData) {
            overlay.vertexBuffer = gpu.vertexBuffer;
        } else if (!overlay.cpuVertexData.empty()) {
            overlay.vertexBuffer = uploadVertexBuffer(overlay.cpuVertexData);
        }
    }
}

void MinimalPrepareRendererResources::queueGeometryUpload(GpuTileResources* resources) {
    if (!resources || resources->queuedForUpload || resources->primitives.empty()) return;
    resources->queuedForUpload = true;
    std::lock_guard<std::mutex> lock(_uploadMutex);
    _pendingGeometryUploads.push_back(resources);
}

void MinimalPrepareRendererResources::removeGeometryUpload(GpuTileResources* resources) noexcept {
    if (!resources) return;
    std::lock_guard<std::mutex> lock(_uploadMutex);
    _pendingGeometryUploads.erase(
        std::remove(_pendingGeometryUploads.begin(), _pendingGeometryUploads.end(), resources),
        _pendingGeometryUploads.end());
    resources->queuedForUpload = false;
}

void MinimalPrepareRendererResources::removePendingRasterAttachments(GpuTileResources* resources) noexcept {
    if (!resources) return;
    std::lock_guard<std::mutex> lock(_uploadMutex);
    for (void* rawRaster : _pendingRasterUploads) {
        auto* raster = reinterpret_cast<RasterTextureResource*>(rawRaster);
        if (!raster) continue;
        raster->pendingAttachments.erase(
            std::remove_if(
                raster->pendingAttachments.begin(),
                raster->pendingAttachments.end(),
                [resources](const RasterTextureResource::PendingAttachment& pending) {
                    return pending.resources == resources;
                }),
            raster->pendingAttachments.end());
    }
}

void MinimalPrepareRendererResources::queueRasterUpload(void* resource) {
    auto* raster = reinterpret_cast<RasterTextureResource*>(resource);
    if (!raster || raster->queuedForUpload || raster->textureResource) return;
    raster->queuedForUpload = true;
    std::lock_guard<std::mutex> lock(_uploadMutex);
    _pendingRasterUploads.push_back(resource);
}

void MinimalPrepareRendererResources::removeRasterUpload(void* resource) noexcept {
    if (!resource) return;
    auto* raster = reinterpret_cast<RasterTextureResource*>(resource);
    std::lock_guard<std::mutex> lock(_uploadMutex);
    _pendingRasterUploads.erase(
        std::remove(_pendingRasterUploads.begin(), _pendingRasterUploads.end(), resource),
        _pendingRasterUploads.end());
    raster->queuedForUpload = false;
}

GLuint MinimalPrepareRendererResources::acquireReusableTexture(int32_t width, int32_t height) {
    auto it = std::find_if(
        _reusableTextures.begin(),
        _reusableTextures.end(),
        [width, height](const ReusableTexture& texture) {
            return texture.width == width && texture.height == height && texture.id != 0;
        });
    if (it == _reusableTextures.end()) return 0;
    const GLuint texture = it->id;
    _reusableTextures.erase(it);
    return texture;
}

void MinimalPrepareRendererResources::cacheReusableTexture(
    GLuint texture,
    int32_t width,
    int32_t height,
    size_t bytes) noexcept {
    if (texture == 0 || width <= 0 || height <= 0) return;
    std::lock_guard<std::mutex> lock(_uploadMutex);
    constexpr size_t maxReusableTextures = 64;
    if (_reusableTextures.size() >= maxReusableTextures) {
        GLuint evicted = _reusableTextures.front().id;
        _reusableTextures.erase(_reusableTextures.begin());
        if (evicted != 0) {
            glDeleteTextures(1, &evicted);
        }
    }
    _reusableTextures.push_back(ReusableTexture{texture, width, height, bytes});
}

} // namespace cesium_poc
