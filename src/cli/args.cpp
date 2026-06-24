#include "riffle/args.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "riffle/factories.hpp"
#include "riffle/schema_json.hpp"

namespace riffle {
namespace {

bool is_value_option(std::string_view token) {
    return token == "-o" || token == "--output" || token == "--format" ||
           token == "--compression" || token == "--on-error" || token == "--type-conflict" ||
           token == "--batch-rows" || token == "--batch-bytes" || token == "--threads" ||
           token == "--schema" || token == "--select" || token == "--exclude" ||
           token == "--rename";
}

std::vector<std::string> split_csv(const std::string& value) {
    std::vector<std::string> out;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

std::expected<void, std::string> set_rename(Config& draft, const std::string& value) {
    for (const auto& item : split_csv(value)) {
        auto eq = item.find('=');
        if (eq == std::string::npos) return std::unexpected("--rename expects from=to: " + item);
        draft.projection.rename.emplace_back(item.substr(0, eq), item.substr(eq + 1));
    }
    return {};
}

std::expected<void, std::string> set_schema(Config& draft, const std::string& path) {
    auto schema = load_schema_file(path);
    if (!schema) return std::unexpected(schema.error());
    draft.schema_override = *schema;
    return {};
}

template <class T, class E>
std::expected<void, std::string> assign(T& dst, std::expected<T, E> parsed) {
    if (!parsed) return std::unexpected(parsed.error());
    dst = *parsed;
    return {};
}

std::expected<void, std::string> parse_size(std::size_t& dst, std::string_view key,
                                            const std::string& value) {
    try {
        dst = std::stoull(value);
        return {};
    } catch (const std::exception&) {
        return std::unexpected("invalid " + std::string(key) + " value: " + value);
    }
}

std::expected<void, std::string> apply_enum(Config& draft, std::string_view key,
                                            const std::string& value) {
    if (key == "--format") return assign(draft.output_format, parse_output_format(value));
    if (key == "--compression") return assign(draft.compression, parse_compression_codec(value));
    if (key == "--on-error") return assign(draft.on_error, parse_on_error(value));
    return assign(draft.type_conflict, parse_type_conflict_policy(value));
}

std::expected<void, std::string> apply_value(Config& draft, std::string_view key,
                                             const std::string& value) {
    if (key == "-o" || key == "--output") {
        draft.output_path = value;
        return {};
    }
    if (key == "--schema") return set_schema(draft, value);
    if (key == "--batch-rows") return parse_size(draft.batch_rows, key, value);
    if (key == "--batch-bytes") return parse_size(draft.batch_bytes, key, value);
    if (key == "--threads") return parse_size(draft.threads, key, value);
    if (key == "--select") {
        draft.projection.select = split_csv(value);
        return {};
    }
    if (key == "--exclude") {
        draft.projection.exclude = split_csv(value);
        return {};
    }
    if (key == "--rename") return set_rename(draft, value);
    return apply_enum(draft, key, value);
}

bool is_unknown_option(const std::string& token) {
    return token.size() > 1 && token[0] == '-' && token != "-";
}

std::expected<Config, std::string> finalize(Config draft) {
    try {
        return make_Config(std::move(draft));
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
}

}

std::expected<Config, std::string> parse_args(std::span<const std::string> args) {
    Config draft;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& token = args[i];
        if (token == "--stats") {
            draft.emit_stats = true;
        } else if (token == "--print-schema") {
            draft.print_schema = true;
        } else if (is_value_option(token)) {
            if (i + 1 >= args.size()) return std::unexpected("missing value for " + token);
            if (auto ok = apply_value(draft, token, args[++i]); !ok)
                return std::unexpected(ok.error());
        } else if (is_unknown_option(token)) {
            return std::unexpected("unknown option " + token);
        } else {
            draft.inputs.push_back(token);
        }
    }
    return finalize(std::move(draft));
}

}
