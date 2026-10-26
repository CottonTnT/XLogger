#pragma once

#include <InlineBuffer.hpp>
#include <format>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>

#include "LogAppender.h"
#include "LogLevel.h"

namespace X::detail {

template <typename... Args>
struct FmtStrWithSrcLoc {
    std::format_string<Args...> fmt;
    std::source_location loc;

    // 利用隐式转换与默认实参，在传入字符串字面量时顺便捕获调用点位置
    template <typename T>
        requires std::convertible_to<const T&, std::string_view>
    consteval FmtStrWithSrcLoc(const T& s,
        std::source_location loc = std::source_location::current())
        : fmt(s)
        , loc(loc)
    {
    }
};

constexpr int USER_CUSTOME_MESSAGE_INLINE_BUFFER_SIZE = 256;
using UserCustomMessageBuffer = InlineBuffer<USER_CUSTOME_MESSAGE_INLINE_BUFFER_SIZE>;

template <typename... Args>
auto format(std::format_string<Args...> fmt, Args&&... args)
    -> UserCustomMessageBuffer
{
    // 1. 准备栈上固定的内联缓冲区
    auto stkBuffer = std::array<char, 1024> { };

    // 2. 第一次尝试：直接格式化到栈缓冲区中
    // std::format_to_n 会返回格式化所需的总字符数 (size)
    const auto result = std::format_to_n(
        stkBuffer.data(),
        stkBuffer.size(),
        fmt,
        std::forward<Args>(args)...);
    const auto total_size = static_cast<std::size_t>(result.size);

    // 3. Fast Path：栈缓冲区能够完全容纳格式化后的字符串
    auto inlineBuffer = UserCustomMessageBuffer { };
    if (total_size <= stkBuffer.size()) {
        inlineBuffer.append(stkBuffer.data(), total_size);
        return inlineBuffer;
    }

    // 4. Slow Path：栈空间不足发生溢出，在堆上开辟精准大小的缓冲区进行二次格式化
    auto overflow_buffer = std::vector<char>(total_size);
    // std::format_to 保证精准写入 overflow_buffer
    std::format_to(overflow_buffer.data(), fmt, std::forward<Args>(args)...);
    inlineBuffer.append(overflow_buffer.data(), total_size);

    return inlineBuffer;
}

} // namespace X::detail

namespace X {

class Logger;
class LoggerManager;
class LogAppender;

class Logger;
using LoggerSPtr = std::shared_ptr<Logger>;

class Logger final {

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    void logImpl_(LogLevel level, std::string_view message, std::source_location loc);

public:
    explicit Logger(std::string name, LoggerSPtr root = nullptr);
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    [[nodiscard]] auto getLogLevel() const -> LogLevel;
    void setLogLevel(LogLevel level) noexcept;

    [[nodiscard]] auto getLoggerName() const -> std::string_view;

    void setFormatPattern(std::string pattern);
    [[nodiscard]] auto getFormatPattern() const noexcept
        -> std::string_view;

    void addAppender(AppenderPtr appender);
    void removeAppender(AppenderPtr appender);
    void clearAppender();

    template <typename... Args>
    void debug(detail::FmtStrWithSrcLoc<std::type_identity_t<Args>...> fmt, Args&&... args);

    template <typename... Args>
    void info(detail::FmtStrWithSrcLoc<std::type_identity_t<Args>...> fmt, Args&&... args);

    template <typename... Args>
    void error(detail::FmtStrWithSrcLoc<std::type_identity_t<Args>...> fmt, Args&&... args);

    template <typename... Args>
    void warn(detail::FmtStrWithSrcLoc<std::type_identity_t<Args>...> fmt, Args&&... args);
};

template <typename... Args>
void Logger::debug(detail::FmtStrWithSrcLoc<std::type_identity_t<Args>...> fmt,
    Args&&... args)
{
    logImpl_(LogLevel::DEBUG,
        detail::format(fmt.fmt, std::forward<Args>(args)...).view(),
        fmt.loc);
}

template <typename... Args>
void Logger::info(
    detail::FmtStrWithSrcLoc<std::type_identity_t<Args>...> fmt,
    Args&&... args)
{
    logImpl_(
        LogLevel::INFO,
        detail::format(fmt.fmt, std::forward<Args>(args)...).view(),
        fmt.loc);
}

template <typename... Args>
void Logger::error(detail::FmtStrWithSrcLoc<std::type_identity_t<Args>...> fmt,
    Args&&... args)
{
    logImpl_(
        LogLevel::ERROR,
        detail::format(fmt.fmt, std::forward<Args>(args)...).view(),
        fmt.loc);
}

template <typename... Args>
void Logger::warn(detail::FmtStrWithSrcLoc<std::type_identity_t<Args>...> fmt,
    Args&&... args)
{
    logImpl_(
        LogLevel::WARN,
        detail::format(fmt.fmt, std::forward<Args>(args)...).view(),
        fmt.loc);
}

} // namespace X
