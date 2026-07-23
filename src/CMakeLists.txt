# config_lib（cliconf::config）はヘッダオンリーの汎用ライブラリ（ConfigManager /
# LoadFromFile*）で、cliconf 自身の Config 構造体に依存しないため、FetchContent で
# サブプロジェクトとして取り込まれた場合（PROJECT_IS_TOP_LEVEL=OFF）でも利用できるよう
# 無条件で add_subdirectory する。
#
# 一方 command_lib・cmd 実行ファイル・サンプル固有の Config 実装
# （src/config/config_loader.hpp 等、取り込み側でコピー・改変する前提のファイル）は
# cliconf 自身のサンプルアプリ固有のコードであり、公開ヘッダ（include/）には置いていない。
# これらはトップレベルビルド時のみ有効にする。ライブラリとして取り込む場合は
# README-library.md の手順に従い、cliconf::config / cliconf::cliconf を使うこと。
add_subdirectory(config)

if(PROJECT_IS_TOP_LEVEL)
    add_subdirectory(command)

    # Main executable
    add_executable(cmd main.cpp)
    set_target_properties(cmd PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    target_link_libraries(cmd PRIVATE command_lib config_sample_lib sanitizer_flags)

    # プリコンパイル済みヘッダーの設定
    if(USE_PCH)
        target_precompile_headers(cmd PRIVATE
            <memory>
            <string>
            <CLI/CLI.hpp>
            <fmt/base.h>
        )
        message(STATUS "Precompiled headers enabled for executables")
    endif()
endif()
