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
    GIT_REPOSITORY https://github.com/yourname/cliconf.git
    GIT_TAG v0.1.0   # 使用するタグまたはコミットハッシュ
)
FetchContent_MakeAvailable(cliconf)
```

`FetchContent_MakeAvailable` を呼ぶと、このプロジェクトが依存する以下のライブラリも自動的に取得されます。

| ライブラリ | 用途 |
| --- | --- |
| CLI11 | CLI オプション解析 |
| fmt | フォーマット出力 |
| toml++ | TOML 読み込み |
| nlohmann/json | JSONC 読み込み |
| fkYAML | YAML 読み込み |
| yyjson | JSON 構築 |
| csv-parser | CSV 読み込み |

### 2. ターゲットにリンクする

```cmake
# config-system を使う場合
target_link_libraries(my_app PRIVATE cliconf::config)

# utility / compat ヘッダを使う場合
target_link_libraries(my_app PRIVATE cliconf::cliconf)

# 両方使う場合
target_link_libraries(my_app PRIVATE
    cliconf::config
    cliconf::cliconf
)
```

---

## config-system の使い方

config-system は `Config` 構造体・スキーマ定義・`RunCli` がアプリ固有のため、
取り込み後にそれぞれ定義する必要があります。

### ステップ1: Config 構造体を定義する

取り込み側プロジェクトで `Config` と `kSubcommandMappings` を定義します。

```cpp
// my_project/include/config/config_loader.hpp

#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Config {
    std::string mode    = "default";
    int         timeout = 30;
};

// サブコマンドが不要な場合も宣言は残す（config_file_loader.cpp から参照される）
struct SubcommandMapping {
    const char *key;
    // SubcommandConfig Config::*member;  // サブコマンドなしの場合はコメントアウト
};
extern const SubcommandMapping kSubcommandMappings[];
extern const std::size_t kSubcommandMappingCount;
```

```cpp
// my_project/src/subcommand.cpp
#include "config/config_loader.hpp"

const SubcommandMapping kSubcommandMappings[] = {};
const std::size_t kSubcommandMappingCount = 0;
```

### ステップ2: スキーマを定義する

```cpp
// my_project/include/config/config_schema.hpp

#pragma once
#include <tuple>
#include <string_view>
#include "config/config_loader.hpp"

// FieldDescriptor は取り込んだ側のヘッダから使える
#include <config/config_schema_base.hpp>  // → このファイルは存在しない（後述の注意参照）

namespace config {

inline constexpr auto kConfigSchema = std::make_tuple(
    FieldDescriptor{"--mode",    "mode",    "Operation mode",     &Config::mode},
    FieldDescriptor{"--timeout", "timeout", "Timeout in seconds", &Config::timeout}
);

} // namespace config
```

> **注意**: `FieldDescriptor` は `include/config/config_schema.hpp` に定義されています。
> 取り込み側では `kConfigSchema` だけを再定義します。`FieldDescriptor` 自体は
> このプロジェクトのヘッダをインクルードして使います。
>
> ```cpp
> // FieldDescriptor の取得元
> #include <config/config_schema.hpp>  // → FieldDescriptor テンプレートが使える
> ```
>
> ただし、このヘッダには `kConfigSchema` の定義も含まれるため、
> **取り込み側では `config_schema.hpp` を丸ごとコピーして置き換えてください**。
> 詳細は [docs/config-system-porting.md](docs/config-system-porting.md) を参照。

### ステップ3: ConfigManager を組み込む

```cpp
// my_project/src/main.cpp

#include <cstdlib>
#include <vector>
#include <CLI/CLI.hpp>
#include <fmt/base.h>
#include "config/config_manager.hpp"
#include "config/config_validator.hpp"

int main(int argc, char *argv[]) {
    CLI::App app{"My Application"};

    std::vector<std::string> config_files;
    app.add_option("-c,--config", config_files, "Configuration file(s)");

    config::ConfigManager config_manager;
    config_manager.RegisterOptions(app);   // スキーマ由来の全オプションを登録

    try {
        app.parse(argc, argv);
    } catch (const CLI::CallForHelp &e) {
        std::exit(app.exit(e));
    }

    // CLI引数 > 後方ファイル > 前方ファイル > デフォルト値 の順で解決
    Config conf = config_manager.Resolve(config_files);

    const std::string error = config::Validate(conf);
    if (!error.empty()) {
        fmt::print(stderr, "Error: {}\n", error);
        return 1;
    }

    fmt::print("mode: {}\n", conf.mode);
    fmt::print("timeout: {}\n", conf.timeout);
    return 0;
}
```

### ステップ4: CMakeLists.txt でソースを追加する

```cmake
# サブコマンドマッピングの実体ファイルをビルドに含める
add_executable(my_app
    src/main.cpp
    src/subcommand.cpp   # kSubcommandMappings の実体定義
)

target_include_directories(my_app PRIVATE include)

target_link_libraries(my_app PRIVATE
    cliconf::config
)
```

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

// compat::expected（C++17 では tl::expected、C++23 では std::expected に自動切り替え）
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
