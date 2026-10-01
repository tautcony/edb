#include "CComHelper.h"

#include <gtest/gtest.h>

TEST(CComHelperTest, NormalizesComPortPaths) {
    EXPECT_EQ(CComHelper::NormalizePortPath("COM1"), "\\\\.\\COM1");
    EXPECT_EQ(CComHelper::NormalizePortPath("COM9"), "\\\\.\\COM9");
    EXPECT_EQ(CComHelper::NormalizePortPath("COM10"), "\\\\.\\COM10");
    EXPECT_EQ(CComHelper::NormalizePortPath("com56"), "\\\\.\\com56");
    EXPECT_EQ(CComHelper::NormalizePortPath("\\\\.\\COM10"), "\\\\.\\COM10");
    EXPECT_EQ(CComHelper::NormalizePortPath("COM10x"), "COM10x");
}
