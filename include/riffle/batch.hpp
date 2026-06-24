#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "riffle/types.hpp"

namespace arrow {
class RecordBatch;
class Schema;
}  // namespace arrow

namespace riffle {

// Hot-path accumulator for one column: native values are appended per row and
// bulk-converted to an Arrow array once per batch. Only the buffer matching the
// column's type is populated (ints also hold TIMESTAMP micros and BOOL 0/1).
struct ColumnBuilder {
    ColumnSchema schema;
    std::vector<std::int64_t> ints;
    std::vector<double> doubles;
    std::vector<std::string> strings;
    std::vector<std::uint8_t> valid;  // 1 = present, 0 = null
    std::size_t null_count = 0;
};

// Set of column builders forming the current batch.
struct BatchBuilder {
    std::vector<ColumnBuilder> columns;
    std::size_t n_rows = 0;
};

// A frozen columnar batch ready for a Writer.
struct RecordBatch {
    std::shared_ptr<arrow::RecordBatch> data;
    std::size_t n_rows = 0;
};

// Build an Arrow schema mirroring an InferredSchema.
std::shared_ptr<arrow::Schema> arrow_schema_of(const InferredSchema& schema);

BatchBuilder make_batch_builder(const InferredSchema& schema);
std::expected<void, std::string> append_row(BatchBuilder& builder, const Row& row,
                                            TypeConflictPolicy policy = TypeConflictPolicy::WIDEN);
std::expected<RecordBatch, std::string> build_batch(BatchBuilder& builder);
std::expected<void, std::string> widen_column(ColumnBuilder& column, ColumnType to);

}  // namespace riffle
