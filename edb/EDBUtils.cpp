#include "EDBUtils.h"

#include <charconv>
#include <system_error>

std::optional<uint32_t> parsePage(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    uint32_t page = 0;
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto result = std::from_chars(begin, end, page);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }
    return page;
}

unsigned char blockChksum(const char* block, size_t blockSize) {
    unsigned char sum = 0x5A;
    for (size_t i = 0; i < blockSize; i++) {
        sum += static_cast<unsigned char>(block[i]);
    }
    return sum;
}
