# 設定システム 実装解説

このドキュメントは設定システムの **実装の仕組み** を解説する。
利用方法は [config-system-guide.md](config-system-guide.md)、設計概要は [config-system.md](config-system.md)、
移植手順は [config-system-porting.md](config-system-porting.md) を参照。

---

## 全体の登場人物

| ファイル | 役割 |
| --- | --- |
| `include/config/config_loader.hpp` | `Config` / `PluginConfig` / `SubcommandConfig` / `SubcommandMapping` の型定義 |
| `include/config/config_schema.hpp` | `FieldDescriptor` テンプレートと `kConfigSchema` の定義 |
| `include/config/config_manager.hpp` | `ConfigManager` クラス宣言 |
| `src/config/config_manager.cpp` | `RegisterOptions` / `Resolve` の実装 |
| `src/config/config_file_loader.cpp` | TOML / JSON / YAML 読み込みの実装 |
| `src/command/subcommand.cpp` | `kSubcommandMappings` の実体定義とサブコマンド実行 |
| `src/command/cli.cpp` | `RunCli` — 各コンポーネントの組み立てと優先度マージ |

---

## コアとなるアイデア：FieldDescriptor と kConfigSchema

設定システムの中心は `config_schema.hpp` にある **スキーマ定義** だ。

```cpp
// include/config/config_schema.hpp

template <typename Owner, typename T>
struct FieldDescriptor {
    std::string_view cli_option;   // "--settings.value"
    std::string_view config_key;   // "settings.value"
    std::string_view description;  // CLIヘルプ文字列
    T Owner::*member;              // &Config::value（ポインタ・トゥ・メンバー）
};

inline constexpr auto kConfigSchema = std::make_tuple(
    FieldDescriptor{"--title",          "title",          "Application title", &Config::title},
    FieldDescriptor{"--settings.value", "settings.value", "Numeric value",     &Config::value}
);
```

`kConfigSchema` は `std::tuple` であり、各要素は `FieldDescriptor<Config, T>` だ。
`T` はフィールドごとに異なる（`std::string`、`std::uint64_t` など）ため、
**型の多相性を持つコレクションを静的に表現できる** のが `std::tuple` を使う理由である。

`T Owner::*member` はポインタ・トゥ・メンバーであり、`conf.*field.member` と記述することで
型安全にフィールドへアクセスできる。

新しいオプションを追加するときは **このタプルに1行追記するだけ** でよい。
CLIへの登録・設定ファイルの読み込み・優先度解決のすべてが自動的に動作する。

---

## std::apply によるタプル展開

`kConfigSchema` のループには `std::apply`（C++17）を使う。
`std::apply` はタプルの全要素を引数として展開しラムダへ渡す。
各要素が異なる型を持っていても **型情報を保持したまま** 処理できる。

以下が基本パターンだ。

```cpp
std::apply(
    [&](auto &&...field) {          // field は各 FieldDescriptor
        ([&] {
            // field.member の型を使った処理
        }(), ...);                  // fold expression で各 field に適用
    },
    kConfigSchema
);
```

`([&]{ ... }(), ...)` は **即時呼び出しラムダの fold expression** である。
ラムダを呼び出す式 `[&]{ ... }()` をカンマ演算子で展開することで、
各 `field` に対してラムダ本体を順番に実行する。

---

## CLI11 へのオプション登録（RegisterOptions）

```cpp
// src/config/config_manager.cpp

void ConfigManager::RegisterOptions(CLI::App &app) {
    std::size_t idx = 0;
    std::apply(
        [&](auto &&...field) {
            ([&] {
                auto *opt = app.add_option(
                    std::string(field.cli_option),
                    cli_values_.*field.member,   // パース結果の書き込み先
                    std::string(field.description)
                );
                const std::size_t i = idx++;
                opt->each([this, i](const std::string &) { cli_set_[i] = true; });
            }(), ...);
        },
        kConfigSchema
    );
}
```

ポイントは2つある。

**1. `cli_values_.*field.member` をパース先に指定する**

`cli_values_` は `Config` 型のメンバ変数だ。
`field.member` はポインタ・トゥ・メンバーなので、`cli_values_.*field.member` と書くことで
「`cli_values_` の中の `field` に対応するフィールド」を参照できる。
CLI11 はここへ直接パース結果を書き込む。

**2. `opt->each()` コールバックで「指定されたか」を記録する**

`opt->each()` はそのオプションが実際にパースされたときだけ呼ばれる。
これで `cli_set_[i] = true` をセットし、「ユーザーが明示的に指定したフィールド」を追跡する。
`cli_set_` がないと、デフォルト値（例: `0` や空文字列）と「未指定」が区別できない。

コンストラクタでは `cli_set_` をスキーマサイズ分だけ `false` で初期化している。

```cpp
ConfigManager::ConfigManager()
    : cli_values_{}, file_values_{}, cli_set_(kSchemaSize, false) {}
```

---

## 設定ファイルの読み込み（LoadFromFile）

ファイルパスの拡張子で形式を判別し、対応するローダーへ委譲する。

```cpp
// src/config/config_file_loader.cpp

void LoadFromFile(const std::string &file_path, Config &conf) {
    const auto ext = std::filesystem::path(file_path).extension().string();
    if      (ext == ".toml")                LoadFromToml(file_path, conf);
    else if (ext == ".json")                LoadFromJson(file_path, conf);
    else if (ext == ".yaml" || ext == ".yml") LoadFromYaml(file_path, conf);
    else throw std::runtime_error("Unsupported config file extension: " + ext);
}
```

各フォーマットのローダーは同じ構造を持つ。以下は TOML の例だ。

```cpp
void LoadFromToml(const std::string &file_path, Config &conf) {
    const auto tbl = toml::parse_file(file_path);

    // スキーマフィールドを自動マッピング
    std::apply(
        [&](auto &&...field) {
            ([&] {
                using FieldType = std::remove_reference_t<decltype(conf.*field.member)>;
                auto val = ResolveTomlKey<FieldType>(tbl, field.config_key);
                if (val.has_value()) {
                    conf.*field.member = *val;
                }
            }(), ...);
        },
        kConfigSchema
    );

    // plugins や subcommands は個別に処理（後述）
}
```

`using FieldType = std::remove_reference_t<decltype(conf.*field.member)>` で
**フィールドの型をコンパイル時に取得**し、`ResolveTomlKey<FieldType>` へ渡す。
これにより、型ごとの変換コードを書かずに済む。

戻り値は `std::optional<T>` なので、「キーが存在しない」と「値が `0` や空文字列」を区別できる。
`has_value()` が `false` なら何もしない（デフォルト値を維持する）。

### ドット区切りキーの解決

`"settings.value"` のようなドット区切り文字列をネストされたテーブルへ辿る処理は
反復ループで実装している。

```cpp
template <typename T>
std::optional<T> ResolveTomlKey(const toml::table &tbl, std::string_view dotted_key) {
    const toml::table *current = &tbl;
    std::string_view remaining = dotted_key;

    while (true) {
        const auto dot_pos = remaining.find('.');
        if (dot_pos == std::string_view::npos) {
            return (*current)[remaining].template value<T>();  // リーフ到達
        }
        const auto head = remaining.substr(0, dot_pos);  // "settings"
        remaining = remaining.substr(dot_pos + 1);       // "value"
        current = (*current)[head].as_table();
        if (current == nullptr) return std::nullopt;     // キーが存在しない
    }
}
```

JSON・YAML も同じアルゴリズムで実装されており、パーサー固有のAPIだけが異なる。

### スキーマ外フィールドの扱い

`plugins`（`std::vector<PluginConfig>`）や `subcommands` セクションは
複合型のため `kConfigSchema` の管理対象外だ。
各ローダー内で個別に読み込む。

```cpp
// plugins 配列（TOML の例）
if (const auto *arr = tbl["plugin"].as_array()) {
    for (const auto &el : *arr) {
        if (const auto *table = el.as_table()) {
            PluginConfig plugin;
            plugin.file   = (*table)["file"].value_or(std::string{});
            plugin.number = (*table)["number"].value_or(std::uint64_t{0});
            conf.plugins.push_back(plugin);
        }
    }
}

// subcommands セクション（kSubcommandMappings で自動化）
if (const auto *subs = tbl["subcommands"].as_table()) {
    for (std::size_t i = 0; i < kSubcommandMappingCount; ++i) {
        const auto &mapping = kSubcommandMappings[i];
        if (const auto *sub_tbl = (*subs)[mapping.key].as_table()) {
            (conf.*mapping.member).a = (*sub_tbl)["a"].value_or(0);
            (conf.*mapping.member).b = (*sub_tbl)["b"].value_or(0);
        }
    }
}
```

サブコマンドは `SubcommandMapping` 配列（`kSubcommandMappings`）を使ってループ処理することで、
サブコマンドを追加しても `config_file_loader.cpp` の修正は不要にしている。

---

## 優先度解決（Resolve）

```cpp
// src/config/config_manager.cpp

Config ConfigManager::Resolve(const std::string &explicit_config_path) {
    Config result{};  // デフォルト値で初期化

    // 設定ファイルを読み込む
    file_values_ = Config{};
    const std::string config_path =
        explicit_config_path.empty() ? FindDefaultConfig() : explicit_config_path;
    if (!config_path.empty()) {
        LoadFromFile(config_path, file_values_);
    }

    // デフォルト < ファイル
    std::apply([&](auto &&...field) {
        ((result.*field.member = file_values_.*field.member), ...);
    }, kConfigSchema);

    // ファイル < CLI（明示指定されたフィールドのみ）
    std::size_t idx = 0;
    std::apply(
        [&](auto &&...field) {
            ([&] {
                if (cli_set_[idx++]) {
                    result.*field.member = cli_values_.*field.member;
                }
            }(), ...);
        },
        kConfigSchema
    );

    return result;
}
```

処理は3段階だ。

1. `result` をデフォルト値で構築（`Config{}` はメンバーの初期値を使う）
2. `kConfigSchema` の全フィールドを `file_values_` で上書き（ファイルが未存在なら `file_values_` もデフォルト値のまま）
3. `cli_set_[i] == true` のフィールドのみ `cli_values_` で上書き

`Resolve()` が返すのは **スキーマ定義フィールド（`title`、`value`）のみ** である。
`plugins` と `SubcommandConfig` は `GetFileValues()` で `file_values_` を直接取得し、
呼び出し元の `cli.cpp` でマージする。

---

## cli.cpp でのマージと全体の流れ

```cpp
// src/command/cli.cpp — RunCli の抜粋

config::ConfigManager config_manager;
config_manager.RegisterOptions(app);     // スキーマ由来の全オプションを CLI11 に登録

SetCallbackSubcommands(app, config);     // add, subtract（callback方式）
SetGotSubcommands(app, config);          // multiply, divide（got_subcommand方式）

app.parse(argc, argv);                   // CLI11 がパース → cli_values_ に書き込まれる
ExecuteGotSubcommands(app, config);      // got_subcommand方式の即時実行

// スキーマフィールドの優先度解決
const Config resolved = config_manager.Resolve(config_file);
config.title = resolved.title;
config.value = resolved.value;

// スキーマ外フィールドはファイルの生値から取得
const Config &file_vals = config_manager.GetFileValues();
config.plugins = file_vals.plugins;

// サブコマンド: CLI で指定されていればその値、未指定ならファイル値
for (std::size_t i = 0; i < kSubcommandMappingCount; ++i) {
    const auto &m = kSubcommandMappings[i];
    if (!app.got_subcommand(m.key)) {
        config.*m.member = file_vals.*m.member;
    }
}
```

サブコマンドの優先度は `app.got_subcommand(m.key)` で判定する。
CLI から `add 3 5` と渡されれば `config.add` は CLI11 が書き込んだ値が優先される。
渡されなければ設定ファイルの `[subcommands.add]` の値が使われる。

---

## サブコマンドの2つの実装方式

このプロジェクトにはサブコマンドの実装方式が2種類あり、使い分けの参考として両方を示している。

| 方式 | 対象 | 実行タイミング |
| --- | --- | --- |
| callback方式 | `add`, `subtract` | `app.parse()` 内でそのまま実行 |
| got_subcommand方式 | `multiply`, `divide` | `app.parse()` の後に `got_subcommand()` で判定して実行 |

```cpp
// callback方式（SetCallbackSubcommands）
subcommand->callback([&config]() { ExecuteAdd(config.add); });

// got_subcommand方式（ExecuteGotSubcommands）
if (app.got_subcommand("multiply")) {
    ExecuteMultiply(config.multiply);
}
```

callback方式はコードが短く直感的だが、`parse()` 内で実行されるため
「パース後に設定ファイルの値をマージしてから実行したい」というケースには使えない。
そのような場合は got_subcommand方式を使う。

---

## 新しいスキーマフィールドを追加する手順

例として `--logging.level` オプション（型: `std::string`）を追加する。

**1. `Config` 構造体にフィールドを追加する（`config_loader.hpp`）**

```cpp
struct Config {
    std::string title = "title";
    std::uint64_t value = 10;
    std::string log_level = "info";  // 追加
    // ...
};
```

**2. `kConfigSchema` に1行追記する（`config_schema.hpp`）**

```cpp
inline constexpr auto kConfigSchema = std::make_tuple(
    FieldDescriptor{"--title",          "title",          "Application title", &Config::title},
    FieldDescriptor{"--settings.value", "settings.value", "Numeric value",     &Config::value},
    FieldDescriptor{"--logging.level",  "logging.level",  "Log level",         &Config::log_level}  // 追加
);
```

これだけで以下がすべて自動的に動作する。

- `--logging.level` という CLI オプションの登録
- `config.toml` の `[logging]` セクション `level` キーの読み込み
- CLI 指定 > ファイル値 > デフォルト値の優先度解決
- `ShowConfig()` での表示

対応する設定ファイルの記述：

```toml
[logging]
level = "debug"
```

---

## まとめ

このシステムの設計を貫く3つの仕組みを整理する。

**1. `FieldDescriptor` + `kConfigSchema`**

CLIオプション名・設定ファイルキー・メンバーポインタを1か所に集約している。
追加・変更時の修正箇所を「`Config` 構造体」と「`kConfigSchema`」の2ファイルに限定するのが設計の核心だ。

**2. `std::apply` + fold expression**

`std::tuple` に格納されたフィールドをコンパイル時に展開し、型情報を保持したまま処理する。
`using FieldType = std::remove_reference_t<decltype(conf.*field.member)>` で
各フィールドの型を動的に取得できるため、型変換コードを一切書かずに済む。

**3. `cli_set_` による明示指定の追跡**

`cli_set_[i]` が `true` のフィールドのみ CLI 値で上書きすることで、
「デフォルト値 `0` と未指定」を区別した正確な優先度解決を実現している。
