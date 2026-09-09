#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>

#define EOS_VID 0xCAFE
#define EOS_PID 0x4003
#define EDB_MODE_BIN true
#define EDB_MODE_TEXT false
#define BIN_BLOCK_SIZE 32768
#define BIN_BLOB_SIZE BIN_BLOCK_SIZE

class EDBTransport;

struct AlignedBufferDeleter {
    void operator()(char* buffer) const {
#ifdef _WIN32
        _aligned_free(buffer);
#else
        free(buffer);
#endif
    }
};

using AlignedBuffer = std::unique_ptr<char, AlignedBufferDeleter>;

typedef struct flashImg {
    struct FileDeleter {
        void operator()(FILE* file) const {
            if (file) {
                fclose(file);
            }
        }
    };
    using FilePtr = std::unique_ptr<FILE, FileDeleter>;

    FilePtr f;
    char* filename = nullptr;
    uint32_t toPage = 0;
    bool bootImg = false;
} flashImg;

class EDBInterface {
private:
    static const size_t wrBufSize = 512;
    std::unique_ptr<EDBTransport> transport;
    AlignedBuffer wrBuf;
    AlignedBuffer sendBuf;
    const char* serialPath = nullptr;

public:
    char* c_mnt_path = (char*)"/tmp/edbMount";

    EDBInterface();
    ~EDBInterface();
    EDBInterface(const EDBInterface&) = delete;
    EDBInterface& operator=(const EDBInterface&) = delete;

    void reset(bool mode);
    bool waitStr(char* str);
    bool wrStr(const char* str);
    bool wrDat(char* dat, size_t len);
    bool rdDat(char* dat, size_t len, size_t* rbcnt);
    bool eraseBlock(unsigned int block);
    int flash(const flashImg& item);
    bool reboot();
    bool vm_suspend();
    bool vm_resume();
    bool vm_reset();
    bool mscmode();
    bool ping();
    void close();
    void setSerialPort(const char* path);
    int open(bool mode);
};
