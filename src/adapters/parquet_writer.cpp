#include <arrow/api.h>
#include <arrow/io/file.h>
#include <parquet/arrow/writer.h>

#include "riffle/writer.hpp"
#include "riffle/writer_backends.hpp"

namespace riffle {
namespace {

arrow::Compression::type to_codec(CompressionCodec codec) {
    switch (codec) {
        case CompressionCodec::SNAPPY:
            return arrow::Compression::SNAPPY;
        case CompressionCodec::ZSTD:
            return arrow::Compression::ZSTD;
        default:
            return arrow::Compression::UNCOMPRESSED;
    }
}

std::shared_ptr<parquet::WriterProperties> make_props(CompressionCodec codec) {
    return parquet::WriterProperties::Builder().compression(to_codec(codec))->build();
}

class ParquetWriter : public Writer {
public:
    ParquetWriter(std::unique_ptr<parquet::arrow::FileWriter> writer,
                  std::shared_ptr<arrow::io::FileOutputStream> sink)
        : writer_(std::move(writer)), sink_(std::move(sink)) {}

    std::expected<void, std::string> write(const RecordBatch& batch) override {
        auto status = writer_->WriteRecordBatch(*batch.data);
        if (status.ok()) return {};
        return std::unexpected(status.ToString());
    }

    std::expected<void, std::string> finish() override {
        auto status = writer_->Close();
        if (status.ok()) return {};
        return std::unexpected(status.ToString());
    }

private:
    std::unique_ptr<parquet::arrow::FileWriter> writer_;
    std::shared_ptr<arrow::io::FileOutputStream> sink_;
};

}

std::expected<std::unique_ptr<Writer>, std::string> open_parquet_writer(
    const Config& config, const InferredSchema& schema) {
    auto sink = arrow::io::FileOutputStream::Open(config.output_path);
    if (!sink.ok()) return std::unexpected(sink.status().ToString());
    auto writer =
        parquet::arrow::FileWriter::Open(*arrow_schema_of(schema), arrow::default_memory_pool(),
                                         *sink, make_props(config.compression));
    if (!writer.ok()) return std::unexpected(writer.status().ToString());
    return std::make_unique<ParquetWriter>(std::move(*writer), *sink);
}

}
