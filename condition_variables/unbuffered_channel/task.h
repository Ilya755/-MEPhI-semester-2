#pragma once

#include <mutex>
#include <condition_variable>
#include <optional>

class TimeOut : public std::exception {
    const char* what() const noexcept override {
        return "Timeout";
    }
};

template<typename T>
class UnbufferedChannel {
private:
    std::optional<T> val;
    std::mutex mutex_;
    std::condition_variable cv_put;
    std::condition_variable cv_get;
    std::condition_variable cv_busy;

public:
    UnbufferedChannel()
        : val(std::nullopt)
        {}

    void Put(const T& data) {
        std::unique_lock<std::mutex> lock(mutex_);
        while (val != std::nullopt) {
            cv_put.wait(lock);
        }
        val = data;
        cv_get.notify_one();
        cv_busy.wait(lock);
    }

    T Get(std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
        std::unique_lock<std::mutex> lock(mutex_);
        auto end = std::chrono::steady_clock::now() + timeout;
        while (val == std::nullopt) {
            if (timeout.count() == 0) {
                cv_get.wait(lock);
            }
            else if (timeout.count() != 0 && cv_get.wait_until(lock, end) == std::cv_status::timeout) {
                throw TimeOut();
            }
        }
        T get_val = val.value();
        val = std::nullopt;
        cv_put.notify_one();
        cv_busy.notify_one();
        return get_val;
    }
};
