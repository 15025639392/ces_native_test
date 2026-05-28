#pragma once

#include <CesiumAsync/ITaskProcessor.h>

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace cesium_poc {

class BackgroundTaskProcessor final : public CesiumAsync::ITaskProcessor {
public:
    explicit BackgroundTaskProcessor(size_t workerCount = 4) {
        const size_t count = workerCount == 0 ? 1 : workerCount;
        _workers.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            _workers.emplace_back([this]() { this->runWorker(); });
        }
    }

    ~BackgroundTaskProcessor() override {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stopped = true;
        }
        _condition.notify_all();
        for (std::thread& worker : _workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void startTask(std::function<void()> f) override {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_stopped) return;
            _tasks.emplace_back(std::move(f));
        }
        _condition.notify_one();
    }

private:
    void runWorker() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _condition.wait(lock, [this]() {
                    return _stopped || !_tasks.empty();
                });
                if (_stopped && _tasks.empty()) {
                    return;
                }
                task = std::move(_tasks.front());
                _tasks.pop_front();
            }
            task();
        }
    }

    std::mutex _mutex;
    std::condition_variable _condition;
    std::deque<std::function<void()>> _tasks;
    std::vector<std::thread> _workers;
    bool _stopped = false;
};

} // namespace cesium_poc
