#include "riffle/input.hpp"

#include <arrow/io/compressed.h>
#include <arrow/io/file.h>
#include <arrow/io/stdio.h>
#include <arrow/util/compression.h>

#include <cstdint>
#include <memory>
#include <string_view>

namespace riffle {
namespace {

class ArrowByteSource : public ByteSource {
public:
    ArrowByteSource(std::unique_ptr<arrow::util::Codec> codec,
                    std::shared_ptr<arrow::io::InputStream> stream)
        : codec_(std::move(codec)), stream_(std::move(stream)) {}

    std::size_t read(char* dst, std::size_t n) override {
        auto got = stream_->Read(static_cast<std::int64_t>(n), dst);
        return got.ok() ? static_cast<std::size_t>(*got) : 0;
    }

private:
    std::unique_ptr<arrow::util::Codec> codec_;
    std::shared_ptr<arrow::io::InputStream> stream_;
};

class NullByteSource : public ByteSource {
public:
    std::size_t read(char*, std::size_t) override { return 0; }
};

bool ends_with(std::string_view text, std::string_view suffix) {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

arrow::Compression::type codec_for(const std::string& path) {
    if (ends_with(path, ".gz")) return arrow::Compression::GZIP;
    if (ends_with(path, ".zst")) return arrow::Compression::ZSTD;
    return arrow::Compression::UNCOMPRESSED;
}

std::shared_ptr<arrow::io::InputStream> open_raw(const std::string& path) {
    if (path == "-") return std::make_shared<arrow::io::StdinStream>();
    auto file = arrow::io::ReadableFile::Open(path);
    return file.ok() ? *file : nullptr;
}

}  // namespace

std::unique_ptr<ByteSource> open_input(const std::string& path) {
    auto raw = open_raw(path);
    if (raw == nullptr) return std::make_unique<NullByteSource>();
    auto compression = codec_for(path);
    if (compression == arrow::Compression::UNCOMPRESSED) {
        return std::make_unique<ArrowByteSource>(nullptr, raw);
    }
    auto codec = arrow::util::Codec::Create(compression);
    if (!codec.ok()) return std::make_unique<NullByteSource>();
    auto stream = arrow::io::CompressedInputStream::Make(codec->get(), raw);
    if (!stream.ok()) return std::make_unique<NullByteSource>();
    return std::make_unique<ArrowByteSource>(std::move(*codec), *stream);
}

}  // namespace riffle
