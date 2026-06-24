#include <iostream>
#include <string>
#include <vector>

#include "riffle/args.hpp"
#include "riffle/constants.hpp"
#include "riffle/convert.hpp"
#include "riffle/stats.hpp"

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    auto config = riffle::parse_args(args);
    if (!config) {
        std::cerr << "riffle: usage: " << config.error() << "\n";
        return riffle::EXIT_USAGE;
    }
    auto stats = riffle::convert(*config);
    if (config->emit_stats) riffle::emit_stats(stats);
    return riffle::exit_code(stats);
}
