#include "LoggerFactory.h"
#include "Logger.h"

#include <Singleton.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace X::detail {

class LoggerFactoryImpl final : public SingletonWithConstructToken<LoggerFactoryImpl> {

private:
    static constexpr std::string_view ROOT_LOGGER_NAME = "ROOT";

    mutable std::mutex mtx_;
    std::unordered_map<std::string, LoggerSPtr> loggerMap_;
    LoggerSPtr rootLogger_;

public:
    explicit LoggerFactoryImpl(ConstructToken /*unused*/)
        : rootLogger_(std::make_shared<Logger>(std::string { ROOT_LOGGER_NAME }))
    {
        rootLogger_->addAppender(X::LogAppender::makeStdoutAppender());
    }

    [[nodiscard]] auto getLogger(std::string name)
        -> LoggerSPtr
    {
        // ASSERT_RETVAL2(!name.empty(), nullptr, "logger name cannot be empty");

        std::lock_guard<std::mutex> lock(mtx_);
        auto iterator = loggerMap_.find(std::string(name));
        if (iterator != loggerMap_.end()) {
            return iterator->second;
        }

        auto logger = std::make_shared<Logger>(std::move(name), rootLogger_);
        loggerMap_.emplace(logger->getLoggerName(), logger);
        return logger;
    }
};

} // namespace X::detail

namespace X {
auto LoggerFactory::getLogger(std::string name)
    -> LoggerSPtr
{
    return detail::LoggerFactoryImpl::getInstance()->getLogger(std::move(name));
}
} // namespace X