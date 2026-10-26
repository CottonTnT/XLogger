// 多线程并发日志输出测试

#include <catch2/catch_test_macros.hpp>

#include "Logger.h"
#include "TestSupport.hpp"

#include <memory>
#include <thread>
#include <vector>

using test_support::CaptureAppender;

TEST_CASE("多线程并发日志", "[xlogger][concurrency]")
{
    auto logger = std::make_shared<X::Logger>("concurrency-logger");
    auto capture = std::make_shared<CaptureAppender>();
    logger->addAppender(capture);
    logger->setFormatPattern("{message}");

    constexpr int THREADS = 4;
    constexpr int MESSAGES_PER_THREAD = 500;

    std::vector<std::thread> threads;
    threads.reserve(THREADS);
    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([logger, t] {
            for (int i = 0; i < MESSAGES_PER_THREAD; ++i) {
                logger->info("t{}:{}", t, i);
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    CHECK(capture->count() == static_cast<std::size_t>(THREADS * MESSAGES_PER_THREAD));
}
