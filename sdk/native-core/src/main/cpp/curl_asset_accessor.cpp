#include "curl_asset_accessor.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/IAssetRequest.h>
#include <CesiumAsync/IAssetResponse.h>

#include <android/log.h>

#include <curl/curl.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace cesium_poc {
namespace {

class CurlAssetResponse final : public CesiumAsync::IAssetResponse {
public:
    CurlAssetResponse(
        uint16_t statusCode,
        std::string contentType,
        CesiumAsync::HttpHeaders headers,
        std::vector<std::byte> data)
        : _statusCode(statusCode),
          _contentType(std::move(contentType)),
          _headers(std::move(headers)),
          _data(std::move(data)) {}

    uint16_t statusCode() const override { return _statusCode; }
    std::string contentType() const override { return _contentType; }
    const CesiumAsync::HttpHeaders& headers() const override { return _headers; }
    std::span<const std::byte> data() const override { return _data; }

private:
    uint16_t _statusCode = 0;
    std::string _contentType;
    CesiumAsync::HttpHeaders _headers;
    std::vector<std::byte> _data;
};

class CurlAssetRequest final : public CesiumAsync::IAssetRequest {
public:
    CurlAssetRequest(
        std::string method,
        std::string url,
        CesiumAsync::HttpHeaders headers,
        std::unique_ptr<CurlAssetResponse> response)
        : _method(std::move(method)),
          _url(std::move(url)),
          _headers(std::move(headers)),
          _response(std::move(response)) {}

    const std::string& method() const override { return _method; }
    const std::string& url() const override { return _url; }
    const CesiumAsync::HttpHeaders& headers() const override { return _headers; }
    const CesiumAsync::IAssetResponse* response() const override { return _response.get(); }

private:
    std::string _method;
    std::string _url;
    CesiumAsync::HttpHeaders _headers;
    std::unique_ptr<CurlAssetResponse> _response;
};

size_t writeBody(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* data = reinterpret_cast<std::vector<std::byte>*>(userdata);
    const size_t bytes = size * nmemb;
    const std::byte* first = reinterpret_cast<const std::byte*>(ptr);
    data->insert(data->end(), first, first + bytes);
    return bytes;
}

std::shared_ptr<CesiumAsync::IAssetRequest> performCurlRequest(
    const std::string& method,
    const std::string& url,
    const std::vector<CesiumAsync::IAssetAccessor::THeader>& headers,
    const std::span<const std::byte>& contentPayload) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return std::make_shared<CurlAssetRequest>(
            method,
            url,
            CesiumAsync::HttpHeaders(),
            std::make_unique<CurlAssetResponse>(
                0,
                std::string(),
                CesiumAsync::HttpHeaders(),
                std::vector<std::byte>()));
    }

    std::vector<std::byte> body;
    struct curl_slist* requestHeaders = nullptr;
    for (const auto& header : headers) {
        requestHeaders = curl_slist_append(
            requestHeaders,
            (header.first + ": " + header.second).c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 8000L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "cesium-native-android-poc/0.1");
    curl_easy_setopt(curl, CURLOPT_CAPATH, "/system/etc/security/cacerts");
    if (requestHeaders) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, requestHeaders);
    }
    if (method != "GET") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
        if (!contentPayload.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, contentPayload.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(contentPayload.size()));
        }
    }

    const CURLcode result = curl_easy_perform(curl);
    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    char* contentType = nullptr;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contentType);
    const std::string contentTypeValue = contentType ? std::string(contentType) : std::string();

    if (requestHeaders) {
        curl_slist_free_all(requestHeaders);
    }
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        __android_log_print(
            ANDROID_LOG_WARN,
            "CesiumBridge",
            "curl request failed code=%d url=%s error=%s",
            static_cast<int>(result),
            url.c_str(),
            curl_easy_strerror(result));
        body.clear();
        responseCode = 0;
    }
    CesiumAsync::HttpHeaders responseHeaders;
    return std::make_shared<CurlAssetRequest>(
        method,
        url,
        CesiumAsync::HttpHeaders(headers.begin(), headers.end()),
        std::make_unique<CurlAssetResponse>(
            static_cast<uint16_t>(std::max<long>(0, std::min<long>(responseCode, 65535))),
            contentTypeValue,
            std::move(responseHeaders),
            std::move(body)));
}

class CurlAssetAccessor final : public CesiumAsync::IAssetAccessor {
public:
    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    get(
        const CesiumAsync::AsyncSystem& asyncSystem,
        const std::string& url,
        const std::vector<THeader>& headers = {}) override {
        return request(asyncSystem, "GET", url, headers, {});
    }

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    request(
        const CesiumAsync::AsyncSystem& asyncSystem,
        const std::string& verb,
        const std::string& url,
        const std::vector<THeader>& headers = std::vector<THeader>(),
        const std::span<const std::byte>& contentPayload = {}) override {
        std::vector<std::byte> payload(contentPayload.begin(), contentPayload.end());
        return asyncSystem.runInWorkerThread([verb, url, headers, payload = std::move(payload)]() {
            return performCurlRequest(verb, url, headers, std::span<const std::byte>(payload));
        });
    }

    void tick() noexcept override {}
};

} // namespace

std::shared_ptr<CesiumAsync::IAssetAccessor> createCurlAssetAccessor() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    return std::make_shared<CurlAssetAccessor>();
}

} // namespace cesium_poc
