#include <cstdlib>
#include <vector>

#include <CLI/CLI.hpp>
#include <fmt/base.h>
#include <fmt/format.h>

#include "command/subcommand.hpp"
#include "config/config_manager.hpp"
#include "config/config_schema.hpp"
#include "config/config_validator.hpp"

namespace {

void ShowConfig(const Config &conf) {
    std::apply(
        [&](auto &&...field) { ([&] { fmt::print("{}: {}\n", field.config_key, conf.*field.member); }(), ...); },
        config::kConfigSchema
    );
    for (const auto &p : conf.plugins) {
        fmt::print("plugin: file={}, number={}\n", p.file, p.number);
    }
    for (std::size_t i = 0; i < kSubcommandMappingCount; ++i) {
        const auto &m = kSubcommandMappings[i];
        fmt::print("subcommands.{}: a={}, b={}\n", m.key, (conf.*m.member).a, (conf.*m.member).b);
    }
}

void BuildApp(CLI::App &app, std::vector<std::string> &config_files, config::ConfigManager &config_manager, Config &config) {
    app.add_option("-c,--config", config_files, "Configuration file(s)");
    config_manager.RegisterOptions(app);
    SetCallbackSubcommands(app, config);
    SetGotSubcommands(app, config);
}

void MergeNonSchemaFields(Config &config, const CLI::App &app, const Config &file_values) {
    config.plugins = file_values.plugins;

    for (std::size_t i = 0; i < kSubcommandMappingCount; ++i) {
        const auto &m = kSubcommandMappings[i];
        if (!app.got_subcommand(m.key)) {
            config.*m.member = file_values.*m.member;
        }
    }
}

} // namespace

int RunCli(int argc, char *argv[]) {
    CLI::App app{"Command line parser demonstration with different subcommand styles"};
    argv = app.ensure_utf8(argv);

    std::vector<std::string> config_files;
    config::ConfigManager config_manager;
    Config config;

    BuildApp(app, config_files, config_manager, config);

    try {
        app.parse(argc, argv);
    } catch (const CLI::CallForHelp &e) {
        std::exit(app.exit(e));
    }

    const Config resolved = config_manager.Resolve(config_files);
    config.title = resolved.title;
    config.value = resolved.value;

    MergeNonSchemaFields(config, app, config_manager.GetFileValues());

    const std::string error = config::Validate(config);
    if (!error.empty()) {
        fmt::print(stderr, "Error: {}\n", error);
        return 1;
    }

    ExecuteGotSubcommands(app, config);

    ShowConfig(config);

    return 0;
}
