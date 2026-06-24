#include "riffle/timestamp.hpp"

#include <gtest/gtest.h>

namespace riffle {

TEST(ParseTimestampUs, EpochIsZero) {
    EXPECT_EQ(parse_timestamp_us("1970-01-01T00:00:00Z").value(), 0);
}

TEST(ParseTimestampUs, OneDayLater) {
    EXPECT_EQ(parse_timestamp_us("1970-01-02T00:00:00Z").value(), 86400LL * 1000000);
}

TEST(ParseTimestampUs, TimeOfDay) {
    EXPECT_EQ(parse_timestamp_us("1970-01-01T01:02:03Z").value(), 3723LL * 1000000);
}

TEST(ParseTimestampUs, FractionalSeconds) {
    EXPECT_EQ(parse_timestamp_us("1970-01-01T00:00:00.500Z").value(), 500000);
}

TEST(ParseTimestampUs, RejectsGarbage) {
    EXPECT_FALSE(parse_timestamp_us("hello").has_value());
}

TEST(ParseTimestampUs, RejectsOutOfRange) {
    EXPECT_FALSE(parse_timestamp_us("2026-13-01T00:00:00Z").has_value());
}

TEST(LooksLikeTimestamp, AcceptsIso) {
    EXPECT_TRUE(looks_like_timestamp("2026-06-24T10:00:00Z"));
}

TEST(LooksLikeTimestamp, RejectsPlainString) {
    EXPECT_FALSE(looks_like_timestamp("info"));
}

}  // namespace riffle
