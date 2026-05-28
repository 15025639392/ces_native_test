#pragma once

#include "cesium_types.h"

#include <Cesium3DTilesSelection/IPrepareRendererResources.h>
#include <CesiumAsync/Future.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumGltf/ImageAsset.h>
#include <CesiumGltf/MeshPrimitive.h>
#include <CesiumGltf/Model.h>
#include <CesiumRasterOverlays/RasterOverlayTile.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace cesium_poc {

class MinimalPrepareRendererResources final
    : public Cesium3DTilesSelection::IPrepareRendererResources {
public:
    struct Marker {};

    ~MinimalPrepareRendererResources() override;

    CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources>
    prepareInLoadThread(
        const CesiumAsync::AsyncSystem& asyncSystem,
        Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
        const glm::dmat4&,
        const std::any&) override;

    void* prepareInMainThread(
        Cesium3DTilesSelection::Tile& tile,
        void* pLoadThreadResult) override;

    void free(
        Cesium3DTilesSelection::Tile&,
        void* pLoadThreadResult,
        void* pMainThreadResult) noexcept override;

    void* prepareRasterInLoadThread(
        CesiumGltf::ImageAsset&,
        const std::any&) override;

    void* prepareRasterInMainThread(
        CesiumRasterOverlays::RasterOverlayTile&,
        void* pLoadThreadResult) override;

    void freeRaster(
        const CesiumRasterOverlays::RasterOverlayTile&,
        void* pLoadThreadResult,
        void* pMainThreadResult) noexcept override;

    void attachRasterInMainThread(
        const Cesium3DTilesSelection::Tile&,
        int32_t,
        const CesiumRasterOverlays::RasterOverlayTile&,
        void*,
        const glm::dvec2&,
        const glm::dvec2&) override;

    void detachRasterInMainThread(
        const Cesium3DTilesSelection::Tile&,
        int32_t,
        const CesiumRasterOverlays::RasterOverlayTile&,
        void*) noexcept override;

    static bool isGpuResource(const void* resource);

    void prioritizeVisibleResources(const std::vector<const GpuTileResources*>& visibleResources);
    size_t processPendingGeometryUploads(size_t maxPrimitives);
    size_t processPendingRasterUploads(size_t maxTextures);
    size_t pendingGeometryUploads() const;
    size_t pendingRasterUploads() const;

private:
    struct ReusableTexture {
        GLuint id = 0;
        int32_t width = 0;
        int32_t height = 0;
        size_t bytes = 0;
    };

    struct RasterAtlas {
        std::shared_ptr<GpuTexture> textureResource;
        int32_t width = 4096;
        int32_t height = 4096;
        int32_t cursorX = 0;
        int32_t cursorY = 0;
        int32_t rowHeight = 0;
    };

    static void appendPrimitive(
        const CesiumGltf::Model& model,
        const CesiumGltf::MeshPrimitive& primitive,
        const glm::dmat4& transform,
        const Cesium3DTilesSelection::Tile& tile,
        GpuTileResources& resources);

    static std::vector<uint32_t> readIndices(
        const CesiumGltf::Model& model,
        const CesiumGltf::MeshPrimitive& primitive);

    static void uploadIndexBuffer(
        const std::vector<uint32_t>& data,
        GpuPrimitive& gpu);

    static void uploadPrimitiveBuffers(GpuPrimitive& gpu);

    void queueGeometryUpload(GpuTileResources* resources);
    void removeGeometryUpload(GpuTileResources* resources) noexcept;
    void removePendingRasterAttachments(GpuTileResources* resources) noexcept;
    void queueRasterUpload(void* resource);
    void removeRasterUpload(void* resource) noexcept;
    bool uploadRasterToAtlas(
        int32_t width,
        int32_t height,
        const std::vector<std::byte>& pixelData,
        std::shared_ptr<GpuTexture>& textureResource,
        glm::dvec2& atlasTranslation,
        glm::dvec2& atlasScale);
    GLuint acquireReusableTexture(int32_t width, int32_t height);
    void cacheReusableTexture(GLuint texture, int32_t width, int32_t height, size_t bytes) noexcept;

    mutable std::mutex _uploadMutex;
    std::vector<GpuTileResources*> _pendingGeometryUploads;
    std::vector<void*> _pendingRasterUploads;
    std::vector<ReusableTexture> _reusableTextures;
    std::vector<RasterAtlas> _rasterAtlases;
    uint64_t _uploadPriorityCounter = 0;
};

} // namespace cesium_poc
