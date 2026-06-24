#pragma once

#include "riffle/types.hpp"

namespace riffle {

void emit_stats(const ConvertStats& stats);

int exit_code(const ConvertStats& stats);

}
