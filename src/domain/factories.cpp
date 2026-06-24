#include "riffle/factories.hpp"

#include <set>
#include <stdexcept>

namespace riffle {
namespace {

void require(bool ok, const char* message) {
    if (!ok) throw std::invalid_argument(message);
}

bool has_duplicate_names(const std::vector<ColumnSchema>& columns) {
    std::set<std::string_view> seen;
    for (const auto& col : columns) {
        if (!seen.insert(col.name).second) return true;
    }
    return false;
}

}

ColumnSchema make_ColumnSchema(ColumnSchema draft) {
    require(!draft.name.empty(), "ColumnSchema.name must not be empty");
    if (draft.json_path.empty()) draft.json_path = draft.name;
    return draft;
}

InferredSchema make_InferredSchema(InferredSchema draft) {
    require(!has_duplicate_names(draft.columns), "InferredSchema column names must be unique");
    return draft;
}

Config make_Config(Config draft) {
    require(!draft.inputs.empty(), "Config.inputs must not be empty");
    require(!draft.output_path.empty() || draft.print_schema,
            "Config.output_path must not be empty");
    require(draft.batch_rows >= 1, "Config.batch_rows must be >= 1");
    require(draft.threads >= 1, "Config.threads must be >= 1");
    require(draft.nested == NestedMode::FLATTEN || draft.output_format == OutputFormat::PARQUET,
            "--nested native requires parquet output");
    return draft;
}

ParseError make_ParseError(ParseError draft) {
    require(draft.line_no >= 1, "ParseError.line_no must be >= 1");
    require(!draft.reason.empty(), "ParseError.reason must not be empty");
    return draft;
}

ConvertStats make_ConvertStats() {
    return {};
}

}
