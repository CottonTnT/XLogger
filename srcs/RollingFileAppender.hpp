#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <functional>
#include <iostream>
#include <latch>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <FixedBuffer.hpp>
#include <SystemUtil.hpp>
#include <XType.hpp>
#include <unistd.h>

#include "RollingLogFile.hpp"

namespace X::detail {

/* ======================== RollingFileAppender ======================== */

/**
 * @brief 异步滚动文件 appender（muduo AsyncLogging + LogFile 风格）。
 * 前台 append 只把字节 memcpy 进双缓冲，后台线程负责写盘、按大小/按天滚动与周期 fflush。
 */
class RollingFileAppender {

private:
    static constexpr std::size_t BUFFER_SIZE = 4_mb; // 4MB，muduo 默认量级
    static constexpr std::size_t MAX_BUFFER_NUMBER = 25;

    using FixBuf = FixedBuffer<BUFFER_SIZE>;
    using BufPtr = std::unique_ptr<FixBuf>;

    // 配置
    std::filesystem::path logDirPath_;
    // 生产者/异步状态（mutex_ 保护）
    std::mutex mutex_;
    std::condition_variable cond_;
    BufPtr currentBuffer_;
    BufPtr nextBuffer_;
    std::vector<BufPtr> buffers_;
    // 必须声明在缓冲区之后：析构时先 join 后台线程，再销毁缓冲区，避免 use-after-free
    std::jthread backendWorker_;

public:
    explicit RollingFileAppender(std::filesystem::path logDirPath,
        std::size_t rollSize,
        std::chrono::seconds flushInterval,
        std::size_t checkEveryN);
    ~RollingFileAppender();

    RollingFileAppender(const RollingFileAppender&) = delete;
    auto operator=(const RollingFileAppender&) -> RollingFileAppender& = delete;
    RollingFileAppender(RollingFileAppender&&) = delete;
    auto operator=(RollingFileAppender&&) -> RollingFileAppender& = delete;

    void append(std::string_view msg);

private:
    auto makeLogFileName_()
        -> std::filesystem::path;

    void backendWorkerThread_(std::stop_token stopToken, std::latch& started, std::size_t rollSize, std::chrono::seconds flushInterval, std::size_t checkEveryN);
    void rollFileOrNot_();

    static auto isMessageOversized_(std::string_view msg) -> bool;

    static auto createNewBuffer_() -> BufPtr;
};

inline RollingFileAppender::RollingFileAppender(std::filesystem::path logDirPath,
    std::size_t rollSize,
    std::chrono::seconds flushInterval,
    std::size_t checkEveryN)
    : logDirPath_ { std::move(logDirPath) }
    , currentBuffer_ { createNewBuffer_() }
    , nextBuffer_ { createNewBuffer_() }
{
    if (logDirPath_.empty()) {
        throw std::invalid_argument("XLogger: log file base name cannot be empty");
    }
    if (!std::filesystem::is_directory(logDirPath_)) {
        throw std::invalid_argument("XLogger: log directory does not exist");
    }

    buffers_.reserve(16);

    auto started = std::latch { 1 };
    // 用 lambda 包装成员函数：std::jthread 只能给「首参为 std::stop_token」的可调用对象注入 stop_token，
    // 而成员函数指针的首参被对象指针（this）占用，无法直接注入。
    backendWorker_ = std::jthread(
        [this](std::stop_token stopToken, std::latch& started, std::size_t rollSize, std::chrono::seconds flushInterval, std::size_t checkEveryN) {
            backendWorkerThread_(stopToken, started, rollSize, flushInterval, checkEveryN);
        },
        std::ref(started), rollSize, flushInterval, checkEveryN);
    // 等后台线程完成初始文件打开后再返回，保证构造后文件已就绪
    started.wait();
}

inline RollingFileAppender::~RollingFileAppender()
{
    // 请求后台线程停止并唤醒它（否则它会一直睡到 flushInterval 超时才退出）
    backendWorker_.request_stop();
    cond_.notify_all();
    // backendWorker_ 声明在成员列表末尾，其析构会 join 后台线程，
    // 保证在 currentBuffer_/nextBuffer_/buffers_ 销毁之前线程已退出。
}

inline void RollingFileAppender::append(std::string_view msg)
{
    if (isMessageOversized_(msg)) {
        return;
    }

    auto _ = std::scoped_lock<std::mutex>(mutex_);

    if (msg.size() > currentBuffer_->avail()) {
        buffers_.push_back(std::move(currentBuffer_));
        if (nextBuffer_) {
            currentBuffer_ = std::move(nextBuffer_);
        } else {
            currentBuffer_ = createNewBuffer_();
        }
        cond_.notify_one();
    }
    std::ignore = currentBuffer_->append(msg.data(), msg.size());
}

inline void RollingFileAppender::backendWorkerThread_(std::stop_token stopToken, std::latch& started, std::size_t rollSize, std::chrono::seconds flushInterval, std::size_t checkEveryN)
{
    // 文件状态（仅后台线程访问）
    auto file = X::detail::RollLogFileStream { logDirPath_, rollSize, flushInterval, checkEveryN };
    started.count_down(); // 初始文件已打开，通知构造完成

    auto newBuffer1 = createNewBuffer_();
    auto newBuffer2 = createNewBuffer_();

    auto buffersToWrite = std::vector<BufPtr> { };
    buffersToWrite.reserve(16);

    while (!stopToken.stop_requested()) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            // 空闲时休眠，等待 append 通知（缓冲已满）或 flush 周期；
            // 收到停止请求时立即唤醒，做最后一次排空。
            cond_.wait_for(lock, flushInterval, [&] {
                return stopToken.stop_requested() || !buffers_.empty();
            });

            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_ = std::move(newBuffer1);
            buffersToWrite.swap(buffers_);
            if (!nextBuffer_) {
                nextBuffer_ = std::move(newBuffer2);
            }
        }

        // 消费者跟不上时丢弃最旧的缓冲，保留前 2 个，避免内存失控
        // todo:1. 加上告警机制 2.工业环境应该都支持动态内存扩容 3. 也可以考虑使用环形缓冲区，避免频繁 new/delete 4. 也可以考虑使用内存池，避免频繁 new/delete
        if (buffersToWrite.size() > MAX_BUFFER_NUMBER) {
            const auto dropped = buffersToWrite.size() - 2;
            buffersToWrite.erase(buffersToWrite.begin() + 2, buffersToWrite.end());
            std::cerr << "XLogger RollingFileAppender: dropped " << dropped << " buffers (log messages lost)\n";
        }

        for (const auto& buf : buffersToWrite) {
            file.append(buf->data(), buf->length());
        }

        if (buffersToWrite.size() > 2) {
            // drop non-bzero-ed buffers, avoid trashing
            buffersToWrite.resize(2);
        }

        // 回收缓冲，保留 2 个备用
        if (newBuffer1 == nullptr) {
            newBuffer1 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            (*newBuffer1).reset();
        }
        if (newBuffer2 == nullptr) {
            newBuffer2 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            (*newBuffer2).reset();
        }
        buffersToWrite.clear();
        file.flush();
    }

    // 退出前最后排空一次，确保已 append 的日志完整落盘
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (currentBuffer_) {
            buffers_.push_back(std::move(currentBuffer_));
        }
        buffersToWrite.swap(buffers_);
    }
    for (const auto& buf : buffersToWrite) {
        file.append(buf->data(), buf->length());
    }
    file.flush();
}

inline auto RollingFileAppender::isMessageOversized_(std::string_view msg)
    -> bool
{
    if (msg.size() > BUFFER_SIZE) {
        std::cerr << "XLogger RollingFileAppender: message size " << msg.size()
                  << " exceeds buffer size " << BUFFER_SIZE << ", dropped\n";
        return true;
    }
    return false;
}

inline auto RollingFileAppender::createNewBuffer_() -> BufPtr
{
    return std::make_unique<FixBuf>();
}

} // namespace X::detail
