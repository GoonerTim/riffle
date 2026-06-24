#pragma once

#include <cstddef>
#include <istream>
#include <optional>
#include <string>
#include <string_view>

#include "riffle/constants.hpp"

namespace riffle {

class LineReader {
public:
    explicit LineReader(std::istream& input, std::size_t max_line = MAX_LINE_BYTES,
                        std::size_t buffer = READ_BUFFER_BYTES);
    std::optional<std::string_view> next();

private:
    bool fill();
    std::size_t next_newline() const;
    void append_capped(std::size_t start, std::size_t count);
    bool read_line();

    std::istream& input_;
    std::size_t max_line_;
    std::string chunk_;
    std::size_t pos_ = 0;
    std::size_t end_ = 0;
    std::string line_;
};

}  // namespace riffle
