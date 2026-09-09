#include "EDBTransport.h"
#include "EDBLog.h"
#include "CComHelper.h"
#include "EDBWinReg.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <cwchar>
#include <iostream>
#include <string>

namespace {
    std::wstring toWide(const char* value) {
        if (!value) {
            return std::wstring();
        }
        const int length = MultiByteToWideChar(CP_UTF8, 0, value, -1, nullptr, 0);
        std::wstring result(length > 0 ? length : 0, L'\0');
        if (length > 0) {
            MultiByteToWideChar(CP_UTF8, 0, value, -1, &result[0], length);
            result.resize(length - 1);
        }
        return result;
    }

    bool isTargetVolume(const wchar_t* volumeName, const wchar_t* filesystem) {
        std::wstring volume(volumeName ? volumeName : L"");
        std::wstring fs(filesystem ? filesystem : L"");
        std::transform(volume.begin(), volume.end(), volume.begin(),
                       [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
        std::transform(fs.begin(), fs.end(), fs.begin(),
                       [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
        return (volume.find(L"existos") != std::wstring::npos ||
                volume.find(L"eosrecdisk") != std::wstring::npos) &&
               fs.find(L"fat") != std::wstring::npos;
    }

    std::wstring findMassStorageRoot() {
        DWORD length = GetLogicalDriveStringsW(0, nullptr);
        if (!length) {
            return std::wstring();
        }
        std::wstring drives(length + 1, L'\0');
        GetLogicalDriveStringsW(length, &drives[0]);
        for (const wchar_t* drive = drives.c_str(); *drive; drive += wcslen(drive) + 1) {
            wchar_t volumeName[MAX_PATH] = {};
            wchar_t filesystem[MAX_PATH] = {};
            if (GetVolumeInformationW(drive, volumeName, MAX_PATH, nullptr, nullptr,
                                      nullptr, filesystem, MAX_PATH) &&
                isTargetVolume(volumeName, filesystem)) {
                return drive;
            }
        }
        return std::wstring();
    }

    std::string findSerialPort() {
        const std::string port = findUsbSerialCom();
        return port == "NONE" ? std::string() : port;
    }

    class EDBTransportWindows final : public EDBTransport {
    private:
        HANDLE hCMDf = INVALID_HANDLE_VALUE;
        HANDLE hDATf = INVALID_HANDLE_VALUE;
        CComHelper com;
        bool serialMode = false;

        HANDLE openFile(const std::wstring& path) {
            return CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
                               nullptr);
        }

        std::ptrdiff_t readHandle(HANDLE handle, char* buffer, size_t length,
                                  bool seek) {
            if (seek) {
                SetFilePointer(handle, 0, nullptr, FILE_BEGIN);
            }
            DWORD count = 0;
            if (!ReadFile(handle, buffer, static_cast<DWORD>(length), &count, nullptr)) {
                return -1;
            }
            return static_cast<std::ptrdiff_t>(count);
        }

        std::ptrdiff_t writeHandle(HANDLE handle, const char* buffer, size_t length,
                                   bool seek) {
            if (seek) {
                SetFilePointer(handle, 0, nullptr, FILE_BEGIN);
            }
            DWORD count = 0;
            if (!WriteFile(handle, buffer, static_cast<DWORD>(length), &count, nullptr)) {
                return -1;
            }
            return static_cast<std::ptrdiff_t>(count);
        }

        std::ptrdiff_t readCurrent(HANDLE handle, char* buffer, size_t length) {
            if (serialMode) {
                DWORD count = 0;
                return com.Read(buffer, static_cast<int>(length), &count)
                           ? static_cast<std::ptrdiff_t>(count)
                           : -1;
            }
            return readHandle(handle, buffer, length, true);
        }

        std::ptrdiff_t writeCurrent(HANDLE handle, const char* buffer, size_t length) {
            if (serialMode) {
                return com.Write(const_cast<char*>(buffer), static_cast<int>(length))
                           ? static_cast<std::ptrdiff_t>(length)
                           : -1;
            }
            return writeHandle(handle, buffer, length, true);
        }

        void resetSerial(bool binaryMode) {
            if (!serialMode) {
                return;
            }
            com.SetRTS(binaryMode);
            Sleep(20);
            com.SetDTR(false);
        }

        void closeSerial() {
            if (!serialMode) {
                return;
            }
            com.Close();
            serialMode = false;
        }

    public:
        ~EDBTransportWindows() override { close(); }

        int open(EDBTransportMode mode, const char* preferredPath) override {
            if (mode == EDBTransportMode::Serial) {
                std::string port = preferredPath ? preferredPath : "";
                if (port.empty()) {
                    EDB_LOG_INFO("Transport", "Detecting ExistOS serial port...");
                    port = findSerialPort();
                } else {
                    EDB_LOG_INFO("Transport", "Using requested serial port: " << port);
                }
                if (port.empty()) {
                    EDB_LOG_ERROR("Transport", "No ExistOS serial port found.");
                    return -1;
                }
                EDB_LOG_INFO("Transport", "Identified serial port: " << port);
                EDB_LOG_INFO("Transport", "Opening serial port...");
                if (!com.Open(port)) {
                    EDB_LOG_ERROR("Transport", "Unable to open serial port: " << port);
                    return -1;
                }
                if (!com.Set()) {
                    EDB_LOG_ERROR("Transport", "Unable to configure serial port.");
                    com.Close();
                    return -1;
                }
                serialMode = true;
                EDB_LOG_INFO("Transport", "Serial port configured: 115200 baud, 8N2.");
                return 0;
            }

            const int maxAttempts = 3;
            std::wstring root;
            for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
                EDB_LOG_INFO("Transport", "Searching for ExistOS mass storage volume (attempt "
                                             << attempt << "/" << maxAttempts << ")...");
                root = findMassStorageRoot();
                if (!root.empty()) {
                    break;
                }
                if (attempt < maxAttempts) {
                    EDB_LOG_WARN("Transport", "Mass storage volume not found; retrying in 2 seconds.");
                    Sleep(2000);
                }
            }
            if (root.empty()) {
                EDB_LOG_ERROR("Transport", "No ExistOS mass storage volume found after "
                                             << maxAttempts << " attempts.");
                return -1;
            }
            EDB_LOG_INFO("Transport", "Identified mass storage volume.");
            EDB_LOG_INFO("Transport", "Opening mass storage command and data ports...");
            hCMDf = openFile(root + L"cmd_port");
            hDATf = openFile(root + L"dat_port");
            if (hCMDf == INVALID_HANDLE_VALUE || hDATf == INVALID_HANDLE_VALUE) {
                EDB_LOG_ERROR("Transport", "Unable to open mass storage ports.");
                close();
                return -1;
            }
            EDB_LOG_INFO("Transport", "Mass storage ports opened.");
            return 0;
        }

        void reset(bool binaryMode) override {
            resetSerial(binaryMode);
        }

        void close() override {
            if (hCMDf != INVALID_HANDLE_VALUE) {
                CloseHandle(hCMDf);
                hCMDf = INVALID_HANDLE_VALUE;
            }
            if (hDATf != INVALID_HANDLE_VALUE) {
                CloseHandle(hDATf);
                hDATf = INVALID_HANDLE_VALUE;
            }
            closeSerial();
        }

        std::ptrdiff_t readCommand(char* buffer, size_t length) override {
            return readCurrent(hCMDf, buffer, length);
        }

        std::ptrdiff_t writeCommand(const char* buffer, size_t length) override {
            return writeCurrent(hCMDf, buffer, length);
        }

        std::ptrdiff_t readData(char* buffer, size_t length) override {
            return readCurrent(hDATf, buffer, length);
        }

        std::ptrdiff_t writeData(const char* buffer, size_t length) override {
            return writeCurrent(hDATf, buffer, length);
        }
    };
} // namespace

EDBTransport* createEDBTransport() {
    return new EDBTransportWindows();
}
