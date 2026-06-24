#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "riffle/schema.hpp"
#include "riffle/types.hpp"

namespace riffle {
namespace {

InferredSchema abc() {
    return {.columns = {{"a", ColumnType::INT64, true, "a"},
                        {"b", ColumnType::STRING, true, "b"},
                        {"c", ColumnType::DOUBLE, true, "c"}}};
}

std::vector<std::string> names(const InferredSchema& s) {
    std::vector<std::string> out;
    for (const auto& col : s.columns) out.push_back(col.name);
    return out;
}

}  // namespace

TEST(ApplyProjection, NoSpecKeepsAll) {
    auto out = apply_projection(abc(), {});
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(names(*out), (std::vector<std::string>{"a", "b", "c"}));
}

TEST(ApplyProjection, SelectKeepsOnlyListedInSelectOrder) {
    auto out = apply_projection(abc(), {.select = {"c", "a"}});
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(names(*out), (std::vector<std::string>{"c", "a"}));
}

TEST(ApplyProjection, SelectUnknownColumnIsError) {
    EXPECT_FALSE(apply_projection(abc(), {.select = {"a", "nope"}}).has_value());
}

TEST(ApplyProjection, ExcludeDropsListed) {
    auto out = apply_projection(abc(), {.exclude = {"b"}});
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(names(*out), (std::vector<std::string>{"a", "c"}));
}

TEST(ApplyProjection, RenameChangesNameKeepsJsonPath) {
    auto out = apply_projection(abc(), {.rename = {{"a", "id"}}});
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->columns[0].name, "id");
    EXPECT_EQ(out->columns[0].json_path, "a");
}

TEST(ApplyProjection, RenameCausingDuplicateIsError) {
    EXPECT_FALSE(apply_projection(abc(), {.rename = {{"a", "b"}}}).has_value());
}

}  // namespace riffle
