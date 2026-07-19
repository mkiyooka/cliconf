# config_lib: ヘッダオンリーの INTERFACE ターゲット
# ConfigManager / LoadFromFile* はテンプレートのためヘッダに実装済み。
# config_validator.cpp のみ実体ファイルとしてビルドする。

add_library(config_lib STATIC config_validator.cpp)
add_library(cliconf::config ALIAS config_lib)

target_include_directories(config_lib PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(config_lib PUBLIC
    CLI11::CLI11
    tomlplusplus::tomlplusplus
    nlohmann_json::nlohmann_json
    fkYAML_target  # PUBLIC: config_file_loader.hpp がヘッダオンリーのため伝播が必要
    fmt::fmt
)
target_link_libraries(config_lib PRIVATE sanitizer_flags)
