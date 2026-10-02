#include "EDBUtils.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

TEST(ParsePageTest, AcceptsValidRange) {
    const auto zero = parsePage("0");
    ASSERT_TRUE(zero);
    EXPECT_EQ(*zero, 0u);

    const auto maximum = parsePage("4294967295");
    ASSERT_TRUE(maximum);
    EXPECT_EQ(*maximum, (std::numeric_limits<uint32_t>::max)());
}

TEST(ParsePageTest, RejectsInvalidInput) {
    EXPECT_FALSE(parsePage(""));
    EXPECT_FALSE(parsePage("-1"));
    EXPECT_FALSE(parsePage("12x"));
    EXPECT_FALSE(parsePage("4294967296"));
}

TEST(BlockChecksumTest, UsesSeedAndWrapsAtByteBoundary) {
    const char data[] = {0x00, 0x01, 0x7F, static_cast<char>(0xFF)};
    EXPECT_EQ(blockChksum(data, 0), 0x5A);
    EXPECT_EQ(blockChksum(data, sizeof(data)), 0xD9);
    EXPECT_EQ(blockChksum(data, sizeof(data)), blockChksum(data, sizeof(data)));
}
