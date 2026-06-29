#pragma once

// C++20 std::span が使える場合はそちらを使い、そうでなければ tcb::span にフォールバックする。
// 利用側は compat::span を使う。

#if defined(__cpp_lib_span) && __cpp_lib_span >= 202002L

#include <span>

namespace compat {
using std::span;
using std::dynamic_extent;
} // namespace compat

#else

#include <tcb/span.hpp>

namespace compat {
using tcb::span;
using tcb::dynamic_extent;
} // namespace compat

#endif
