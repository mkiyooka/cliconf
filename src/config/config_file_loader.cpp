#include "config_file_loader.hpp"

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

#include "config/config_schema.hpp"

namespace config {

namespace {

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

void LoadFromToml(const std::string &file_path, Config &conf) {
    const auto tbl = toml::parse_file(file_path);

    std::apply(
        [&](auto &&...field) {
            (
                [&] {
                    using FieldType = std::remove_reference_t<decltype(conf.*field.member)>;
                    auto val = ResolveDottedKey<TomlAccessor, FieldType>(tbl, field.config_key);
                    if (val.has_value()) {
                        conf.*field.member = *val;
                    }
                }(),
                ...
            );
        },
        kConfigSchema
    );

    if (const auto *arr = tbl["plugin"].as_array()) {
        conf.plugins.clear();
        for (const auto &el : *arr) {
            if (const auto *table = el.as_table()) {
                PluginConfig plugin;
                plugin.file = (*table)["file"].value_or(std::string{});
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

void LoadFromJson(const std::string &file_path, Config &conf) {
    std::ifstream ifs(file_path);
    if (!ifs) {
        throw std::runtime_error("Cannot open file: " + file_path);
    }
    const auto j = nlohmann::json::parse(
        ifs, /*callback cb=*/nullptr,
        /*allow_exceptions=*/true,
        /*ignore_comments=*/true
    );

    std::apply(
        [&](auto &&...field) {
            (
                [&] {
                    using FieldType = std::remove_reference_t<decltype(conf.*field.member)>;
                    auto val = ResolveDottedKey<JsonAccessor, FieldType>(j, field.config_key);
                    if (val.has_value()) {
                        conf.*field.member = *val;
                    }
                }(),
                ...
            );
        },
        kConfigSchema
    );

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

void LoadFromYaml(const std::string &file_path, Config &conf) {
    std::ifstream ifs(file_path);
    if (!ifs) {
        throw std::runtime_error("Cannot open file: " + file_path);
    }
    const auto root = fkyaml::node::deserialize(ifs);

    std::apply(
        [&](auto &&...field) {
            (
                [&] {
                    using FieldType = std::remove_reference_t<decltype(conf.*field.member)>;
                    auto val = ResolveDottedKey<YamlAccessor, FieldType>(root, field.config_key);
                    if (val.has_value()) {
                        conf.*field.member = *val;
                    }
                }(),
                ...
            );
        },
        kConfigSchema
    );

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

} // namespace

// ──────────────────────────────────────────────
// 公開API
// ──────────────────────────────────────────────

void LoadFromFile(const std::string &file_path, Config &conf) {
    const auto ext = std::filesystem::path(file_path).extension().string();
    if (ext == ".toml") {
        LoadFromToml(file_path, conf);
    } else if (ext == ".json") {
        LoadFromJson(file_path, conf);
    } else if (ext == ".yaml" || ext == ".yml") {
        LoadFromYaml(file_path, conf);
    } else {
        throw std::runtime_error("Unsupported config file extension: " + ext);
    }
}

std::vector<std::string> ExpandManifest(const std::string &manifest_path) {
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

void LoadFromFiles(const std::vector<std::string> &file_paths, Config &conf) {
    for (const auto &path : file_paths) {
        const auto ext = std::filesystem::path(path).extension().string();
        if (ext == ".conf") {
            for (const auto &expanded : ExpandManifest(path)) {
                LoadFromFile(expanded, conf);
            }
        } else {
            LoadFromFile(path, conf);
        }
    }
}

std::string FindDefaultConfig() {
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
