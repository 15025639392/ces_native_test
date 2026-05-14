#pragma once

#include "cesium_types.h"

#include <Cesium3DTilesSelection/BoundingVolume.h>

#include <glm/vec2.hpp>

#include <memory>
#include <vector>

namespace cesium_poc {

struct SatelliteTileId {
    uint32_t z = 0;
    uint32_t x = 0;
    uint32_t y = 0;
};

bool satelliteTileId(
    const Cesium3DTilesSelection::BoundingVolume& boundingVolume,
    uint32_t& z,
    uint32_t& x,
    uint32_t& y);

std::vector<SatelliteTileId> satelliteTileIdsForRegion(
    const Cesium3DTilesSelection::BoundingVolume& boundingVolume);

glm::dvec2 webMercatorTileUv(
    double longitudeRadians,
    double latitudeRadians,
    uint32_t z,
    uint32_t x,
    uint32_t y);

glm::dvec2 unclampedWebMercatorTileUv(
    double longitudeRadians,
    double latitudeRadians,
    uint32_t z,
    uint32_t x,
    uint32_t y);

std::shared_ptr<const ImageRgba> imageForSatelliteTile(
    uint32_t z,
    uint32_t x,
    uint32_t y);

} // namespace cesium_poc
