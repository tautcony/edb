#include "EDBSerialPosix.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <string>
#include <vector>

namespace {
    struct WriteStep {
        ssize_t result;
        int error;
    };

    std::vector<WriteStep> writeSteps;
    std::vector<size_t> requestedLengths;
    std::string writtenBytes;
    size_t nextWriteStep = 0;

    ssize_t scriptedWrite(int, const void* data, size_t length) {
        requestedLengths.push_back(length);
        const WriteStep step = writeSteps.at(nextWriteStep++);
        if (step.result < 0) {
            errno = step.error;
            return -1;
        }
        const size_t count = static_cast<size_t>(step.result);
        if (count > length) {
            return -1;
        }
        writtenBytes.append(static_cast<const char*>(data), count);
        return step.result;
    }

    void resetScript(const std::vector<WriteStep>& steps) {
        writeSteps = steps;
        requestedLengths.clear();
        writtenBytes.clear();
        nextWriteStep = 0;
    }
} // namespace

TEST(EDBSerialPosixTest, CompletesPartialWritesAndRetriesInterruptedWrites) {
    resetScript({{2, 0}, {-1, EINTR}, {3, 0}});

    EXPECT_EQ(edb_serial_detail::writeAll(1, "hello", 5, scriptedWrite), 5);
    EXPECT_EQ(requestedLengths, (std::vector<size_t>{5, 3, 3}));
    EXPECT_EQ(writtenBytes, "hello");
}

TEST(EDBSerialPosixTest, ReturnsPartialCountWhenWriteFailsAfterProgress) {
    resetScript({{2, 0}, {-1, EIO}});

    EXPECT_EQ(edb_serial_detail::writeAll(1, "hello", 5, scriptedWrite), 2);
    EXPECT_EQ(writtenBytes, "he");
}
