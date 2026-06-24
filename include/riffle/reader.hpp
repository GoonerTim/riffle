#pragma once

#include <istream>
#include <optional>
#include <string>
#include <string_view>

namespace riffle {

class LineReader {
public:
    explicit LineReader(std::istream& input);
    std::optional<std::string_view> next();

private:
    std::istream& input_;
    std::string buffer_;
};

}
