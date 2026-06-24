#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "riffle/types.hpp"

namespace arrow {
class ArrayBuilder;
class RecordBatch;
class Schema;
}  // namespace arrow

namespace riffle {

// Hot-path accumulator for one column (mutable, reused across batches).
struct ColumnBuilder {
    ColumnSchema schema;
    std::shared_ptr<arrow::ArrayBuilder> builder;
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
std::expected<void, std::string> append_row(BatchBuilder& builder, const Row& row);
std::expected<RecordBatch, std::string> build_batch(BatchBuilder& builder);
std::expected<void, std::string> widen_column(ColumnBuilder& column, ColumnType to);

}  // namespace riffle
