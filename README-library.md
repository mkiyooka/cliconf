# cliconf — ライブラリとして使う

このドキュメントは `cliconf` を **FetchContent で取り込んでライブラリとして使う** 場合の手引きです。

テンプレートとして Fork してアプリを作る場合は [README.md](README.md) を参照してください。

---

## 提供するターゲット

| CMake ターゲット | 内容 |
| --- | --- |
| `cliconf::config` | config-system（TOML / JSONC / YAML 読み込み・CLI11 連携） |
| `cliconf::cliconf` | utility / compat ヘッダ（yyjson ラッパー・expected / span） |

---

## 前提

- CMake 3.21 以上（`PROJECT_IS_TOP_LEVEL` を使用）
- C++17 以上

---

## 取り込み方（FetchContent）

### 1. CMakeLists.txt に追加する

```cmake
include(FetchContent)

FetchContent_Declare(cliconf
    GIT_REPOSITORY https://github.com/mkiyooka/cliconf.git
    GIT_TAG main   # 使用するタグまたはコミットハッシュ
)
FetchContent_MakeAvailable(cliconf)
```

`FetchContent_MakeAvailable` を呼ぶと、このプロジェクトが依存する以下のライブラリも自動的に取得されます
（`cliconf::cliconf` のみ使う場合は yyjson / csv-parser 等の一部のみが対象です。詳細は次項）。

| ライブラリ | 用途 | 導入済みか判定するターゲット名 |
| --- | --- | --- |
| CLI11 | CLI オプション解析 | `CLI11::CLI11` |
| fmt | フォーマット出力 | `fmt::fmt` |
| toml++ | TOML 読み込み | `tomlplusplus::tomlplusplus` |
| nlohmann/json | JSONC 読み込み | `nlohmann_json::nlohmann_json` |
| fkYAML | YAML 読み込み | `fkYAML_target`（cliconf 内部の合成ターゲット） |
| yyjson | JSON 構築 | `yyjson` |
| csv-parser | CSV 読み込み | `csv` |
| tl::expected | エラーハンドリング | `tl::expected` |
| tcb::span | `std::span` バックポート | `tcb_span_target`（cliconf 内部の合成ターゲット） |

### 依存ライブラリの競合を避ける（排他制御）

取り込み側プロジェクトが上記と同じライブラリを別バージョンで既に `FetchContent` や
`find_package` 済みの場合、cliconf は **自前での取得をスキップし、既存のターゲットを
そのまま使います**。判定は「上表のターゲット名が `FetchContent_MakeAvailable(cliconf)`
の時点で既に存在するかどうか」で行われるため、cliconf を取り込む**前**に自分のバージョンを
確定させておくだけで、意図したバージョンが優先されます。

```cmake
include(FetchContent)

# 先に自分のバージョンの fmt を確定させておく
FetchContent_Declare(fmt
    URL https://github.com/fmtlib/fmt/archive/refs/tags/11.0.2.tar.gz
    URL_HASH SHA256=...
)
FetchContent_MakeAvailable(fmt)   # ここで fmt::fmt が生成される

# この時点で fmt::fmt が既に存在するため、
# cliconf 側の fmt 取得はスキップされる
FetchContent_Declare(cliconf
    GIT_REPOSITORY https://github.com/mkiyooka/cliconf.git
    GIT_TAG main
)
FetchContent_MakeAvailable(cliconf)
```

逆に順序を守らず `cliconf` を先に取り込むと、cliconf が指定したバージョンが
先に確定してしまい、後から行う自分の `FetchContent_Declare` は無視されるので注意してください
（`FetchContent` は同名コンテンツを一度取得すると、以後の再宣言を無視する仕様のため）。

なお `CLI11` / `fmt` / `toml++` / `nlohmann/json` / `fkYAML` はサンプルアプリ
（`cliconf::config` 用）専用の依存で、`FetchContent_MakeAvailable(cliconf)` のように
サブプロジェクトとして取り込んだ場合は取得自体行われません。`cliconf::cliconf`
（ヘッダオンリー utility/compat）のみが必要な場合は yyjson / csv-parser / tl::expected /
tcb::span のみを意識すれば十分です。

### 2. ターゲットにリンクする

```cmake
# config-system を使う場合
target_link_libraries(my_app PRIVATE cliconf::config)

# utility / compat ヘッダを使う場合
target_link_libraries(my_app PRIVATE cliconf::cliconf)
```

---

## config-system の使い方

config-system は `ConfigManager<Config, Schema, ExtraLoader>` というテンプレートクラスです。
利用側で `Config` 構造体・スキーマ・ExtraLoader を定義して渡すことで、
TOML / JSONC / YAML の読み込みと CLI11 オプション登録が自動化されます。

### ステップ1: Config 構造体を定義する

```cpp
// my_project/include/config/config_loader.hpp
#pragma once
#include <string>

struct Config {
    std::string mode    = "default";
    int         timeout = 30;
};
```

### ステップ2: スキーマを定義する

`FieldDescriptor` は `cliconf/field_descriptor.hpp` で提供されます。

```cpp
// my_project/include/config/config_schema.hpp
#pragma once
#include <tuple>
#include "cliconf/field_descriptor.hpp"
#include "config/config_loader.hpp"

namespace config {

inline constexpr auto kConfigSchema = std::make_tuple(
    FieldDescriptor{"--mode",    "mode",    "Operation mode",     &Config::mode},
    FieldDescriptor{"--timeout", "timeout", "Timeout in seconds", &Config::timeout}
);

} // namespace config
```

`FieldDescriptor` の4引数の意味:

| 引数 | 例 | 説明 |
| --- | --- | --- |
| 第1引数 | `"--mode"` | CLI11 に登録するオプション名 |
| 第2引数 | `"mode"` | 設定ファイルのキー（ドット区切りでネスト可） |
| 第3引数 | `"Operation mode"` | CLI ヘルプに表示する説明文 |
| 第4引数 | `&Config::mode` | Config 構造体のメンバーポインタ |

### ステップ3: ExtraLoader を定義する（スキーマ外フィールドがある場合）

`std::vector` などスキーマで自動マッピングできないフィールドは `ExtraLoader` で読み込みます。
不要な場合は `config::NoExtraLoader`（デフォルト）をそのまま使えます。

`ExtraLoader` が実装できるメソッドは4種類です。

| メソッド | 呼び出しタイミング |
| --- | --- |
| `LoadToml(const toml::table&, Config&)` | `.toml` 読み込み時、スキーマ自動マッピングの直後 |
| `LoadJson(const nlohmann::json&, Config&)` | `.json` 読み込み時、スキーマ自動マッピングの直後 |
| `LoadYaml(const fkyaml::node&, Config&)` | `.yaml/.yml` 読み込み時、スキーマ自動マッピングの直後 |
| `LoadCsv(Config&)` | `Resolve()` でスキーマ・CLI 解決が完了した後（全ファイル読み込み後） |

```cpp
// my_project/include/config/config_schema.hpp（続き）
#include <fkYAML/node.hpp>
#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>
#include <cliconf/utility/csv_wrapper.hpp>
#include "config/config_loader.hpp"

namespace config {

struct MyExtraLoader {
    void LoadToml(const toml::table &tbl, Config &conf) const {
        // 例: [[item]] 配列を読み込む
        if (const auto *arr = tbl["item"].as_array()) {
            conf.items.clear();
            for (const auto &el : *arr) {
                if (const auto *t = el.as_table()) {
                    conf.items.push_back((*t)["name"].value_or(std::string{}));
                }
            }
        }
    }
    void LoadJson(const nlohmann::json & /*j*/, Config & /*conf*/) const {}
    void LoadYaml(const fkyaml::node & /*root*/, Config & /*conf*/) const {}

    // CSV ファイルパスが conf.register_csv に入っていれば読み込む。
    // Resolve() 後に呼ばれるため、CLI / 設定ファイルで指定したパスが確定済み。
    void LoadCsv(Config &conf) const {
        if (conf.register_csv.empty()) return;
        utility::CsvReader reader(conf.register_csv);
        auto result = reader.ReadFiltered(
            [](const csv::CSVRow &row) { return row["enabled"].get<int>() == 1; },
            {"address", "value"}
        );
        if (result) {
            conf.register_records = *result;
        }
    }
};

} // namespace config
```

スキーマ外フィールドが全くない場合は ExtraLoader の定義は不要です。

### ステップ4: ConfigManager を組み込む

```cpp
// my_project/src/main.cpp
#include <cstdlib>
#include <vector>
#include <CLI/CLI.hpp>
#include <fmt/base.h>
#include "config/config_file_loader.hpp"
#include "config/config_manager.hpp"
#include "config/config_schema.hpp"

int main(int argc, char *argv[]) {
    CLI::App app{"My Application"};

    std::vector<std::string> config_files;
    app.add_option("-c,--config", config_files, "Configuration file(s)");

    // ConfigManager にスキーマと ExtraLoader を渡す
    config::ConfigManager<Config, decltype(config::kConfigSchema)> manager{config::kConfigSchema};
    manager.RegisterOptions(app);

    try {
        app.parse(argc, argv);
    } catch (const CLI::CallForHelp &e) {
        std::exit(app.exit(e));
    }

    // CLI引数 > 後方ファイル > 前方ファイル > デフォルト値 の順で解決
    Config conf = manager.Resolve(config_files);

    fmt::print("mode: {}\n", conf.mode);
    fmt::print("timeout: {}\n", conf.timeout);
    return 0;
}
```

ExtraLoader を使う場合:

```cpp
config::ConfigManager<Config, decltype(config::kConfigSchema), config::MyExtraLoader> manager{
    config::kConfigSchema, config::MyExtraLoader{}
};
```

`Resolve()` 後にスキーマ外フィールドを取得するには `GetFileValues()` を使います:

```cpp
Config conf = manager.Resolve(config_files);
const Config &file_vals = manager.GetFileValues();
conf.items = file_vals.items;  // ExtraLoader が読み込んだ値
```

### ステップ5: CMakeLists.txt でビルドする

```cmake
add_executable(my_app src/main.cpp)

target_include_directories(my_app PRIVATE include)

target_link_libraries(my_app PRIVATE cliconf::config)
```

---

## 設定ファイルの書き方

ドット区切りのキーはネストとして解釈されます。

```cpp
FieldDescriptor{"--server.host", "server.host", "Server hostname", &Config::server_host},
```

```toml
# TOML
[server]
host = "example.com"
```

```json
// JSONC
{ "server": { "host": "example.com" } }
```

```yaml
# YAML
server:
  host: example.com
```

---

## CSV を設定の一部として扱う

`ExtraLoader::LoadCsv` を使うと、CSV ファイルのパスを「設定値のひとつ」としてスキーマに登録し、
`Resolve()` 後に自動的に CSV を読み込む仕組みを作れます。

### 設計の考え方

```text
kConfigSchema で定義
  └── conf.register_csv = "registers.csv"  ← TOML / CLI で上書き可能

Resolve() 完了後
  └── ExtraLoader::LoadCsv(conf) が呼ばれる
        └── conf.register_csv を読んで conf.register_records に書き込む
```

CSV のパスがスキーマフィールドなので、設定ファイルや CLI オプション（`--register-csv`）で
実行時に切り替えられます。

### 実装例

#### Config 構造体に CSV パスとベクタフィールドを追加する

```cpp
struct Config {
    std::string mode        = "default";
    std::string register_csv;              // CSV ファイルパス
    std::vector<double> register_records;  // LoadCsv で書き込まれる
};
```

#### スキーマに CSV パスフィールドを追加する

```cpp
inline constexpr auto kConfigSchema = std::make_tuple(
    FieldDescriptor{"--mode",         "mode",         "Operation mode",        &Config::mode},
    FieldDescriptor{"--register-csv", "register_csv", "Path to register CSV",  &Config::register_csv}
);
```

```toml
# config/default.toml
mode = "prod"
register_csv = "data/registers.csv"
```

#### ExtraLoader::LoadCsv で読み込む

```cpp
struct MyExtraLoader {
    void LoadCsv(Config &conf) const {
        if (conf.register_csv.empty()) return;
        utility::CsvReader reader(conf.register_csv);
        auto result = reader.ReadFiltered(
            [](const csv::CSVRow &row) { return row["enabled"].get<int>() == 1; },
            {"value"}
        );
        if (result) {
            conf.register_records.clear();
            for (double v : *result) {
                conf.register_records.push_back(v);
            }
        }
    }
    void LoadToml(const toml::table &, Config &) const {}
    void LoadJson(const nlohmann::json &, Config &) const {}
    void LoadYaml(const fkyaml::node &, Config &) const {}
};
```

#### 呼び出し側

```cpp
config::ConfigManager<Config, decltype(config::kConfigSchema), config::MyExtraLoader> manager{
    config::kConfigSchema, config::MyExtraLoader{}
};
manager.RegisterOptions(app);
app.parse(argc, argv);

Config conf = manager.Resolve(config_files);
// この時点で conf.register_records には CSV から読み込んだ値が入っている
```

CSV ファイルパスが空（未指定）のときは `LoadCsv` 内でスキップするため、
CSV が不要な実行モードではパスを指定しなければ何も起きません。

---

## utility / compat の使い方

config-system を使わず utility や compat だけ使う場合は `cliconf::cliconf` のみリンクします。

```cmake
target_link_libraries(my_app PRIVATE cliconf::cliconf)
```

```cpp
// yyjson ラッパー
#include <cliconf/utility/yyjson_wrapper.hpp>

utility::JsonBuilder builder;
auto obj = builder.AddNested("data");
builder.Add(obj, "value", 42);
fmt::print("{}\n", builder.Build());

// CSV ラッパー
#include <cliconf/utility/csv_wrapper.hpp>

utility::CsvReader reader("data.csv");
auto result = reader.ReadFiltered(
    [](const csv::CSVRow &row) { return row["flag"].get<int>() == 1; },
    {"value_a", "value_b"}
);

// compat::expected（C++23 で std::expected に自動切り替え）
#include <cliconf/compat/expected.hpp>

compat::expected<int, std::string> safe_divide(int a, int b) {
    if (b == 0) return compat::unexpected(std::string{"division by zero"});
    return a / b;
}
```

---

## 詳細ドキュメント

- [docs/config-system.md](docs/config-system.md) — config-system の設計概要
- [docs/config-system-guide.md](docs/config-system-guide.md) — フィールド追加・設定ファイルの書き方
- [docs/config-system-porting.md](docs/config-system-porting.md) — 取り込み後の具体的なセットアップ手順（ステップバイステップ）
- [docs/config-system-internals.md](docs/config-system-internals.md) — 実装の仕組み（FieldDescriptor・std::apply 等）
