#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "riffle/types.hpp"

namespace riffle {

std::expected<InferredSchema, std::string> parse_schema_json(std::string_view text);

std::expected<InferredSchema, std::string> load_schema_file(const std::string& path);

std::string write_schema_json(const InferredSchema& schema);

}
