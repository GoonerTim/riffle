#include "riffle/reader.hpp"

#include <gtest/gtest.h>

#include <sstream>

namespace riffle {

TEST(LineReader, SplitsOnNewline) {
    std::istringstream in("alpha\nbeta\n");
    LineReader reader(in);
    EXPECT_EQ(reader.next(), "alpha");
    EXPECT_EQ(reader.next(), "beta");
    EXPECT_FALSE(reader.next().has_value());
}

TEST(LineReader, SkipsEmptyLines) {
    std::istringstream in("a\n\n\nb\n");
    LineReader reader(in);
    EXPECT_EQ(reader.next(), "a");
    EXPECT_EQ(reader.next(), "b");
    EXPECT_FALSE(reader.next().has_value());
}

TEST(LineReader, HandlesMissingTrailingNewline) {
    std::istringstream in("a\nb");
    LineReader reader(in);
    EXPECT_EQ(reader.next(), "a");
    EXPECT_EQ(reader.next(), "b");
    EXPECT_FALSE(reader.next().has_value());
}

TEST(LineReader, StripsCarriageReturn) {
    std::istringstream in("a\r\nb\r\n");
    LineReader reader(in);
    EXPECT_EQ(reader.next(), "a");
    EXPECT_EQ(reader.next(), "b");
}

}
