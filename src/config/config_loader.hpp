#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct PluginConfig {
    std::string file;
    std::uint64_t number = 0;
};

struct SubcommandConfig {
    int a = 0;
    int b = 0;
};

struct Config {
    std::string title = "title";
    std::uint64_t value = 10;
    std::vector<PluginConfig> plugins;
    SubcommandConfig add;
    SubcommandConfig subtract;
    SubcommandConfig multiply;
    SubcommandConfig divide;
};

/// サブコマンド名と Config メンバーポインタのマッピング
struct SubcommandMapping {
    const char *key;
    SubcommandConfig Config::*member;
};

/// サブコマンドマッピング配列（src/command/subcommand.cpp で定義）
extern const SubcommandMapping kSubcommandMappings[];
extern const std::size_t kSubcommandMappingCount;
