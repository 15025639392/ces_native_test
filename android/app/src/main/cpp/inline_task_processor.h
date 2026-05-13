#pragma once

#include <CesiumAsync/ITaskProcessor.h>

#include <functional>
#include <thread>

namespace cesium_poc {

class InlineTaskProcessor final : public CesiumAsync::ITaskProcessor {
public:
    void startTask(std::function<void()> f) override {
        std::thread(std::move(f)).detach();
    }
};

} // namespace cesium_poc
