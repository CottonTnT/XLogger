#include "LogFormatter.h"
#include "FormatItemImpl.hpp"
#include "LogRecordView.h"

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace {

enum class ParseState {
    NORMAL, // 普通字面量
    FIELD,  // 正在解析 {name:spec}
};

} // namespace

namespace X::detail {

LogFormatter::LogFormatter(std::string pattern)
    : pattern_ { std::move(pattern) }
{
    parsePattern_();
}

LogFormatter::~LogFormatter() = default;

auto LogFormatter::format(const LogRecordView& event) const
    -> FormattedRecordBuffer
{
    auto buf = FormattedRecordBuffer { };

    for (const auto& item : formatItemList_) {
        item->format(event, buf);
    }
    return buf;
}

auto LogFormatter::parsePattern_()
    -> int
{
    auto normal_str = std::string { }; // 累积普通字面量
    auto field = std::string { };      // 累积字段名与 spec

    auto flushLiteral = [&] {
        if (not normal_str.empty()) {
            formatItemList_.push_back(createLiteralFormatItem(normal_str));
            normal_str.clear();
        }
    };

    auto state = ParseState::NORMAL;

    for (auto i = size_t { 0 }; i < pattern_.size();) {
        const auto c = pattern_[i];

        switch (state) {
        case ParseState::NORMAL: {
            if (c == '{') {
                // 转义的花括号：{{ -> 字面 '{'
                if (i + 1 < pattern_.size() and pattern_[i + 1] == '{') {
                    normal_str.push_back('{');
                    i += 2;
                    break;
                }
                // 进入字段解析前，先落库累积的字面量
                flushLiteral();
                field.clear();
                state = ParseState::FIELD;
                ++i; // 跳过 '{'
                break;
            }
            if (c == '}') {
                // 转义的花括号：}} -> 字面 '}'
                if (i + 1 < pattern_.size() and pattern_[i + 1] == '}') {
                    normal_str.push_back('}');
                    i += 2;
                    break;
                }
                throw std::invalid_argument("XLogger: format pattern has an unmatched '}'");
            }
            normal_str.push_back(c);
            ++i;
            break;
        }
        case ParseState::FIELD: {
            if (c == '}') {
                ++i; // 跳过 '}'
                const auto colon = field.find(':');
                const auto name = colon == std::string::npos ? field : field.substr(0, colon);
                const auto spec = colon == std::string::npos ? std::string { } : field.substr(colon + 1);
                if (name.empty()) {
                    throw std::invalid_argument("XLogger: format pattern has an empty field name");
                }
                formatItemList_.push_back(createKeyFormatItem(name, spec));
                field.clear();
                state = ParseState::NORMAL;
                break;
            }
            field.push_back(c);
            ++i;
            break;
        }
        }
    }

    // 循环结束仍处于 FIELD 状态，说明遇到了未闭合的 '{'
    if (state == ParseState::FIELD) {
        throw std::invalid_argument("XLogger: format pattern has an unterminated field (missing '}')");
    }

    flushLiteral();
    return 0;
}

} // namespace X::detail
