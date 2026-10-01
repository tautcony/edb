#include "EDBCLIOptions.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

TEST(EDBCLIOptionsTest, ParsesRepeatedFileOptionsIndependently) {
    const std::string firstPath = "edb-cli-first-image-test.bin";
    const std::string secondPath = "edb-cli-second-image-test.bin";
    {
        std::ofstream first(firstPath.c_str(), std::ios::binary);
        std::ofstream second(secondPath.c_str(), std::ios::binary);
        ASSERT_TRUE(first.good());
        ASSERT_TRUE(second.good());
        first << "first";
        second << "second";
    }

    CLI::App app{"EDB CLI file option test"};
    std::vector<flashImg> images;
    std::deque<std::string> imageNames;
    addFileOption(app, images, imageNames);
    const std::string arguments = "-f " + firstPath + " 64 -f " + secondPath + " 128 b";

    EXPECT_NO_THROW(app.parse(arguments));
    ASSERT_EQ(images.size(), 2u);
    EXPECT_EQ(images[0].toPage, 64u);
    EXPECT_FALSE(images[0].bootImg);
    EXPECT_EQ(images[1].toPage, 128u);
    EXPECT_TRUE(images[1].bootImg);
    EXPECT_STREQ(images[0].filename, firstPath.c_str());
    EXPECT_STREQ(images[1].filename, secondPath.c_str());

    std::remove(firstPath.c_str());
    std::remove(secondPath.c_str());
}
