#pragma once

#include "cesium_types.h"

#include <Cesium3DTilesSelection/IPrepareRendererResources.h>
#include <CesiumAsync/Future.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumGltf/ImageAsset.h>
#include <CesiumGltf/MeshPrimitive.h>
#include <CesiumGltf/Model.h>
#include <CesiumRasterOverlays/RasterOverlayTile.h>

#include <memory>

namespace cesium_poc {

class MinimalPrepareRendererResources final
    : public Cesium3DTilesSelection::IPrepareRendererResources {
public:
    struct Marker {};

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

private:
    static bool isLoadThreadResource(void* resource);

    static std::unique_ptr<LoadTileResources> prepareTileImageInLoadThread(
        const Cesium3DTilesSelection::TileLoadResult& tileLoadResult);

    static void appendPrimitive(
        const CesiumGltf::Model& model,
        const CesiumGltf::MeshPrimitive& primitive,
        const glm::dmat4& transform,
        const Cesium3DTilesSelection::Tile& tile,
        const LoadTileResources* loadResources,
        GpuTileResources& resources);

    static std::vector<uint32_t> readIndices(
        const CesiumGltf::Model& model,
        const CesiumGltf::MeshPrimitive& primitive);

    static void uploadIndexBuffer(
        const std::vector<uint32_t>& data,
        GpuPrimitive& gpu);

    static std::shared_ptr<GpuTexture> acquireImageTexture(const ImageryTileResource& imagery);

    static std::shared_ptr<GpuTexture> createFallbackTexture(const Cesium3DTilesSelection::Tile& tile);
};

} // namespace cesium_poc
