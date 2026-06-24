#include "riffle/schema.hpp"
#include "riffle/types.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace riffle {
namespace {

ColumnType resolve(std::vector<ColumnType> seen, TypeConflictPolicy policy) {
    return resolve_type_conflict(seen, policy).value();
}

}  // namespace

TEST(ResolveTypeConflict, EmptyMeansNull) {
    EXPECT_EQ(resolve({}, TypeConflictPolicy::WIDEN), ColumnType::NULLTYPE);
}

TEST(ResolveTypeConflict, OnlyNullStaysNull) {
    EXPECT_EQ(resolve({ColumnType::NULLTYPE, ColumnType::NULLTYPE}, TypeConflictPolicy::WIDEN),
              ColumnType::NULLTYPE);
}

TEST(ResolveTypeConflict, NullIsIgnoredAlongsideRealType) {
    EXPECT_EQ(resolve({ColumnType::INT64, ColumnType::NULLTYPE}, TypeConflictPolicy::WIDEN),
              ColumnType::INT64);
}

TEST(ResolveTypeConflict, WidenIntAndDoubleToDouble) {
    EXPECT_EQ(resolve({ColumnType::INT64, ColumnType::DOUBLE}, TypeConflictPolicy::WIDEN),
              ColumnType::DOUBLE);
}

TEST(ResolveTypeConflict, WidenIncompatibleFallsBackToString) {
    EXPECT_EQ(resolve({ColumnType::INT64, ColumnType::BOOL}, TypeConflictPolicy::WIDEN),
              ColumnType::STRING);
}

TEST(ResolveTypeConflict, StringPolicyAlwaysString) {
    EXPECT_EQ(resolve({ColumnType::INT64, ColumnType::DOUBLE}, TypeConflictPolicy::STRING),
              ColumnType::STRING);
}

TEST(ResolveTypeConflict, ErrorPolicyRejectsConflict) {
    std::vector<ColumnType> seen{ColumnType::INT64, ColumnType::STRING};
    EXPECT_FALSE(resolve_type_conflict(seen, TypeConflictPolicy::ERROR).has_value());
}

}  // namespace riffle
