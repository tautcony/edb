#include "CComHelper.h"

#include <string>
#include <iostream>
#include <vector>
using namespace std;

std::wstring stringToWString(const std::string& orig) {
    int length = MultiByteToWideChar(CP_UTF8, 0, orig.c_str(), -1, NULL, 0);
    if (length <= 0) {
        return std::wstring();
    }
    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, orig.c_str(), -1, &result[0], length);
    result.resize(length - 1);
    return result;
}

bool CComHelper::Open(string com) {
    Close();
    const std::wstring path = stringToWString(com);
    hCom = CreateFileW(path.c_str(), GENERIC_WRITE | GENERIC_READ, 0, NULL,
                       OPEN_EXISTING, 0, NULL);

    if (hCom == INVALID_HANDLE_VALUE) {
        return false;
    }

    return true;
}

CComHelper::~CComHelper() {
    Close();
}

bool CComHelper::Set() {
    if (!SetupComm(hCom, 500, 500)) {
        return false;
    }
    COMMTIMEOUTS TimeOuts; //设定读超时
    TimeOuts.ReadIntervalTimeout = MAXDWORD;
    TimeOuts.ReadTotalTimeoutMultiplier = 100;
    TimeOuts.ReadTotalTimeoutConstant = 1000;

    TimeOuts.WriteTotalTimeoutConstant = 10; //设定写超时
    TimeOuts.WriteTotalTimeoutMultiplier = 100;
    if (!SetCommTimeouts(hCom, &TimeOuts)) {
        return false;
    }

    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(hCom, &dcb)) {
        return false;
    }
    dcb.BaudRate = 115200;
    dcb.ByteSize = 8;           //每个字节有8位
    dcb.Parity = NOPARITY;      //无奇偶校验位
    dcb.StopBits = TWOSTOPBITS; //两个停止位
    if (!SetCommState(hCom, &dcb)) {
        return false;
    }

    return PurgeComm(hCom, PURGE_TXCLEAR | PURGE_RXCLEAR) != FALSE;
}

bool CComHelper::Read(char* data, int length, DWORD* dwCount) {

    bool bReadStat = ReadFile(hCom, data, (DWORD)length, dwCount, NULL);
    return bReadStat;
}

bool CComHelper::Write(char* data, int length) {
    DWORD dwWrite = (DWORD)length;
    COMSTAT ComStat;
    DWORD dwError;
    ClearCommError(hCom, &dwError, &ComStat);
    bool bWriteStat = WriteFile(hCom, data, dwWrite, &dwWrite, NULL);
    if (!bWriteStat || dwWrite != static_cast<DWORD>(length)) {
        return false;
    }
    PurgeComm(hCom, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
    return true;
}

bool CComHelper::WriteStr(const char* data) {
    return Write((char*)data, strlen(data));
}

void CComHelper::SetDTR(bool set) {
    if (set) {
        EscapeCommFunction(hCom, SETDTR);
    } else {
        EscapeCommFunction(hCom, CLRDTR);
    }
}

void CComHelper::SetRTS(bool set) {
    if (set) {
        EscapeCommFunction(hCom, SETRTS);
    } else {
        EscapeCommFunction(hCom, CLRRTS);
    }
}

bool CComHelper::Close() {
    if (hCom == INVALID_HANDLE_VALUE) {
        return true;
    }
    bool result = CloseHandle(hCom);
    hCom = INVALID_HANDLE_VALUE;
    return result;
}
