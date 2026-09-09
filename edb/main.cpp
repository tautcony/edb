#include "EDBInterface.h"
#include "EDBLog.h"
#include "EDBUtils.h"
#include <cerrno>
#include <climits>
#include <cstring>
#include <signal.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <cstdlib>
#include <limits>
#include <vector>

std::vector<flashImg> imglist;

EDBInterface edb;
volatile sig_atomic_t interruptRequested = 0;

void showUsage() {
    std::cout << "Usage: edb [options]\n\n"
              << "Actions:\n"
              << "  -f, --file <path> <page> [b]  Flash a binary image; add 'b' for boot image.\n"
              << "  -m, --msc                    Enter mass-storage mode after connecting.\n"
              << "  -c, --check                  Check the connection, then exit.\n\n"
              << "Transport:\n"
              << "  -s, --mass-storage           Use USB mass storage (default).\n"
              << "      --serial                 Auto-detect a serial transport.\n"
              << "  -p, --port <path>            Use the specified serial port.\n\n"
              << "Other:\n"
              << "  -r, --reboot                 Reboot after all operations complete.\n"
              << "  -h, --help                  Show this help and exit.\n";
}

void handleInterrupt(int id) {
    (void)id;
    interruptRequested = 1;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    signal(SIGINT, handleInterrupt);
#else
    struct sigaction sigHandler = {};
    sigHandler.sa_handler = handleInterrupt;
    sigemptyset(&sigHandler.sa_mask);
    sigaction(SIGINT, &sigHandler, NULL);
#endif

    bool reboot = false;
    bool mscmode = false;
    bool checkOnly = false;
    bool useMassStorage = true;
    bool transportSelected = false;
    const char* serialPath = nullptr;

    if (argc < 2) {
        showUsage();
        return 1;
    }

    // if (geteuid() != 0) {
    //     cout << "Please run with root privileges!" << endl;
    //     return -1;
    // }

    for (int i = 1; i < argc; i++) {
        const char* argument = argv[i];
        if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
            showUsage();
            return 0;
        } else if (strcmp(argument, "-f") == 0 ||
                   strcmp(argument, "--file") == 0) {
            if (i + 2 >= argc) {
                EDB_LOG_ERROR("CLI", "Option " << argument
                                                 << " requires <path> and <page>.");
                showUsage();
                return 2;
            }
            flashImg item;
            if (!parsePage(argv[i + 2], &item.toPage)) {
                EDB_LOG_ERROR("CLI", "Invalid flash page: " << argv[i + 2]);
                return 2;
            }
            item.f.reset(fopen(argv[i + 1], "rb"));
            if (!item.f) {
                EDB_LOG_ERROR("CLI", "Unable to open firmware file: " << argv[i + 1]);
                return 2;
            }
            item.filename = argv[i + 1];
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
        } else if (strcmp(argument, "-r") == 0 ||
                   strcmp(argument, "--reboot") == 0) {
            reboot = true;
        } else if (strcmp(argument, "-m") == 0 ||
                   strcmp(argument, "--msc") == 0) {
            mscmode = true;
        } else if (strcmp(argument, "-s") == 0 ||
                   strcmp(argument, "--mass-storage") == 0) {
            if (transportSelected && !useMassStorage) {
                EDB_LOG_ERROR("CLI", "Options --mass-storage and serial transport cannot be combined.");
                return 2;
            }
            useMassStorage = true;
            transportSelected = true;
        } else if (strcmp(argument, "--serial") == 0) {
            if (transportSelected && useMassStorage) {
                EDB_LOG_ERROR("CLI", "Options --serial and --mass-storage cannot be combined.");
                return 2;
            }
            useMassStorage = false;
            transportSelected = true;
        } else if (strcmp(argument, "-p") == 0 ||
                   strcmp(argument, "--port") == 0) {
            if (i + 1 >= argc) {
                EDB_LOG_ERROR("CLI", "Option " << argument << " requires <path>.");
                showUsage();
                return 2;
            }
            if (transportSelected && useMassStorage) {
                EDB_LOG_ERROR("CLI", "Options --port and --mass-storage cannot be combined.");
                return 2;
            }
            useMassStorage = false;
            serialPath = argv[++i];
            transportSelected = true;
        } else if (strcmp(argument, "-c") == 0 ||
                   strcmp(argument, "--check") == 0) {
            checkOnly = true;
        } else {
            EDB_LOG_ERROR("CLI", "Unknown option: " << argument);
            showUsage();
            return 2;
        }
    }

    if (serialPath) {
        edb.setSerialPort(serialPath);
    }
    EDB_LOG_INFO("CLI", "Opening " << (useMassStorage ? "USB mass storage" : "serial transport") << "...");
    if (edb.open(useMassStorage)) {
        EDB_LOG_ERROR("CLI", "Unable to open the selected transport.");
        edb.close();
        return -1;
    }
    EDB_LOG_INFO("CLI", "Transport opened.");

    EDB_LOG_INFO("CLI", "Sending PING and waiting for PONG...");
    if (edb.ping() == false) {
        EDB_LOG_ERROR("CLI", "Device did not respond to PING.");
        edb.close();
        return 10;
    }
    EDB_LOG_INFO("CLI", "Device responded with PONG.");

    if (interruptRequested) {
        EDB_LOG_WARN("CLI", "Interrupted by user.");
        edb.close();
        return 130;
    }

    if (checkOnly) {
        EDB_LOG_INFO("CLI", "PASS: device connection and transport access check passed.");
        edb.close();
        return 0;
    }

    if (mscmode) {
        if (!edb.vm_suspend() || !edb.mscmode()) {
            EDB_LOG_ERROR("CLI", "Unable to enter mass-storage mode.");
            edb.close();
            return 11;
        }
    }

    bool flashSucceeded = edb.vm_suspend();
    if (!flashSucceeded) {
        EDB_LOG_ERROR("CLI", "Unable to suspend the device VM.");
    }
    for (flashImg& item : imglist) {
        if (!flashSucceeded || interruptRequested) {
            flashSucceeded = false;
            break;
        }
        if (edb.flash(item) != 0) {
            flashSucceeded = false;
            break;
        }
    }
    // edb.vm_reset();
    // edb.vm_resume();

    if (flashSucceeded && reboot && !edb.reboot()) {
        EDB_LOG_ERROR("CLI", "Unable to reboot the device.");
        flashSucceeded = false;
    }

    edb.close();

    if (interruptRequested) {
        EDB_LOG_WARN("CLI", "Interrupted by user.");
        return 130;
    }
    return flashSucceeded ? 0 : 11;
}
