#pragma once

#include <XType.hpp>

#include <string_view>

namespace X::detail {

/**
 * @brief 线程信息 ADL 定制点 tag。
 *
 * 库使用者可以通过重新打开 `namespace X::detail` 并定义**非模板**同名重载来覆盖默认实现，
 * 由于调用点使用非限定调用，ADL 会优先选择用户提供的非模板重载（非模板优于模板默认实现）。
 *
 * 示例（放在使用方自己的 .cpp 文件中）：
 * @code
 * namespace X::detail {
 *     auto getCurrentThreadId(ThreadInfoHook) -> u64 { return getRealThreadId(); }
 *     auto getCurrentThreadName(ThreadInfoHook) -> std::string_view { return "worker"; }
 * }
 * @endcode
 */
struct ThreadInfoHook { };

/// 默认线程 ID：返回 0（占位），可被使用方通过 ADL 覆盖。
template <typename = void>
inline auto getCurrentThreadId(ThreadInfoHook) -> u64
{
    return 0;
}

/// 默认线程名称：返回空（占位），可被使用方通过 ADL 覆盖。
template <typename = void>
inline auto getCurrentThreadName(ThreadInfoHook) -> std::string_view
{
    return {};
}

} // namespace X::detail
