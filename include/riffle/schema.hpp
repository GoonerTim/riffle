#pragma once

#include <cstddef>
#include <expected>
#include <map>
#include <span>
#include <string>
#include <vector>

#include "riffle/ports.hpp"
#include "riffle/types.hpp"

namespace riffle {

// Reduce a column's observed types to a single type per the conflict policy.
std::expected<ColumnType, std::string> resolve_type_conflict(std::span<const ColumnType> seen,
                                                             TypeConflictPolicy policy);

// RowSink that observes field types over a sample to infer an output schema.
// Columns appear in first-seen order; per path the distinct observed types are
// resolved by the conflict policy. ISO-8601 strings are inferred as TIMESTAMP.
class InferenceSink : public RowSink {
public:
    explicit InferenceSink(TypeConflictPolicy policy) : policy_(policy) {}
    void begin_row() override {}
    std::expected<void, std::string> field(std::string_view path, CellValue value) override;
    std::expected<void, std::string> end_row() override;
    InferredSchema schema() const;

private:
    TypeConflictPolicy policy_;
    std::vector<std::string> order_;
    std::map<std::string, std::vector<ColumnType>> seen_;
    std::size_t rows_ = 0;
};

// Overlay an explicit schema onto an inferred one (replace by name, else append).
InferredSchema merge_override(InferredSchema inferred, const InferredSchema& override);

}  // namespace riffle
