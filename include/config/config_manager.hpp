#pragma once

#include <string>
#include <tuple>
#include <vector>

#include <CLI/CLI.hpp>

#include "config/config_file_loader.hpp"

namespace config {

/**
 * @brief CLIオプションと設定ファイルを統合管理するクラス（ヘッダオンリー・テンプレート）
 *
 * @tparam Config    アプリ固有の設定構造体
 * @tparam Schema    kConfigSchema の型（std::tuple<FieldDescriptor<Config, T>...>）
 * @tparam ExtraLoader スキーマ外フィールド読み込み拡張（省略可: NoExtraLoader）
 *
 * 使い方:
 * @code
 * config::ConfigManager<MyConfig, decltype(kMySchema)> mgr(kMySchema);
 * mgr.RegisterOptions(app);
 * app.parse(argc, argv);
 * MyConfig conf = mgr.Resolve(config_paths);
 * @endcode
 */
template <typename Config, typename Schema, typename ExtraLoader = NoExtraLoader>
class ConfigManager {
public:
    explicit ConfigManager(Schema schema, ExtraLoader extra = ExtraLoader{})
        : schema_(std::move(schema)),
          extra_(std::move(extra)),
          cli_set_(std::tuple_size_v<Schema>, false) {}

    void RegisterOptions(CLI::App &app) {
        std::size_t idx = 0;
        std::apply(
            [&](auto &&...field) {
                (
                    [&] {
                        auto *opt = app.add_option(
                            std::string(field.cli_option), cli_values_.*field.member,
                            std::string(field.description)
                        );
                        const std::size_t i = idx++;
                        opt->each([this, i](const std::string & /*unused*/) { cli_set_[i] = true; });
                    }(),
                    ...
                );
            },
            schema_
        );
    }

    Config Resolve(const std::vector<std::string> &config_paths) {
        Config result{};

        file_values_ = Config{};
        std::vector<std::string> paths = config_paths;
        if (paths.empty()) {
            const std::string default_path = FindDefaultConfig();
            if (!default_path.empty()) {
                paths.push_back(default_path);
            }
        }
        if (!paths.empty()) {
            LoadFromFiles(paths, schema_, file_values_, extra_);
        }

        std::apply([&](auto &&...field) { ((result.*field.member = file_values_.*field.member), ...); }, schema_);

        std::size_t idx = 0;
        std::apply(
            [&](auto &&...field) {
                (
                    [&] {
                        if (cli_set_[idx++]) {
                            result.*field.member = cli_values_.*field.member;
                        }
                    }(),
                    ...
                );
            },
            schema_
        );

        return result;
    }

    const Config &GetFileValues() const { return file_values_; }

private:
    Schema schema_;
    ExtraLoader extra_;
    Config cli_values_;
    Config file_values_;
    std::vector<bool> cli_set_;
};

} // namespace config
