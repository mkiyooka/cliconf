# config_lib（cliconf::config）: ヘッダオンリー INTERFACE ターゲット
#
# ConfigManager（config/config_manager.hpp）と LoadFromFile*（config/config_file_loader.hpp）
# はどちらもテンプレートのみで実装された汎用コードで、cliconf 自身の Config 構造体には
# 依存しない。取り込み側プロジェクトは自分の Config 構造体・スキーマを渡して使う。
# FetchContent でサブプロジェクトとして取り込まれた場合（PROJECT_IS_TOP_LEVEL=OFF）でも
# 利用できるよう、無条件で定義する。
add_library(config_lib INTERFACE)
add_library(cliconf::config ALIAS config_lib)

target_include_directories(config_lib INTERFACE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(config_lib INTERFACE
    CLI11::CLI11
    tomlplusplus::tomlplusplus
    nlohmann_json::nlohmann_json
    fkYAML_target
)

# ------------------------------------------------------------------------------
# サンプルアプリ（cmd 実行ファイル）専用の Config 実装
#
# config_loader.hpp（サンプル Config 構造体）・config_schema.hpp（サンプルスキーマ）・
# config_validator.hpp/cpp（サンプル固有の検証ロジック）は cliconf 自身のサンプルアプリの
# ためのコードであり、取り込み側が README-library.md の手順に従って用意する
# 同名パスのファイル（取り込み側プロジェクトの include/config/config_loader.hpp 等）と
# 衝突しうるため、公開ヘッダ（include/）には置かず src/config/ に閉じ込め、
# トップレベルビルド時のみビルドする。
# ------------------------------------------------------------------------------
if(PROJECT_IS_TOP_LEVEL)
    add_library(config_sample_lib STATIC config_validator.cpp)

    target_include_directories(config_sample_lib PUBLIC ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(config_sample_lib PUBLIC
        cliconf::config
        fmt::fmt
    )
    target_link_libraries(config_sample_lib PRIVATE sanitizer_flags)
endif()
