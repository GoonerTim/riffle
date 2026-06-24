#pragma once

#include <expected>
#include <memory>
#include <string>
#include <string_view>

#include "riffle/ports.hpp"

namespace riffle {

// Parses one JSON-lines record at a time with the simdjson on-demand API,
// pushing flattened leaf fields straight into a RowSink (no intermediate Row).
// Nested objects are flattened to MAX_FLATTEN_DEPTH; deeper values stay raw text.
class JsonParser {
public:
    JsonParser();
    ~JsonParser();

    // Parse one object line into the sink (begin_row/field*/end_row).
    // Returns an error string for malformed input or a non-object top level.
    std::expected<void, std::string> parse(std::string_view line, RowSink& sink);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace riffle
