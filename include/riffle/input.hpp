#pragma once

#include <memory>
#include <string>

#include "riffle/reader.hpp"

namespace riffle {

std::unique_ptr<ByteSource> open_input(const std::string& path);

}  // namespace riffle
