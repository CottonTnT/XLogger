# XLogger

A lightweight C++20 logging library. Message formatting via `std::format`, call-site capture via `std::source_location`, composable semantic `{name}` format fields (like `std::format`), and a pluggable Appender system. Namespace is `X` (implementation details live in `X::detail`).

The project is **mid-refactor**: it was extracted from a larger codebase (muduo-style `net`, `fiber`, `common`, `config`, `timer` modules) into a standalone logger-only library with a flat directory layout. Read **Current WIP state** below before assuming anything compiles.

## Build & test

Build tool is [xmake](https://xmake.io) ≥ 2.8.0 (not CMake). Root script is [xmake.lua](xmake.lua).

```bash
xmake                  # build everything (lib + tests by default)
xmake build xlogger    # build only the static lib
xmake build xlogger_test && xmake run xlogger_test   # build & run Catch2 tests
xmake test             # run tests via xmake test runner
xmake run xlogger_test "[xlogger][loglevel]"          # run a single Catch2 tag
```

Config options (all default `true`):

```bash
xmake f --buildTests=n --buildExamples=n --buildTools=n
xmake f --coverage=y    # debug-mode coverage
```

- Language standard: `gnu++20` (C++20 with extensions); GCC on Linux is forced via `/usr/bin/g++` ([xmake.lua](xmake.lua)).
- Compiler flags come from [xmake/ComplierOptions.lua](xmake/ComplierOptions.lua): `-Wall -Wextra -Wpedantic -Werror`, plus `-fPIC` and `-rdynamic` on Linux. Debug mode defines `X_DEBUG`; release defines `NDEBUG`.
- Builds a **static** library target `xlogger` from `srcs/**.cpp`, with `include/` as a PUBLIC include dir and `srcs/` as a private one.

## Dependencies

- `common` package from private xmake repo `cot-repo https://github.com/CottonTnT/my-xmake-repo.git` (configured in [xmake.lua](xmake.lua)). Provides header-only utilities used across the lib:
  - `XType.hpp` — `u8`/`u32`/`u64` integer typedefs (in `include/LogLevel.h`, `include/ThreadInfo.h`, `srcs/LogRecordView.h`, `srcs/LogAppenderImpl.hpp`)
  - `InlineBuffer.hpp` — fixed-capacity inline buffer that spills to heap when full (`include/Logger.h`, `srcs/LogFormatter.h`)
  - `Singleton.hpp` — `SingletonWithConstructToken`/`ConstructToken` (used by `LoggerFactory`)
  - `StringUtil.hpp` — `StringUtil::hashString` (used by `string2LogLevel`)
- `pthread` (`add_syslinks`).
- Tests use Catch2 via xmake's package manager.

## Directory layout

```
include/    public headers: Logger.h, LogAppender.h, LogLevel.h, LoggerFactory.h, ThreadInfo.h
srcs/       implementation + internal headers (included directly, header-only impls)
test/       Catch2 unit tests (LoggerTest, LogFormatterTest, LogLevelTest, ConcurrencyTest, TestSupport.hpp)
examples/   runnable usage examples (xlogger_example: explicit Logger + LoggerFactory file-local logger)
tools/      empty placeholder (empty xmake.lua)
xmake/      ComplierOptions.lua — the compiler_options rule
```

The old nested layout (`include/logger/`, `srcs/logger/`, `srcs/net/`, `srcs/common/`, …) was deleted in the refactor. `tools/` is an empty stub and contributes nothing yet; `examples/` now has a runnable `xlogger_example` target.

## Architecture

Core flow: a `Logger` formats a `LogRecordView` (via a `LogFormatter`) into an `InlineBuffer`, then hands the resulting string to each registered `LogAppender`.

**Key files:**

- [include/Logger.h](include/Logger.h) — `Logger` (pimpl, non-copyable/non-movable), `LoggerSPtr = std::shared_ptr<Logger>`. Public logging methods are the `debug/info/warn/error` templates. Holds `detail::FmtStrWithSrcLoc` (a `consteval` wrapper that captures `std::source_location` at the call site) and `detail::format` (stack-buffer fast path via `std::format_to_n`, heap slow path on overflow).
- [srcs/LoggerImpl.hpp](srcs/LoggerImpl.hpp) — `Logger::Impl`. Owns name, `std::atomic<LogLevel>`, format pattern, `std::vector<AppenderPtr>`, and `rootLogger_`. Guarded by `std::shared_mutex`. `submit()` formats + fans out to appenders, delegating to `rootLogger_` when the appender list is empty; formatting exceptions are swallowed and written to `stderr`.
- [srcs/LogFormatter.h](srcs/LogFormatter.h) + [srcs/LogFormatter.cpp](srcs/LogFormatter.cpp) — parses the pattern into a `std::vector<std::unique_ptr<const FormatItem>>`.
- [srcs/FormatItem.h](srcs/FormatItem.h) / [srcs/FormatItemProxy.hpp](srcs/FormatItemProxy.hpp) / [srcs/FormatItemImpl.hpp](srcs/FormatItemImpl.hpp) — type-erased format items. `FormatItem` is the abstract base; `FormatItemProxy<ItemImpl>` erases a concrete item; concrete items + a `string -> factory` map live in `FormatItemImpl.hpp`. Unknown field names throw `std::invalid_argument`.
- [srcs/LogRecordView.h](srcs/LogRecordView.h) — plain data struct `LogRecordView` (a log event: logger name, level, timestamp, elapsed, thread/fiber id, thread name, filename, line, message).
- [include/LogAppender.h](include/LogAppender.h) + [srcs/LogAppenderProxy.hpp](srcs/LogAppenderProxy.hpp) + [srcs/LogAppenderImpl.hpp](srcs/LogAppenderImpl.hpp) — `LogAppender` abstract base (`append`/`flush`/`sync`). `AppenderProxy<Impl>` erases a concrete impl constrained by the `LogAppenderConcept` concept. Only `StdoutAppender` is implemented; `makeStdoutAppender()` is the factory. [srcs/LogAppender.cpp](srcs/LogAppender.cpp) is the thin TU that includes `LogAppenderImpl.hpp` to instantiate the definitions (mirrors `Logger.cpp` → `LoggerImpl.hpp`).
- [include/LogLevel.h](include/LogLevel.h) + [srcs/LogLevel.cpp](srcs/LogLevel.cpp) — `enum class LogLevel : u8 { ERROR=6, WARN=5, INFO=3, DEBUG=2 }` (higher = more severe), plus `logLevel2String`, `string2LogLevel` (hash-based, case-insensitive) and comparison operators.
- [include/LoggerFactory.h](include/LoggerFactory.h) + [srcs/LoggerFactory.cpp](srcs/LoggerFactory.cpp) — singleton root-logger registry. `LoggerFactory::getLogger(name)` returns cached named loggers that delegate to the `ROOT` logger (which defaults to a stdout appender).
- [include/ThreadInfo.h](include/ThreadInfo.h) — ADL customization point `ThreadInfoHook`; `getCurrentThreadId`/`getCurrentThreadName` default to `0`/empty but a non-template overload in `X::detail` is preferred by unqualified lookup.

**Format fields** (`{name}` / `{name:spec}` syntax, like `std::format`): `{message}`, `{level}`, `{logger}`, `{date:strftime}`, `{elapsed}`, `{file}`, `{line}`, `{thread_id}`, `{thread_name}`, `{fiber_id}`, `{newline}`, `{tab}`; `{{`/`}}` escape literal braces; unknown fields throw. Default pattern is `detail::DEFAULT_LOG_PATTERN` in [srcs/LoggerImpl.hpp](srcs/LoggerImpl.hpp).

## Conventions

- **Header-only internals**: most `srcs/*.hpp` files contain inline member definitions; only the few `.cpp` files are compiled. Internal headers are meant to be `#include`d by implementation `.cpp`s, not by consumers.
- **Type erasure via proxy templates**: `FormatItemProxy<Impl>` and `AppenderProxy<Impl>` wrap concrete classes so the interface stays a virtual base. Add a new appender/format item by writing a concrete class and registering a factory.
- **Pimpl + shared_mutex**: `Logger` hides everything behind `Logger::Impl`; level is a lock-free `std::atomic`, mutable state (formatter, appender list) is under `std::shared_mutex`.
- **Comments are a mix of Chinese and English** (Chinese mostly in doc-comments). Match the surrounding file's language when adding comments.
- **Spelling quirks are intentional in field names** — `LogRecordView::customeMsg` and `Logger::Impl::logFomatter_` are misspelled but consistent; changing them means touching every reference.

## Current WIP state — read before editing

- **The public API changed in the refactor.** The old `LogService.hpp` facade (`X::LogService::info(logger, ...)`) was deleted and replaced by `Logger::info/debug/warn/error` member templates. Tests and examples now use the new API; the README is up to date.
- Async logging ([srcs/AsyncLogger.h](srcs/AsyncLogger.h), [srcs/EventFixedBuffer.hpp](srcs/EventFixedBuffer.hpp)) is entirely commented out.
- File / rolling-file / socket / system appenders are unimplemented placeholders (commented out in [srcs/LogAppenderImpl.hpp](srcs/LogAppenderImpl.hpp)).
- Known semantic gaps (also in README): `{elapsed}` emits epoch milliseconds (not elapsed-since-logger-creation), and `{fiber_id}` is hardcoded to `0`.
