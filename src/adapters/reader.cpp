#include "riffle/reader.hpp"

#include <algorithm>
#include <cstring>

namespace riffle {

std::size_t StdByteSource::read(char* dst, std::size_t n) {
    input_.read(dst, static_cast<std::streamsize>(n));
    return static_cast<std::size_t>(input_.gcount());
}

LineReader::LineReader(ByteSource& source, std::size_t max_line, std::size_t buffer)
    : source_(source), max_line_(max_line) {
    chunk_.resize(buffer == 0 ? 1 : buffer);
}

bool LineReader::fill() {
    end_ = source_.read(chunk_.data(), chunk_.size());
    pos_ = 0;
    return end_ > 0;
}

std::size_t LineReader::next_newline() const {
    const void* nl = std::memchr(chunk_.data() + pos_, '\n', end_ - pos_);
    if (nl == nullptr) return std::string::npos;
    return static_cast<std::size_t>(static_cast<const char*>(nl) - chunk_.data());
}

void LineReader::append_capped(std::size_t start, std::size_t count) {
    std::size_t room = max_line_ > line_.size() ? max_line_ - line_.size() : 0;
    line_.append(chunk_, start, std::min(count, room));
}

bool LineReader::read_line() {
    line_.clear();
    bool got = false;
    while (true) {
        if (pos_ >= end_ && !fill()) return got;
        got = true;
        std::size_t nl = next_newline();
        std::size_t take = (nl == std::string::npos ? end_ : nl) - pos_;
        append_capped(pos_, take);
        pos_ += take;
        if (nl != std::string::npos) {
            ++pos_;
            return true;
        }
    }
}

std::optional<std::string_view> LineReader::next() {
    while (read_line()) {
        if (!line_.empty() && line_.back() == '\r') line_.pop_back();
        if (!line_.empty()) return std::string_view(line_);
    }
    return std::nullopt;
}

}  // namespace riffle
