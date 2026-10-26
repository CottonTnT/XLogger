-- xlogger 使用示例（binary）
--
-- 演示两种常见用法：
--   1. 显式构造 Logger + 自定义格式 + stdout appender（见 main.cpp）
--   2. 类 .cpp 通过 LoggerFactory 构造文件作用域全局 logger（见 order_service.cpp）

target("xlogger_example")
    set_kind("binary")
    add_deps("xlogger")
    add_files("*.cpp")
    set_languages("c++20")
    add_cxflags("-Wall", "-Wextra")
target_end()
