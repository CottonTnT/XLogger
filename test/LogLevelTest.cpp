// LogLevel 转换与比较运算符测试

#include <catch2/catch_test_macros.hpp>

#include "LogLevel.h"

TEST_CASE("logLevel2String 返回对应级别字符串", "[xlogger][loglevel]")
{
    CHECK(X::logLevel2String(X::LogLevel::DEBUG) == "DEBUG");
    CHECK(X::logLevel2String(X::LogLevel::INFO) == "INFO");
    CHECK(X::logLevel2String(X::LogLevel::WARN) == "WARN");
    CHECK(X::logLevel2String(X::LogLevel::ERROR) == "ERROR");
}

TEST_CASE("logLevel2String 对非法级别返回 UNKNOWN", "[xlogger][loglevel]")
{
    CHECK(X::logLevel2String(static_cast<X::LogLevel>(99)) == "UNKNOWN");
}

TEST_CASE("string2LogLevel 大小写均可识别", "[xlogger][loglevel]")
{
    CHECK(X::string2LogLevel("debug") == X::LogLevel::DEBUG);
    CHECK(X::string2LogLevel("INFO") == X::LogLevel::INFO);
    CHECK(X::string2LogLevel("warn") == X::LogLevel::WARN);
    CHECK(X::string2LogLevel("ERROR") == X::LogLevel::ERROR);
}

TEST_CASE("string2LogLevel 对未知字符串返回 DEBUG", "[xlogger][loglevel]")
{
    CHECK(X::string2LogLevel("not-a-level") == X::LogLevel::DEBUG);
}

TEST_CASE("LogLevel 比较运算符", "[xlogger][loglevel]")
{
    CHECK(X::LogLevel::DEBUG < X::LogLevel::INFO);
    CHECK(X::LogLevel::INFO <= X::LogLevel::WARN);
    CHECK(X::LogLevel::WARN < X::LogLevel::ERROR);

    CHECK(X::LogLevel::ERROR > X::LogLevel::WARN);
    CHECK(X::LogLevel::WARN >= X::LogLevel::INFO);

    CHECK(X::LogLevel::INFO == X::LogLevel::INFO);
    CHECK_FALSE(X::LogLevel::INFO == X::LogLevel::WARN);
}

TEST_CASE("LogLevel 比较运算符边界", "[xlogger][loglevel]")
{
    CHECK(X::LogLevel::DEBUG < X::LogLevel::ERROR);
    CHECK(X::LogLevel::ERROR >= X::LogLevel::DEBUG);
    CHECK_FALSE(X::LogLevel::ERROR < X::LogLevel::WARN);
    CHECK_FALSE(X::LogLevel::WARN > X::LogLevel::ERROR);
}
