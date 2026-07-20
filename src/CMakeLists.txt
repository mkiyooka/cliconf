# config_lib（cliconf::config）・command_lib・cmd 実行ファイルはいずれも cliconf
# 自身のサンプルアプリ固有のコードであり、サンプル固有の Config 構造体
# （include/config/config_loader.hpp 等、取り込み側でコピー・改変する前提のファイル）
# に依存する。FetchContent でサブプロジェクトとして取り込まれた場合にまで無条件で
# ビルドされると、取り込み側が用意する同名パスのヘッダ（include/config/config_*.hpp）
# と衝突する（インクルードパスの解決順序次第でどちらが使われるか不定になる）ため、
# トップレベルビルド時のみ有効にする。ライブラリとして取り込む場合は
# README-library.md の手順に従い、ヘッダオンリーの cliconf::cliconf を使うこと。
if(PROJECT_IS_TOP_LEVEL)
    add_subdirectory(config)
    add_subdirectory(command)

    # Main executable
    add_executable(cmd main.cpp)
    set_target_properties(cmd PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    target_link_libraries(cmd PRIVATE command_lib config_lib sanitizer_flags)

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
