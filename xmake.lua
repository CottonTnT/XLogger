-- 项目名称与版本
set_project("XLogger")
set_version("0.1.0")

-- 设置需要的最低 xmake 版本
set_xmakever("2.8.0")

-- 设置支持的编译模式，默认 Debug
add_rules("mode.debug", "mode.release", "mode.releasedbg", "mode.minsizerel")
set_defaultmode("debug")

add_rules("plugin.compile_commands.autoupdate", {outputdir=".vscode"})

-- 设置语言标准 (gnu++20 对应 Enable C++ Extensions 的 C++20)
set_languages("c11", "gnu++20")

-- 如果主机是 Linux，指定使用 GCC/G++ 编译器
if is_host("linux") then
    set_config("cc", "/usr/bin/gcc")
    set_config("cxx", "/usr/bin/g++")
end

-- 自定义构建选项：控制 examples、tools、tests 的构建
option("buildExamples")
    set_default(true)
    set_showmenu(true)
    set_description("Build examples")
option_end()

option("buildTools")
    set_default(true)
    set_showmenu(true)
    set_description("Build tools")
option_end()

option("buildTests")
    set_default(true)
    set_showmenu(true)
    set_description("Build tests")
option_end()

-- 导入编译选项规则（-Wall / -Wextra / -Wpedantic 等）
includes("xmake/ComplierOptions.lua")

-- common 模块来自远程 xmake 包仓库 my-xmake-repo
add_repositories("cot-repo https://github.com/CottonTnT/my-xmake-repo.git")
add_requires("common")

if has_config("buildExamples") then
    includes("examples")
end

if has_config("buildTools") then
    includes("tools")
end

if has_config("buildTests") then
    includes("test")
end

-- 定义 XLogger 静态库目标
target("xlogger")
    set_kind("static")

    -- 源文件与头文件（仅纳入 srcs/ 与 include/，避免把 examples/tools/test 一起编译）
    add_files("srcs/**.cpp")
    add_headerfiles("include/**.h", "include/**.hpp")

    -- C++20 标准，并向依赖者/下游 (public) 自动传递要求
    set_languages("c++20")

    -- 包含目录：PUBLIC 暴露给下游；PRIVATE 仅供库内部编译使用
    add_includedirs("include", { public = true })
    add_includedirs("srcs")

    -- 继承全局编译选项规则
    add_rules("compiler_options")

    -- 依赖 common 头文件库（public：下游可透明使用 XType/InlineBuffer 等）
    add_packages("common", { public = true })

    -- 链接依赖（Threads::Threads）
    add_syslinks("pthread", { public = true })
target_end()
