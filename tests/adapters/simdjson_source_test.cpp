#include "riffle/reader.hpp"
#include "riffle/simdjson_source.hpp"
#include "riffle/types.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>
#include <variant>

namespace riffle {
namespace {

const CellValue* field(const Row& row, std::string_view path) {
    for (const auto& f : row.fields) {
        if (f.path == path) return &f.value;
    }
    return nullptr;
}

}  // namespace

TEST(SimdjsonSource, ParsesScalarFields) {
    std::istringstream in(R"({"a":1,"b":"x","c":true})");
    LineReader reader(in);
    SimdjsonSource source(reader);
    auto row = source.next();
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(std::get<std::int64_t>(*field(*row, "a")), 1);
    EXPECT_EQ(std::get<std::string>(*field(*row, "b")), "x");
    EXPECT_EQ(std::get<bool>(*field(*row, "c")), true);
}

TEST(SimdjsonSource, FlattensNestedObject) {
    std::istringstream in(R"({"req":{"code":200}})");
    LineReader reader(in);
    SimdjsonSource source(reader);
    auto row = source.next();
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(std::get<std::int64_t>(*field(*row, "req.code")), 200);
}

TEST(SimdjsonSource, NullBecomesMonostate) {
    std::istringstream in(R"({"a":null})");
    LineReader reader(in);
    SimdjsonSource source(reader);
    auto row = source.next();
    ASSERT_TRUE(row.has_value());
    EXPECT_TRUE(std::holds_alternative<std::monostate>(*field(*row, "a")));
}

TEST(SimdjsonSource, SkipsNonObjectAndRecordsError) {
    std::istringstream in("123\n");
    LineReader reader(in);
    SimdjsonSource source(reader);
    EXPECT_FALSE(source.next().has_value());
    ASSERT_EQ(source.errors().size(), 1u);
    EXPECT_EQ(source.errors()[0].line_no, 1u);
}

}  // namespace riffle
