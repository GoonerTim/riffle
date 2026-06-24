#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "riffle/types.hpp"

namespace riffle {

// Parse a JSON schema document {"columns":[{name,type,nullable?,json_path?},...]}
// into an InferredSchema, or return an error message.
std::expected<InferredSchema, std::string> parse_schema_json(std::string_view text);

// Read and parse a schema JSON file by path.
std::expected<InferredSchema, std::string> load_schema_file(const std::string& path);

}  // namespace riffle
