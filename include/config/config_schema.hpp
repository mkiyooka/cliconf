#pragma once

#include <cstdint>
#include <string>
#include <tuple>

#include <fkYAML/node.hpp>
#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>

#include "cliconf/field_descriptor.hpp"
#include "config/config_loader.hpp"

namespace config {

// ──────────────────────────────────────────────
// スキーマ定義（カスタマイズ対象）
//
// 新しいフィールドを追加するときはここに1行追加するだけでよい。
// CLIオプション登録・設定ファイル読み込み・優先度解決はすべて自動化される。
// ──────────────────────────────────────────────

inline constexpr auto kConfigSchema = std::make_tuple(
    FieldDescriptor{"--title",          "title",          "Application title", &Config::title},
    FieldDescriptor{"--settings.value", "settings.value", "Numeric value",     &Config::value}
);

// ──────────────────────────────────────────────
// ExtraLoader: スキーマ外フィールドの読み込み（カスタマイズ対象）
//
// plugin 配列・subcommand 設定など std::vector や入れ子構造体など、
// スキーマ自動マッピングでは扱えないフィールドをここで読み込む。
// ──────────────────────────────────────────────

struct AppExtraLoader {
    void LoadToml(const toml::table &tbl, Config &conf) const {
        if (const auto *arr = tbl["plugin"].as_array()) {
            conf.plugins.clear();
            for (const auto &el : *arr) {
                if (const auto *table = el.as_table()) {
                    PluginConfig plugin;
                    plugin.file   = (*table)["file"].value_or(std::string{});
                    plugin.number = (*table)["number"].value_or(std::uint64_t{0});
                    conf.plugins.push_back(plugin);
                }
            }
        }
        if (const auto *subs = tbl["subcommands"].as_table()) {
            for (std::size_t i = 0; i < kSubcommandMappingCount; ++i) {
                const auto &mapping = kSubcommandMappings[i];
                if (const auto *sub_tbl = (*subs)[mapping.key].as_table()) {
                    (conf.*mapping.member).a = (*sub_tbl)["a"].value_or(0);
                    (conf.*mapping.member).b = (*sub_tbl)["b"].value_or(0);
                }
            }
        }
    }

    void LoadJson(const nlohmann::json &j, Config &conf) const {
        if (j.contains("plugin") && j.at("plugin").is_array()) {
            conf.plugins.clear();
            for (const auto &el : j.at("plugin")) {
                PluginConfig plugin;
                if (el.contains("file") && el.at("file").is_string()) {
                    plugin.file = el.at("file").get<std::string>();
                }
                if (el.contains("number") && el.at("number").is_number_unsigned()) {
                    plugin.number = el.at("number").get<std::uint64_t>();
                }
                conf.plugins.push_back(plugin);
            }
        }
    }

    void LoadYaml(const fkyaml::node &root, Config &conf) const {
        const auto key = std::string("plugin");
        if (root.is_mapping() && root.contains(key) && root.at(key).is_sequence()) {
            conf.plugins.clear();
            for (const auto &el : root.at(key)) {
                PluginConfig plugin;
                if (el.is_mapping() && el.contains("file")) {
                    plugin.file = el.at("file").get_value<std::string>();
                }
                if (el.is_mapping() && el.contains("number")) {
                    plugin.number = el.at("number").get_value<std::uint64_t>();
                }
                conf.plugins.push_back(plugin);
            }
        }
    }
};

} // namespace config
