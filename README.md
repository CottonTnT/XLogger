# XLogger

一个基于 C++20 的轻量级日志库，使用 `std::format` 进行消息格式化、`std::source_location` 自动捕获调用点，并提供可组合的格式模式与可扩展的 Appender 体系。

> 当前项目处于重构/完善阶段，API 与目录结构仍可能调整。

## 特性

- **C++20**：`std::format`、`std::source_location`、concepts、`std::type_identity` 等。
- **格式模式**：通过 `{message}` `{level}` `{date:...}` 等语义化字段自由组合输出格式（见下文格式模式表）。
- **级别过滤**：`DEBUG / INFO / WARN / ERROR`，低于 logger 级别的日志会被丢弃。
- **root logger 委托**：未挂 Appender 的 logger 会自动把日志转发到其 root logger。
- **可扩展 Appender**：实现 `LogAppender` 接口即可接入任意输出目标。
- **线程信息 ADL 定制点**：线程 ID / 名称默认填占位值，可由库使用方通过 ADL 自行扩展。
- **自动捕获文件与行号**：`Logger` 的日志接口借助 `std::source_location` 记录调用点。

## 依赖与要求

- 编译器：GCC / Clang，需支持 C++20（项目默认使用 `gnu++20`）。
- 构建工具：[xmake](https://xmake.io) ≥ 2.8.0。
- 私有包仓库：`https://github.com/CottonTnT/my-xmake-repo.git`（已在 `xmake.lua` 中配置为 `cot-repo`）。
- 依赖包：`common`（提供 `XType.hpp`、`InlineBuffer.hpp`、`Singleton.hpp`、`StringUtil.hpp` 等头文件），由 xmake 自动拉取。

## 构建与测试

```bash
# 构建全部目标（默认开启 examples / tools / tests）
xmake

# 仅构建库
xmake build xlogger

# 构建并运行测试（Catch2）
xmake build xlogger_test
xmake run xlogger_test

# 或通过 xmake 的测试运行器
xmake test

# 仅运行某个标签的用例
xmake run xlogger_test "[xlogger][loglevel]"

# 构建并运行示例
xmake build xlogger_example
xmake run xlogger_example
```

常用配置选项（默认均为 `true`）：

```bash
xmake f --buildTests=n      # 关闭测试
xmake f --buildExamples=n   # 关闭示例
xmake f --buildTools=n      # 关闭工具
xmake f --coverage=y        # 开启代码覆盖率（仅 debug 模式）
```

> 注意：编译选项默认开启 `-Wall -Wextra -Wpedantic -Werror`。

## 快速上手

```cpp
#include "Logger.h"
#include "LogAppender.h"
#include "LogLevel.h"
#include "LoggerFactory.h"

int main()
{
    // 方式一：显式创建 logger，并自定义级别 / 格式 / 输出目标
    auto logger = std::make_shared<X::Logger>("mylogger");
    logger->setLogLevel(X::LogLevel::INFO);
    logger->addAppender(X::LogAppender::makeStdoutAppender());
    logger->setFormatPattern("{date:%Y-%m-%d %H:%M:%S} [{level}] [{logger}] {message}{newline}");

    logger->info("hello {}", 42);
    logger->warn("be careful");
    logger->error("boom {}", 7);

    // 方式二：通过 LoggerFactory 按名获取 logger（委托到 ROOT，默认输出到 stdout）
    auto appLogger = X::LoggerFactory::getLogger("app");
    appLogger->info("app started, version={}", 1);

    return 0;
}
```

> 更完整的用法（含“类 `.cpp` 内用 `LoggerFactory` 构造文件作用域 logger”的推荐写法）见 [examples/](examples/)。

## 公开 API 概览

| 头文件 | 说明 |
| --- | --- |
| `Logger.h` | `Logger` / `LoggerSPtr`：日志器，管理级别、格式模式与 Appender 列表，提供 `debug / info / warn / error` |
| `LoggerFactory.h` | `LoggerFactory`：单例工厂，按名获取缓存的 logger（委托到 `ROOT`） |
| `LogAppender.h` | `LogAppender` / `AppenderPtr`：Appender 抽象接口与工厂 |
| `LogLevel.h` | `LogLevel` 枚举及 `logLevel2String` / `string2LogLevel` |
| `ThreadInfo.h` | 线程 ID / 名称的 ADL 定制点（`ThreadInfoHook`） |

### Logger

```cpp
auto logger = std::make_shared<X::Logger>("name");          // 无 root
auto child  = std::make_shared<X::Logger>("child", parent); // 指定 root

logger->setLogLevel(X::LogLevel::WARN);      // 设置级别
logger->setFormatPattern("{message}{newline}"); // 设置格式模式
logger->addAppender(appender);               // 添加 Appender
logger->removeAppender(appender);            // 移除 Appender
logger->clearAppender();                     // 清空 Appender
logger->getLogLevel();                       // 读取级别
logger->getFormatPattern();                  // 读取格式模式
logger->getLoggerName();                     // 读取 logger 名称
logger->debug("fmt {}", args...);            // DEBUG
logger->info("fmt {}", args...);             // INFO
logger->warn("fmt {}", args...);             // WARN
logger->error("fmt {}", args...);            // ERROR
```

> 构造 `Logger` 时名称不能为空，否则抛出 `std::invalid_argument`；`addAppender(nullptr)` 会被安全忽略。

### LoggerFactory

`LoggerFactory::getLogger(name)` 按名称返回一个**全局缓存**的 logger（同名只创建一次）。返回的 logger 自身没有 Appender，会把日志委托给 `ROOT` logger（默认输出到 stdout）：

```cpp
#include "LoggerFactory.h"

auto logger = X::LoggerFactory::getLogger("MyClass");
logger->info("hello {}", 42);
```

推荐做法：在类的 `.cpp` 中用文件作用域变量持有它，避免对外暴露（internal linkage）：

```cpp
// MyClass.cpp
#include "LoggerFactory.h"

namespace {
const auto logger = X::LoggerFactory::getLogger("MyClass");
}

void MyClass::doSomething()
{
    logger->info("doing something, value={}", 42);
}
```

格式化字符串采用 `std::format` 语法。

## 格式模式

格式模式由若干语义化字段 `{name}` / `{name:spec}` 与普通文本组合而成（类似 `std::format` 的格式串）：

| 字段 | 含义 |
| --- | --- |
| `{message}` | 日志消息 |
| `{level}` | 日志级别 |
| `{logger}` | 日志器名称 |
| `{date}` | 日期时间，可跟 `:...` 指定 strftime 格式，如 `{date:%Y-%m-%d %H:%M:%S}` |
| `{elapsed}` | 运行毫秒数（见“已知限制”） |
| `{file}` | 文件名 |
| `{line}` | 行号 |
| `{thread_id}` | 线程 ID（默认占位 0） |
| `{thread_name}` | 线程名称（默认空） |
| `{fiber_id}` | 协程 ID（占位 0） |
| `{tab}` | 制表符 |
| `{newline}` | 换行 |
| `{{` / `}}` | 字面量花括号 |

默认模式：

```
{date:%Y-%m-%d %H:%M:%S} [{elapsed}ms] {thread_id}{tab}{thread_name}{tab}{fiber_id}{tab}[{level}]{tab}[{logger}]{tab}[{file}:{line}]{tab}{tab}{message}{newline}
```

未识别的字段（如 `{foo}`）、空字段名 `{}`、未闭合的 `{`、孤立的 `}` 都会抛出 `std::invalid_argument`。

## 日志级别

```cpp
enum class LogLevel : u8 {
    ERROR = 6,
    WARN  = 5,
    INFO  = 3,
    DEBUG = 2,
};
```

- `logLevel2String(LogLevel) -> std::string_view`：级别转字符串，非法值返回 `"UNKNOWN"`。
- `string2LogLevel(std::string_view) -> LogLevel`：大小写不敏感，未知字符串返回 `LogLevel::DEBUG`。
- 支持 `==` `<` `<=` `>` `>=` 比较运算（数值越大级别越高）。

## Appender

`LogAppender` 抽象了日志的最终输出目标，需实现三个方法：

```cpp
virtual void append(std::string_view message) = 0;
virtual void flush() = 0;
virtual void sync() = 0;
```

当前内置工厂：

- `LogAppender::makeStdoutAppender()`：输出到标准输出。

自定义 Appender 只需继承 `X::LogAppender` 并实现上述接口即可接入。

> 文件 Appender / 滚动文件 Appender 目前尚未实现（源码中为注释/TODO），后续再补充。

## 线程信息定制（ADL）

`{thread_id}`（线程 ID）与 `{thread_name}`（线程名称）默认分别输出 `0` 和空串。库使用方可以通过 ADL 定制点自行提供真实实现：

```cpp
// 放在使用方自己的 .cpp 文件中
#include "ThreadInfo.h"

namespace X::detail {
    auto getCurrentThreadId(ThreadInfoHook) -> u64
    {
        return /* 你的线程 ID 实现 */;
    }
    auto getCurrentThreadName(ThreadInfoHook) -> std::string_view
    {
        return /* 你的线程名实现 */;
    }
}
```

调用点使用非限定调用，用户提供的**非模板**重载会优先于库内置的模板默认实现被选中。

## 已知限制

- `{elapsed}` 目前输出的是 epoch 毫秒数，与“logger 创建后累计运行毫秒数”的注释语义不符。
- `{fiber_id}`（协程 ID）未接入协程，恒输出 `0`。
- 线程名默认空、线程 ID 默认 0（可通过 ADL 定制点扩展）。
- `AsyncLogger.h` / `EventFixedBuffer.hpp`（异步日志）整体注释未启用。
- `SocketAppender` / `SystemAppender` 仅为占位声明。
- 文件 / 滚动文件 Appender 尚未实现。

## 目录结构

```
include/           公开头文件（Logger / LoggerFactory / LogAppender / LogLevel / ThreadInfo）
srcs/              库实现与内部头文件
test/              Catch2 单元测试
examples/          使用示例（显式构造 Logger + LoggerFactory 文件作用域 logger）
tools/             工具（待补充）
xmake.lua          根构建脚本
```

## License

待定（TBD）。
