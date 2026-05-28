#pragma once

#include <CesiumAsync/ITaskProcessor.h>

#include <functional>

namespace cesium_poc {

class InlineTaskProcessor final : public CesiumAsync::ITaskProcessor {
public:
    void startTask(std::function<void()> f) override {
        f();
    }
};

} // namespace cesium_poc
