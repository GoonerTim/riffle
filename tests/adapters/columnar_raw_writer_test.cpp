#include "riffle/batch.hpp"
#include "riffle/factories.hpp"
#include "riffle/types.hpp"
#include "riffle/writer.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <string>

namespace riffle {
namespace {

template <class T>
T read(std::istream& in) {
    T value{};
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return value;
}

RecordBatch two_int_rows() {
    InferredSchema schema{.columns = {{"v", ColumnType::INT64, true, "v"}}};
    auto builder = make_batch_builder(schema);
    (void)append_row(builder, Row{{{"v", CellValue{std::int64_t{10}}}}});
    (void)append_row(builder, Row{{{"v", CellValue{std::int64_t{20}}}}});
    return build_batch(builder).value();
}

}  // namespace

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
    char magic[8] = {};
    in.read(magic, 8);
    EXPECT_EQ(std::string(magic, 8), "RIFFLEC1");
    EXPECT_EQ(read<std::uint32_t>(in), 1u);              // columns
    EXPECT_EQ(read<std::uint32_t>(in), 1u);              // name length
    EXPECT_EQ(static_cast<char>(in.get()), 'v');         // name
    in.get();                                            // column type byte
    EXPECT_EQ(read<std::uint32_t>(in), 2u);              // rows in batch
    EXPECT_EQ(read<std::uint8_t>(in), 0u);               // row 0 not null
    EXPECT_EQ(read<std::int64_t>(in), 10);
    EXPECT_EQ(read<std::uint8_t>(in), 0u);               // row 1 not null
    EXPECT_EQ(read<std::int64_t>(in), 20);
}

}  // namespace riffle
