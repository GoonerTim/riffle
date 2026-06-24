#pragma once

#include <expected>
#include <memory>
#include <string>
#include <string_view>

#include "riffle/ports.hpp"

namespace riffle {

class JsonParser {
public:
    JsonParser();
    ~JsonParser();

    std::expected<void, std::string> parse(std::string_view line, RowSink& sink);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
