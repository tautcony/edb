#include "EDBUtils.h"

#include <cerrno>
#include <cstdlib>
#include <limits>

bool parsePage(const char* text, uint32_t* page) {
    if (!text || !page || !*text || text[0] == '-') {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (errno == ERANGE || *end != '\0' ||
        value > (std::numeric_limits<uint32_t>::max)()) {
        return false;
    }
    *page = static_cast<uint32_t>(value);
    return true;
}

unsigned char blockChksum(const char* block, size_t blockSize) {
    unsigned char sum = 0x5A;
    for (size_t i = 0; i < blockSize; i++) {
        sum += static_cast<unsigned char>(block[i]);
    }
    return sum;
}