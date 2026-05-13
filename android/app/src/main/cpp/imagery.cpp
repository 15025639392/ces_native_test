#include "imagery.h"

#include <android/log.h>
#include <curl/curl.h>

#include <Cesium3DTilesSelection/BoundingVolume.h>
#include <Cesium3DTilesSelection/Tile.h>
#include <CesiumGeospatial/BoundingRegion.h>
#include <CesiumGeospatial/GlobeRectangle.h>

#include <glm/common.hpp>

#include <algorithm>
#include <condition_variable>
#include <cmath>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include <stb_image.h>

namespace cesium_poc {
namespace {

double webMercatorTileX(double longitudeRadians, uint32_t z) {
    const double n = std::pow(2.0, static_cast<double>(z));
    const double lonDegrees = longitudeRadians * 180.0 / kPi;
    return (lonDegrees + 180.0) / 360.0 * n;
}

double webMercatorTileY(double latitudeRadians, uint32_t z) {
    const double n = std::pow(2.0, static_cast<double>(z));
    const double clampedLat = glm::clamp(latitudeRadians * 180.0 / kPi, -85.05112878, 85.05112878);
    const double latRad = clampedLat * kPi / 180.0;
    return (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / kPi) * 0.5 * n;
}

uint32_t tileIndex(double tileCoordinate, uint32_t z) {
    const double n = std::pow(2.0, static_cast<double>(z));
    return static_cast<uint32_t>(glm::clamp(std::floor(tileCoordinate), 0.0, n - 1.0));
}

std::string satelliteTileUrl(uint32_t z, uint32_t x, uint32_t y) {
    const uint32_t server = ((x + y) % 4) + 1;
    std::ostringstream stream;
    stream << "https://webst0" << server
           << ".is.autonavi.com/appmaptile?style=6&x=" << x
           << "&y=" << y
           << "&z=" << z;
    return stream.str();
}

std::string satelliteTileKey(uint32_t z, uint32_t x, uint32_t y) {
    std::ostringstream stream;
    stream << "gaode-satellite/" << z << "/" << x << "/" << y;
    return stream.str();
}

std::unordered_map<std::string, std::shared_ptr<const ImageRgba>>& imageCache() {
    static std::unordered_map<std::string, std::shared_ptr<const ImageRgba>> cache;
    return cache;
}

std::mutex& imageCacheMutex() {
    static std::mutex mutex;
    return mutex;
}

size_t writeCurlBytes(char* ptr, size_t size, size_t nmemb, void* userdata) {
    const size_t byteCount = size * nmemb;
    auto* bytes = reinterpret_cast<std::vector<uint8_t>*>(userdata);
    bytes->insert(bytes->end(), reinterpret_cast<uint8_t*>(ptr), reinterpret_cast<uint8_t*>(ptr) + byteCount);
    return byteCount;
}

class DownloadPermit {
public:
    DownloadPermit() {
        std::unique_lock<std::mutex> lock(mutex());
        cv().wait(lock, []() { return activeDownloads() < 24; });
        ++activeDownloads();
    }

    ~DownloadPermit() {
        {
            std::lock_guard<std::mutex> lock(mutex());
            --activeDownloads();
        }
        cv().notify_one();
    }

private:
    static std::mutex& mutex() {
        static std::mutex value;
        return value;
    }

    static std::condition_variable& cv() {
        static std::condition_variable value;
        return value;
    }

    static int& activeDownloads() {
        static int value = 0;
        return value;
    }
};

bool downloadUrl(const std::string& url, std::vector<uint8_t>& bytes) {
    static std::once_flag curlInitFlag;
    std::call_once(curlInitFlag, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });
    const DownloadPermit permit;

    for (int attempt = 0; attempt < 2; ++attempt) {
        bytes.clear();
        CURL* curl = curl_easy_init();
        if (!curl) return false;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "CesiumNativeAndroidPoC/0.1");
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 1800L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeCurlBytes);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bytes);

        const CURLcode result = curl_easy_perform(curl);
        long responseCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        curl_easy_cleanup(curl);
        if (result == CURLE_OK && responseCode >= 200 && responseCode < 300 && !bytes.empty()) {
            return true;
        }
    }
    return false;
}

} // namespace

bool satelliteTileId(
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
    uint32_t candidateZ = static_cast<uint32_t>(glm::clamp(
        std::floor(std::log2((2.0 * kPi) / lonSpan)),
        0.0,
        18.0));
    constexpr double edgeEpsilon = 1e-12;
    while (true) {
        const double westTileX = webMercatorTileX(west, candidateZ);
        const double eastTileX = webMercatorTileX(std::max(west, east - edgeEpsilon), candidateZ);
        const double northTileY = webMercatorTileY(north, candidateZ);
        const double southTileY = webMercatorTileY(std::min(north, south + edgeEpsilon), candidateZ);
        const uint32_t minX = tileIndex(westTileX, candidateZ);
        const uint32_t maxX = tileIndex(eastTileX, candidateZ);
        const uint32_t minY = tileIndex(northTileY, candidateZ);
        const uint32_t maxY = tileIndex(southTileY, candidateZ);
        if ((minX == maxX && minY == maxY) || candidateZ == 0) {
            z = candidateZ;
            x = minX;
            y = minY;
            return true;
        }
        --candidateZ;
    }
}

std::vector<SatelliteTileId> satelliteTileIdsForRegion(
    const Cesium3DTilesSelection::BoundingVolume& boundingVolume) {
    const CesiumGeospatial::BoundingRegion* region =
        Cesium3DTilesSelection::getBoundingRegionFromBoundingVolume(boundingVolume);
    if (!region) return {};

    const CesiumGeospatial::GlobeRectangle& rectangle = region->getRectangle();
    const double west = rectangle.getWest();
    const double east = rectangle.getEast();
    const double south = rectangle.getSouth();
    const double north = rectangle.getNorth();
    const double lonSpan = std::max(east - west, 2.0 * kPi / std::pow(2.0, 18.0));
    uint32_t z = static_cast<uint32_t>(glm::clamp(
        std::floor(std::log2((2.0 * kPi) / lonSpan)) + 1.0,
        0.0,
        18.0));
    constexpr double edgeEpsilon = 1e-12;
    constexpr uint32_t maxImageryTilesPerGeometryTile = 4;

    while (true) {
        const uint32_t minX = tileIndex(webMercatorTileX(west, z), z);
        const uint32_t maxX = tileIndex(webMercatorTileX(std::max(west, east - edgeEpsilon), z), z);
        const uint32_t minY = tileIndex(webMercatorTileY(north, z), z);
        const uint32_t maxY = tileIndex(webMercatorTileY(std::min(north, south + edgeEpsilon), z), z);
        const uint32_t count = (maxX - minX + 1) * (maxY - minY + 1);
        if (count <= maxImageryTilesPerGeometryTile || z == 0) {
            std::vector<SatelliteTileId> tiles;
            tiles.reserve(count);
            for (uint32_t y = minY; y <= maxY; ++y) {
                for (uint32_t x = minX; x <= maxX; ++x) {
                    tiles.push_back({z, x, y});
                }
            }
            return tiles;
        }
        --z;
    }
}

glm::dvec2 unclampedWebMercatorTileUv(
    double longitudeRadians,
    double latitudeRadians,
    uint32_t z,
    uint32_t x,
    uint32_t y) {
    const double tileX = webMercatorTileX(longitudeRadians, z);
    const double tileY = webMercatorTileY(latitudeRadians, z);
    return glm::dvec2(tileX - static_cast<double>(x), tileY - static_cast<double>(y));
}

glm::dvec2 webMercatorTileUv(
    double longitudeRadians,
    double latitudeRadians,
    uint32_t z,
    uint32_t x,
    uint32_t y) {
    const glm::dvec2 uv = unclampedWebMercatorTileUv(longitudeRadians, latitudeRadians, z, x, y);
    return glm::dvec2(
        glm::clamp(uv.x, 0.0, 1.0),
        glm::clamp(uv.y, 0.0, 1.0));
}

std::shared_ptr<const ImageRgba> imageForSatelliteTile(
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

} // namespace cesium_poc
