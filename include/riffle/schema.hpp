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

std::expected<ColumnType, std::string> resolve_type_conflict(std::span<const ColumnType> seen,
                                                             TypeConflictPolicy policy);

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

InferredSchema merge_override(InferredSchema inferred, const InferredSchema& override);

}
