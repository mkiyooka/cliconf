#pragma once

#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include "config/config_loader.hpp"

namespace config {

/**
 * @brief CLIオプションと設定ファイルを統合管理するクラス
 *
 * CLI11への全オプション登録と、設定ファイル読み込みを担い、
 * 優先度 (CLI引数 > 設定ファイル > デフォルト値) に従って最終的な Config を生成する。
 *
 * 使い方:
 * @code
 * ConfigManager mgr;
 * std::vector<std::string> config_paths;
 * app.add_option("-c,--config", config_paths, "Configuration file(s)");
 * mgr.RegisterOptions(app);   // スキーマ由来の全オプションを登録
 * app.parse(argc, argv);      // CLI11がパース
 * Config conf = mgr.Resolve(config_paths);  // 優先度解決
 * @endcode
 */
class ConfigManager {
public:
    ConfigManager();

    /**
     * @brief スキーマに基づいてCLI::Appにオプションを登録する
     * @param app CLI11のアプリケーションオブジェクト
     */
    void RegisterOptions(CLI::App &app);

    /**
     * @brief スキーマ定義フィールドを優先度に従って解決して返す
     *
     * スキーマ (config_schema.hpp) に定義されたフィールドのみを解決する。
     * plugins や SubcommandConfig などスキーマ外フィールドは GetFileValues() で取得すること。
     *
     * 優先度: CLI引数 > 後方ファイル > 前方ファイル > デフォルト値
     *
     * @param config_paths 設定ファイルパスのリスト (.conf マニフェストを含む)。空の場合はデフォルト探索
     * @return スキーマフィールドが解決済みの Config
     * @throws std::runtime_error 複数のデフォルト設定ファイルが存在する場合
     * @throws std::runtime_error 設定ファイルのパースに失敗した場合
     */
    Config Resolve(const std::vector<std::string> &config_paths);

    /**
     * @brief 設定ファイルから読み込んだ生の値を返す
     *
     * Resolve() 呼び出し後に有効になる。
     * スキーマ外フィールド (plugins, SubcommandConfig 等) をユーザ側でマージするために使用する。
     *
     * @return 設定ファイルから読み込んだ Config（ファイル未指定時はデフォルト値）
     */
    const Config &GetFileValues() const { return file_values_; }

private:
    Config cli_values_;         ///< CLI11のパース結果書き込み先
    Config file_values_;        ///< 設定ファイルから読み込んだ値（Resolve() 後に有効）
    std::vector<bool> cli_set_; ///< 各スキーマフィールドがCLIで明示指定されたか
};

} // namespace config
