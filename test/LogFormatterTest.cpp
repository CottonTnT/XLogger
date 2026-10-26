// LogFormatter 格式字段与解析健壮性测试（通过 Logger + CaptureAppender 端到端验证）

#include <catch2/catch_test_macros.hpp>

#include "Logger.h"
#include "TestSupport.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>

using test_support::CaptureAppender;

namespace {

struct Fixture {
    std::shared_ptr<X::Logger> logger;
    std::shared_ptr<CaptureAppender> capture;

    explicit Fixture(std::string pattern)
    {
        logger = std::make_shared<X::Logger>("formatter-logger");
        capture = std::make_shared<CaptureAppender>();
        logger->addAppender(capture);
        logger->setFormatPattern(std::move(pattern));
    }
};

} // namespace

TEST_CASE("格式字段 {message} {newline}", "[xlogger][formatter]")
{
    Fixture fx { "{message}{newline}" };
    fx.logger->info("msg");
    CHECK(fx.capture->lastMessage() == "msg\n");
}

TEST_CASE("格式字段 {level} {logger}", "[xlogger][formatter]")
{
    Fixture fx { "[{level}] [{logger}] {message}" };
    fx.logger->error("x");
    CHECK(fx.capture->lastMessage() == "[ERROR] [formatter-logger] x");
}

TEST_CASE("格式字段 {tab} 与字面量百分号", "[xlogger][formatter]")
{
    Fixture fx { "a{tab}b%c" };
    fx.logger->info("ignored");
    CHECK(fx.capture->lastMessage() == "a\tb%c");
}

TEST_CASE("花括号转义 {{ 与 }}", "[xlogger][formatter]")
{
    Fixture fx { "{{message}} {message}" };
    fx.logger->info("hello");
    CHECK(fx.capture->lastMessage() == "{message} hello");
}

TEST_CASE("格式字段 {date:...}", "[xlogger][formatter]")
{
    Fixture fx { "{date:%Y-%m-%d}" };
    fx.logger->info("ignored");
    auto msg = fx.capture->lastMessage();
    REQUIRE(msg.size() == 10);
    CHECK(msg[4] == '-');
    CHECK(msg[7] == '-');
}

TEST_CASE("格式字段 {date} 无 spec 使用默认格式", "[xlogger][formatter]")
{
    Fixture fx { "{date}" };
    fx.logger->info("ignored");
    auto msg = fx.capture->lastMessage();
    REQUIRE(msg.size() == 19);
    CHECK(msg[4] == '-');
    CHECK(msg[7] == '-');
    CHECK(msg[10] == ' ');
    CHECK(msg[13] == ':');
    CHECK(msg[16] == ':');
}

TEST_CASE("格式字段 {file} 与 {line}", "[xlogger][formatter]")
{
    Fixture fx { "{file}:{line}" };
    fx.logger->info("ignored");
    auto msg = fx.capture->lastMessage();
    CHECK(msg.find("LogFormatterTest.cpp") != std::string::npos);
    auto sep = msg.rfind(':');
    REQUIRE(sep != std::string::npos);
    auto digits = msg.substr(sep + 1);
    CHECK_FALSE(digits.empty());
    CHECK(std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; }));
}

TEST_CASE("格式字段 {thread_id} 与 {fiber_id} 默认占位", "[xlogger][formatter]")
{
    Fixture fx { "{thread_id}|{fiber_id}" };
    fx.logger->info("ignored");
    CHECK(fx.capture->lastMessage() == "0|0");
}

TEST_CASE("未知字段名抛异常", "[xlogger][formatter]")
{
    auto logger = std::make_shared<X::Logger>("unknown-field-logger");
    CHECK_THROWS_AS(logger->setFormatPattern("{foo}"), std::invalid_argument);
}

TEST_CASE("超大消息走堆慢路径", "[xlogger][formatter]")
{
    Fixture fx { "{message}" };
    std::string big(2048, 'x');
    fx.logger->info("{}", big);
    CHECK(fx.capture->lastMessage() == big);
}

TEST_CASE("畸形格式：未闭合 { 抛异常", "[xlogger][formatter]")
{
    auto logger = std::make_shared<X::Logger>("bad-pattern-logger");
    CHECK_THROWS_AS(logger->setFormatPattern("{date:unclosed"), std::invalid_argument);
}

TEST_CASE("畸形格式：孤立 } 抛异常", "[xlogger][formatter]")
{
    auto logger = std::make_shared<X::Logger>("bad-pattern-logger-2");
    CHECK_THROWS_AS(logger->setFormatPattern("abc}"), std::invalid_argument);
}

TEST_CASE("畸形格式：空字段名 {} 抛异常", "[xlogger][formatter]")
{
    auto logger = std::make_shared<X::Logger>("bad-pattern-logger-3");
    CHECK_THROWS_AS(logger->setFormatPattern("{}"), std::invalid_argument);
}
