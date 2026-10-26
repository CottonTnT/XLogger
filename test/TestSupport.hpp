#pragma once

#include "LogAppender.h"

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace test_support {

/// 线程安全捕获型 Appender：记录每一条消息，供测试断言。
class CaptureAppender : public X::LogAppender {
public:
    void append(std::string_view message) override
    {
        auto lock = std::lock_guard<std::mutex> { mtx_ };
        messages_.emplace_back(message);
    }
    void flush() override { }
    void sync() override { }

    [[nodiscard]] auto messages() const -> std::vector<std::string>
    {
        auto lock = std::lock_guard<std::mutex> { mtx_ };
        return messages_;
    }

    [[nodiscard]] auto lastMessage() const -> std::string
    {
        auto lock = std::lock_guard<std::mutex> { mtx_ };
        return messages_.empty() ? std::string {} : messages_.back();
    }

    [[nodiscard]] auto count() const -> std::size_t
    {
        auto lock = std::lock_guard<std::mutex> { mtx_ };
        return messages_.size();
    }

private:
    mutable std::mutex mtx_;
    std::vector<std::string> messages_;
};

} // namespace test_support
