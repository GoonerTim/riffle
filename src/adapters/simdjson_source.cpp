#include "riffle/simdjson_source.hpp"

#include <simdjson.h>

#include <sstream>
#include <string>

#include "riffle/factories.hpp"

namespace riffle {
namespace {

CellValue cell_of(simdjson::dom::element element) {
    if (element.is_int64()) return CellValue{element.get_int64().value()};
    if (element.is_uint64()) return CellValue{static_cast<std::int64_t>(element.get_uint64().value())};
    if (element.is_double()) return CellValue{element.get_double().value()};
    if (element.is_bool()) return CellValue{element.get_bool().value()};
    if (element.is_string()) return CellValue{std::string(element.get_string().value())};
    if (element.is_null()) return CellValue{};
    std::ostringstream text;
    text << element;
    return CellValue{text.str()};
}

void flatten(Row& row, const std::string& prefix, simdjson::dom::object object, int depth) {
    for (auto [key, value] : object) {
        std::string path = prefix.empty() ? std::string(key) : prefix + "." + std::string(key);
        if (value.is_object() && depth + 1 < MAX_FLATTEN_DEPTH) {
            flatten(row, path, value.get_object().value(), depth + 1);
        } else {
            row.fields.push_back({std::move(path), cell_of(value)});
        }
    }
}

std::expected<Row, std::string> parse_one(simdjson::dom::parser& parser, const std::string& line) {
    auto doc = parser.parse(simdjson::padded_string(line));
    if (doc.error()) return std::unexpected(simdjson::error_message(doc.error()));
    if (!doc.value().is_object()) return std::unexpected("top-level value is not an object");
    Row row;
    flatten(row, "", doc.value().get_object().value(), 0);
    return row;
}

}  // namespace

struct SimdjsonSource::Impl {
    simdjson::dom::parser parser;
};

SimdjsonSource::SimdjsonSource(LineReader& reader)
    : reader_(reader), impl_(std::make_unique<Impl>()) {}

SimdjsonSource::~SimdjsonSource() = default;

std::optional<Row> SimdjsonSource::next() {
    for (auto line = reader_.next(); line; line = reader_.next()) {
        ++line_no_;
        auto row = parse_one(impl_->parser, *line);
        if (row) return std::move(*row);
        errors_.push_back(make_ParseError({.line_no = line_no_, .reason = row.error(), .raw = *line}));
    }
    return std::nullopt;
}

}  // namespace riffle
