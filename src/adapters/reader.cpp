#include "riffle/reader.hpp"

namespace riffle {

LineReader::LineReader(std::istream& input) : input_(input) {}

std::optional<std::string> LineReader::next() {
    std::string line;
    while (std::getline(input_, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) return line;
    }
    return std::nullopt;
}

}  // namespace riffle
