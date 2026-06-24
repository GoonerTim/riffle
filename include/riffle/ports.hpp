#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "riffle/types.hpp"

namespace riffle {

// Port: a push-based receiver of one parsed row's flattened fields. A parser
// calls begin_row(), then field() for each leaf, then end_row(). Avoiding an
// intermediate Row object lets sinks write straight into their target (column
// builders or a type accumulator) with no per-row materialization.
class RowSink {
public:
    virtual ~RowSink() = default;
    virtual void begin_row() = 0;
    virtual std::expected<void, std::string> field(std::string_view path, CellValue value) = 0;
    virtual std::expected<void, std::string> end_row() = 0;
};

}  // namespace riffle
