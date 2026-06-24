#include "riffle/nested.hpp"

#include <arrow/api.h>
#include <arrow/record_batch.h>
#include <simdjson.h>

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace riffle {
namespace {

namespace dom = simdjson::dom;
using Kind = NestedType::Kind;

std::expected<void, std::string> check(const arrow::Status& status) {
    if (status.ok()) return {};
    return std::unexpected(status.ToString());
}

NestedType scalar(Kind kind) {
    NestedType type;
    type.kind = kind;
    return type;
}

NestedType infer_value(dom::element value);
NestedType unify(const NestedType& a, const NestedType& b);

NestedType infer_object(dom::object obj) {
    NestedType type = scalar(Kind::Struct);
    for (auto [key, val] : obj) {
        type.fields.push_back({std::string(key), std::make_shared<NestedType>(infer_value(val))});
    }
    return type;
}

NestedType infer_array(dom::array arr) {
    NestedType element = scalar(Kind::Null);
    for (dom::element item : arr) element = unify(element, infer_value(item));
    NestedType type = scalar(Kind::List);
    type.element = std::make_shared<NestedType>(std::move(element));
    return type;
}

NestedType infer_value(dom::element value) {
    switch (value.type()) {
        case dom::element_type::OBJECT:
            return infer_object(value.get_object());
        case dom::element_type::ARRAY:
            return infer_array(value.get_array());
        case dom::element_type::STRING:
            return scalar(Kind::String);
        case dom::element_type::DOUBLE:
            return scalar(Kind::Double);
        case dom::element_type::INT64:
        case dom::element_type::UINT64:
            return scalar(Kind::Int);
        case dom::element_type::BOOL:
            return scalar(Kind::Bool);
        default:
            return scalar(Kind::Null);
    }
}

NestedField* find_field(std::vector<NestedField>& fields, std::string_view name) {
    for (auto& field : fields)
        if (field.name == name) return &field;
    return nullptr;
}

NestedType unify_scalar(Kind a, Kind b) {
    if (a == b) return scalar(a);
    if ((a == Kind::Int && b == Kind::Double) || (a == Kind::Double && b == Kind::Int))
        return scalar(Kind::Double);
    return scalar(Kind::String);
}

NestedType unify_struct(const NestedType& a, const NestedType& b) {
    NestedType merged = a;
    for (const auto& field : b.fields) {
        if (auto* existing = find_field(merged.fields, field.name))
            existing->type = std::make_shared<NestedType>(unify(*existing->type, *field.type));
        else
            merged.fields.push_back(field);
    }
    return merged;
}

NestedType unify(const NestedType& a, const NestedType& b) {
    if (a.kind == Kind::Null) return b;
    if (b.kind == Kind::Null) return a;
    if (a.kind == Kind::Struct && b.kind == Kind::Struct) return unify_struct(a, b);
    if (a.kind == Kind::List && b.kind == Kind::List) {
        NestedType type = scalar(Kind::List);
        type.element = std::make_shared<NestedType>(unify(*a.element, *b.element));
        return type;
    }
    if (a.kind == Kind::Struct || a.kind == Kind::List || b.kind == Kind::Struct ||
        b.kind == Kind::List)
        return scalar(Kind::String);
    return unify_scalar(a.kind, b.kind);
}

std::shared_ptr<arrow::DataType> arrow_type_of(const NestedType& type);

std::shared_ptr<arrow::DataType> struct_type_of(const NestedType& type) {
    arrow::FieldVector fields;
    for (const auto& field : type.fields)
        fields.push_back(arrow::field(field.name, arrow_type_of(*field.type), true));
    return arrow::struct_(fields);
}

std::shared_ptr<arrow::DataType> arrow_type_of(const NestedType& type) {
    switch (type.kind) {
        case Kind::Int:
            return arrow::int64();
        case Kind::Double:
            return arrow::float64();
        case Kind::Bool:
            return arrow::boolean();
        case Kind::Struct:
            return struct_type_of(type);
        case Kind::List:
            return arrow::list(arrow::field("item", arrow_type_of(*type.element), true));
        default:
            return arrow::utf8();
    }
}

std::expected<void, std::string> append_value(arrow::ArrayBuilder* builder, dom::element value,
                                              const NestedType& type);

std::expected<void, std::string> append_int(arrow::ArrayBuilder* builder, dom::element value) {
    auto* typed = static_cast<arrow::Int64Builder*>(builder);
    std::int64_t out = 0;
    if (value.get(out) == simdjson::SUCCESS) return check(typed->Append(out));
    return check(typed->AppendNull());
}

std::expected<void, std::string> append_double(arrow::ArrayBuilder* builder, dom::element value) {
    auto* typed = static_cast<arrow::DoubleBuilder*>(builder);
    double out = 0;
    if (value.get(out) == simdjson::SUCCESS) return check(typed->Append(out));
    std::int64_t whole = 0;
    if (value.get(whole) == simdjson::SUCCESS)
        return check(typed->Append(static_cast<double>(whole)));
    return check(typed->AppendNull());
}

std::expected<void, std::string> append_bool(arrow::ArrayBuilder* builder, dom::element value) {
    auto* typed = static_cast<arrow::BooleanBuilder*>(builder);
    bool out = false;
    if (value.get(out) == simdjson::SUCCESS) return check(typed->Append(out));
    return check(typed->AppendNull());
}

std::expected<void, std::string> append_string(arrow::ArrayBuilder* builder, dom::element value) {
    auto* typed = static_cast<arrow::StringBuilder*>(builder);
    if (value.is_string()) {
        std::string_view text;
        (void)value.get(text);
        return check(typed->Append(text.data(), static_cast<int>(text.size())));
    }
    std::ostringstream rendered;
    rendered << value;
    return check(typed->Append(rendered.str()));
}

std::expected<void, std::string> append_struct(arrow::StructBuilder* builder, dom::object obj,
                                               const NestedType& type) {
    if (auto ok = check(builder->Append()); !ok) return ok;
    for (std::size_t i = 0; i < type.fields.size(); ++i) {
        auto* child = builder->field_builder(static_cast<int>(i));
        auto found = obj[type.fields[i].name];
        if (found.error()) {
            if (auto ok = check(child->AppendNull()); !ok) return ok;
            continue;
        }
        if (auto ok = append_value(child, found.value(), *type.fields[i].type); !ok) return ok;
    }
    return {};
}

std::expected<void, std::string> append_list(arrow::ListBuilder* builder, dom::array arr,
                                             const NestedType& type) {
    if (auto ok = check(builder->Append()); !ok) return ok;
    auto* values = builder->value_builder();
    for (dom::element item : arr)
        if (auto ok = append_value(values, item, *type.element); !ok) return ok;
    return {};
}

std::expected<void, std::string> append_value(arrow::ArrayBuilder* builder, dom::element value,
                                              const NestedType& type) {
    if (value.is_null()) return check(builder->AppendNull());
    switch (type.kind) {
        case Kind::Struct:
            if (!value.is_object()) return check(builder->AppendNull());
            return append_struct(static_cast<arrow::StructBuilder*>(builder), value.get_object(),
                                 type);
        case Kind::List:
            if (!value.is_array()) return check(builder->AppendNull());
            return append_list(static_cast<arrow::ListBuilder*>(builder), value.get_array(), type);
        case Kind::Int:
            return append_int(builder, value);
        case Kind::Double:
            return append_double(builder, value);
        case Kind::Bool:
            return append_bool(builder, value);
        default:
            return append_string(builder, value);
    }
}

std::expected<void, std::string> append_object(arrow::RecordBatchBuilder& builder, dom::object obj,
                                               const NestedType& top) {
    for (std::size_t i = 0; i < top.fields.size(); ++i) {
        auto* field = builder.GetField(static_cast<int>(i));
        auto found = obj[top.fields[i].name];
        if (found.error()) {
            if (auto ok = check(field->AppendNull()); !ok) return ok;
            continue;
        }
        if (auto ok = append_value(field, found.value(), *top.fields[i].type); !ok) return ok;
    }
    return {};
}

}  // namespace

std::expected<NestedType, std::string> infer_nested_schema(const std::vector<std::string>& sample) {
    dom::parser parser;
    NestedType top = scalar(Kind::Struct);
    for (const auto& line : sample) {
        auto doc = parser.parse(line);
        if (doc.error() || !doc.is_object()) continue;
        top = unify(top, infer_object(doc.get_object()));
    }
    return top;
}

std::shared_ptr<arrow::Schema> nested_arrow_schema(const NestedType& top) {
    arrow::FieldVector fields;
    for (const auto& field : top.fields)
        fields.push_back(arrow::field(field.name, arrow_type_of(*field.type), true));
    return arrow::schema(fields);
}

NestedBuilder::NestedBuilder(std::unique_ptr<arrow::RecordBatchBuilder> builder, NestedType top)
    : builder_(std::move(builder)), top_(std::move(top)) {}

NestedBuilder::~NestedBuilder() = default;

std::expected<std::unique_ptr<NestedBuilder>, std::string> NestedBuilder::make(
    const NestedType& top) {
    auto builder =
        arrow::RecordBatchBuilder::Make(nested_arrow_schema(top), arrow::default_memory_pool());
    if (!builder.ok()) return std::unexpected(builder.status().ToString());
    return std::unique_ptr<NestedBuilder>(new NestedBuilder(std::move(*builder), top));
}

std::expected<void, std::string> NestedBuilder::append_line(std::string_view line) {
    static thread_local dom::parser parser;
    auto doc = parser.parse(line.data(), line.size());
    if (doc.error()) return std::unexpected(simdjson::error_message(doc.error()));
    if (!doc.is_object()) return std::unexpected("top-level value is not an object");
    if (auto ok = append_object(*builder_, doc.get_object(), top_); !ok) return ok;
    ++rows_;
    return {};
}

std::expected<RecordBatch, std::string> NestedBuilder::flush() {
    auto batch = builder_->Flush();
    if (!batch.ok()) return std::unexpected(batch.status().ToString());
    RecordBatch out{*batch, rows_};
    rows_ = 0;
    return out;
}

}  // namespace riffle
