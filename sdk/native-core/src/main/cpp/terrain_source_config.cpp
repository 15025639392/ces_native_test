#include "terrain_source_config.h"

#include <mutex>

namespace cesium_poc {
namespace {

std::mutex& terrainConfigMutex() {
    static std::mutex mutex;
    return mutex;
}

std::string& configuredTerrainLayerJsonUrl() {
    static std::string value;
    return value;
}

} // namespace

void setTerrainLayerJsonUrl(const std::string& url) {
    std::lock_guard<std::mutex> lock(terrainConfigMutex());
    configuredTerrainLayerJsonUrl() = url;
}

std::string terrainLayerJsonUrl() {
    std::lock_guard<std::mutex> lock(terrainConfigMutex());
    return configuredTerrainLayerJsonUrl();
}

} // namespace cesium_poc
