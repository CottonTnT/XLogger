// RollingFileAppender 单元测试
//
// RollingFileAppender 是异步滚动文件 appender：前台 append 只把字节写入内存双缓冲，
// 后台线程负责落盘、按大小滚动与周期 flush。因此断言文件内容前需要轮询等待后台线程
// 完成落盘 —— `LogAppender` 公共接口只暴露 `append`，没有同步的 `flush`。

#include <catch2/catch_test_macros.hpp>

#include "LogAppender.h"
#include "Logger.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

// 工厂默认的按大小滚动阈值（字节）；测试里显式传入，以便搭配较短的 flushInterval
constexpr std::size_t kDefaultRollSize = 512ull * 1024ull * 1024ull;

// 唯一临时目录，析构时自动清理（即使断言失败也会清理）
class TempDir {
public:
    TempDir()
        : path_ { makeUniqueDir_() }
    {
    }

    ~TempDir()
    {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }

    TempDir(const TempDir&) = delete;
    auto operator=(const TempDir&) -> TempDir& = delete;

    [[nodiscard]] auto path() const -> const fs::path& { return path_; }

private:
    static auto makeUniqueDir_() -> fs::path
    {
        static int counter = 0;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto dir = fs::temp_directory_path()
            / ("xlogger_rolling_" + std::to_string(stamp) + "_" + std::to_string(++counter));
        fs::create_directories(dir);
        return dir;
    }

    fs::path path_;
};

// 列出目录中形如 logfile.*.log 的日志文件
auto listLogFiles(const fs::path& dir) -> std::vector<fs::path>
{
    std::vector<fs::path> result;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto name = entry.path().filename().string();
        if (name.starts_with("logfile.") && name.ends_with(".log")) {
            result.push_back(entry.path());
        }
    }
    return result;
}

// 读取单个文件的全部内容
auto readFile(const fs::path& path) -> std::string
{
    std::ifstream in(path, std::ios::binary);
    return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
}

// 拼接目录中所有日志文件的内容
auto readAllLogContent(const fs::path& dir) -> std::string
{
    std::string all;
    for (const auto& file : listLogFiles(dir)) {
        all += readFile(file);
    }
    return all;
}

// 轮询等待条件成立，直到超时
template <typename Predicate>
auto waitUntil(Predicate pred,
    std::chrono::milliseconds timeout = std::chrono::seconds(5)) -> bool
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return pred();
}

} // namespace

TEST_CASE("RollingFileAppender 异步写入并落盘", "[xlogger][rollingfile]")
{
    TempDir dir;
    // flushInterval=1s：后台线程空闲时按此周期落盘，测试无需等待默认的 3s
    auto appender = X::LogAppender::makeRollingFileAppender(
        dir.path().string(), kDefaultRollSize, /*flushInterval=*/1);
    REQUIRE(appender != nullptr);

    appender->append("first\n");
    appender->append("second\n");
    appender->append("third\n");

    // 后台线程异步落盘，轮询等待全部消息写入
    const auto written = waitUntil([&] {
        const auto content = readAllLogContent(dir.path());
        return content.find("first") != std::string::npos
            && content.find("second") != std::string::npos
            && content.find("third") != std::string::npos;
    });
    REQUIRE(written);

    // 尚未滚动，应只有一个文件，且文件名形如 logfile.<hostname><时间戳>.log
    const auto files = listLogFiles(dir.path());
    REQUIRE(files.size() == 1);

    const auto name = files.front().filename().string();
    CHECK(name.starts_with("logfile."));
    CHECK(name.ends_with(".log"));
    CHECK(std::regex_search(name,
        std::regex(R"(\d{4}-\d{2}-\d{2}-\d{2}:\d{2}:\d{2})")));
}

TEST_CASE("RollingFileAppender 按大小滚动", "[xlogger][rollingfile]")
{
    TempDir dir;
    constexpr std::size_t rollSize = 64;
    auto appender = X::LogAppender::makeRollingFileAppender(
        dir.path().string(), rollSize, /*flushInterval=*/1);
    REQUIRE(appender != nullptr);

    // 第一条消息超过 rollSize，写入后触发第一次滚动（生成第二个文件）
    const std::string big(128, 'A');
    appender->append(big);

    // 等待 big 落盘：此时后台已完成滚动，目录中已有 >= 2 个文件
    const auto rolled = waitUntil([&] {
        return readAllLogContent(dir.path()).find("AAAA") != std::string::npos
            && listLogFiles(dir.path()).size() >= 2;
    });
    REQUIRE(rolled);

    // 第二条短消息写入滚动后的文件，不应再触发滚动
    appender->append("tail\n");
    const auto tailWritten = waitUntil([&] {
        return readAllLogContent(dir.path()).find("tail") != std::string::npos;
    });
    REQUIRE(tailWritten);

    const auto files = listLogFiles(dir.path());
    CHECK(files.size() == 2);

    const auto all = readAllLogContent(dir.path());
    CHECK(all.find(big) != std::string::npos);
    CHECK(all.find("tail") != std::string::npos);
}

TEST_CASE("RollingFileAppender 无效参数抛异常", "[xlogger][rollingfile]")
{
    // 空路径
    CHECK_THROWS_AS(X::LogAppender::makeRollingFileAppender(""), std::invalid_argument);

    // 不存在的目录
    const auto missing = fs::temp_directory_path() / "xlogger_rolling_missing_dir_xyz";
    std::error_code ec;
    fs::remove_all(missing, ec); // 清理可能的残留，保证不存在
    CHECK_THROWS_AS(X::LogAppender::makeRollingFileAppender(missing.string()),
        std::invalid_argument);

    // 传入的是普通文件而非目录
    TempDir dir;
    const auto regularFile = dir.path() / "regular.txt";
    {
        std::ofstream out(regularFile);
        out << "not a directory";
    }
    CHECK_THROWS_AS(X::LogAppender::makeRollingFileAppender(regularFile.string()),
        std::invalid_argument);
}

TEST_CASE("RollingFileAppender 经 Logger 端到端输出", "[xlogger][rollingfile]")
{
    TempDir dir;
    auto appender = X::LogAppender::makeRollingFileAppender(
        dir.path().string(), kDefaultRollSize, /*flushInterval=*/1);
    REQUIRE(appender != nullptr);

    auto logger = std::make_shared<X::Logger>("rolling-test");
    logger->setFormatPattern("{message}{newline}");
    logger->addAppender(appender);

    logger->info("via-logger {}", 42);

    const auto written = waitUntil([&] {
        return readAllLogContent(dir.path()).find("via-logger 42") != std::string::npos;
    });
    REQUIRE(written);
}
