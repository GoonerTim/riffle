#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace riffle {

// Parse an ISO-8601 UTC timestamp into microseconds since the Unix epoch.
// Accepts "YYYY-MM-DDTHH:MM:SS[.fff...][Z]"; returns nullopt on bad input.
std::optional<std::int64_t> parse_timestamp_us(std::string_view text);

// True if the text parses as an ISO-8601 timestamp.
bool looks_like_timestamp(std::string_view text);

}  // namespace riffle
