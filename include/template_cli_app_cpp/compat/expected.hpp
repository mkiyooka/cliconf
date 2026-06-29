#pragma once

// C++23 std::expected が使える場合はそちらを使い、そうでなければ tl::expected にフォールバックする。
// 利用側は compat::expected<T, E> / compat::unexpected<E> を使う。

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L

#include <expected>

namespace compat {
using std::expected;
using std::unexpected;
} // namespace compat

#else

#include <tl/expected.hpp>

namespace compat {
using tl::expected;
using tl::unexpected;
} // namespace compat

#endif
