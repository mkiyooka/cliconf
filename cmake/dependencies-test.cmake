# テストフレームワーク用のサードパーティライブラリ定義
# トップレベルビルド（テスト・ベンチをビルドする場合）のみ include される
# （CMakeLists.txt 参照）ため PROJECT_IS_TOP_LEVEL のガードは不要だが、
# TARGET_CHECK は取り込み側との干渉防止のため他の依存と同様に指定する。

# doctest - Testing framework
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
add_external_package(doctest ext/doctest-2.5.3
    TARGET_CHECK doctest::doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz
    URL_HASH SHA256=174ebc4e769928959614789c5b4e9c3d0a0f81a62bb608756b127bfebfb21331
)
if(NOT doctest_ALREADY_PROVIDED)
    FetchContent_MakeAvailable(doctest)
endif()

# nanobench - Benchmarking library (header-only)
add_external_package(nanobench ext/nanobench-4.3.11
    TARGET_CHECK nanobench::nanobench
    URL https://github.com/martinus/nanobench/archive/refs/tags/v4.3.11.tar.gz
    URL_HASH SHA256=53a5a913fa695c23546661bf2cd22b299e10a3e994d9ed97daf89b5cada0da70
)
if(NOT nanobench_ALREADY_PROVIDED)
    FetchContent_MakeAvailable(nanobench)
endif()

# Mark doctest as system library to exclude it from clang-tidy checks
if(TARGET doctest)
    get_target_property(doctest_include_dirs doctest INTERFACE_INCLUDE_DIRECTORIES)
    if(doctest_include_dirs)
        set_target_properties(doctest PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${doctest_include_dirs}"
        )
    endif()
endif()
