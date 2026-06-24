#pragma once

#include <cstddef>
#include <cstdint>

namespace riffle {

// Batch / memory tuning (see docs/riffle.md → Constants).
inline constexpr std::size_t DEFAULT_BATCH_ROWS = 65536;
inline constexpr std::size_t MAX_BATCH_BYTES = 256ull * 1024 * 1024;
inline constexpr std::size_t INFER_SAMPLE_ROWS = 10000;
inline constexpr std::size_t DEFAULT_ROW_GROUP_BYTES = 128ull * 1024 * 1024;
inline constexpr std::size_t READ_BUFFER_BYTES = 1ull * 1024 * 1024;
inline constexpr std::size_t MAX_LINE_BYTES = 64ull * 1024 * 1024;
inline constexpr int MAX_FLATTEN_DEPTH = 4;

// CLI exit codes.
inline constexpr int EXIT_OK = 0;
inline constexpr int EXIT_USAGE = 2;
inline constexpr int EXIT_INPUT = 3;
inline constexpr int EXIT_DATA = 4;
inline constexpr int EXIT_OUTPUT = 5;

}  // namespace riffle
