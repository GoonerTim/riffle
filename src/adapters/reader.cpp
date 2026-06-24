#include "riffle/reader.hpp"

namespace riffle {

LineReader::LineReader(std::istream& input) : input_(input) {}

std::optional<std::string_view> LineReader::next() {
    while (std::getline(input_, buffer_)) {
        if (!buffer_.empty() && buffer_.back() == '\r') buffer_.pop_back();
        if (!buffer_.empty()) return std::string_view(buffer_);
    }
    return std::nullopt;
}

}
