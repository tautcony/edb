#pragma once

#include <cstddef>

enum class EDBTransportMode {
    MassStorage,
    Serial
};

class EDBTransport {
public:
    virtual ~EDBTransport() {}

    virtual int open(EDBTransportMode mode, const char* preferredPath) = 0;
    virtual void close() = 0;
    virtual void reset(bool binaryMode) = 0;
    virtual std::ptrdiff_t readCommand(char* buffer, size_t length) = 0;
    virtual std::ptrdiff_t writeCommand(const char* buffer, size_t length) = 0;
    virtual std::ptrdiff_t readData(char* buffer, size_t length) = 0;
    virtual std::ptrdiff_t writeData(const char* buffer, size_t length) = 0;
};

EDBTransport* createEDBTransport();
