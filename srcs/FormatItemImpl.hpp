#include "FormatItem.h"
#include "FormatItemProxy.hpp"
#include "LogFormatter.h"
#include "LogLevel.h"
#include "LogRecordView.h"

#include <chrono>
#include <cstddef>
#include <ctime>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <unordered_map>
#include <utility>

namespace X::detail {

/* ======================== FormatterItem ======================== */

class LiteralFormatItem {
public:
    explicit LiteralFormatItem(std::string str)
        : str_ { std::move(str) }
    {
    }

    void format(const LogRecordView&, FormattedRecordBuffer& buf) const
    {
        buf.append(str_);
    }

private:
    std::string str_;
};

/**
 * @brief 消息format
 */
class CustomMessageFormatItem {
public:
    void format(const LogRecordView& event, FormattedRecordBuffer& buf) const
    {
        buf.append(event.customeMsg);
    }
};

// 日志级别format
class LogLevelFormatItem {
public:
    void format(const LogRecordView& event, FormattedRecordBuffer& buf) const
    {
        buf.append(logLevel2String(event.logLevel));
    }
};

/**
 * @brief 执行时间format
 */
class ElapseFormatItem {
public:
    void format(const LogRecordView& event, FormattedRecordBuffer& buf) const
    {
        auto count = std::chrono::duration_cast<std::chrono::milliseconds>(event.elapsed);
        buf.appendInteger(count.count());
    }
};

/**
 * @brief 日志器名称format
 */
class LoggerNameFormatItem {
public:
    void format(const LogRecordView& event, FormattedRecordBuffer& buf) const
    {
        buf.append(event.loggerName);
    }
};

/**
 * @brief 线程ID format
 */
class ThreadIdFormatItem {
public:
    void format(const LogRecordView& event, FormattedRecordBuffer& buf) const
    {
        buf.appendInteger(event.threadId);
    }
};

/**
 * @brief 协程ID format（暂未接入协程id，先输出占位值）
 */
class FiberIdFormatItem {
public:
    void format(const LogRecordView&, FormattedRecordBuffer& buf) const
    {
        buf.append("0");
    }
};

/**
 * @brief 线程名称format
 */
class ThreadNameFormatItem {
public:
    void format(const LogRecordView& event, FormattedRecordBuffer& buf) const
    {
        buf.append(event.threadName);
    }
};

class NewLineFormatItem {
public:
    void format(const LogRecordView&, FormattedRecordBuffer& buf) const
    {
        buf.append("\n");
    }
};

/**
 * @brief 文件名format
 */
class FilenameFormatItem {
public:
    void format(const LogRecordView& event, FormattedRecordBuffer& buf) const
    {
        buf.append(event.filename);
    }
};

/**
 * @brief 行号format
 */
class LineFormatItem {
public:
    void format(const LogRecordView& event, FormattedRecordBuffer& buf) const
    {
        buf.appendInteger(event.lineNumber);
    }
};

/**
 * @brief tab format
 */
class TabFormatItem {
public:
    void format(const LogRecordView&, FormattedRecordBuffer& buf) const
    {
        buf.append("\t");
    }
};

/* ======================== FormatItem with Status ======================== */
/**
 * @brief 时间format
 */
class DateTimeFormatItem {

private:
    static constexpr std::string_view DEFAULT_DATE_FORMAT = "%Y-%m-%d %H:%M:%S";

    std::string dateFormat_;

    // 每个实例独立的缓存，避免不同 dateFormat_ 的 logger 互相覆盖缓存导致时间串扰
    mutable time_t lastSec_ = 0;
    mutable char cachedDatetime_[32] = { '\0' };

public:
    explicit DateTimeFormatItem(std::string date_format)
        : dateFormat_ { !date_format.empty() ? std::move(date_format) : std::string { DEFAULT_DATE_FORMAT } }
    {
    }

    // todo:exception
    void format(const LogRecordView& event, FormattedRecordBuffer& buf) const
    {
        const auto duration = event.timestamp.time_since_epoch();
        const time_t curSec = static_cast<time_t>(std::chrono::duration_cast<std::chrono::seconds>(duration).count());
        if (curSec != lastSec_) {
            std::tm buffer { };
#if defined(_WIN32)
            const errno_t result = localtime_s(&buffer, &curSec);
            (void)result;
#else
            /**
             * @brief localtime_r是线程安全的，但是消耗也大：
             * 1. 全局有锁可能排队
             * 2. 需要进行复杂的时区计算(尤其是有过夏令时变更历史的时区)
             * 3. 如果系统没有加载时区信息或者要求响应时区变更, 每次调用这个接口还要去发起磁盘IO，读系统文件/etc/localtime
             */
            (void)localtime_r(&curSec, &buffer);
#endif
            (void)std::strftime(cachedDatetime_, sizeof(cachedDatetime_), dateFormat_.c_str(), &buffer);
            lastSec_ = curSec;
        }

        buf.append(cachedDatetime_);
    }
};

using FormatItemFactoryFunc = std::unique_ptr<const FormatItem> (*)(std::string);

template <typename ItemImpl>
static auto createSimpleFormatItem(std::string /*unused*/)
    -> std::unique_ptr<const FormatItem>
{
    return std::make_unique<FormatItemProxy<ItemImpl>>();
}

template <typename ItemImpl>
static auto createStatusFormatItem(std::string conf)
    -> std::unique_ptr<const FormatItem>
{
    return std::make_unique<FormatItemProxy<ItemImpl>>(conf);
}

static auto getFormatItemFactoryMap()
    -> const std::unordered_map<std::string, FormatItemFactoryFunc>&
{
    static auto FACTORY_MAP = std::unordered_map<std::string, FormatItemFactoryFunc> {
        { "message", &createSimpleFormatItem<CustomMessageFormatItem> },
        { "level", &createSimpleFormatItem<LogLevelFormatItem> },
        { "logger", &createSimpleFormatItem<LoggerNameFormatItem> },
        { "elapsed", &createSimpleFormatItem<ElapseFormatItem> },
        { "file", &createSimpleFormatItem<FilenameFormatItem> },
        { "line", &createSimpleFormatItem<LineFormatItem> },
        { "thread_id", &createSimpleFormatItem<ThreadIdFormatItem> },
        { "fiber_id", &createSimpleFormatItem<FiberIdFormatItem> },
        { "thread_name", &createSimpleFormatItem<ThreadNameFormatItem> },
        { "tab", &createSimpleFormatItem<TabFormatItem> },
        { "newline", &createSimpleFormatItem<NewLineFormatItem> },
        { "date", &createStatusFormatItem<DateTimeFormatItem> }
    };
    return FACTORY_MAP;
}

auto createLiteralFormatItem(std::string literal)
    -> std::unique_ptr<const FormatItem>
{
    return createStatusFormatItem<LiteralFormatItem>(std::move(literal));
}

auto createKeyFormatItem(std::string_view name, std::string conf = "")
    -> std::unique_ptr<const FormatItem>
{
    const auto& factories = getFormatItemFactoryMap();
    const auto it = factories.find(std::string { name });
    if (it == factories.end()) {
        throw std::invalid_argument("XLogger: unknown format field '{" + std::string { name } + "}'");
    }
    return it->second(conf);
}

} // namespace X::detail
