#include "riffle/reader.hpp"

#include <gtest/gtest.h>

#include <sstream>

#include "riffle/constants.hpp"

namespace riffle {

TEST(LineReader, SplitsOnNewline) {
    std::istringstream in("alpha\nbeta\n");
    StdByteSource src(in);
    LineReader reader(src);
    EXPECT_EQ(reader.next(), "alpha");
    EXPECT_EQ(reader.next(), "beta");
    EXPECT_FALSE(reader.next().has_value());
}

TEST(LineReader, SkipsEmptyLines) {
    std::istringstream in("a\n\n\nb\n");
    StdByteSource src(in);
    LineReader reader(src);
    EXPECT_EQ(reader.next(), "a");
    EXPECT_EQ(reader.next(), "b");
    EXPECT_FALSE(reader.next().has_value());
}

TEST(LineReader, HandlesMissingTrailingNewline) {
    std::istringstream in("a\nb");
    StdByteSource src(in);
    LineReader reader(src);
    EXPECT_EQ(reader.next(), "a");
    EXPECT_EQ(reader.next(), "b");
    EXPECT_FALSE(reader.next().has_value());
}

TEST(LineReader, StripsCarriageReturn) {
    std::istringstream in("a\r\nb\r\n");
    StdByteSource src(in);
    LineReader reader(src);
    EXPECT_EQ(reader.next(), "a");
    EXPECT_EQ(reader.next(), "b");
}

TEST(LineReader, TruncatesOverLongLineAndResyncs) {
    std::istringstream in("abcdef\nxy\n");
    StdByteSource src(in);
    LineReader reader(src, /*max_line=*/4);
    EXPECT_EQ(reader.next(), "abcd");
    EXPECT_EQ(reader.next(), "xy");
    EXPECT_FALSE(reader.next().has_value());
}

TEST(LineReader, ReadsLineSpanningBufferBoundary) {
    std::istringstream in("abcdefgh\nij\n");
    StdByteSource src(in);
    LineReader reader(src, MAX_LINE_BYTES, /*buffer=*/4);
    EXPECT_EQ(reader.next(), "abcdefgh");
    EXPECT_EQ(reader.next(), "ij");
    EXPECT_FALSE(reader.next().has_value());
}

}  // namespace riffle
