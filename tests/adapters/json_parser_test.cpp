#include "riffle/json_parser.hpp"
#include "riffle/ports.hpp"
#include "riffle/types.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace riffle {
namespace {

// Records the fields the parser pushes, for assertions.
class RecordSink : public RowSink {
public:
    void begin_row() override { ++rows; }
    std::expected<void, std::string> field(std::string_view path, CellValue value) override {
        fields.emplace_back(std::string(path), std::move(value));
        return {};
    }
    std::expected<void, std::string> end_row() override { return {}; }

    const CellValue* get(std::string_view path) const {
        for (const auto& [p, v] : fields) {
            if (p == path) return &v;
        }
        return nullptr;
    }

    std::vector<std::pair<std::string, CellValue>> fields;
    int rows = 0;
};

}  // namespace

TEST(JsonParser, ParsesScalarFields) {
    JsonParser parser;
    RecordSink sink;
    ASSERT_TRUE(parser.parse(R"({"a":1,"b":"x","c":true})", sink).has_value());
    EXPECT_EQ(std::get<std::int64_t>(*sink.get("a")), 1);
    EXPECT_EQ(std::get<std::string>(*sink.get("b")), "x");
    EXPECT_EQ(std::get<bool>(*sink.get("c")), true);
}

TEST(JsonParser, FlattensNestedObject) {
    JsonParser parser;
    RecordSink sink;
    ASSERT_TRUE(parser.parse(R"({"req":{"code":200}})", sink).has_value());
    EXPECT_EQ(std::get<std::int64_t>(*sink.get("req.code")), 200);
}

TEST(JsonParser, NullBecomesMonostate) {
    JsonParser parser;
    RecordSink sink;
    ASSERT_TRUE(parser.parse(R"({"a":null})", sink).has_value());
    EXPECT_TRUE(std::holds_alternative<std::monostate>(*sink.get("a")));
}

TEST(JsonParser, RejectsNonObject) {
    JsonParser parser;
    RecordSink sink;
    EXPECT_FALSE(parser.parse("123", sink).has_value());
    EXPECT_EQ(sink.rows, 0);  // begin_row not reached for a non-object line
}

TEST(JsonParser, CallsBeginRowOncePerObject) {
    JsonParser parser;
    RecordSink sink;
    ASSERT_TRUE(parser.parse(R"({"a":1})", sink).has_value());
    EXPECT_EQ(sink.rows, 1);
}

}  // namespace riffle
