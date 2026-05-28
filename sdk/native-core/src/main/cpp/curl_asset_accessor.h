#pragma once

#include <CesiumAsync/IAssetAccessor.h>

#include <memory>

namespace cesium_poc {

std::shared_ptr<CesiumAsync::IAssetAccessor> createCurlAssetAccessor();

} // namespace cesium_poc
