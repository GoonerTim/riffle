#pragma once

#include "riffle/types.hpp"

namespace riffle {

// Print a one-line summary of the run to stderr.
void emit_stats(const ConvertStats& stats);

// Map the run's final state to a CLI exit code.
int exit_code(const ConvertStats& stats);

}  // namespace riffle
