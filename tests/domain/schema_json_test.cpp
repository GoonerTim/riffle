#include "riffle/schema_json.hpp"

#include <gtest/gtest.h>

#include "riffle/types.hpp"

namespace riffle {

TEST(ParseSchemaJson, ReadsColumns) {
    auto schema = parse_schema_json(R"({"columns":[
        {"name":"id","type":"string"},
        {"name":"code","type":"int64","nullable":false}
    ]})");
    ASSERT_TRUE(schema.has_value());
    ASSERT_EQ(schema->columns.size(), 2u);
    EXPECT_EQ(schema->columns[0].name, "id");
    EXPECT_EQ(schema->columns[0].type, ColumnType::STRING);
    EXPECT_EQ(schema->columns[1].type, ColumnType::INT64);
    EXPECT_FALSE(schema->columns[1].nullable);
}

TEST(ParseSchemaJson, DefaultsJsonPathToName) {
    auto schema = parse_schema_json(R"({"columns":[{"name":"a","type":"int64"}]})");
    ASSERT_TRUE(schema.has_value());
    EXPECT_EQ(schema->columns[0].json_path, "a");
}

TEST(ParseSchemaJson, KeepsExplicitJsonPath) {
    auto schema =
        parse_schema_json(R"({"columns":[{"name":"code","type":"int64","json_path":"req.code"}]})");
    ASSERT_TRUE(schema.has_value());
    EXPECT_EQ(schema->columns[0].json_path, "req.code");
}

TEST(ParseSchemaJson, RejectsUnknownType) {
    EXPECT_FALSE(parse_schema_json(R"({"columns":[{"name":"a","type":"bogus"}]})").has_value());
}

TEST(ParseSchemaJson, RejectsMalformedJson) {
    EXPECT_FALSE(parse_schema_json("not json").has_value());
}

TEST(ParseSchemaJson, RejectsMissingName) {
    EXPECT_FALSE(parse_schema_json(R"({"columns":[{"type":"int64"}]})").has_value());
}

}
