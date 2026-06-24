#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "riffle/types.hpp"

namespace riffle {

class RowSink {
public:
    virtual ~RowSink() = default;
    virtual void begin_row() = 0;
    virtual std::expected<void, std::string> field(std::string_view path, CellValue value) = 0;
    virtual std::expected<void, std::string> end_row() = 0;
};

}
