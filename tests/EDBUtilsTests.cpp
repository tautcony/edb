#include "EDBUtils.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

TEST(ParsePageTest, AcceptsValidRange) {
    uint32_t page = 0;
    EXPECT_TRUE(parsePage("0", &page));
    EXPECT_EQ(page, 0u);
    EXPECT_TRUE(parsePage("4294967295", &page));
    EXPECT_EQ(page, (std::numeric_limits<uint32_t>::max)());
}

TEST(ParsePageTest, RejectsInvalidInput) {
    uint32_t page = 0;
    EXPECT_FALSE(parsePage(nullptr, &page));
    EXPECT_FALSE(parsePage("", &page));
    EXPECT_FALSE(parsePage("-1", &page));
    EXPECT_FALSE(parsePage("12x", &page));
    EXPECT_FALSE(parsePage("4294967296", &page));
    EXPECT_FALSE(parsePage("1", nullptr));
}

TEST(BlockChecksumTest, UsesSeedAndWrapsAtByteBoundary) {
    const char data[] = {0x00, 0x01, 0x7F, static_cast<char>(0xFF)};
    EXPECT_EQ(blockChksum(data, 0), 0x5A);
    EXPECT_EQ(blockChksum(data, sizeof(data)), 0xD9);
    EXPECT_EQ(blockChksum(data, sizeof(data)), blockChksum(data, sizeof(data)));
}