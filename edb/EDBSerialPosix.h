#pragma once

#include <cstddef>
#include <sys/types.h>

namespace edb_serial_detail {
    using WriteOperation = ssize_t (*)(int, const void*, size_t);

    ssize_t writeAll(int fd, const char* buffer, size_t length,
                     WriteOperation writeOperation);
} // namespace edb_serial_detail

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
