#include "riffle/args.hpp"

#include <stdexcept>

#include "riffle/factories.hpp"

namespace riffle {
namespace {

bool is_value_option(std::string_view token) {
    return token == "-o" || token == "--output" || token == "--format" ||
           token == "--compression" || token == "--on-error" ||
           token == "--type-conflict" || token == "--batch-rows";
}

template <class T, class E>
std::expected<void, std::string> assign(T& dst, std::expected<T, E> parsed) {
    if (!parsed) return std::unexpected(parsed.error());
    dst = *parsed;
    return {};
}

std::expected<void, std::string> set_batch_rows(Config& draft, const std::string& value) {
    try {
        draft.batch_rows = std::stoull(value);
        return {};
    } catch (const std::exception&) {
        return std::unexpected("invalid --batch-rows value: " + value);
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
    if (key == "--batch-rows") return set_batch_rows(draft, value);
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

}  // namespace

std::expected<Config, std::string> parse_args(std::span<const std::string> args) {
    Config draft;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& token = args[i];
        if (token == "--stats") {
            draft.emit_stats = true;
        } else if (is_value_option(token)) {
            if (i + 1 >= args.size()) return std::unexpected("missing value for " + token);
            if (auto ok = apply_value(draft, token, args[++i]); !ok) return std::unexpected(ok.error());
        } else if (is_unknown_option(token)) {
            return std::unexpected("unknown option " + token);
        } else {
            draft.inputs.push_back(token);
        }
    }
    return finalize(std::move(draft));
}

}  // namespace riffle
