#pragma once

#include "cesium_types.h"

#include <unordered_set>
#include <vector>

namespace cesium_poc {

void freeBatchResources(
    BatchResources& batch,
    std::unordered_set<const GpuPrimitive*>& batchedPrimitives);

void rebuildSelectedBatchResources(
    const std::vector<const GpuTileResources*>& selectedResources,
    GLuint batchProgram,
    BatchResources& batch,
    std::unordered_set<const GpuPrimitive*>& batchedPrimitives);

} // namespace cesium_poc
