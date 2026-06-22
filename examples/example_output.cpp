#include <cstdint>
#include <vector>

#include <fmt/format.h>

#include "template_cli_app_cpp/logging/logger_factory.hpp"
#include "template_cli_app_cpp/output/output_context.hpp"
#include "template_cli_app_cpp/recording/recorder_factory.hpp"
#include "template_cli_app_cpp/recording/recorder_manager.hpp"
#include "template_cli_app_cpp/utility/yyjson_wrapper.hpp"

namespace {

enum class OutputModule : std::uint8_t { kResultsCsv, kResultsJson };

// Logger と DataRecorder を使った出力サンプル
//
// Logger のフォーマット:
//   MakeConsole() の pattern 引数で spdlog のパターン文字列を指定する。
//   空文字列（デフォルト）の場合は spdlog 標準書式が使われる。
//   パターン記号: %Y=年 %m=月 %d=日 %H=時 %M=分 %S=秒 %e=ミリ秒
//                 %n=ロガー名 %l=レベル(小文字) %L=レベル(1文字) %v=メッセージ
//
// DataRecorder のフォーマット:
//   - MakeCsvFile()      : ヘッダ行を自動出力。Write() で行を追記する。
//   - MakeJsonLinesFile(): 1行1JSON (NDJSON)。Write() に JSON 文字列を渡す。
void RunOutputSample(output::OutputContext<OutputModule> &output_context) {
    logging::Logger &logger = output_context.GetLogger();
    recording::DataRecorder &csv_recorder = output_context.GetRecorders()[OutputModule::kResultsCsv];
    recording::DataRecorder &json_recorder = output_context.GetRecorders()[OutputModule::kResultsJson];

    constexpr double kInput = 3.5;
    constexpr int kStart = 1;
    constexpr int kCount = 5;
    constexpr int kDivisor = 7;
    constexpr int kTargetValue = 15;

    logger.Log(logging::LogLevel::Info, "=== output sample start ===");
    logger.Log(
        logging::LogLevel::Info,
        fmt::format(
            "input={}, start={}, count={}, divisor={}, target_value={}", kInput, kStart, kCount, kDivisor, kTargetValue
        )
    );

    constexpr double kDoubled = kInput * 2.0;
    std::vector<int> sequence;
    sequence.reserve(kCount);
    for (int i = 0; i < kCount; ++i) {
        sequence.push_back(kStart + i);
    }
    const int remainder = kTargetValue % kDivisor;

    logger.Log(logging::LogLevel::Debug, fmt::format("doubled({}) = {}", kInput, kDoubled));
    logger.Log(logging::LogLevel::Debug, fmt::format("sequence({}, {}) size={}", kStart, kCount, sequence.size()));
    logger.Log(logging::LogLevel::Debug, fmt::format("remainder({}) = {}", kTargetValue, remainder));

    // CSV 出力
    csv_recorder.Enable();
    csv_recorder.Write("{},{:.6f},{}", "doubled", kDoubled, remainder);
    csv_recorder.Write("{},{:.6f},{}", "single", kInput, 2);
    csv_recorder.Flush();
    csv_recorder.Disable();

    // JSON Lines 出力（1行1JSON / NDJSON）
    json_recorder.Enable();

    utility::JsonBuilder builder;

    auto inputs_object = builder.AddNested("inputs");
    builder.AddToNested(inputs_object, "input", kInput);
    builder.AddToNested(inputs_object, "start", kStart);
    builder.AddToNested(inputs_object, "count", kCount);
    builder.AddToNested(inputs_object, "divisor", kDivisor);
    builder.AddToNested(inputs_object, "target_value", kTargetValue);

    auto results_object = builder.AddNested("results");
    builder.AddToNested(results_object, "doubled", kDoubled);
    builder.AddToNested(results_object, "remainder", remainder);
    builder.Add("sequence", sequence);

    json_recorder.Write("{}", builder.Serialize(/* pretty = */ false));
    json_recorder.Flush();
    json_recorder.Disable();

    logger.Log(logging::LogLevel::Info, "=== output sample end ===");
}

} // namespace

int main() {
    // Logger: pattern 引数でタイムスタンプ・ロガー名・レベルを含む書式を指定する。
    auto logger = logging::LoggerFactory::MakeConsole(
        "app", logging::LogLevel::Debug, "[%Y-%m-%d %H:%M:%S.%e][%n][%^%l%$]%v"
    );

    // DataRecorder: CSV と JSON Lines (NDJSON) の 2 形式を使い分ける例。
    recording::RecorderManager<OutputModule> recorder_manager;
    recorder_manager.RegisterRecorder(
        OutputModule::kResultsCsv,
        recording::RecorderFactory::MakeCsvFile("results_csv", "output/results.csv", "name,value,remainder")
    );
    recorder_manager.RegisterRecorder(
        OutputModule::kResultsJson,
        recording::RecorderFactory::MakeJsonLinesFile("results_json", "output/results.jsonl")
    );
    output::OutputContext<OutputModule> output_context(*logger, recorder_manager);

    RunOutputSample(output_context);

    return 0;
}
