# 設定システム利用ガイド

設計・実装の詳細は [config-system.md](config-system.md) を参照。

---

## 基本的な使い方

### 1. Config 構造体にフィールドを追加する

`include/config/config_loader.hpp` にフィールドとデフォルト値を定義する。

```cpp
struct Config {
    std::string title   = "title";
    std::uint64_t value = 10;
    std::string log_level = "info";  // 追加
};
```

### 2. スキーマに1行追加する

`include/config/config_schema.hpp` の `kConfigSchema` に対応エントリを追加する。

```cpp
inline constexpr auto kConfigSchema = std::make_tuple(
    FieldDescriptor{"--title",          "title",          "Application title", &Config::title},
    FieldDescriptor{"--settings.value", "settings.value", "Numeric value",     &Config::value},
    FieldDescriptor{"--logging.level",  "logging.level",  "Log level",         &Config::log_level}  // 追加
);
```

これだけで以下が自動的に有効になる。

- CLI11 への `--logging.level` オプション登録
- TOML / JSONC / YAML の `logging.level` キーからの読み込み
- `cli.cpp` の `ShowConfig` での表示（`kConfigSchema` をループするため自動対応）

### 3. アプリに組み込む

```cpp
#include "config/config_manager.hpp"

CLI::App app{"My App"};

std::vector<std::string> config_files;
app.add_option("-c,--config", config_files, "Configuration file(s)");

config::ConfigManager mgr;
mgr.RegisterOptions(app);   // スキーマ由来の全オプションを登録

app.parse(argc, argv);

// スキーマフィールドを解決（CLI引数 > 後方ファイル > 前方ファイル > デフォルト値）
Config conf = mgr.Resolve(config_files);

// スキーマ外フィールド（plugins 等）はファイルの生値から取得
const Config &file_vals = mgr.GetFileValues();
conf.plugins = file_vals.plugins;
```

### 4. Config の値を使う

```cpp
fmt::print("title: {}\n", conf.title);
fmt::print("value: {}\n", conf.value);

for (const auto& p : conf.plugins) {
    fmt::print("plugin: {} ({})\n", p.file, p.number);
}
```

---

## 設定ファイルの書き方

### TOML（推奨）

```toml
title = "My Application"

[settings]
value = 42

[logging]
level = "warn"

[[plugin]]
file = "a.so"
number = 1
```

### JSONC

```json
{
    "title": "My Application",
    "settings": {
        "value": 42
    },
    "logging": {
        "level": "warn"
    }
}
```

### YAML

```yaml
title: My Application
settings:
    value: 42
logging:
    level: warn
```

---

## CLI からの実行例

```sh
# デフォルト設定ファイル（config/default.toml 等）を自動探索
./build/cmd

# 設定ファイルを明示指定
./build/cmd -c config/example.toml

# 複数ファイルを指定（後勝ちマージ）
./build/cmd -c config/base.toml -c config/override.toml

# マニフェストで組み合わせを定義
./build/cmd -c config/experiment.conf

# CLI 引数で設定ファイルの値を上書き（優先度: CLI > 後方ファイル > 前方ファイル > デフォルト）
./build/cmd -c config/example.toml --title="Override Title" --settings.value=99

# 設定ファイルなしで CLI 引数のみ使用
./build/cmd --title="CLI Only"
```

---

## 設定ファイルの自動探索

`-c / --config` を省略した場合、以下のパスを順に探索する。

- `config/default.toml`
- `config/default.json`
- `config/default.yaml`

複数のファイルが同時に存在する場合はエラーになる。

```text
Error: Multiple default config files found: config/default.toml config/default.json
```

---

## 複数設定ファイルの指定

`--config`（`-c`）は複数回指定できる。後に指定したファイルが前のファイルを上書きする（後勝ちマージ）。

```sh
# base.toml の値を override.toml で部分的に上書き
./build/cmd -c config/base.toml -c config/override.toml add 1 2
```

各ファイルには全フィールドを書く必要はない。ファイルに存在するキーのみが上書きされる。

---

## マニフェストファイル (.conf)

`.conf` 拡張子のファイルはマニフェスト（ファイルリスト）として扱われ、
中に列挙したファイルを順に読み込む。

```conf
# experiment/case_001.conf
# 1行1パス。# 以降はコメント。空行は無視。

app_base.toml
modules/moduleA1.toml
modules/moduleB2.toml
```

相対パスはマニフェストファイルの親ディレクトリからの相対パスとして解決される。

```sh
# マニフェスト1つで複数ファイルの組み合わせを指定
./build/cmd -c experiment/case_001.conf add 1 2

# マニフェストと直接指定を混在させることもできる
./build/cmd -c experiment/case_001.conf -c config/extra.toml add 1 2
```

### 典型的な使い方

実験条件ごとにマニフェストを作成し、共通設定とモジュール設定を組み合わせる。

```text
- config/
    - app_base.toml
    - modules/
        - moduleA1.toml
        - moduleA2.toml
        - moduleB1.toml
        - moduleB2.toml
- experiment/
    - case_001.conf     … app_base + moduleA1 + moduleB2
    - case_002.conf     … app_base + moduleA2 + moduleB1
```

---

## plugins フィールドの扱い

`std::vector<PluginConfig>` はスキーマ管理の対象外で、設定ファイル専用フィールド。
CLI からの指定には対応していない。

```cpp
struct PluginConfig {
    std::string file;
    std::uint64_t number;
};
```

```toml
[[plugin]]
file = "a.so"
number = 1

[[plugin]]
file = "b.so"
number = 2
```

---

## サブコマンド別設定

各サブコマンド（`add`, `subtract`, `multiply`, `divide`）には、
設定ファイルでデフォルトのオペランドを指定できる。

### TOML

```toml
[subcommands.add]
a = 10
b = 5

[subcommands.multiply]
a = 3
b = 7
```

### JSONC

```json
{
    "subcommands": {
        "add":      { "a": 10, "b": 5 },
        "multiply": { "a": 3,  "b": 7 }
    }
}
```

### YAML

```yaml
subcommands:
  add:
    a: 10
    b: 5
  multiply:
    a: 3
    b: 7
```

### 優先度

```text
CLI 引数 > 設定ファイルの subcommands 値 > デフォルト値 (0)
```

サブコマンドを CLI で指定した場合（例: `./build/cmd add 3 4`）、
CLI の値が使われ、設定ファイルの値は無視される。
サブコマンドが指定されなかった場合は設定ファイルの値が使われる。

---

## 他プロジェクトへの移植

config-system はファイルをコピーして移植できる。
`Config` 構造体とスキーマ定義がアプリ固有のため、コピー後に改変が必要なファイルと、
汎用ロジックのためそのまま使えるファイルに分かれる。

### 必要な外部ライブラリ

| ライブラリ | 用途 |
| --- | --- |
| CLI11 | CLI オプション解析 |
| toml++ | TOML ファイルの読み込み |
| nlohmann/json | JSONC ファイルの読み込み |
| fkYAML | YAML ファイルの読み込み |
| fmt | フォーマット出力 |

### コピーするファイル

#### そのままコピーして使うファイル（改変不要）

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

`config_validator.cpp` のバリデーションルールはアプリ固有だが、
関数シグネチャ（`Validate(const Config&) -> std::string`）はそのまま使える。

#### コピーして改変するファイル

**`include/config/config_loader.hpp`** — `Config` 構造体の定義

`Config` のフィールドを移植先アプリ用に書き換える。
`SubcommandMapping` と `kSubcommandMappings` の宣言はサブコマンドが不要な場合も残す（`config_file_loader.cpp` から参照されるため）。

```cpp
// 移植先で書き換える例
struct Config {
    std::string mode = "default";   // アプリ固有のフィールドに置き換える
    int retry_count  = 3;
    // SubcommandConfig は不要なら削除してよい
};
```

**`include/config/config_schema.hpp`** — `kConfigSchema` のエントリ

`Config` のフィールドに合わせてエントリを書き換える。

```cpp
inline constexpr auto kConfigSchema = std::make_tuple(
    FieldDescriptor{"--mode",         "mode",         "Operation mode", &Config::mode},
    FieldDescriptor{"--retry-count",  "retry_count",  "Retry count",    &Config::retry_count}
);
```

**`src/command/subcommand.cpp`** — `kSubcommandMappings` の実体定義

サブコマンドが変われば書き換える。サブコマンドが不要な場合は配列を空にする。

```cpp
const SubcommandMapping kSubcommandMappings[] = {};
const std::size_t kSubcommandMappingCount = 0;
```

### 移植先で新規作成するファイル

`src/command/cli.cpp` に相当するファイルをアプリ固有ロジックとして新規作成する。
[src/command/cli.cpp](../src/command/cli.cpp) の `BuildApp` / `MergeNonSchemaFields` / `RunCli` を参考に、
移植先の `Config` フィールドに合わせて実装する。

```cpp
// 最小構成の例
int RunCli(int argc, char* argv[]) {
    CLI::App app{"My App"};

    std::vector<std::string> config_files;
    app.add_option("-c,--config", config_files, "Configuration file(s)");

    config::ConfigManager mgr;
    mgr.RegisterOptions(app);

    try {
        app.parse(argc, argv);
    } catch (const CLI::CallForHelp& e) {
        std::exit(app.exit(e));
    }

    Config conf = mgr.Resolve(config_files);

    // スキーマ外フィールド（plugins 等）はファイルの生値から手動マージ
    const Config& file_vals = mgr.GetFileValues();
    conf.plugins = file_vals.plugins;

    const std::string error = config::Validate(conf);
    if (!error.empty()) {
        fmt::print(stderr, "Error: {}\n", error);
        return 1;
    }

    // アプリ固有の処理
    return 0;
}
```

### ファイル一覧まとめ

| 対応 | ファイル |
| --- | --- |
| そのままコピー | `include/config/config_manager.hpp` |
| そのままコピー | `src/config/config_file_loader.hpp` |
| そのままコピー | `src/config/config_file_loader.cpp` |
| そのままコピー | `src/config/config_manager.cpp` |
| そのままコピー | `src/config/config_loader.cpp` |
| そのままコピー | `src/config/config_validator.cpp` |
| コピーして改変 | `include/config/config_loader.hpp`（`Config` 構造体） |
| コピーして改変 | `include/config/config_schema.hpp`（`kConfigSchema` のエントリ） |
| コピーして改変 | `src/command/subcommand.cpp`（`kSubcommandMappings` の実体） |
| 新規作成 | `cli.cpp` 相当（`BuildApp` / `RunCli`） |
