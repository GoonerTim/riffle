#pragma once

#include "riffle/types.hpp"

namespace riffle {

// Validate/normalize a draft value structure, or throw std::invalid_argument
// when an invariant is violated.
ColumnSchema make_ColumnSchema(ColumnSchema draft);
InferredSchema make_InferredSchema(InferredSchema draft);
Config make_Config(Config draft);
ParseError make_ParseError(ParseError draft);
ConvertStats make_ConvertStats();

}  // namespace riffle
