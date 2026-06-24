#pragma once

#include <optional>

#include "riffle/types.hpp"

namespace riffle {

// Port: a stream of parsed input rows. nullopt from next() means end of input.
class RowSource {
public:
    virtual ~RowSource() = default;
    virtual std::optional<Row> next() = 0;
};

}  // namespace riffle
