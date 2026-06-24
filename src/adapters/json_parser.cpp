#include "riffle/json_parser.hpp"

#include <simdjson.h>

#include <string>
#include <string_view>

#include "riffle/constants.hpp"
#include "riffle/types.hpp"

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
        case od::json_type::number:
            return number_cell(value);
        case od::json_type::boolean:
            return CellValue{value.get_bool().value()};
        case od::json_type::null:
            return CellValue{};
        case od::json_type::string:
            return CellValue{value.get_string().value()};
        default:
            return CellValue{value.raw_json().value()};
    }
}

std::string join_path(std::string_view prefix, std::string_view key) {
    return prefix.empty() ? std::string(key) : std::string(prefix) + "." + std::string(key);
}

std::expected<void, std::string> walk(od::object object, std::string_view prefix, int depth,
                                      RowSink& sink);

std::expected<void, std::string> walk_field(od::field field, std::string_view prefix, int depth,
                                            RowSink& sink) {
    std::string_view key = field.unescaped_key(false).value();
    od::value value = field.value();
    if (value.type() == od::json_type::object && depth + 1 < MAX_FLATTEN_DEPTH) {
        return walk(value.get_object(), join_path(prefix, key), depth + 1, sink);
    }
    if (prefix.empty()) return sink.field(key, cell_of(value));
    return sink.field(join_path(prefix, key), cell_of(value));
}

std::expected<void, std::string> walk(od::object object, std::string_view prefix, int depth,
                                      RowSink& sink) {
    for (auto field : object) {
        od::field unwrapped;
        if (std::move(field).get(unwrapped)) continue;
        if (auto ok = walk_field(unwrapped, prefix, depth, sink); !ok) return ok;
    }
    return {};
}

}

struct JsonParser::Impl {
    od::parser parser;
    std::string buffer;
};

JsonParser::JsonParser() : impl_(std::make_unique<Impl>()) {}
JsonParser::~JsonParser() = default;

std::expected<void, std::string> JsonParser::parse(std::string_view line, RowSink& sink) {
    impl_->buffer.assign(line);
    impl_->buffer.reserve(line.size() + simdjson::SIMDJSON_PADDING);
    od::document doc;
    if (auto error = impl_->parser.iterate(impl_->buffer).get(doc)) {
        return std::unexpected(simdjson::error_message(error));
    }
    od::object root;
    if (doc.get_object().get(root)) return std::unexpected("top-level value is not an object");
    sink.begin_row();
    if (auto ok = walk(root, "", 0, sink); !ok) return ok;
    return sink.end_row();
}

}
