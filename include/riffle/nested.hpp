#pragma once

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "riffle/batch.hpp"

namespace arrow {
class Schema;
class RecordBatchBuilder;
}  // namespace arrow

namespace riffle {

struct NestedType;

struct NestedField {
    std::string name;
    std::shared_ptr<NestedType> type;
};

// Inferred tree type for native nested output: objects map to Arrow struct,
// arrays to Arrow list, scalars to the matching primitive.
struct NestedType {
    enum class Kind { Null, Int, Double, Bool, String, Struct, List };
    Kind kind = Kind::Null;
    std::vector<NestedField> fields;      // Kind::Struct
    std::shared_ptr<NestedType> element;  // Kind::List
};

std::expected<NestedType, std::string> infer_nested_schema(const std::vector<std::string>& sample);
std::shared_ptr<arrow::Schema> nested_arrow_schema(const NestedType& top);

class NestedBuilder {
public:
    static std::expected<std::unique_ptr<NestedBuilder>, std::string> make(const NestedType& top);
    ~NestedBuilder();

    std::expected<void, std::string> append_line(std::string_view line);
    std::size_t rows() const { return rows_; }
    std::expected<RecordBatch, std::string> flush();

private:
    NestedBuilder(std::unique_ptr<arrow::RecordBatchBuilder> builder, NestedType top);

    std::unique_ptr<arrow::RecordBatchBuilder> builder_;
    NestedType top_;
    std::size_t rows_ = 0;
};

}  // namespace riffle
