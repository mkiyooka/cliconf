#pragma once

#include <string_view>

namespace config {

/**
 * @brief Config構造体のフィールドとCLIオプション・設定ファイルキーのマッピング記述子
 *
 * @tparam Owner フィールドを持つ構造体型
 * @tparam T フィールドの型
 *
 * 利用例:
 * @code
 * inline constexpr auto kMySchema = std::make_tuple(
 *     FieldDescriptor{"--title", "title", "Application title", &MyConfig::title},
 *     FieldDescriptor{"--port",  "port",  "Port number",       &MyConfig::port}
 * );
 * @endcode
 */
template <typename Owner, typename T>
struct FieldDescriptor {
    std::string_view cli_option;  ///< CLI11 オプション名 (例: "--settings.value")
    std::string_view config_key;  ///< 設定ファイルキー (例: "settings.value", ドット区切りでネスト表現)
    std::string_view description; ///< CLI ヘルプ文字列
    T Owner::*member;             ///< ポインタ・トゥ・メンバー
};

// CTAD補助 (C++17): FieldDescriptor<Owner, T>(...) の型推論を有効にする
template <typename Owner, typename T>
FieldDescriptor(std::string_view, std::string_view, std::string_view, T Owner::*) -> FieldDescriptor<Owner, T>;

} // namespace config
