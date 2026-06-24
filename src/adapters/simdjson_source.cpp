#include "riffle/simdjson_source.hpp"

#include <simdjson.h>

#include <string>
#include <string_view>

#include "riffle/factories.hpp"

namespace riffle {
namespace {

namespace od = simdjson::ondemand;

CellValue number_cell(od::value value) {
    od::number num;
    if (value.get_number().get(num)) return CellValue{};
    if (num.get_number_type() == od::number_type::floating_point_number) {
        return CellValue{num.get_double()};
    }
    if (num.is_uint64()) return CellValue{static_cast<std::int64_t>(num.get_uint64())};
    return CellValue{num.get_int64()};
}

CellValue cell_of(od::value value) {
    switch (value.type()) {
        case od::json_type::number: return number_cell(value);
        case od::json_type::boolean: return CellValue{value.get_bool().value()};
        case od::json_type::null: return CellValue{};
        case od::json_type::string: return CellValue{std::string(value.get_string().value())};
        default: return CellValue{std::string(value.raw_json().value())};
    }
}

std::string join_path(const std::string& prefix, std::string_view key) {
    return prefix.empty() ? std::string(key) : prefix + "." + std::string(key);
}

void flatten(Row& row, const std::string& prefix, od::object object, int depth);

void flatten_field(Row& row, const std::string& prefix, od::field field, int depth) {
    std::string path = join_path(prefix, field.unescaped_key().value());
    od::value value = field.value();
    if (value.type() == od::json_type::object && depth + 1 < MAX_FLATTEN_DEPTH) {
        flatten(row, path, value.get_object(), depth + 1);
    } else {
        row.fields.push_back({std::move(path), cell_of(value)});
    }
}

void flatten(Row& row, const std::string& prefix, od::object object, int depth) {
    for (auto field : object) {
        od::field unwrapped;
        if (!std::move(field).get(unwrapped)) flatten_field(row, prefix, unwrapped, depth);
    }
}

std::expected<Row, std::string> parse_one(od::parser& parser, simdjson::padded_string& padded) {
    od::document doc;
    if (auto error = parser.iterate(padded).get(doc)) {
        return std::unexpected(simdjson::error_message(error));
    }
    od::object root;
    if (doc.get_object().get(root)) return std::unexpected("top-level value is not an object");
    Row row;
    flatten(row, "", root, 0);
    return row;
}

}  // namespace

struct SimdjsonSource::Impl {
    od::parser parser;
};

SimdjsonSource::SimdjsonSource(LineReader& reader)
    : reader_(reader), impl_(std::make_unique<Impl>()) {}

SimdjsonSource::~SimdjsonSource() = default;

std::optional<Row> SimdjsonSource::next() {
    for (auto line = reader_.next(); line; line = reader_.next()) {
        ++line_no_;
        simdjson::padded_string padded(*line);
        auto row = parse_one(impl_->parser, padded);
        if (row) return std::move(*row);
        errors_.push_back(make_ParseError({.line_no = line_no_, .reason = row.error(), .raw = *line}));
    }
    return std::nullopt;
}

}  // namespace riffle
