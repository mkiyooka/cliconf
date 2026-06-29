#pragma once

#include <string>
#include <vector>

#include "config/config_loader.hpp"

namespace config {

/**
 * @brief 設定ファイルを読み込んでConfigに書き込む
 *
 * ファイル拡張子から形式を自動判別する。
 * スキーマ (config_schema.hpp) のconfig_keyに従って自動マッピングする。
 * ファイルに存在するキーのみConfigフィールドを上書きし、存在しないキーはそのまま。
 *
 * @param file_path 読み込むファイルパス (.toml / .json / .yaml / .yml)
 * @param conf 書き込み先のConfig
 * @throws std::runtime_error ファイルが開けない場合またはパース失敗時
 * @throws std::runtime_error 未対応の拡張子の場合
 */
void LoadFromFile(const std::string &file_path, Config &conf);

/**
 * @brief 複数の設定ファイルを順に読み込んでConfigに書き込む（後勝ちマージ）
 *
 * .conf 拡張子のファイルはマニフェストとして扱い、中身のパスリストを展開する。
 * マニフェスト内の相対パスは .conf ファイルの親ディレクトリからの相対パスとして解決する。
 *
 * @param file_paths 読み込むファイルパスのリスト
 * @param conf 書き込み先のConfig
 */
void LoadFromFiles(const std::vector<std::string> &file_paths, Config &conf);

/**
 * @brief マニフェストファイル (.conf) を読み込んでファイルパスリストに展開する
 *
 * 1行1パスのプレーンテキスト形式。# 以降はコメント。空行は無視。
 * 相対パスは .conf ファイルの親ディレクトリからの相対パスとして解決する。
 *
 * @param manifest_path マニフェストファイルのパス
 * @return 展開されたファイルパスのリスト
 * @throws std::runtime_error ファイルが開けない場合
 */
std::vector<std::string> ExpandManifest(const std::string &manifest_path);

/**
 * @brief デフォルト設定ファイルを探索して返す
 *
 * 探索パス: config/default.toml, config/default.json, config/default.yaml
 * 複数同時に存在する場合はエラー。
 *
 * @return 見つかったファイルパス。見つからなければ空文字列。
 * @throws std::runtime_error 複数のデフォルト設定ファイルが同時に存在する場合
 */
std::string FindDefaultConfig();

} // namespace config
