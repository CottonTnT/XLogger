#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace X {

class LogRecordView;

class LogAppender;
using AppenderPtr = std::shared_ptr<LogAppender>;

class LogAppender {
protected:
    LogAppender() = default;

public:
    LogAppender(const LogAppender&) = delete;
    auto operator=(const LogAppender&) -> LogAppender& = delete;
    LogAppender(LogAppender&&) = delete;
    auto operator=(LogAppender&&) -> LogAppender& = delete;
    virtual ~LogAppender() = default;

    virtual void append(std::string_view message) = 0;

    [[nodiscard]] static auto makeStdoutAppender()
        -> AppenderPtr;

    [[nodiscard]] static auto makeRollingFileAppender(
        std::string baseName,
        std::size_t rollSize = 512 * 1024 * 1024, // 按大小滚动阈值（字节）
        int flushInterval = 3) // 秒；空闲时周期 flush 间隔
        -> AppenderPtr;
};

}
