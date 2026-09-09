#include "EDBTransport.h"
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
                    std::cout << "  Detecting ExistOS serial port..." << std::endl;
                    port = findSerialPort();
                } else {
                    std::cout << "  Using requested serial port: " << port << std::endl;
                }
                if (port.empty()) {
                    std::cerr << "  [FAIL] No ExistOS serial port found." << std::endl;
                    return -1;
                }
                std::cout << "  Identified serial port: " << port << std::endl;
                std::cout << "  Opening serial port..." << std::endl;
                if (!com.Open(port)) {
                    std::cerr << "  [FAIL] Unable to open serial port: " << port << std::endl;
                    return -1;
                }
                com.Set();
                serialMode = true;
                std::cout << "  Serial port configured: 115200 baud, 8N2." << std::endl;
                return 0;
            }

            std::cout << "  Searching for ExistOS mass storage volume..." << std::endl;
            const std::wstring root = findMassStorageRoot();
            if (root.empty()) {
                std::cerr << "  [FAIL] No ExistOS mass storage volume found." << std::endl;
                return -1;
            }
            std::wcout << L"  Identified mass storage volume: " << root << std::endl;
            std::wcout << L"  Opening " << root << L"cmd_port and " << root << L"dat_port..." << std::endl;
            hCMDf = openFile(root + L"cmd_port");
            hDATf = openFile(root + L"dat_port");
            if (hCMDf == INVALID_HANDLE_VALUE || hDATf == INVALID_HANDLE_VALUE) {
                std::cerr << "  [FAIL] Unable to open mass storage ports." << std::endl;
                close();
                return -1;
            }
            std::cout << "  Mass storage ports opened." << std::endl;
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
