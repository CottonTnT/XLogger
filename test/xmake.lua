-- xlogger 测试目标（Catch2）
--
-- 通过 `xmake test` 运行，或直接构建 xlogger_test 目标。
-- 使用系统 Catch2（头文件 /usr/include/catch2 + libCatch2.a / libCatch2Main.a），
-- 避免 xmake 包管理安装的 catch2 为空目录导致链接缺 vtable。

target("xlogger_test")
    set_kind("binary")
    add_deps("xlogger")
    add_files("*.cpp")
    set_languages("c++20")
    add_cxflags("-Wall", "-Wextra")
    add_links("Catch2Main", "Catch2")
    add_tests("xlogger_test")
target_end()
