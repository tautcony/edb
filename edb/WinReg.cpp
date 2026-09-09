#include "WinReg.h"

#define debug(fmt, ...)

#include <iostream>

using namespace std;

void wcharTochar(const wchar_t* wcharSrc, char* chrDst, int chrDstlength) {
    WideCharToMultiByte(CP_ACP, 0, wcharSrc, -1, chrDst, chrDstlength, NULL, NULL);
}

bool QueryRegKey(LPCWSTR keyPath, LPCWSTR ValueName, char* Value, int valueLength) {
    HKEY hKey;
    if (ERROR_SUCCESS == RegOpenKey(HKEY_LOCAL_MACHINE, keyPath, &hKey)) {
        debug("OpenRegKey success!\n");
    } else {
        debug("OpenRegKey failed!\n");
        return false;
    }

    DWORD dwType = REG_SZ;
    DWORD dwLen = MAX_PATH;
    BYTE data[MAX_PATH];
    if (ERROR_SUCCESS != RegQueryValueEx(hKey, ValueName, 0, &dwType,
                                         (LPBYTE)data, &dwLen)) {
        RegCloseKey(hKey);
        return false;
    }

    WideCharToMultiByte(CP_ACP, 0, reinterpret_cast<LPCWCH>(data), -1,
                        Value, valueLength, NULL, NULL);
    RegCloseKey(hKey);
    return true;
}

vector<string> QueryEUSBPort() {
    HKEY hKey;
    vector<string> COM;

    LPCTSTR lpSubKey = EUSB_KEYNAME;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpSubKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        debug("open USB registry key failed !");
        return COM;
    }

    DWORD dwIndex = 0;
    TCHAR valueName[MAX_PATH];
    char strValue[MAX_PATH];
    LONG status;
    TCHAR paraPath[MAX_PATH];

    do {
        status = RegEnumKey(hKey, dwIndex++, valueName, MAX_PATH);
        if (status == ERROR_SUCCESS) {
            lstrcpyW(paraPath, EUSB_KEYNAME);
            lstrcatW(paraPath, _T("\\"));
            lstrcatW(paraPath, valueName);
            lstrcatW(paraPath, _T("\\Device Parameters"));

            if (QueryRegKey(paraPath, _T("PortName"), strValue, MAX_PATH) == false) {
                debug("open USB Port Parameters registry key failed !");
                continue;
            }
            COM.push_back(strValue);
        }
    } while (status != ERROR_NO_MORE_ITEMS);

    RegCloseKey(hKey);
    return COM;
}

vector<string> QuerySerialPort() {
    HKEY hKey;
    vector<string> COM;

    LPCTSTR lpSubKey = SERIALPATH;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpSubKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        debug("open registry key failed !");
        return COM;
    }

    TCHAR valueName[MAX_PATH];
    BYTE portName[MAX_PATH];
    LONG status;
    DWORD dwIndex = 0;
    DWORD dwSizeValueName = MAX_PATH;
    DWORD dwSizeofPortName = MAX_PATH;
    DWORD Type;
    char strValue[MAX_PATH];
    int length = MAX_PATH;

    do {
        status = RegEnumValue(hKey, dwIndex++, valueName, &dwSizeValueName,
                              NULL, &Type, portName, &dwSizeofPortName);
        if (status == ERROR_SUCCESS) {
            WideCharToMultiByte(CP_ACP, 0, reinterpret_cast<LPCWCH>(portName), -1,
                                strValue, length, NULL, NULL);
            COM.push_back(strValue);
        }
        dwSizeValueName = MAX_PATH;
        dwSizeofPortName = MAX_PATH;
    } while (status != ERROR_NO_MORE_ITEMS);

    RegCloseKey(hKey);
    return COM;
}

string findUsbSerialCom() {
    vector<string> COMListAvailable = QuerySerialPort();
    vector<string> EUSBPort = QueryEUSBPort();
    string COM = "NONE";

    if (EUSBPort.size() == 0) {
        return COM;
    }

    for (string& EUSB : EUSBPort) {
        for (string& ACOM : COMListAvailable) {
            if (EUSB.compare(ACOM) == 0) {
                COM = ACOM;
                break;
            }
        }
    }

    return COM;
}
