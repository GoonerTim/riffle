#pragma once

#include "riffle/types.hpp"

namespace riffle {

// Run the full conversion pipeline for the given configuration.
ConvertStats convert(const Config& config);

}  // namespace riffle
