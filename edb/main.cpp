#include "EDBInterface.h"
#include "EDBLog.h"
#include "EDBUtils.h"

#include <CLI/CLI.hpp>

#include <csignal>
#include <cstdio>
#include <deque>
#include <iostream>
#include <string>
#include <vector>

std::vector<flashImg> imglist;
std::deque<std::string> imageNames;

EDBInterface edb;
volatile sig_atomic_t interruptRequested = 0;

void handleInterrupt(int id) {
    (void)id;
    interruptRequested = 1;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, handleInterrupt);

    CLI::App app{"EDB Embedded Device Bootloader"};
    app.set_help_flag("-h,--help", "Show this help and exit.");

    bool checkOnly = false;
    bool mscAction = false;
    bool reboot = false;
    bool massStorageSelected = false;
    bool serialSelected = false;
    std::string serialPath;

    app.add_option_function<std::vector<std::string>>(
           "-f,--file",
           [](const std::vector<std::string>& values) {
               if (values.size() < 2 || values.size() > 3) {
                   throw CLI::ValidationError("--file requires <path> <page> [b]");
               }
               flashImg item;
               if (!parsePage(values[1].c_str(), &item.toPage)) {
                   throw CLI::ValidationError("Invalid flash page: " + values[1]);
               }
               item.f.reset(fopen(values[0].c_str(), "rb"));
               if (!item.f) {
                   throw CLI::ValidationError("Unable to open firmware file: " + values[0]);
               }
               imageNames.push_back(values[0]);
               item.filename = const_cast<char*>(imageNames.back().c_str());
               if (values.size() == 3) {
                   if (values[2] != "b") {
                       throw CLI::ValidationError("The optional --file argument must be 'b'");
                   }
                   item.bootImg = true;
               }
               EDB_LOG_INFO("CLI", "Firmware target page: " << item.toPage);
               imglist.push_back(std::move(item));
           },
           "Flash a binary image: <path> <page> [b].")
        ->expected(2, 3)
        ->multi_option_policy(CLI::MultiOptionPolicy::TakeAll);

    app.add_flag("-m,--msc", mscAction,
                 "Switch MSC to system-data mode via CDC, then exit.");
    app.add_flag("-c,--check", checkOnly,
                 "Check the selected transport, then exit.");
    app.add_flag("-r,--reboot", reboot,
                 "Reboot after all requested operations complete.");
    app.add_flag("-s,--mass-storage", massStorageSelected,
                 "Use MSC transport (default).");
    app.add_flag("--serial", serialSelected,
                 "Use CDC transport and auto-detect the port.");
    app.add_option("-p,--port", serialPath,
                   "Use the specified CDC port.");

    try {
        if (argc < 2) {
            std::cout << app.help() << std::endl;
            return 1;
        }
        app.parse(argc, argv);
    } catch (const CLI::ParseError& error) {
        return app.exit(error);
    }

    const bool hasPort = !serialPath.empty();
    if (massStorageSelected && (serialSelected || hasPort)) {
        EDB_LOG_ERROR("CLI", "MSC and CDC transport options cannot be combined.");
        return 2;
    }
    if (checkOnly && (mscAction || reboot || !imglist.empty())) {
        EDB_LOG_ERROR("CLI", "--check cannot be combined with other actions.");
        return 2;
    }
    if (mscAction && (reboot || !imglist.empty())) {
        EDB_LOG_ERROR("CLI", "--msc cannot be combined with flashing or reboot.");
        return 2;
    }

    const bool useMassStorage = !serialSelected && !hasPort;
    if (!checkOnly && !useMassStorage) {
        EDB_LOG_ERROR("CLI", "CDC transport currently supports status checks only; "
                             "EDB control and flash commands require MSC.");
        return 2;
    }
    if (hasPort) {
        edb.setSerialPort(serialPath.c_str());
    }

    const bool needsMscTransport = !checkOnly;
    if (needsMscTransport) {
        EDB_LOG_INFO("CLI", "Opening " << (useMassStorage ? "MSC" : "CDC") << "...");
        if (edb.open(useMassStorage) != 0) {
            EDB_LOG_ERROR("CLI", "Unable to open the selected transport.");
            edb.close();
            return 1;
        }
        EDB_LOG_INFO("CLI", "Transport opened.");
    } else {
        EDB_LOG_INFO("CLI", "Status check will use CDC.");
    }

    if (checkOnly) {
        EDB_LOG_INFO("CLI", "Opening CDC status check...");
        const bool checked = edb.checkViaCdc();
        if (!checked) {
            EDB_LOG_ERROR("CLI", "CDC status check failed.");
            edb.close();
            return 10;
        }
        EDB_LOG_INFO("CLI", "PASS: transport and CDC status check passed.");
        edb.close();
        return 0;
    }

    if (mscAction) {
        EDB_LOG_INFO("CLI", "Switching MSC to system-data mode via MSC command port...");
        const bool switched = edb.mscmode();
        edb.close();
        if (!switched) {
            EDB_LOG_ERROR("CLI", "Unable to switch MSC to system-data mode.");
            return 11;
        }
        EDB_LOG_INFO("CLI", "MSC system-data mode requested.");
        EDB_LOG_INFO("CLI", "MSC system-data mode is now active. Restart or exit this mode "
                            "on the device before running EDB command operations again.");
        return 0;
    }

    bool operationSucceeded = true;
    if (!imglist.empty()) {
        operationSucceeded = edb.vm_suspend();
        if (!operationSucceeded) {
            EDB_LOG_ERROR("CLI", "Unable to suspend the device VM.");
        }
        for (flashImg& item : imglist) {
            if (!operationSucceeded || interruptRequested) {
                operationSucceeded = false;
                break;
            }
            if (edb.flash(item) != 0) {
                operationSucceeded = false;
                break;
            }
        }
    }

    if (operationSucceeded && reboot && !edb.reboot()) {
        EDB_LOG_ERROR("CLI", "Unable to reboot the device.");
        operationSucceeded = false;
    } else if (operationSucceeded && !reboot && !imglist.empty() &&
               !edb.vm_resume()) {
        EDB_LOG_ERROR("CLI", "Unable to resume the device VM.");
        operationSucceeded = false;
    }

    edb.close();
    if (interruptRequested) {
        EDB_LOG_WARN("CLI", "Interrupted by user.");
        return 130;
    }
    return operationSucceeded ? 0 : 11;
}
