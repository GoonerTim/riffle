#include "riffle/nested.hpp"

#include <arrow/api.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace riffle {
namespace {

const NestedField* field_of(const NestedType& type, std::string_view name) {
    for (const auto& field : type.fields)
        if (field.name == name) return &field;
    return nullptr;
}

}  // namespace

TEST(Nested, InfersStructListAndWidensScalars) {
    std::vector<std::string> sample = {
        R"({"a":1,"o":{"x":1},"l":[1,2]})",
        R"({"a":2.5,"o":{"x":3,"y":"hi"},"l":[3]})",
    };
    auto top = infer_nested_schema(sample);
    ASSERT_TRUE(top.has_value());

    const auto* a = field_of(*top, "a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->type->kind, NestedType::Kind::Double);  // int then double -> double

    const auto* o = field_of(*top, "o");
    ASSERT_NE(o, nullptr);
    EXPECT_EQ(o->type->kind, NestedType::Kind::Struct);
    EXPECT_NE(field_of(*o->type, "x"), nullptr);  // union of fields across rows
    EXPECT_NE(field_of(*o->type, "y"), nullptr);

    const auto* l = field_of(*top, "l");
    ASSERT_NE(l, nullptr);
    EXPECT_EQ(l->type->kind, NestedType::Kind::List);
    EXPECT_EQ(l->type->element->kind, NestedType::Kind::Int);
}

TEST(Nested, BuildsArrowStructAndListColumns) {
    std::vector<std::string> sample = {R"({"id":1,"o":{"x":1},"l":[1,2]})"};
    auto top = infer_nested_schema(sample);
    ASSERT_TRUE(top.has_value());
    auto schema = nested_arrow_schema(*top);
    EXPECT_EQ(schema->GetFieldByName("o")->type()->id(), arrow::Type::STRUCT);
    EXPECT_EQ(schema->GetFieldByName("l")->type()->id(), arrow::Type::LIST);

    auto builder = NestedBuilder::make(*top);
    ASSERT_TRUE(builder.has_value());
    ASSERT_TRUE((*builder)->append_line(sample[0]).has_value());
    EXPECT_EQ((*builder)->rows(), 1u);
    auto batch = (*builder)->flush();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch->n_rows, 1u);
    EXPECT_EQ(batch->data->num_columns(), 3);
}

TEST(Nested, RejectsNonObjectLine) {
    auto top = infer_nested_schema({R"({"a":1})"});
    auto builder = NestedBuilder::make(*top);
    ASSERT_TRUE(builder.has_value());
    EXPECT_FALSE((*builder)->append_line("[1,2,3]").has_value());
    EXPECT_EQ((*builder)->rows(), 0u);
}

}  // namespace riffle
