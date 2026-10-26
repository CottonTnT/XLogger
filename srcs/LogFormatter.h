#pragma once
#include <InlineBuffer.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace X::detail {

class LogRecordView;
class FormatItem;

constexpr std::size_t MAX_FORMATTED_RECORD_INLINE_CAPACITY = 256;

using FormattedRecordBuffer = InlineBuffer<MAX_FORMATTED_RECORD_INLINE_CAPACITY>;

class LogFormatter;
using LogFormatterPtr = std::shared_ptr<const LogFormatter>;

/**
 * @details 格式字段说明（语义化 `{name}` / `{name:spec}` 语法，类似 std::format）:
 * - {message} 消息
 * - {level} 日志级别
 * - {logger} 日志器名称
 * - {date:...} 日期时间，冒号后跟 strftime 格式，如 {date:%Y-%m-%d %H:%M:%S}；省略时用默认格式
 * - {elapsed} 该日志器创建后的累计运行毫秒数
 * - {file} 文件名
 * - {line} 行号
 * - {thread_id} 线程id
 * - {fiber_id} 协程id
 * - {thread_name} 线程名称
 * - {newline} 换行
 * - {tab} 制表符
 * - {{ 与 }} 表示字面量花括号
 *
 * 默认格式：{date:%Y-%m-%d %H:%M:%S} [{elapsed}ms] {thread_id}{tab}{thread_name}{tab}{fiber_id}{tab}[{level}]{tab}[{logger}]{tab}[{file}:{line}]{tab}{tab}{message}{newline}
 *
 * 默认格式描述：年-月-日 时:分:秒 [累计运行毫秒数] \t 线程id \t 线程名称 \t 协程id \t [日志级别] \t [日志器名称] \t 文件名:行号 \t 日志消息 换行符
 */

class LogFormatter {
private:
    std::string pattern_;
    std::vector<std::unique_ptr<const FormatItem>> formatItemList_;

    auto parsePattern_()
        -> int;

public:
    explicit LogFormatter(std::string pattern);
    ~LogFormatter();

    [[nodiscard]] auto format(const LogRecordView& event) const
        -> FormattedRecordBuffer;

    [[nodiscard]] auto getPattern() const noexcept
        -> std::string_view
    {
        return pattern_;
    }

}; // class FormatPattern

} // namespace X::detail
