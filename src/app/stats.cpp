#include "riffle/stats.hpp"

#include <iostream>

#include "riffle/constants.hpp"

namespace riffle {

void emit_stats(const ConvertStats& stats) {
    std::cerr << "riffle: rows_read=" << stats.rows_read << " rows_written=" << stats.rows_written
              << " rows_skipped=" << stats.rows_skipped << " bytes_out=" << stats.bytes_out
              << " elapsed_ms=" << stats.elapsed_ms << "\n";
}

int exit_code(const ConvertStats& stats) {
    return stats.final_state == PipelineState::DONE ? EXIT_OK : EXIT_DATA;
}

}
