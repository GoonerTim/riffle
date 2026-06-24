#pragma once

#include <cstddef>
#include <istream>
#include <optional>
#include <string>
#include <string_view>

#include "riffle/constants.hpp"

namespace riffle {

class ByteSource {
public:
    virtual ~ByteSource() = default;
    virtual std::size_t read(char* dst, std::size_t n) = 0;
};

class StdByteSource : public ByteSource {
public:
    explicit StdByteSource(std::istream& input) : input_(input) {}
    std::size_t read(char* dst, std::size_t n) override;

private:
    std::istream& input_;
};

class LineReader {
public:
    explicit LineReader(ByteSource& source, std::size_t max_line = MAX_LINE_BYTES,
                        std::size_t buffer = READ_BUFFER_BYTES);
    std::optional<std::string_view> next();

private:
    bool fill();
    std::size_t next_newline() const;
    void append_capped(std::size_t start, std::size_t count);
    bool read_line();

    ByteSource& source_;
    std::size_t max_line_;
    std::string chunk_;
    std::size_t pos_ = 0;
    std::size_t end_ = 0;
    std::string line_;
};

}  // namespace riffle
