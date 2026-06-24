#pragma once

#include <istream>
#include <optional>
#include <string>

namespace riffle {

// Streams non-empty logical lines from an input stream (CRLF tolerant).
class LineReader {
public:
    explicit LineReader(std::istream& input);
    std::optional<std::string> next();

private:
    std::istream& input_;
};

}  // namespace riffle
