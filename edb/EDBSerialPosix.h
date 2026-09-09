#pragma once

#include <cstddef>
#include <sys/types.h>

class EDBSerialPosix {
private:
    int fd = -1;

public:
    int open(const char* preferredPath);
    void close();
    void reset(bool binaryMode);
    std::ptrdiff_t read(char* buffer, size_t length);
    std::ptrdiff_t write(const char* buffer, size_t length);
};
