#pragma once

#include <expected>
#include <string>

#include "riffle/types.hpp"

namespace riffle {

ConvertStats convert(const Config& cfg);

std::expected<InferredSchema, std::string> infer_schema(const Config& cfg);

}
