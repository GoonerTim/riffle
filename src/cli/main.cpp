#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "riffle/args.hpp"
#include "riffle/constants.hpp"
#include "riffle/convert.hpp"
#include "riffle/schema_json.hpp"
#include "riffle/stats.hpp"
#include "riffle/version.hpp"

namespace {

bool has_flag(const std::vector<std::string>& args, std::string_view flag) {
    return std::ranges::find(args, flag) != args.end();
}

void print_help() {
    std::cout << "riffle " << riffle::version() << "\n"
              << "Streaming JSON-lines -> Parquet/columnar converter.\n\n"
              << "Usage: riffle <inputs...> -o <output> [options]\n"
              << "       riffle <inputs...> --print-schema\n"
              << "Use '-' as an input to read from stdin.\n\n"
              << "Options:\n"
              << "  -o, --output <path>        Output file (required unless --print-schema)\n"
              << "      --format <fmt>         parquet | columnar-raw (default parquet)\n"
              << "      --compression <codec>  none | snappy | zstd (default zstd)\n"
              << "      --schema <file>        JSON schema overriding inference\n"
              << "      --batch-rows <n>       Rows per batch (default 65536)\n"
              << "      --on-error <mode>      skip | abort | collect (default skip)\n"
              << "      --type-conflict <pol>  widen | string | error (default widen)\n"
              << "      --select <cols>        Keep only these columns (comma-separated)\n"
              << "      --exclude <cols>       Drop these columns (comma-separated)\n"
              << "      --rename <from=to,...> Rename output columns\n"
              << "      --print-schema         Print the inferred schema as JSON and exit\n"
              << "      --stats                Print conversion stats to stderr\n"
              << "  -h, --help                 Show this help and exit\n"
              << "      --version              Show version and exit\n";
}

int run_print_schema(const riffle::Config& config) {
    auto schema = riffle::infer_schema(config);
    if (!schema) {
        std::cerr << "riffle: schema: " << schema.error() << "\n";
        return riffle::EXIT_DATA;
    }
    std::cout << riffle::write_schema_json(*schema) << "\n";
    return riffle::EXIT_OK;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (has_flag(args, "-h") || has_flag(args, "--help")) {
        print_help();
        return riffle::EXIT_OK;
    }
    if (has_flag(args, "--version")) {
        std::cout << "riffle " << riffle::version() << "\n";
        return riffle::EXIT_OK;
    }
    auto config = riffle::parse_args(args);
    if (!config) {
        std::cerr << "riffle: usage: " << config.error() << "\n";
        return riffle::EXIT_USAGE;
    }
    if (config->print_schema) return run_print_schema(*config);
    auto stats = riffle::convert(*config);
    if (config->emit_stats) riffle::emit_stats(stats);
    return riffle::exit_code(stats);
}
