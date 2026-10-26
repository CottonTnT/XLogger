#include "LogAppender.h"
#include "Logger.h"
#include "LogLevel.h"

#include "order_service.h"

#include <memory>

int main()
{
    // 方式一：显式构造一个 logger，并自定义格式与输出目标。
    auto logger = std::make_shared<X::Logger>("main");
    logger->setLogLevel(X::LogLevel::INFO);
    logger->setFormatPattern("{date:%H:%M:%S} [{level}] [{logger}] {message}{newline}");
    logger->addAppender(X::LogAppender::makeStdoutAppender());

    logger->info("hello xlogger, answer={}", 42);
    logger->debug("这条 DEBUG 因级别过滤被丢弃");
    logger->warn("warning: value={}", 7);
    logger->error("something went wrong: code={}", -1);

    // 方式二：业务类内部使用 LoggerFactory 的文件作用域 logger（见 order_service.cpp）。
    OrderService service;
    service.placeOrder(1001, "Alice");
    service.cancelOrder(1001);

    return 0;
}
