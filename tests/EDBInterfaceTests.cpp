#include "EDBInterface.h"
#include "EDBTransport.h"
#include "EDBUtils.h"

#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {
    class FakeTransport final : public EDBTransport {
    public:
        int openResult = 0;
        std::ptrdiff_t writeCommandResult = 512;
        std::ptrdiff_t writeDataResult = -1;
        std::vector<std::string> commandResponses;
        std::vector<std::string> commands;
        std::vector<size_t> dataWriteSizes;
        size_t commandReadIndex = 0;
        int resetCount = 0;

        int open(EDBTransportMode, const char*) override { return openResult; }
        void close() override {}
        void reset(bool) override { ++resetCount; }

        std::ptrdiff_t readCommand(char* buffer, size_t length) override {
            if (commandReadIndex >= commandResponses.size()) {
                return -1;
            }
            const std::string& response = commandResponses[commandReadIndex++];
            const size_t count = response.size() < length ? response.size() : length;
            memcpy(buffer, response.data(), count);
            return static_cast<std::ptrdiff_t>(count);
        }

        std::ptrdiff_t writeCommand(const char* buffer, size_t length) override {
            commands.emplace_back(buffer, length);
            return writeCommandResult;
        }

        std::ptrdiff_t readData(char*, size_t) override { return -1; }

        std::ptrdiff_t writeData(const char*, size_t length) override {
            dataWriteSizes.push_back(length);
            return writeDataResult;
        }
    };

    std::unique_ptr<EDBInterface> makeInterface(FakeTransport** transport) {
        auto fake = std::unique_ptr<FakeTransport>(new FakeTransport());
        *transport = fake.get();
        return std::unique_ptr<EDBInterface>(
            new EDBInterface(std::unique_ptr<EDBTransport>(fake.release())));
    }
}

TEST(EDBInterfaceTest, PingWritesCommandAndAcceptsPong) {
    FakeTransport* transport = nullptr;
    std::unique_ptr<EDBInterface> interface = makeInterface(&transport);
    ASSERT_EQ(interface->open(false), 0);
    transport->commandResponses.push_back("PONG\n");

    EXPECT_TRUE(interface->ping());
    ASSERT_EQ(transport->commands.size(), 1u);
    EXPECT_EQ(transport->commands[0].substr(0, 5), "PING\n");
}

TEST(EDBInterfaceTest, RejectsShortCommandWrite) {
    FakeTransport* transport = nullptr;
    std::unique_ptr<EDBInterface> interface = makeInterface(&transport);
    ASSERT_EQ(interface->open(false), 0);
    transport->writeCommandResult = 1;

    EXPECT_FALSE(interface->wrStr("PING\n"));
}

TEST(EDBInterfaceTest, ReportsShortResponseAndCopiesOnlyAvailableBytes) {
    FakeTransport* transport = nullptr;
    std::unique_ptr<EDBInterface> interface = makeInterface(&transport);
    ASSERT_EQ(interface->open(false), 0);
    transport->commandResponses.push_back("OK");
    char response[8] = {};
    size_t bytesRead = 0;

    EXPECT_FALSE(interface->rdDat(response, 5, &bytesRead));
    EXPECT_EQ(bytesRead, 2u);
    EXPECT_EQ(std::string(response, bytesRead), "OK");
}

TEST(EDBInterfaceTest, RetriesAfterUnexpectedResponse) {
    FakeTransport* transport = nullptr;
    std::unique_ptr<EDBInterface> interface = makeInterface(&transport);
    ASSERT_EQ(interface->open(false), 0);
    transport->commandResponses.push_back("NOPE\n");
    transport->commandResponses.push_back("READY\n");

    EXPECT_TRUE(interface->waitStr(const_cast<char*>("READY\n")));
    EXPECT_EQ(transport->resetCount, 1);
}

TEST(EDBInterfaceTest, FlashesOneCompleteBlockAfterChecksumConfirmation) {
    FakeTransport* transport = nullptr;
    std::unique_ptr<EDBInterface> interface = makeInterface(&transport);
    ASSERT_EQ(interface->open(false), 0);
    transport->writeDataResult = BIN_BLOB_SIZE;

    std::vector<char> block(BIN_BLOB_SIZE, static_cast<char>(0xFF));
    block[0] = 1;
    const unsigned int checksum = blockChksum(block.data(), block.size());
    char checksumResponse[16] = {};
    snprintf(checksumResponse, sizeof(checksumResponse), "CHKSUM:%02x\n", checksum);
    transport->commandResponses = {"PONG\n", "READY\n", checksumResponse, "EROK\n", "PGOK\n"};

    FILE* file = tmpfile();
    ASSERT_NE(file, nullptr);
    ASSERT_EQ(fwrite(block.data(), 1, block.size(), file), block.size());
    rewind(file);
    flashImg image;
    image.f.reset(file);
    image.filename = const_cast<char*>("one-block.bin");
    image.toPage = 64;

    EXPECT_TRUE(interface->flash(image));
    ASSERT_EQ(transport->dataWriteSizes.size(), 1u);
    EXPECT_EQ(transport->dataWriteSizes[0], BIN_BLOB_SIZE);
}

TEST(EDBInterfaceTest, EmptyFirmwareDoesNotWriteADataBlock) {
    FakeTransport* transport = nullptr;
    std::unique_ptr<EDBInterface> interface = makeInterface(&transport);
    ASSERT_EQ(interface->open(false), 0);
    transport->commandResponses.push_back("PONG\n");
    transport->commandResponses.push_back("READY\n");

    FILE* file = tmpfile();
    ASSERT_NE(file, nullptr);
    flashImg image;
    image.f.reset(file);
    image.filename = const_cast<char*>("empty.bin");

    EXPECT_TRUE(interface->flash(image));
    EXPECT_TRUE(transport->dataWriteSizes.empty());
}