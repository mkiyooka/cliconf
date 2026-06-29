#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>

#include <stdexcept>
#include <string>

#include "config/config_loader.hpp"
#include "config/config_manager.hpp"
#include "config_file_loader.hpp"
#include "support/temp_file.hpp"

// ──────────────────────────────────────────────
// FindDefaultConfig のテスト
// ──────────────────────────────────────────────

TEST_CASE("FindDefaultConfig returns empty when no default file exists") {
    // config/default.* が存在しないことを前提とする
    // (テスト実行ディレクトリにデフォルトファイルがなければパス)
    if (!std::filesystem::exists("config/default.toml") && !std::filesystem::exists("config/default.json") &&
        !std::filesystem::exists("config/default.yaml")) {
        CHECK(config::FindDefaultConfig().empty());
    }
}

// ──────────────────────────────────────────────
// TOML 読み込みテスト
// ──────────────────────────────────────────────

TEST_CASE("LoadFromFile: TOML basic fields") {
    const TempFile temp_file("test_config.toml", R"(
title = "TestApp"

[settings]
value = 42
)");

    Config conf;
    config::LoadFromFile(temp_file.Str(), conf);

    CHECK(conf.title == "TestApp");
    CHECK(conf.value == 42);
    CHECK(conf.plugins.empty());
}

TEST_CASE("LoadFromFile: TOML with plugins") {
    const TempFile temp_file("test_config_plugins.toml", R"(
title = "WithPlugins"

[settings]
value = 10

[[plugin]]
file = "a.so"
number = 1

[[plugin]]
file = "b.so"
number = 2
)");

    Config conf;
    config::LoadFromFile(temp_file.Str(), conf);

    CHECK(conf.title == "WithPlugins");
    CHECK(conf.plugins.size() == 2);
    CHECK(conf.plugins[0].file == "a.so");
    CHECK(conf.plugins[0].number == 1);
    CHECK(conf.plugins[1].file == "b.so");
    CHECK(conf.plugins[1].number == 2);
}

TEST_CASE("LoadFromFile: TOML partial fields keep defaults") {
    // title のみ設定、value は未設定 → デフォルト値が維持される
    const TempFile temp_file("test_config_partial.toml", R"(
title = "PartialOnly"
)");

    Config conf; // デフォルト: title="title", value=10
    config::LoadFromFile(temp_file.Str(), conf);

    CHECK(conf.title == "PartialOnly");
    CHECK(conf.value == 10); // デフォルト値維持
}

// ──────────────────────────────────────────────
// JSONC 読み込みテスト
// ──────────────────────────────────────────────

TEST_CASE("LoadFromFile: JSON basic fields") {
    const TempFile temp_file("test_config.json", R"({
    // JSONC comment
    "title": "JsonApp",
    "settings": {
        "value": 99
    }
})");

    Config conf;
    config::LoadFromFile(temp_file.Str(), conf);

    CHECK(conf.title == "JsonApp");
    CHECK(conf.value == 99);
}

TEST_CASE("LoadFromFile: JSON with plugins") {
    const TempFile temp_file("test_config_plugins.json", R"({
    "title": "JsonPlugins",
    "settings": { "value": 5 },
    "plugin": [
        { "file": "x.so", "number": 7 },
        { "file": "y.so", "number": 8 }
    ]
})");

    Config conf;
    config::LoadFromFile(temp_file.Str(), conf);

    CHECK(conf.title == "JsonPlugins");
    CHECK(conf.plugins.size() == 2);
    CHECK(conf.plugins[0].file == "x.so");
    CHECK(conf.plugins[1].number == 8);
}

// ──────────────────────────────────────────────
// YAML 読み込みテスト
// ──────────────────────────────────────────────

TEST_CASE("LoadFromFile: YAML basic fields") {
    const TempFile temp_file("test_config.yaml", R"(
title: YamlApp
settings:
  value: 77
)");

    Config conf;
    config::LoadFromFile(temp_file.Str(), conf);

    CHECK(conf.title == "YamlApp");
    CHECK(conf.value == 77);
}

TEST_CASE("LoadFromFile: YAML with plugins") {
    const TempFile temp_file("test_config_plugins.yaml", R"(
title: YamlPlugins
settings:
  value: 3
plugin:
  - file: p.so
    number: 11
  - file: q.so
    number: 22
)");

    Config conf;
    config::LoadFromFile(temp_file.Str(), conf);

    CHECK(conf.title == "YamlPlugins");
    CHECK(conf.plugins.size() == 2);
    CHECK(conf.plugins[0].file == "p.so");
    CHECK(conf.plugins[1].number == 22);
}

// ──────────────────────────────────────────────
// 未対応拡張子エラー
// ──────────────────────────────────────────────

TEST_CASE("LoadFromFile: unsupported extension throws") {
    const TempFile temp_file("test_config.xml", "<config/>");
    Config conf;
    CHECK_THROWS_AS(config::LoadFromFile(temp_file.Str(), conf), std::runtime_error);
}

// ──────────────────────────────────────────────
// ConfigManager::Resolve の優先度テスト
// ──────────────────────────────────────────────

TEST_CASE("ConfigManager::Resolve: file values override defaults") {
    const TempFile temp_file("test_resolve.toml", R"(
title = "FromFile"

[settings]
value = 55
)");

    config::ConfigManager manager;
    const Config conf = manager.Resolve({temp_file.Str()});

    CHECK(conf.title == "FromFile");
    CHECK(conf.value == 55);
}

TEST_CASE("ConfigManager::Resolve: CLI values override file values") {
    const TempFile temp_file("test_resolve_cli.toml", R"(
title = "FileTitle"

[settings]
value = 100
)");

    CLI::App app{"test"};
    config::ConfigManager manager;
    std::string config_path = temp_file.Str();
    app.add_option("-c,--config", config_path, "Config file");
    manager.RegisterOptions(app);
    // vector形式でパース (プログラム名を除いた引数のみ)
    app.parse(std::vector<std::string>{"--settings.value=999"});

    const Config conf = manager.Resolve({config_path});

    CHECK(conf.title == "FileTitle"); // ファイルから
    CHECK(conf.value == 999);         // CLIが優先
}

TEST_CASE("ConfigManager::Resolve: empty path uses defaults") {
    config::ConfigManager manager;
    const Config conf = manager.Resolve({});

    // デフォルト値
    CHECK(conf.title == "title");
    CHECK(conf.value == 10);
    CHECK(conf.plugins.empty());
}
