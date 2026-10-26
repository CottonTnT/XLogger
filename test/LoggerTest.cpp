// Logger / LogAppender 集成测试

#include <catch2/catch_test_macros.hpp>

#include "LogAppender.h"
#include "Logger.h"
#include "TestSupport.hpp"

#include <memory>
#include <stdexcept>
#include <string>

using test_support::CaptureAppender;

TEST_CASE("Logger 基本属性", "[xlogger][logger]")
{
    auto logger = std::make_shared<X::Logger>("test-logger");
    CHECK(logger->getLoggerName() == "test-logger");
    CHECK(logger->getLogLevel() == X::LogLevel::DEBUG);

    logger->setLogLevel(X::LogLevel::WARN);
    CHECK(logger->getLogLevel() == X::LogLevel::WARN);
}

TEST_CASE("Logger 空名称构造抛异常", "[xlogger][logger]")
{
    CHECK_THROWS_AS(std::make_shared<X::Logger>(""), std::invalid_argument);
}

TEST_CASE("Logger 格式模式读写", "[xlogger][logger]")
{
    auto logger = std::make_shared<X::Logger>("pattern-logger");
    logger->setFormatPattern("{message}");
    CHECK(logger->getFormatPattern() == "{message}");
}

TEST_CASE("Logger 默认格式模式", "[xlogger][logger]")
{
    auto logger = std::make_shared<X::Logger>("default-pattern-logger");
    CHECK_FALSE(logger->getFormatPattern().empty());
}

TEST_CASE("Logger 添加空 Appender 被忽略", "[xlogger][logger]")
{
    auto logger = std::make_shared<X::Logger>("null-appender-logger");
    logger->addAppender(nullptr);
    logger->info("dropped {}", 1);
    SUCCEED();
}

TEST_CASE("Logger 按指定格式输出", "[xlogger][logger]")
{
    auto logger = std::make_shared<X::Logger>("fmt-logger");
    auto capture = std::make_shared<CaptureAppender>();
    logger->addAppender(capture);
    logger->setFormatPattern("{message}{newline}");

    logger->info("hello {}", 42);
    CHECK(capture->lastMessage() == "hello 42\n");
}

TEST_CASE("Logger 默认格式包含消息与换行", "[xlogger][logger]")
{
    auto logger = std::make_shared<X::Logger>("default-logger");
    auto capture = std::make_shared<CaptureAppender>();
    logger->addAppender(capture);

    logger->error("boom {}", 7);
    CHECK(capture->lastMessage().find("boom 7") != std::string::npos);
    CHECK(capture->lastMessage().back() == '\n');
}

TEST_CASE("LogAppender 工厂返回非空实例", "[xlogger][appender]")
{
    CHECK(X::LogAppender::makeStdoutAppender() != nullptr);
}

TEST_CASE("Logger 级别过滤", "[xlogger][logger]")
{
    auto logger = std::make_shared<X::Logger>("level-filter-logger");
    auto capture = std::make_shared<CaptureAppender>();
    logger->addAppender(capture);
    logger->setLogLevel(X::LogLevel::ERROR);
    logger->setFormatPattern("{message}");

    logger->info("info-message");
    CHECK(capture->count() == 0);

    logger->error("error-message");
    CHECK(capture->lastMessage() == "error-message");
}

TEST_CASE("Logger warn 输出", "[xlogger][logger]")
{
    auto logger = std::make_shared<X::Logger>("warn-logger");
    auto capture = std::make_shared<CaptureAppender>();
    logger->addAppender(capture);
    logger->setFormatPattern("{level}|{message}");

    logger->warn("careful {}", 3);
    CHECK(capture->lastMessage() == "WARN|careful 3");
}

TEST_CASE("Logger root 委托", "[xlogger][logger]")
{
    auto root = std::make_shared<X::Logger>("root");
    auto capture = std::make_shared<CaptureAppender>();
    root->addAppender(capture);
    root->setFormatPattern("{message}");

    auto child = std::make_shared<X::Logger>("child", root);

    child->info("delegated");
    CHECK(capture->lastMessage() == "delegated");
}

TEST_CASE("Logger removeAppender 后不再输出", "[xlogger][logger]")
{
    auto logger = std::make_shared<X::Logger>("remove-logger");
    auto capture = std::make_shared<CaptureAppender>();
    logger->addAppender(capture);
    logger->setFormatPattern("{message}");

    logger->info("before");
    CHECK(capture->count() == 1);

    logger->removeAppender(capture);
    logger->info("after");
    CHECK(capture->count() == 1);
}

TEST_CASE("Logger clearAppender 后不再输出", "[xlogger][logger]")
{
    auto logger = std::make_shared<X::Logger>("clear-logger");
    auto capture = std::make_shared<CaptureAppender>();
    logger->addAppender(capture);
    logger->setFormatPattern("{message}");

    logger->info("before");
    CHECK(capture->count() == 1);

    logger->clearAppender();
    logger->info("after");
    CHECK(capture->count() == 1);
}

TEST_CASE("多个 Appender 收到同一消息", "[xlogger][logger]")
{
    auto logger = std::make_shared<X::Logger>("multi-appender-logger");
    auto first = std::make_shared<CaptureAppender>();
    auto second = std::make_shared<CaptureAppender>();
    logger->addAppender(first);
    logger->addAppender(second);
    logger->setFormatPattern("{message}");

    logger->info("fan-out");
    CHECK(first->lastMessage() == "fan-out");
    CHECK(second->lastMessage() == "fan-out");
}
