#pragma once

#include <expected>
#include <span>
#include <string>

#include "riffle/types.hpp"

namespace riffle {

std::expected<Config, std::string> parse_args(std::span<const std::string> args);

}
