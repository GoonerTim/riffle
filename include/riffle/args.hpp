#pragma once

#include <expected>
#include <span>
#include <string>

#include "riffle/types.hpp"

namespace riffle {

// Parse CLI arguments (without argv[0]) into a validated Config.
std::expected<Config, std::string> parse_args(std::span<const std::string> args);

}  // namespace riffle
