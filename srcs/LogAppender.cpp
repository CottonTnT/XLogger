#include "LogAppender.h"
#include "LogAppenderImpl.hpp"
#include "LogAppenderProxy.hpp"
#include "RollingFileAppender.hpp"
namespace X {

auto LogAppender::makeStdoutAppender()
    -> AppenderPtr
{
    return std::make_shared<detail::AppenderProxy<detail::StdoutAppender>>();
}

auto LogAppender::makeRollingFileAppender(std::string logDirPath,
    std::size_t rollSize, int flushInterval)
    -> AppenderPtr
{
    return std::make_shared<detail::AppenderProxy<detail::RollingFileAppender>>(
        std::move(logDirPath), rollSize, std::chrono::seconds(flushInterval), 3);
}
} // namespace X
