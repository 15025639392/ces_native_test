#include "imagery_source_config.h"

#include <mutex>

namespace cesium_poc {
namespace {

constexpr const char* kDefaultSatelliteImageryUrlTemplate =
    "http://webst0{s}.is.autonavi.com/appmaptile?style=6&x={x}&y={y}&z={z}";

std::string& configuredSatelliteImageryUrlTemplate() {
    static std::string value = kDefaultSatelliteImageryUrlTemplate;
    return value;
}

std::mutex& imageryConfigMutex() {
    static std::mutex mutex;
    return mutex;
}

} // namespace

void setSatelliteImageryUrlTemplate(const std::string& urlTemplate) {
    const std::string nextTemplate =
        urlTemplate.empty() ? kDefaultSatelliteImageryUrlTemplate : urlTemplate;
    std::lock_guard<std::mutex> lock(imageryConfigMutex());
    configuredSatelliteImageryUrlTemplate() = nextTemplate;
}

std::string satelliteImageryUrlTemplate() {
    std::lock_guard<std::mutex> lock(imageryConfigMutex());
    return configuredSatelliteImageryUrlTemplate();
}

} // namespace cesium_poc
