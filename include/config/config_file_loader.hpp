#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <fkYAML/node.hpp>
#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>

namespace config {

// ──────────────────────────────────────────────
// ドット区切りキー解決（共通テンプレート）
// ──────────────────────────────────────────────
//
// Accessor は以下の static メンバを持つ:
//   const Node* GetChild(const Node& node, const std::string& key)
//   std::optional<T> GetLeaf<T>(const Node& node, const std::string& key)

template <typename Accessor, typename T, typename Node>
std::optional<T> ResolveDottedKey(const Node &root, std::string_view dotted_key) {
    const Node *current = &root;
    std::string_view remaining = dotted_key;

    while (true) {
        const auto dot_pos = remaining.find('.');
        if (dot_pos == std::string_view::npos) {
            return Accessor::template GetLeaf<T>(*current, std::string(remaining));
        }
        const auto head = std::string(remaining.substr(0, dot_pos));
        remaining = remaining.substr(dot_pos + 1);
        current = Accessor::GetChild(*current, head);
        if (current == nullptr) {
            return std::nullopt;
        }
    }
}

// ──────────────────────────────────────────────
// TOML アクセサ
// ──────────────────────────────────────────────

struct TomlAccessor {
    static const toml::table *GetChild(const toml::table &node, const std::string &key) {
        return node[key].as_table();
    }

    template <typename T>
    static std::optional<T> GetLeaf(const toml::table &node, const std::string &key) {
        return node[key].template value<T>();
    }
};

// ──────────────────────────────────────────────
// JSON/JSONC アクセサ
// ──────────────────────────────────────────────

struct JsonAccessor {
    static const nlohmann::json *GetChild(const nlohmann::json &node, const std::string &key) {
        if (!node.is_object() || !node.contains(key) || !node.at(key).is_object()) {
            return nullptr;
        }
        return &node.at(key);
    }

    template <typename T>
    static std::optional<T> GetLeaf(const nlohmann::json &node, const std::string &key) {
        if (!node.is_object() || !node.contains(key)) {
            return std::nullopt;
        }
        return node.at(key).get<T>();
    }
};

// ──────────────────────────────────────────────
// YAML アクセサ
// ──────────────────────────────────────────────

struct YamlAccessor {
    static const fkyaml::node *GetChild(const fkyaml::node &node, const std::string &key) {
        if (!node.is_mapping() || !node.contains(key) || !node.at(key).is_mapping()) {
            return nullptr;
        }
        return &node.at(key);
    }

    template <typename T>
    static std::optional<T> GetLeaf(const fkyaml::node &node, const std::string &key) {
        if (!node.is_mapping() || !node.contains(key)) {
            return std::nullopt;
        }
        return node.at(key).get_value<T>();
    }
};

// ──────────────────────────────────────────────
// スキーマ自動マッピング（各フォーマット共通）
// ──────────────────────────────────────────────

template <typename Config, typename Schema, typename Accessor, typename Node>
void ApplySchema(const Schema &schema, const Node &root, Config &conf) {
    std::apply(
        [&](auto &&...field) {
            (
                [&] {
                    using FieldType = std::remove_reference_t<decltype(conf.*field.member)>;
                    auto val = ResolveDottedKey<Accessor, FieldType>(root, field.config_key);
                    if (val.has_value()) {
                        conf.*field.member = *val;
                    }
                }(),
                ...
            );
        },
        schema
    );
}

// ──────────────────────────────────────────────
// ExtraLoader: スキーマ外フィールド読み込み拡張ポイント
// ──────────────────────────────────────────────
//
// 利用側はスキーマ外フィールド（配列型など）の読み込みをここで定義する。
// 未使用の場合は空の ExtraLoaders{} を渡す。
//
// 例:
//   struct MyExtraLoaders {
//       void LoadToml(const toml::table& tbl, MyConfig& conf) const {
//           if (const auto* arr = tbl["items"].as_array()) { ... }
//       }
//       void LoadJson(const nlohmann::json& j, MyConfig& conf) const { ... }
//       void LoadYaml(const fkyaml::node& root, MyConfig& conf) const { ... }
//   };

struct NoExtraLoader {
    template <typename Node, typename Config>
    void LoadToml(const Node & /*unused*/, Config & /*unused*/) const {}
    template <typename Node, typename Config>
    void LoadJson(const Node & /*unused*/, Config & /*unused*/) const {}
    template <typename Node, typename Config>
    void LoadYaml(const Node & /*unused*/, Config & /*unused*/) const {}
};

// ──────────────────────────────────────────────
// フォーマット別ローダー（テンプレート）
// ──────────────────────────────────────────────

template <typename Config, typename Schema, typename ExtraLoader = NoExtraLoader>
void LoadFromToml(const std::string &file_path, const Schema &schema, Config &conf,
                  const ExtraLoader &extra = ExtraLoader{}) {
    const auto tbl = toml::parse_file(file_path);
    ApplySchema<Config, Schema, TomlAccessor>(schema, tbl, conf);
    extra.LoadToml(tbl, conf);
}

template <typename Config, typename Schema, typename ExtraLoader = NoExtraLoader>
void LoadFromJson(const std::string &file_path, const Schema &schema, Config &conf,
                  const ExtraLoader &extra = ExtraLoader{}) {
    std::ifstream ifs(file_path);
    if (!ifs) {
        throw std::runtime_error("Cannot open file: " + file_path);
    }
    const auto j = nlohmann::json::parse(ifs, nullptr, true, true);
    ApplySchema<Config, Schema, JsonAccessor>(schema, j, conf);
    extra.LoadJson(j, conf);
}

template <typename Config, typename Schema, typename ExtraLoader = NoExtraLoader>
void LoadFromYaml(const std::string &file_path, const Schema &schema, Config &conf,
                  const ExtraLoader &extra = ExtraLoader{}) {
    std::ifstream ifs(file_path);
    if (!ifs) {
        throw std::runtime_error("Cannot open file: " + file_path);
    }
    const auto root = fkyaml::node::deserialize(ifs);
    ApplySchema<Config, Schema, YamlAccessor>(schema, root, conf);
    extra.LoadYaml(root, conf);
}

// ──────────────────────────────────────────────
// 公開 API
// ──────────────────────────────────────────────

template <typename Config, typename Schema, typename ExtraLoader = NoExtraLoader>
void LoadFromFile(const std::string &file_path, const Schema &schema, Config &conf,
                  const ExtraLoader &extra = ExtraLoader{}) {
    const auto ext = std::filesystem::path(file_path).extension().string();
    if (ext == ".toml") {
        LoadFromToml(file_path, schema, conf, extra);
    } else if (ext == ".json") {
        LoadFromJson(file_path, schema, conf, extra);
    } else if (ext == ".yaml" || ext == ".yml") {
        LoadFromYaml(file_path, schema, conf, extra);
    } else {
        throw std::runtime_error("Unsupported config file extension: " + ext);
    }
}

inline std::vector<std::string> ExpandManifest(const std::string &manifest_path) {
    std::ifstream ifs(manifest_path);
    if (!ifs) {
        throw std::runtime_error("Cannot open manifest file: " + manifest_path);
    }

    const auto parent_dir = std::filesystem::path(manifest_path).parent_path();
    std::vector<std::string> result;
    std::string line;

    while (std::getline(ifs, line)) {
        if (line.empty() || line.find_first_not_of(" \t") == std::string::npos) {
            continue;
        }
        if (line[line.find_first_not_of(" \t")] == '#') {
            continue;
        }
        auto path = std::filesystem::path(line);
        if (path.is_relative()) {
            path = parent_dir / path;
        }
        result.push_back(path.lexically_normal().string());
    }

    return result;
}

template <typename Config, typename Schema, typename ExtraLoader = NoExtraLoader>
void LoadFromFiles(const std::vector<std::string> &file_paths, const Schema &schema, Config &conf,
                   const ExtraLoader &extra = ExtraLoader{}) {
    for (const auto &path : file_paths) {
        const auto ext = std::filesystem::path(path).extension().string();
        if (ext == ".conf") {
            for (const auto &expanded : ExpandManifest(path)) {
                LoadFromFile(expanded, schema, conf, extra);
            }
        } else {
            LoadFromFile(path, schema, conf, extra);
        }
    }
}

inline std::string FindDefaultConfig() {
    const std::vector<std::string> candidates = {
        "config/default.toml",
        "config/default.json",
        "config/default.yaml",
    };

    std::vector<std::string> found;
    std::copy_if(candidates.begin(), candidates.end(), std::back_inserter(found), [](const std::string &path) {
        return std::filesystem::exists(path);
    });

    if (found.size() > 1) {
        std::string msg = "Multiple default config files found:";
        for (const auto &p : found) {
            msg += " " + p;
        }
        throw std::runtime_error(msg);
    }

    return found.empty() ? std::string{} : found.front();
}

} // namespace config
