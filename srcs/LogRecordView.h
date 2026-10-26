#pragma once
#include "LogLevel.h"

#include <XType.hpp>

#include <chrono>
#include <string_view>

/**
 * @brief 日志事件，用于记录日志现场，比如该日志的级别，文件名/行号，日志消息，线程/协程号，所属日志器名称等。
 */

namespace X::detail {

struct LogRecordView {

    std::string_view loggerName;
    LogLevel logLevel;
    std::chrono::system_clock::duration elapsed;
    std::chrono::system_clock::time_point timestamp;
    u64 threadId;
    std::string_view threadName;
    std::string_view filename;
    std::string_view customeMsg;
    u32 lineNumber;
};

}