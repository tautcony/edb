#include "EDBInterface.h"
#include "EDBLog.h"
#include "EDBTransport.h"
#include "EDBUtils.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <time.h>
#ifdef _WIN32
#include <malloc.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {
    AlignedBuffer alignedBuffer(size_t alignment, size_t size) {
#ifdef _WIN32
        return AlignedBuffer(static_cast<char*>(_aligned_malloc(size, alignment)));
#else
        void* buffer = nullptr;
        if (posix_memalign(&buffer, alignment, size) != 0) {
            return AlignedBuffer(nullptr);
        }
        return AlignedBuffer(static_cast<char*>(buffer));
#endif
    }
} // namespace

long long getTime() {
#ifdef _WIN32
    return static_cast<long long>(GetTickCount64());
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

EDBInterface::EDBInterface()
    : transport(createEDBTransport()) {}

EDBInterface::EDBInterface(std::unique_ptr<EDBTransport> transport)
    : transport(std::move(transport)) {}

EDBInterface::~EDBInterface() {
    close();
}

bool EDBInterface::waitStr(char* str) {
    int retry = 2;
    while (retry > 0) {
        EDB_LOG_DEBUG("EDB", "Waiting for device response: " << str);
        memset(wrBuf.get(), 0, wrBufSize);
        if (transport->readCommand(wrBuf.get(), wrBufSize) > 0 &&
            strcmp(str, wrBuf.get()) == 0) {
            return true;
        }
        EDB_LOG_WARN("EDB", "Device response mismatch; retry " << retry);
        transport->reset(false);
        retry--;
    }
    return false;
}

void EDBInterface::reset(bool mode) {
    transport->reset(mode == EDB_MODE_BIN);
}

bool EDBInterface::wrStr(const char* str) {
    if (!str) {
        return false;
    }
    const size_t length = strlen(str);
    if (length >= wrBufSize) {
        EDB_LOG_ERROR("EDB", "Command is too long.");
        return false;
    }
    memset(wrBuf.get(), 0, wrBufSize);
    memcpy(wrBuf.get(), str, length);
    return transport->writeCommand(wrBuf.get(), wrBufSize) ==
           static_cast<std::ptrdiff_t>(wrBufSize);
}

bool EDBInterface::wrDat(char* dat, size_t len) {
    return transport->writeData(dat, len) == static_cast<std::ptrdiff_t>(len);
}

bool EDBInterface::rdDat(char* dat, size_t len, size_t* rbcnt) {
    if (!dat || !rbcnt || len > wrBufSize) {
        return false;
    }
    memset(wrBuf.get(), 0, wrBufSize);
    const std::ptrdiff_t ret = transport->readCommand(wrBuf.get(), wrBufSize);
    if (ret <= 0) {
        *rbcnt = 0;
        return false;
    }
    const size_t bytesRead = static_cast<size_t>(ret);
    *rbcnt = bytesRead < len ? bytesRead : len;
    memcpy(dat, wrBuf.get(), *rbcnt);
    return bytesRead >= len;
}

bool EDBInterface::eraseBlock(unsigned int block) {
    char cmdbuf[64];
    snprintf(cmdbuf, sizeof(cmdbuf), "ERASEB:%d\n", block);
    if (!wrStr(cmdbuf)) {
        return false;
    }
    if (!waitStr((char*)"EROK\n")) {
        EDB_LOG_ERROR("EDB", "Erase block timed out: " << block);
        return false;
    }
    return true;
}

int EDBInterface::flash(const flashImg& item) {
    char cmdbuf[64];
    size_t cnt;
    size_t rbcnt;
    reset(EDB_MODE_TEXT);
    if (!ping()) {
        EDB_LOG_ERROR("EDB", "Device is not responding.");
        return false;
    }
    if (!wrStr("RESETDBUF\n")) {
        EDB_LOG_ERROR("EDB", "Unable to reset device buffer.");
        return false;
    }
    if (!waitStr((char*)"READY\n")) {
        EDB_LOG_ERROR("EDB", "Device is not ready after buffer reset.");
        return false;
    }
    EDB_LOG_INFO("EDB", "Writing " << item.filename << "...");
    if (fseek(item.f.get(), 0, SEEK_END) != 0) {
        EDB_LOG_ERROR("EDB", "Unable to seek firmware file.");
        return false;
    }
    const long fileSize = ftell(item.f.get());
    if (fileSize < 0) {
        EDB_LOG_ERROR("EDB", "Unable to determine firmware file size.");
        return false;
    }
    rewind(item.f.get());
    if (ferror(item.f.get())) {
        EDB_LOG_ERROR("EDB", "Unable to rewind firmware file.");
        return false;
    }
    const size_t fsize = static_cast<size_t>(fileSize);
    uint8_t chksum;
    unsigned int rcshkdum;
    long long st;
    rewind(item.f.get());

    uint32_t page_cnt = item.toPage;
    uint32_t block_cnt = page_cnt / 64;
    uint32_t last_block = 0;
    while (true) {
        block_cnt = page_cnt / 64;
        st = getTime();
        memset(sendBuf.get(), 0xFF, BIN_BLOB_SIZE);
        memset(cmdbuf, 0, sizeof(cmdbuf));
        cnt = fread(sendBuf.get(), 1, BIN_BLOB_SIZE, item.f.get());
        if (cnt == 0) {
            if (ferror(item.f.get())) {
                EDB_LOG_ERROR("EDB", "Unable to read firmware file.");
                return false;
            }
            break;
        }
        chksum = blockChksum(sendBuf.get(), BIN_BLOB_SIZE);
        reset(EDB_MODE_BIN);
        if (!wrDat(sendBuf.get(), BIN_BLOB_SIZE)) {
            EDB_LOG_ERROR("EDB", "Unable to write firmware block.");
            return false;
        }
        reset(EDB_MODE_TEXT);
        if (!wrStr("BUFCHK\n") || !rdDat(cmdbuf, 10, &rbcnt) ||
            sscanf(cmdbuf, "CHKSUM:%02x\n", &rcshkdum) != 1) {
            EDB_LOG_ERROR("EDB", "Unable to read device checksum.");
            return false;
        }
        if (rcshkdum != chksum) {
            EDB_LOG_ERROR("EDB", "Checksum error: expected " << std::hex
                                                             << static_cast<int>(chksum)
                                                             << ", got " << rcshkdum);
            return false;
        }
        if (block_cnt != last_block && !eraseBlock(block_cnt)) {
            EDB_LOG_ERROR("EDB", "Erase block timed out: " << block_cnt);
            return false;
        }
        snprintf(cmdbuf, sizeof(cmdbuf), "PROGP:%d,%d\n", page_cnt,
                 item.bootImg ? 1 : 0);
        if (!wrStr(cmdbuf)) {
            return false;
        }
        if (!waitStr((char*)"PGOK\n")) {
            EDB_LOG_ERROR("EDB", "Program page timed out: " << page_cnt);
            return false;
        }
        if (page_cnt % 200 == 0) {
            const long long elapsed = getTime() - st;
            const long long speed = elapsed > 0 ? BIN_BLOB_SIZE / elapsed : 0;
            std::ostringstream progress;
            progress << "Upload " << ftell(item.f.get()) << "/" << fsize
                     << " bytes | page " << page_cnt << " | block " << block_cnt
                     << " | checksum " << std::hex << static_cast<int>(chksum)
                     << "==" << rcshkdum << std::dec << " | " << speed << " KB/s";
            if (speed > 0) {
                progress << " | " << (fsize - ftell(item.f.get())) / speed / 1000
                         << " s remaining";
            } else {
                progress << " | remaining unknown";
            }
            edb_log::Logger::progress(progress.str());
        }
        page_cnt += BIN_BLOB_SIZE / 2048;
        last_block = block_cnt;
    }

    edb_log::Logger::endProgress();
    if (item.bootImg) {
        EDB_LOG_INFO("EDB", "Setting NCB...");
        snprintf(cmdbuf, sizeof(cmdbuf), "MKNCB: %d, %zu\n",
                 item.toPage / 64, fsize / 2048);
        if (!wrStr(cmdbuf)) {
            return false;
        }
        if (!waitStr((char*)"MKOK\n")) {
            EDB_LOG_ERROR("EDB", "Setting NCB page timed out: " << item.toPage / 64);
            return false;
        }
    }
    return true;
}

bool EDBInterface::reboot() {
    return wrStr("REBOOT\n");
}
bool EDBInterface::vm_suspend() {
    return wrStr("VMSUSPEND\n");
}
bool EDBInterface::vm_resume() {
    return wrStr("VMRESUME\n");
}
bool EDBInterface::vm_reset() {
    return wrStr("VMRESET\n");
}
bool EDBInterface::mscmode() {
    return wrStr("MSCDATA\n");
}

bool EDBInterface::ping() {
    int retry = 5;
    while (retry) {
        memset(wrBuf.get(), 0, wrBufSize);
        reset(EDB_MODE_TEXT);
        if (!wrStr("PING\n")) {
            return false;
        }
        if (transport->readCommand(wrBuf.get(), wrBufSize) > 0 &&
            strcmp(wrBuf.get(), "PONG\n") == 0) {
            return true;
        }
#ifdef _WIN32
        Sleep(2000);
#else
        sleep(2);
#endif
        retry--;
    }
    return false;
}

void EDBInterface::close() {
    if (transport) {
        transport->close();
    }
}

void EDBInterface::setSerialPort(const char* path) {
    serialPath = path;
}

int EDBInterface::open(bool useMassStorage) {
    wrBuf = alignedBuffer(512, wrBufSize);
    sendBuf = alignedBuffer(512, BIN_BLOB_SIZE);
    if (!wrBuf || !sendBuf) {
        EDB_LOG_ERROR("EDB", "Unable to allocate aligned I/O buffers.");
        close();
        return -1;
    }
    return transport->open(useMassStorage ? EDBTransportMode::MassStorage
                                          : EDBTransportMode::Serial,
                           useMassStorage ? c_mnt_path : serialPath);
}
