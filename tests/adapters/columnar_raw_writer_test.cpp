#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <string>

#include "riffle/batch.hpp"
#include "riffle/factories.hpp"
#include "riffle/types.hpp"
#include "riffle/writer.hpp"

namespace riffle {
namespace {

template <class T>
T read(std::istream& in) {
    T value{};
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return value;
}

void put_int(BatchSink& sink, std::int64_t v) {
    sink.begin_row();
    (void)sink.field("v", CellValue{v});
    (void)sink.end_row();
}

RecordBatch two_int_rows() {
    InferredSchema schema{.columns = {{"v", ColumnType::INT64, true, "v"}}};
    auto builder = make_batch_builder(schema);
    BatchSink sink(builder, TypeConflictPolicy::WIDEN);
    put_int(sink, 10);
    put_int(sink, 20);
    return build_batch(builder).value();
}

}

TEST(ColumnarRawWriter, RoundTripsIntColumn) {
    InferredSchema schema{.columns = {{"v", ColumnType::INT64, true, "v"}}};
    const std::string path = ::testing::TempDir() + "riffle_raw.bin";
    auto cfg = make_Config(
        {.inputs = {"x"}, .output_path = path, .output_format = OutputFormat::COLUMNAR_RAW});

    auto writer = open_writer(cfg, schema);
    ASSERT_TRUE(writer.has_value());
    ASSERT_TRUE((*writer)->write(two_int_rows()).has_value());
    ASSERT_TRUE((*writer)->finish().has_value());

    std::ifstream in(path, std::ios::binary);
    std::array<char, 8> magic{};
    in.read(magic.data(), magic.size());
    EXPECT_EQ(std::string(magic.data(), magic.size()), "RIFFLEC1");
    EXPECT_EQ(read<std::uint32_t>(in), 1u);
    EXPECT_EQ(read<std::uint32_t>(in), 1u);
    EXPECT_EQ(static_cast<char>(in.get()), 'v');
    in.get();
    EXPECT_EQ(read<std::uint32_t>(in), 2u);
    EXPECT_EQ(read<std::uint8_t>(in), 0u);
    EXPECT_EQ(read<std::int64_t>(in), 10);
    EXPECT_EQ(read<std::uint8_t>(in), 0u);
    EXPECT_EQ(read<std::int64_t>(in), 20);
}

}
