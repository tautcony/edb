#include "EDBInterface.h"
#include <cstring>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <vector>

using namespace std;

vector<flashImg> imglist;

EDBInterface edb;

void showUsage() {
    cout << "Usage:" << endl;
    cout << "\t-f <bin file> <page> [b] (Specify 'b' to flash as boot image.)" << endl;
    cout << "\t-p <serial port> Use serial transport instead of USB MSC." << endl;
    cout << "\t--serial       Auto-detect a serial transport." << endl;
    cout << "\t-s             Use USB MSC transport." << endl;
    cout << "\t-r Reboot if all operations are done." << endl;
    cout << "\t-m Enter Mass Storage mode." << endl;
    cout << "\t-c, --check Check device connection and mount access only." << endl;
}

void handleInterrupt(int id) {
    (void)id;
    printf("\nInterrupted\n");
    edb.close();
    imglist.clear();
    exit(-1);
}

int main(int argc, char* argv[]) {
    struct sigaction sigHandler;
    sigHandler.sa_handler = handleInterrupt;
    sigaction(SIGINT, &sigHandler, NULL);

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
                printf("Open: %s Failed.\n", argv[i + 1]);
                return -1;
            }
            item.filename = argv[i + 1];
            item.toPage = atoi(argv[i + 2]);
            if (i + 3 < argc) {
                if (strcmp(argv[i + 3], "b") == 0) {
                    printf("Set as boot img.\n");
                    item.bootImg = true;
                    i++;
                }
            }
            printf("Flash to page: %d\n", item.toPage);
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
    if (edb.open(useMassStorage)) {
        edb.close();
        return -1;
    }

    if (edb.ping() == false) {
        cout << "Device not responding." << endl;
        edb.close();
        return 10;
    }

    if (checkOnly) {
        cout << "Device connection and mount check passed." << endl;
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
