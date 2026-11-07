#pragma once

#include <cassert>
#include <chrono>
#include <cstddef>
#include <filesystem>

#include <FsUtil.hpp>
#include <SystemUtil.hpp>

static constexpr auto isSameDay(
    std::chrono::system_clock::time_point lhs,
    std::chrono::system_clock::time_point rhs) -> bool
{
    using namespace std::chrono;
    return floor<days>(lhs) == floor<days>(rhs);
}

namespace X::detail {

class RollLogFileStream {
private:
    std::filesystem::path logDirPath_;
    std::unique_ptr<FsUtil::AppendFileStream<>> fstreamPtr_;
    std::size_t rollSize_;
    std::chrono::seconds flushInterval_;
    std::size_t checkEveryN_;
    std::size_t appendTimes_;
    std::chrono::system_clock::time_point lastRollTime_;
    std::chrono::system_clock::time_point lastFlushTime_;

    auto makeLogFilePath_(std::chrono::system_clock::time_point now) -> std::filesystem::path;
    auto rollFile_() -> void;

public:
    explicit RollLogFileStream(std::filesystem::path logFilePath,
        std::size_t rollSize,
        std::chrono::seconds flushInterval,
        std::size_t checkEvenyN);
    ~RollLogFileStream();

    RollLogFileStream(const RollLogFileStream&) = delete;
    auto operator=(const RollLogFileStream&) -> RollLogFileStream& = delete;
    RollLogFileStream(RollLogFileStream&&) = delete;
    auto operator=(RollLogFileStream&&) -> RollLogFileStream& = delete;

    void append(const char* msg, std::size_t len);
    void flush();
};

inline RollLogFileStream::RollLogFileStream(std::filesystem::path logFilePath,
    std::size_t rollSize,
    std::chrono::seconds flushInterval,
    std::size_t checkEvenyN)
    : logDirPath_ { std::move(logFilePath) }
    , rollSize_ { rollSize }
    , flushInterval_ { flushInterval }
    , checkEveryN_ { checkEvenyN }
    , appendTimes_ { 0 }

{
    rollFile_();
}

inline RollLogFileStream::~RollLogFileStream()
{
    if (fstreamPtr_) {
        flush();
    }
}

inline void RollLogFileStream::append(const char* msg, std::size_t len)
{
    fstreamPtr_->append(msg, len);
    if (fstreamPtr_->writtenBytes() >= rollSize_) {
        rollFile_();
        return;
    }

    if (++appendTimes_ < checkEveryN_) {
        return;
    }

    appendTimes_ = 0;
    auto now = std::chrono::system_clock::now();
    if (!isSameDay(now, lastRollTime_)) {
        rollFile_();
        return;
    }
    if (now - lastFlushTime_ >= flushInterval_) {
        flush();
        lastFlushTime_ = now;
    }
}

inline void RollLogFileStream::flush()
{
    fstreamPtr_->flush();
}

auto RollLogFileStream::makeLogFilePath_(std::chrono::system_clock::time_point now)
    -> std::filesystem::path
{
    assert(std::filesystem::is_directory(logDirPath_));

    char timebuf[32] = { '\0' };

    std::ignore = std::format_to(timebuf, "{:%Y-%m-%d-%H:%M:%S}", now);

    auto logFilePath = logDirPath_;
    auto logFilename = std::string { "logfile." };

    // todo: add process name
    logFilename += X::SystemUtil::getCurrentHostname();
    logFilename += timebuf;
    logFilename += ".log";
    return logFilePath.append(logFilename);
}

inline void RollLogFileStream::rollFile_()
{

    auto now = std::chrono::system_clock::now();
    if (now > lastRollTime_) {
        fstreamPtr_ = std::make_unique<FsUtil::AppendFileStream<>>(makeLogFilePath_(now));
        lastRollTime_ = now;
        lastFlushTime_ = now;
    }
}

} // namespace X::detail