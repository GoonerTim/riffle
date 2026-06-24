#include <arrow/api.h>

#include <cstdint>
#include <fstream>
#include <ostream>
#include <string_view>

#include "riffle/writer_backends.hpp"

namespace riffle {
namespace {

void put(std::ostream& out, const void* data, std::size_t size) {
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
}

void put_u8(std::ostream& out, std::uint8_t value) {
    put(out, &value, 1);
}
void put_u32(std::ostream& out, std::uint32_t value) {
    put(out, &value, 4);
}
void put_i64(std::ostream& out, std::int64_t value) {
    put(out, &value, 8);
}
void put_f64(std::ostream& out, double value) {
    put(out, &value, 8);
}

void put_str(std::ostream& out, std::string_view text) {
    put_u32(out, static_cast<std::uint32_t>(text.size()));
    put(out, text.data(), text.size());
}

struct Cell {
    const arrow::Array& array;
    std::int64_t index;
};

template <class A>
const A& as_array(const arrow::Array& array) {
    return static_cast<const A&>(array);
}

void write_typed(std::ostream& out, Cell cell, ColumnType type) {
    switch (type) {
        case ColumnType::INT64:
            return put_i64(out, as_array<arrow::Int64Array>(cell.array).Value(cell.index));
        case ColumnType::DOUBLE:
            return put_f64(out, as_array<arrow::DoubleArray>(cell.array).Value(cell.index));
        case ColumnType::BOOL:
            return put_u8(out, as_array<arrow::BooleanArray>(cell.array).Value(cell.index));
        case ColumnType::TIMESTAMP:
            return put_i64(out, as_array<arrow::TimestampArray>(cell.array).Value(cell.index));
        case ColumnType::STRING:
            return put_str(out, as_array<arrow::StringArray>(cell.array).GetView(cell.index));
        default:
            return;
    }
}

void write_column(std::ostream& out, const arrow::Array& array, ColumnType type) {
    for (std::int64_t j = 0; j < array.length(); ++j) {
        if (array.IsNull(j)) {
            put_u8(out, 1);
            continue;
        }
        put_u8(out, 0);
        write_typed(out, Cell{array, j}, type);
    }
}

void write_header(std::ostream& out, const InferredSchema& schema) {
    out.write("RIFFLEC1", 8);
    put_u32(out, static_cast<std::uint32_t>(schema.columns.size()));
    for (const auto& column : schema.columns) {
        put_str(out, column.name);
        put_u8(out, static_cast<std::uint8_t>(column.type));
    }
}

class ColumnarRawWriter : public Writer {
public:
    ColumnarRawWriter(std::ofstream out, InferredSchema schema)
        : out_(std::move(out)), schema_(std::move(schema)) {}

    std::expected<void, std::string> write(const RecordBatch& batch) override {
        put_u32(out_, static_cast<std::uint32_t>(batch.n_rows));
        for (std::size_t i = 0; i < schema_.columns.size(); ++i) {
            write_column(out_, *batch.data->column(static_cast<int>(i)), schema_.columns[i].type);
        }
        if (!out_) return std::unexpected("columnar-raw write failed");
        return {};
    }

    std::expected<void, std::string> finish() override {
        out_.flush();
        return {};
    }

private:
    std::ofstream out_;
    InferredSchema schema_;
};

}

std::expected<std::unique_ptr<Writer>, std::string> open_columnar_raw_writer(
    const Config& config, const InferredSchema& schema) {
    std::ofstream out(config.output_path, std::ios::binary);
    if (!out) return std::unexpected("cannot open output: " + config.output_path);
    write_header(out, schema);
    return std::make_unique<ColumnarRawWriter>(std::move(out), schema);
}

}
