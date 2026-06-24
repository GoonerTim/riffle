#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace riffle {

std::optional<std::int64_t> parse_timestamp_us(std::string_view text);

bool looks_like_timestamp(std::string_view text);

}
