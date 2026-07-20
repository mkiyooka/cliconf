# テストフレームワーク用のサードパーティライブラリ定義

# doctest - Testing framework
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
add_external_package(doctest ext/doctest-2.5.3
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz
    URL_HASH SHA256=174ebc4e769928959614789c5b4e9c3d0a0f81a62bb608756b127bfebfb21331
)
FetchContent_MakeAvailable(doctest)

# nanobench - Benchmarking library (header-only)
add_external_package(nanobench ext/nanobench-4.3.11
    URL https://github.com/martinus/nanobench/archive/refs/tags/v4.3.11.tar.gz
    URL_HASH SHA256=53a5a913fa695c23546661bf2cd22b299e10a3e994d9ed97daf89b5cada0da70
)
FetchContent_MakeAvailable(nanobench)

# Mark doctest as system library to exclude it from clang-tidy checks
if(TARGET doctest)
    get_target_property(doctest_include_dirs doctest INTERFACE_INCLUDE_DIRECTORIES)
    if(doctest_include_dirs)
        set_target_properties(doctest PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${doctest_include_dirs}"
        )
    endif()
endif()
