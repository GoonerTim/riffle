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
}

namespace riffle {

struct ColumnBuilder {
    ColumnSchema schema;
    std::vector<std::int64_t> ints;
    std::vector<double> doubles;
    std::vector<std::string> strings;
    std::vector<std::uint8_t> valid;
    std::size_t null_count = 0;
};

struct BatchBuilder {
    std::vector<ColumnBuilder> columns;
    std::size_t n_rows = 0;
    std::size_t byte_size = 0;
};

struct RecordBatch {
    std::shared_ptr<arrow::RecordBatch> data;
    std::size_t n_rows = 0;
};

std::shared_ptr<arrow::Schema> arrow_schema_of(const InferredSchema& schema);

BatchBuilder make_batch_builder(const InferredSchema& schema);
std::expected<RecordBatch, std::string> build_batch(BatchBuilder& builder);
std::expected<void, std::string> widen_column(ColumnBuilder& column, ColumnType to);

struct StringHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
};

class BatchSink : public RowSink {
public:
    BatchSink(BatchBuilder& builder, TypeConflictPolicy policy);
    void begin_row() override {}
    std::expected<void, std::string> field(std::string_view path, CellValue value) override;
    std::expected<void, std::string> end_row() override;

    bool fatal() const { return fatal_; }

private:
    BatchBuilder& builder_;
    TypeConflictPolicy policy_;
    std::unordered_map<std::string, std::size_t, StringHash, std::equal_to<>> index_;
    std::vector<std::uint8_t> seen_;
    bool fatal_ = false;
};

}
