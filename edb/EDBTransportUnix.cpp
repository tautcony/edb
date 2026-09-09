#include "EDBSerialPosix.h"
#include "EDBTransport.h"
#include "EDBLog.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {
    std::string shellQuote(const std::string& value) {
        std::string quoted = "'";
        for (char ch : value) {
            if (ch == '\'') {
                quoted += "'\\''";
            } else {
                quoted += ch;
            }
        }
        quoted += "'";
        return quoted;
    }

    std::string runCommand(const std::string& command) {
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            return std::string();
        }
        std::string output;
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            output += buffer;
        }
        pclose(pipe);
        return output;
    }

    class EDBTransportUnix final : public EDBTransport {
    private:
        int hCMDf = -1;
        int hDATf = -1;
        std::string devicePath;
        bool mounted = false;
        bool serialMode = false;
        EDBSerialPosix serial;

        static char* findField(char* line, const char* field) {
            char* position = strstr(line, field);
            if (!position) {
                return nullptr;
            }
            position += strlen(field);
            char* endPosition = strchr(position, '"');
            if (!endPosition || position == endPosition) {
                return nullptr;
            }
            *endPosition = '\0';
            return position;
        }

        bool findDevice() {
            FILE* lsblkFile = popen(
                "lsblk -d -P -p -o HOTPLUG,VENDOR,LABEL,KNAME", "r");
            if (!lsblkFile) {
                return false;
            }

            char line[512] = "";
            while (fgets(line, sizeof(line), lsblkFile)) {
                if (strstr(line, "HOTPLUG=\"1\"") && strstr(line, "ExistOS")) {
                    char* name = findField(line, "KNAME=\"");
                    if (name) {
                        devicePath = name;
                        pclose(lsblkFile);
                        return true;
                    }
                }
            }
            pclose(lsblkFile);
            return false;
        }

        bool mountDevice(std::string* mountPath) {
            runCommand("udisksctl mount -b " + shellQuote(devicePath));
            const std::string output = runCommand(
                "lsblk -d -P -p -o HOTPLUG,VENDOR,LABEL,MOUNTPOINTS");
            std::string lines = output;
            size_t start = 0;
            while (start < lines.size()) {
                size_t end = lines.find('\n', start);
                if (end == std::string::npos) {
                    end = lines.size();
                }
                std::string line = lines.substr(start, end - start);
                if (line.find("HOTPLUG=\"1\"") != std::string::npos &&
                    line.find("ExistOS") != std::string::npos) {
                    char buffer[512];
                    strncpy(buffer, line.c_str(), sizeof(buffer) - 1);
                    buffer[sizeof(buffer) - 1] = '\0';
                    char* path = findField(buffer, "MOUNTPOINTS=\"");
                    if (path) {
                        *mountPath = path;
                        mounted = true;
                        return true;
                    }
                }
                start = end + 1;
            }
            return false;
        }

    public:
        ~EDBTransportUnix() override { close(); }

        int open(EDBTransportMode mode, const char* preferredPath) override {
            if (mode == EDBTransportMode::Serial) {
                EDB_LOG_INFO("Transport", "Opening serial transport...");
                if (serial.open(preferredPath) != 0) {
                    EDB_LOG_ERROR("Transport", "Unable to open serial transport.");
                    return -1;
                }
                serialMode = true;
                return 0;
            }
            EDB_LOG_INFO("Transport", "Waiting for USB CDC connection...");
            for (int retry = 0; retry < 5; retry++) {
                if (findDevice()) {
                    break;
                }
                if (retry == 4) {
                    EDB_LOG_ERROR("Transport", "Timed out waiting for USB CDC connection.");
                    return -1;
                }
                sleep(2);
            }

            EDB_LOG_INFO("Transport", "USB CDC connected: " << devicePath);
            EDB_LOG_INFO("Transport", "Mounting USB device...");
            std::string mountPath;
            if (!mountDevice(&mountPath)) {
                EDB_LOG_ERROR("Transport", "Mounting failed.");
                return -1;
            }

            const std::string commandPath = mountPath + "/cmd_port";
            const std::string dataPath = mountPath + "/dat_port";
#ifdef O_DIRECT
            const int directFlag = O_DIRECT;
#else
            const int directFlag = 0;
#endif
            hCMDf = ::open(commandPath.c_str(),
                           O_RDWR | O_CREAT | directFlag | O_SYNC, 0666);
            hDATf = ::open(dataPath.c_str(),
                           O_RDWR | O_CREAT | directFlag | O_SYNC, 0666);
            if (hCMDf < 0 || hDATf < 0) {
                EDB_LOG_ERROR("Transport", "Unable to open device files: " << strerror(errno));
                close();
                return -1;
            }
            return 0;
        }

        void reset(bool binaryMode) override {
            if (serialMode) {
                serial.reset(binaryMode);
            }
        }

        void close() override {
            if (serialMode) {
                serial.close();
                serialMode = false;
                return;
            }
            if (hCMDf >= 0) {
                ::close(hCMDf);
                hCMDf = -1;
            }
            if (hDATf >= 0) {
                ::close(hDATf);
                hDATf = -1;
            }
            if (mounted && !devicePath.empty()) {
                EDB_LOG_INFO("Transport", "Unmounting USB device.");
                runCommand("udisksctl unmount -b " + shellQuote(devicePath));
                mounted = false;
            }
            devicePath.clear();
        }

        std::ptrdiff_t readCommand(char* buffer, size_t length) override {
            if (serialMode) {
                return serial.read(buffer, length);
            }
            lseek(hCMDf, 0, SEEK_SET);
            return read(hCMDf, buffer, length);
        }

        std::ptrdiff_t writeCommand(const char* buffer, size_t length) override {
            if (serialMode) {
                return serial.write(buffer, length);
            }
            lseek(hCMDf, 0, SEEK_SET);
            return write(hCMDf, buffer, length);
        }

        std::ptrdiff_t readData(char* buffer, size_t length) override {
            if (serialMode) {
                return serial.read(buffer, length);
            }
            lseek(hDATf, 0, SEEK_SET);
            return read(hDATf, buffer, length);
        }

        std::ptrdiff_t writeData(const char* buffer, size_t length) override {
            if (serialMode) {
                return serial.write(buffer, length);
            }
            lseek(hDATf, 0, SEEK_SET);
            return write(hDATf, buffer, length);
        }
    };
} // namespace

EDBTransport* createEDBTransport() {
    return new EDBTransportUnix();
}
