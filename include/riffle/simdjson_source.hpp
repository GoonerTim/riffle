#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "riffle/ports.hpp"
#include "riffle/reader.hpp"
#include "riffle/types.hpp"

namespace riffle {

// RowSource adapter: parses JSON-lines from a LineReader via simdjson.
// Malformed lines are skipped and recorded in errors().
class SimdjsonSource : public RowSource {
public:
    explicit SimdjsonSource(LineReader& reader);
    ~SimdjsonSource();
    std::optional<Row> next() override;
    const std::vector<ParseError>& errors() const { return errors_; }

private:
    struct Impl;
    LineReader& reader_;
    std::unique_ptr<Impl> impl_;
    std::vector<ParseError> errors_;
    std::size_t line_no_ = 0;
};

}  // namespace riffle
