#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "riffle/ports.hpp"
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
std::expected<RecordBatch, std::string> build_batch(BatchBuilder& builder);
std::expected<void, std::string> widen_column(ColumnBuilder& column, ColumnType to);

// Transparent hashing so a JSON key string_view can probe the index without
// allocating a std::string.
struct StringHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
};

// RowSink that writes each field straight into its column builder, looking up
// the target column by json_path in O(1) (no intermediate Row, no linear scan).
class BatchSink : public RowSink {
public:
    BatchSink(BatchBuilder& builder, TypeConflictPolicy policy);
    void begin_row() override {}
    std::expected<void, std::string> field(std::string_view path, CellValue value) override;
    std::expected<void, std::string> end_row() override;

    // True if a field failed irrecoverably (e.g. type conflict under ERROR
    // policy); such failures must abort regardless of the OnError line policy.
    bool fatal() const { return fatal_; }

private:
    BatchBuilder& builder_;
    TypeConflictPolicy policy_;
    std::unordered_map<std::string, std::size_t, StringHash, std::equal_to<>> index_;
    std::vector<std::uint8_t> seen_;
    bool fatal_ = false;
};

}  // namespace riffle
