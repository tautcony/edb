#pragma once

#include <cstddef>
#include <cstdint>

bool parsePage(const char* text, uint32_t* page);
unsigned char blockChksum(const char* block, size_t blockSize);