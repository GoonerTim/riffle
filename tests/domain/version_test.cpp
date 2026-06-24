#include "riffle/version.hpp"

#include <gtest/gtest.h>

TEST(Version, ReturnsProjectVersion) {
    EXPECT_EQ(riffle::version(), "0.2.1");
}
