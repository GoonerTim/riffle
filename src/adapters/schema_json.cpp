#include "riffle/schema_json.hpp"

#include <simdjson.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "riffle/factories.hpp"

namespace riffle {
namespace {

std::expected<std::string, std::string> require_string(simdjson::dom::element obj,
                                                       std::string_view key) {
    auto field = obj[key];
    if (field.error() || !field.value().is_string()) {
        return std::unexpected("missing or non-string field: " + std::string(key));
    }
    return std::string(field.value().get_string().value());
}

std::string optional_string(simdjson::dom::element obj, std::string_view key,
                            const std::string& fallback) {
    auto field = obj[key];
    if (field.error() || !field.value().is_string()) return fallback;
    return std::string(field.value().get_string().value());
}

bool optional_bool(simdjson::dom::element obj, std::string_view key, bool fallback) {
    auto field = obj[key];
    if (field.error() || !field.value().is_bool()) return fallback;
    return field.value().get_bool().value();
}

std::expected<ColumnSchema, std::string> parse_column(simdjson::dom::element obj) {
    auto name = require_string(obj, "name");
    if (!name) return std::unexpected(name.error());
    auto type = parse_column_type(optional_string(obj, "type", ""));
    if (!type) return std::unexpected(type.error());
    return make_ColumnSchema({.name = *name,
                              .type = *type,
                              .nullable = optional_bool(obj, "nullable", true),
                              .json_path = optional_string(obj, "json_path", "")});
}

std::expected<InferredSchema, std::string> parse_columns(simdjson::dom::element array) {
    InferredSchema schema;
    for (auto element : array) {
        auto column = parse_column(element);
        if (!column) return std::unexpected(column.error());
        schema.columns.push_back(*column);
    }
    return make_InferredSchema(schema);
}

}

std::expected<InferredSchema, std::string> parse_schema_json(std::string_view text) {
    simdjson::dom::parser parser;
    auto doc = parser.parse(simdjson::padded_string(text));
    if (doc.error()) return std::unexpected(simdjson::error_message(doc.error()));
    auto columns = doc.value()["columns"];
    if (columns.error() || !columns.value().is_array()) {
        return std::unexpected("schema JSON must have a 'columns' array");
    }
    try {
        return parse_columns(columns.value());
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
}

std::expected<InferredSchema, std::string> load_schema_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::unexpected("cannot open schema file: " + path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return parse_schema_json(buffer.str());
}

namespace {

std::string json_escape(std::string_view text) {
    std::string out;
    for (char c : text) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

std::string column_json(const ColumnSchema& column) {
    return std::string(R"({"name":")") + json_escape(column.name) + R"(","type":")" +
           std::string(to_string(column.type)) + R"(","nullable":)" +
           (column.nullable ? "true" : "false") + R"(,"json_path":")" +
           json_escape(column.json_path) + R"("})";
}

}  // namespace

std::string write_schema_json(const InferredSchema& schema) {
    std::string out = R"({"columns":[)";
    for (std::size_t i = 0; i < schema.columns.size(); ++i) {
        if (i != 0) out += ",";
        out += column_json(schema.columns[i]);
    }
    out += "]}";
    return out;
}

}
