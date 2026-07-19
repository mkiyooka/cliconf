# 設定システム セットアップガイド

このドキュメントは config-system を既存プロジェクトで使えるようにするための
具体的な手順を説明する。FetchContent で取り込む方法とファイルコピーによる移植の
両方に対応している。

FetchContent による取り込みの概要は [README-library.md](../README-library.md) を参照。
設計の概要は [config-system.md](config-system.md)、利用方法は [config-system-guide.md](config-system-guide.md)、
実装の仕組みは [config-system-internals.md](config-system-internals.md) を参照。

---

## 必要な外部ライブラリ

移植先プロジェクトで以下のライブラリを導入しておく。

| ライブラリ | バージョン | 用途 |
| --- | --- | --- |
| CLI11 | 2.5.0 以上 | CLI オプション解析 |
| toml++ | 3.4.0 以上 | TOML ファイルの読み込み |
| nlohmann/json | 3.12.0 以上 | JSONC ファイルの読み込み |
| fkYAML | 0.4.2 以上 | YAML ファイルの読み込み |
| fmt | 10.0.0 以上 | フォーマット出力 |

CMake での導入例（FetchContent）:

```cmake
include(FetchContent)

FetchContent_Declare(CLI11
    URL https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.5.0.tar.gz
    URL_HASH SHA256=17e02b4cddc2fa348e5dbdbb582c59a3486fa2b2433e70a0c3bacb871334fd55
)
FetchContent_Declare(tomlplusplus
    URL https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.4.0.tar.gz
    URL_HASH SHA256=8517f65938a4faae9ccf8ebb36631a38c1cadfb5efa85d9a72e15b9e97d25155
)
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
)
FetchContent_Declare(fkYAML
    URL https://github.com/fktn-k/fkYAML/releases/download/v0.4.2/fkYAML.tgz
    URL_HASH SHA256=6fbdbd94094b467ea37a64ccfe042588353feda097b1a5348f8cd12b914ad2cd
)
FetchContent_Declare(fmt
    URL https://github.com/fmtlib/fmt/archive/refs/tags/12.0.0.tar.gz
    URL_HASH SHA256=aa3e8fbb6a0066c03454434add1f1fc23299e85758ceec0d7d2d974431481e40
)
FetchContent_MakeAvailable(CLI11 tomlplusplus nlohmann_json fkYAML fmt)

add_library(fkYAML_target INTERFACE)
target_include_directories(fkYAML_target INTERFACE ${fkyaml_SOURCE_DIR}/include)
```

---

## コピーするファイル

### そのままコピーして使うファイル（改変不要）

```text
include/config/
    config_manager.hpp

src/config/
    config_file_loader.hpp
    config_file_loader.cpp
    config_manager.cpp
    config_loader.cpp
    config_validator.cpp
```

`config_file_loader.cpp` は TOML / JSONC / YAML の全形式に対応しており、改変不要。
`config_validator.cpp` のバリデーションルールはアプリ固有だが、
関数シグネチャ（`Validate(const Config&) -> std::string`）はそのまま使える。

### コピーして改変するファイル

```text
include/config/
    config_loader.hpp   （Config 構造体）
    config_schema.hpp   （kConfigSchema のエントリ）

src/command/
    subcommand.cpp      （kSubcommandMappings の実体）
```

### 移植先で新規作成するファイル

`src/command/cli.cpp` に相当するファイルをアプリ固有ロジックとして新規作成する。
[ステップ5](#ステップ5-clicpp-相当のファイルを新規作成する) を参照。

---

## 移植手順

### ステップ1: ファイルをコピーする

上記のファイルを移植先プロジェクトの対応するディレクトリにコピーする。
ディレクトリ構成は以下を推奨するが、CMake の `target_include_directories` を調整すれば任意の構成でもよい。

```text
移植先プロジェクト/
    include/config/
        config_loader.hpp      ← コピー後に改変
        config_manager.hpp     ← そのまま
        config_schema.hpp      ← コピー後に改変
        config_validator.hpp   ← そのまま
    src/config/
        config_file_loader.hpp ← そのまま
        config_file_loader.cpp ← そのまま
        config_manager.cpp     ← そのまま
        config_loader.cpp      ← そのまま
        config_validator.cpp   ← バリデーションルールを改変
    src/command/
        subcommand.cpp         ← コピー後に改変
        cli.cpp                ← 新規作成
```

### ステップ2: CMakeLists.txt に config_lib を追加する

```cmake
add_library(config_lib STATIC
    src/config/config_loader.cpp
    src/config/config_file_loader.cpp
    src/config/config_manager.cpp
    src/config/config_validator.cpp
)
target_include_directories(config_lib PUBLIC include)
target_link_libraries(config_lib PUBLIC
    CLI11::CLI11
    tomlplusplus::tomlplusplus
    nlohmann_json::nlohmann_json
    fkYAML_target
    fmt::fmt
)
```

### ステップ3: Config 構造体を定義する（config_loader.hpp）

アプリのフィールドに合わせて `Config` 構造体を書き換える。
以下の点に注意する。

- `SubcommandMapping` と `kSubcommandMappings` の宣言はサブコマンドが不要な場合も **残す**（`config_file_loader.cpp` から参照されるため）
- サブコマンドが不要な場合は `SubcommandConfig` を削除し、後述の `kSubcommandMappings` を空にする

```cpp
// include/config/config_loader.hpp

struct Config {
    // アプリ固有のフィールドとデフォルト値を定義する
    std::string mode     = "default";
    int         timeout  = 30;
    std::string endpoint = "http://localhost:8080";
    // サブコマンドが必要な場合はここに SubcommandConfig を追加する
};

// サブコマンドマッピング（サブコマンドが不要な場合も宣言は残す）
struct SubcommandMapping {
    const char *key;
    SubcommandConfig Config::*member;  // サブコマンドがなければこの行ごと削除可
};
extern const SubcommandMapping kSubcommandMappings[];
extern const std::size_t kSubcommandMappingCount;
```

### ステップ4: スキーマを定義する（config_schema.hpp）

`Config` のフィールドに対応するエントリを `kConfigSchema` に記述する。

```cpp
// include/config/config_schema.hpp

inline constexpr auto kConfigSchema = std::make_tuple(
    FieldDescriptor{"--mode",     "mode",     "Operation mode",     &Config::mode},
    FieldDescriptor{"--timeout",  "timeout",  "Timeout in seconds", &Config::timeout},
    FieldDescriptor{"--endpoint", "endpoint", "Server endpoint",    &Config::endpoint}
);
```

`FieldDescriptor` の4引数の意味:

| 引数 | 例 | 説明 |
| --- | --- | --- |
| 第1引数 | `"--mode"` | CLI11 に登録するオプション名 |
| 第2引数 | `"mode"` | 設定ファイルのキー（ドット区切りでネスト可: `"server.host"` 等） |
| 第3引数 | `"Operation mode"` | CLI ヘルプに表示する説明文 |
| 第4引数 | `&Config::mode` | Config 構造体のメンバーポインタ |

ネストしたキーの例（設定ファイルで `[server]` セクションの `host` キーを参照する場合）:

```cpp
FieldDescriptor{"--server.host", "server.host", "Server hostname", &Config::server_host}
```

```toml
# 対応する TOML
[server]
host = "example.com"
```

### ステップ5: subcommand.cpp の実体を定義する

サブコマンドがない場合は配列を空にするだけでよい。

```cpp
// src/command/subcommand.cpp（サブコマンドなしの場合）
#include "config/config_loader.hpp"

const SubcommandMapping kSubcommandMappings[] = {};
const std::size_t kSubcommandMappingCount = 0;
```

サブコマンドがある場合は `Config` メンバーへのポインタを登録する。

```cpp
// src/command/subcommand.cpp（サブコマンドありの場合）
const SubcommandMapping kSubcommandMappings[] = {
    {"run",  &Config::run_args},
    {"init", &Config::init_args},
};
const std::size_t kSubcommandMappingCount = std::size(kSubcommandMappings);
```

### ステップ6: cli.cpp 相当のファイルを新規作成する

`ConfigManager` を組み立て、優先度解決・バリデーション・アプリ処理を記述する。

```cpp
// src/command/cli.cpp（最小構成の例）

#include <cstdlib>
#include <vector>

#include <CLI/CLI.hpp>
#include <fmt/base.h>

#include "config/config_manager.hpp"
#include "config/config_validator.hpp"

int RunCli(int argc, char *argv[]) {
    CLI::App app{"My Application"};

    // --config オプションの登録
    std::vector<std::string> config_files;
    app.add_option("-c,--config", config_files, "Configuration file(s)");

    // スキーマ由来の全オプションを CLI11 に登録
    config::ConfigManager config_manager;
    config_manager.RegisterOptions(app);

    try {
        app.parse(argc, argv);
    } catch (const CLI::CallForHelp &e) {
        std::exit(app.exit(e));
    }

    // スキーマフィールドの優先度解決（CLI > ファイル > デフォルト）
    Config conf = config_manager.Resolve(config_files);

    // スキーマ外フィールド（plugins 等）はファイルの生値から手動マージ
    const Config &file_vals = config_manager.GetFileValues();
    conf.plugins = file_vals.plugins;

    // バリデーション
    const std::string error = config::Validate(conf);
    if (!error.empty()) {
        fmt::print(stderr, "Error: {}\n", error);
        return 1;
    }

    // アプリ固有の処理
    // ...

    return 0;
}
```

### ステップ7: バリデーションルールを実装する（config_validator.cpp）

`Validate` 関数にアプリ固有のチェックを実装する。
エラーがない場合は空文字列、エラーがある場合はエラーメッセージを返す。

```cpp
// src/config/config_validator.cpp
#include "config/config_validator.hpp"
#include "config/config_loader.hpp"

namespace config {

std::string Validate(const Config &config) {
    if (config.mode.empty()) {
        return "mode must not be empty";
    }
    if (config.timeout <= 0) {
        return "timeout must be positive";
    }
    return {};  // エラーなし
}

} // namespace config
```

---

## 設定ファイルの対応関係

`kConfigSchema` のエントリと設定ファイルの対応を以下に示す。
ドット区切りのキーはネストとして解釈される。

```cpp
// スキーマ定義
FieldDescriptor{"--server.host", "server.host", "...", &Config::server_host},
FieldDescriptor{"--server.port", "server.port", "...", &Config::server_port},
FieldDescriptor{"--mode",        "mode",        "...", &Config::mode},
```

```toml
# TOML
mode = "production"

[server]
host = "example.com"
port = 8080
```

```json
// JSONC
{
    "mode": "production",
    "server": {
        "host": "example.com",
        "port": 8080
    }
}
```

```yaml
# YAML
mode: production
server:
  host: example.com
  port: 8080
```

---

## plugins のような配列フィールドを追加する場合

`std::vector<T>` のような複合型はスキーマ管理の対象外で、
設定ファイル専用フィールドとして `config_file_loader.cpp` 内で個別に読み込む。

**1. `Config` に配列フィールドを追加する**

```cpp
struct ItemConfig {
    std::string name;
    int priority = 0;
};

struct Config {
    // ...
    std::vector<ItemConfig> items;
};
```

**2. 各ローダーに読み込み処理を追加する**

`config_file_loader.cpp` の `LoadFromToml` / `LoadFromJson` / `LoadFromYaml` それぞれに
配列を読み込むコードを追加する。以下は TOML の例。

```cpp
// LoadFromToml 内に追加
if (const auto *arr = tbl["item"].as_array()) {
    conf.items.clear();
    for (const auto &el : *arr) {
        if (const auto *t = el.as_table()) {
            ItemConfig item;
            item.name     = (*t)["name"].value_or(std::string{});
            item.priority = static_cast<int>((*t)["priority"].value_or(int64_t{0}));
            conf.items.push_back(item);
        }
    }
}
```

**3. `cli.cpp` で手動マージする**

```cpp
const Config &file_vals = config_manager.GetFileValues();
conf.items = file_vals.items;  // ファイルから取得した配列をマージ
```

---

## チェックリスト

移植完了の確認事項。

- `Config` 構造体のフィールドとデフォルト値が定義されている
- `kConfigSchema` に全スキーマフィールドのエントリが追加されている
- `kSubcommandMappings` の実体が定義されている（サブコマンドなしの場合は空配列）
- `config_lib` が CMake でビルドできる
- `--help` でスキーマフィールドのオプションが表示される
- 設定ファイルの値が CLI 未指定時に反映される
- CLI 引数がファイル値より優先される
- バリデーションエラーが正しく報告される
