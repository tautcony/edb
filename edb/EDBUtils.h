#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

std::optional<uint32_t> parsePage(std::string_view text);
unsigned char blockChksum(const char* block, size_t blockSize);
