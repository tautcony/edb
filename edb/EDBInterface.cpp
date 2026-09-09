#include "EDBInterface.h"
#include "EDBTransport.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <time.h>
#include <unistd.h>

using namespace std;

namespace {
    AlignedBuffer alignedBuffer(size_t alignment, size_t size) {
        void* buffer = nullptr;
        if (posix_memalign(&buffer, alignment, size) != 0) {
            return AlignedBuffer(nullptr);
        }
        return AlignedBuffer(static_cast<char*>(buffer));
    }
} // namespace

long long getTime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

unsigned char blockChksum(char* block, unsigned int blockSize) {
    unsigned char sum = 0x5A;
    for (unsigned int i = 0; i < blockSize; i++) {
        sum += block[i];
    }
    return sum;
}

EDBInterface::EDBInterface() : transport(createEDBTransport()) {}

EDBInterface::~EDBInterface() {
    close();
}

bool EDBInterface::waitStr(char* str) {
    int retry = 2;
    while (retry > 0) {
        cout << "Waiting Sync...\r";
        memset(wrBuf.get(), 0, wrBufSize);
        if (transport->readCommand(wrBuf.get(), wrBufSize) > 0 &&
            strcmp(str, wrBuf.get()) == 0) {
            return true;
        }
        cout << "Sync Failed... Retry: " << retry << endl;
        transport->reset(false);
        retry--;
    }
    return false;
}

void EDBInterface::reset(bool mode) {
    transport->reset(mode == EDB_MODE_BIN);
}

void EDBInterface::wrStr(const char* str) {
    if (!str) {
        return;
    }
    memset(wrBuf.get(), 0, wrBufSize);
    strcpy(wrBuf.get(), str);
    transport->writeCommand(wrBuf.get(), wrBufSize);
}

bool EDBInterface::wrDat(char* dat, size_t len) {
    return transport->writeData(dat, len) == static_cast<std::ptrdiff_t>(len);
}

bool EDBInterface::rdDat(char* dat, size_t len, size_t* rbcnt) {
    memset(wrBuf.get(), 0, wrBufSize);
    const std::ptrdiff_t ret = transport->readCommand(wrBuf.get(), wrBufSize);
    memcpy(dat, wrBuf.get(), len);
    *rbcnt = len;
    return ret > 0;
}

bool EDBInterface::eraseBlock(unsigned int block) {
    char cmdbuf[64];
    snprintf(cmdbuf, sizeof(cmdbuf), "ERASEB:%d\n", block);
    wrStr(cmdbuf);
    if (!waitStr((char*)"EROK\n")) {
        printf("Erase block timed out: %d\n", block);
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
        cout << "Device not responding." << endl;
        return false;
    }
    wrStr("RESETDBUF\n");
    if (!waitStr((char*)"READY\n")) {
        cout << "Device not responding." << endl;
        return false;
    }
    cout << "Writing: " << item.filename << "..." << endl;
    fseek(item.f.get(), 0, SEEK_END);
    size_t fsize = ftell(item.f.get());
    uint8_t chksum;
    uint32_t rcshkdum;
    long long st;
    rewind(item.f.get());

    uint32_t page_cnt = item.toPage;
    uint32_t block_cnt = page_cnt / 64;
    uint32_t last_block = 0;
    do {
        block_cnt = page_cnt / 64;
        st = getTime();
        memset(sendBuf.get(), 0xFF, BIN_BLOB_SIZE);
        memset(cmdbuf, 0, sizeof(cmdbuf));
        cnt = fread(sendBuf.get(), 1, BIN_BLOB_SIZE, item.f.get());
        chksum = blockChksum(sendBuf.get(), BIN_BLOB_SIZE);
        reset(EDB_MODE_BIN);
        wrDat(sendBuf.get(), BIN_BLOB_SIZE);
        reset(EDB_MODE_TEXT);
        wrStr("BUFCHK\n");
        rdDat(cmdbuf, 10, &rbcnt);
        sscanf(cmdbuf, "CHKSUM:%02x\n", &rcshkdum);
        if (rcshkdum != chksum) {
            printf("chksum error: expecting %02x, got %02x instead\n",
                   chksum, rcshkdum);
            return false;
        }
        if (block_cnt != last_block && !eraseBlock(block_cnt)) {
            printf("Erase Block Time Out: %d\n", block_cnt);
            return false;
        }
        snprintf(cmdbuf, sizeof(cmdbuf), "PROGP:%d,%d\n", page_cnt,
                 item.bootImg ? 1 : 0);
        wrStr(cmdbuf);
        if (!waitStr((char*)"PGOK\n")) {
            printf("Program page timed out: %d\n", page_cnt);
            return false;
        }
        if (page_cnt % 200 == 0) {
            const long long elapsed = getTime() - st;
            const long long speed = elapsed > 0 ? BIN_BLOB_SIZE / elapsed : 0;
            cout << "Upload: " << ftell(item.f.get()) << "/" << fsize;
            cout << " Page: " << page_cnt << " Block: " << block_cnt;
            printf("  chksum: %02x==%02x, %lld KB/s", chksum, rcshkdum, speed);
            if (speed > 0) {
                cout << "  " << (fsize - ftell(item.f.get())) / speed / 1000;
            } else {
                cout << "  ?";
            }
            cout << "s remaining        \r";
            fflush(stdout);
        }
        page_cnt += BIN_BLOB_SIZE / 2048;
        last_block = block_cnt;
    } while (cnt > 0);

    if (item.bootImg) {
        cout << "\nSetting NCB..." << endl;
        snprintf(cmdbuf, sizeof(cmdbuf), "MKNCB: %d, %zu\n",
                 item.toPage / 64, fsize / 2048);
        wrStr(cmdbuf);
        if (!waitStr((char*)"MKOK\n")) {
            printf("Setting NCB page timed out: %d\n", item.toPage / 64);
            return false;
        }
    }
    return true;
}

void EDBInterface::reboot() {
    wrStr("REBOOT\n");
}
void EDBInterface::vm_suspend() {
    wrStr("VMSUSPEND\n");
}
void EDBInterface::vm_resume() {
    wrStr("VMRESUME\n");
}
void EDBInterface::vm_reset() {
    wrStr("VMRESET\n");
}
void EDBInterface::mscmode() {
    wrStr("MSCDATA\n");
}

bool EDBInterface::ping() {
    int retry = 5;
    while (retry) {
        memset(wrBuf.get(), 0, wrBufSize);
        reset(EDB_MODE_TEXT);
        wrStr("PING\n");
        if (transport->readCommand(wrBuf.get(), wrBufSize) > 0 &&
            strcmp(wrBuf.get(), "PONG\n") == 0) {
            return true;
        }
        sleep(2);
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
        cerr << "Unable to allocate aligned I/O buffers." << endl;
        close();
        return -1;
    }
    return transport->open(useMassStorage ? EDBTransportMode::MassStorage
                                          : EDBTransportMode::Serial,
                           useMassStorage ? c_mnt_path : serialPath);
}
