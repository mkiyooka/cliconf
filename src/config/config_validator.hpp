#pragma once

#include <string>

#include "config/config_loader.hpp"

namespace config {

/// マージ済み Config を検証する。不正な場合はエラーメッセージを返す。
/// 正常時は空文字列を返す。
std::string Validate(const Config &config);

} // namespace config
