#include "EDBInterface.h"
#include "EDBLog.h"
#include <cstring>
#include <signal.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <vector>

std::vector<flashImg> imglist;

EDBInterface edb;

void showUsage() {
    std::cout << "Usage:" << std::endl;
    std::cout << "\t-f <bin file> <page> [b] (Specify 'b' to flash as boot image.)" << std::endl;
    std::cout << "\t-p <serial port> Use serial transport instead of USB MSC." << std::endl;
    std::cout << "\t--serial       Auto-detect a serial transport." << std::endl;
    std::cout << "\t-s             Use USB MSC transport." << std::endl;
    std::cout << "\t-r Reboot if all operations are done." << std::endl;
    std::cout << "\t-m Enter Mass Storage mode." << std::endl;
    std::cout << "\t-c, --check Check device connection and mount access only." << std::endl;
}

void handleInterrupt(int id) {
    (void)id;
    EDB_LOG_WARN("CLI", "Interrupted by user.");
    edb.close();
    imglist.clear();
    exit(-1);
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    signal(SIGINT, handleInterrupt);
#else
    struct sigaction sigHandler;
    sigHandler.sa_handler = handleInterrupt;
    sigaction(SIGINT, &sigHandler, NULL);
#endif

    bool reboot = false;
    bool mscmode = false;
    bool checkOnly = false;
    bool useMassStorage = true;
    const char* serialPath = nullptr;

    if (argc < 2) {
        showUsage();
        return -1;
    }

    // if (geteuid() != 0) {
    //     cout << "Please run with root privileges!" << endl;
    //     return -1;
    // }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0) {
            if (i + 2 >= argc) {
                showUsage();
                return -1;
            }
            flashImg item;
            // printf("Open: %s\n", argv[i + 1]);
            item.f.reset(fopen(argv[i + 1], "rb"));
            if (!item.f) {
                EDB_LOG_ERROR("CLI", "Unable to open firmware file: " << argv[i + 1]);
                return -1;
            }
            item.filename = argv[i + 1];
            item.toPage = atoi(argv[i + 2]);
            if (i + 3 < argc) {
                if (strcmp(argv[i + 3], "b") == 0) {
                    EDB_LOG_INFO("CLI", "Firmware will be written as boot image.");
                    item.bootImg = true;
                    i++;
                }
            }
            EDB_LOG_INFO("CLI", "Firmware target page: " << item.toPage);
            imglist.push_back(std::move(item));
            i += 2;
        }

        if (strcmp(argv[i], "-r") == 0) {
            reboot = true;
        }

        if (strcmp(argv[i], "-m") == 0) {
            mscmode = true;
        }

        if (strcmp(argv[i], "-s") == 0) {
            useMassStorage = true;
        }

        if (strcmp(argv[i], "--serial") == 0) {
            useMassStorage = false;
        }

        if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 >= argc) {
                showUsage();
                return -1;
            }
            useMassStorage = false;
            serialPath = argv[++i];
        }

        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--check") == 0) {
            checkOnly = true;
        }
    }

    if (serialPath) {
        edb.setSerialPort(serialPath);
    }
    EDB_LOG_INFO("CLI", "[1/3] Opening " << (useMassStorage ? "USB mass storage" : "serial transport") << "...");
    if (edb.open(useMassStorage)) {
        EDB_LOG_ERROR("CLI", "[1/3] Unable to open the selected transport.");
        edb.close();
        return -1;
    }
    EDB_LOG_INFO("CLI", "[2/3] Transport opened.");

    EDB_LOG_INFO("CLI", "[3/3] Sending PING and waiting for PONG...");
    if (edb.ping() == false) {
        EDB_LOG_ERROR("CLI", "[3/3] Device did not respond to PING.");
        edb.close();
        return 10;
    }
    EDB_LOG_INFO("CLI", "Device responded with PONG.");

    if (checkOnly) {
        EDB_LOG_INFO("CLI", "PASS: device connection and transport access check passed.");
        edb.close();
        return 0;
    }

    if (mscmode) {
        edb.vm_suspend();
        edb.mscmode();
    }

    edb.vm_suspend();
    for (flashImg& item : imglist) {
        edb.flash(item);
    }
    // edb.vm_reset();
    // edb.vm_resume();

    if (reboot) {
        edb.reboot();
    }

    edb.close();

    return 0;
}
