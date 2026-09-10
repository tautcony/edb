#include "EDBSerialPosix.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <glob.h>
#include <iostream>
#include <limits.h>
#include <sys/ioctl.h>
#include <unistd.h>
#ifdef __linux__
#include <asm/ioctls.h>
#include <asm/termbits.h>
#else
#include <termios.h>
#endif

namespace {
    const char* findSerialPath(const char* preferredPath) {
        if (preferredPath && preferredPath[0] != '\0') {
            return preferredPath;
        }

#ifdef __APPLE__
        static const char* patterns[] = {
            "/dev/cu.usbmodem*", "/dev/cu.usbserial*", "/dev/tty.usbmodem*"};
#else
        static const char* patterns[] = {"/dev/ttyACM*", "/dev/ttyUSB*"};
#endif
        static char path[PATH_MAX];
        for (const char* pattern : patterns) {
            glob_t matches = {};
            if (glob(pattern, 0, nullptr, &matches) == 0 && matches.gl_pathc > 0) {
                strncpy(path, matches.gl_pathv[0], sizeof(path) - 1);
                path[sizeof(path) - 1] = '\0';
                globfree(&matches);
                return path;
            }
            globfree(&matches);
        }
        return nullptr;
    }
} // namespace

int EDBSerialPosix::open(const char* preferredPath) {
    const char* path = findSerialPath(preferredPath);
    if (!path) {
        return -1;
    }
    fd = ::open(path, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        return -1;
    }

#ifdef __linux__
    struct termios2 settings = {};
    if (ioctl(fd, TCGETS2, &settings) != 0) {
        close();
        return -1;
    }
    settings.c_iflag = 0;
    settings.c_oflag = 0;
    settings.c_lflag = 0;
    settings.c_cflag = BOTHER | CS8 | CLOCAL | CREAD;
    settings.c_ispeed = 14400;
    settings.c_ospeed = 14400;
    settings.c_cc[VMIN] = 0;
    settings.c_cc[VTIME] = 10;
    if (ioctl(fd, TCSETS2, &settings) != 0) {
        close();
        return -1;
    }
    ioctl(fd, TCFLSH, TCIOFLUSH);
#else
    struct termios settings = {};
    if (tcgetattr(fd, &settings) != 0) {
        close();
        return -1;
    }
    cfmakeraw(&settings);
    cfsetispeed(&settings, B14400);
    cfsetospeed(&settings, B14400);
    settings.c_cflag |= CLOCAL | CREAD;
    settings.c_cflag &= ~CSTOPB;
    settings.c_cflag &= ~CSIZE;
    settings.c_cflag |= CS8;
    settings.c_cc[VMIN] = 0;
    settings.c_cc[VTIME] = 10;
    if (tcsetattr(fd, TCSANOW, &settings) != 0) {
        close();
        return -1;
    }
    tcflush(fd, TCIOFLUSH);
#endif
    return 0;
}

void EDBSerialPosix::close() {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

void EDBSerialPosix::reset(bool binaryMode) {
    if (fd < 0) {
        return;
    }
    int modemBits = 0;
    if (ioctl(fd, TIOCMGET, &modemBits) == 0) {
        if (binaryMode) {
            modemBits |= TIOCM_RTS;
        } else {
            modemBits &= ~TIOCM_RTS;
        }
        modemBits &= ~TIOCM_DTR;
        ioctl(fd, TIOCMSET, &modemBits);
    }
    usleep(20000);
}

std::ptrdiff_t EDBSerialPosix::read(char* buffer, size_t length) {
    return ::read(fd, buffer, length);
}

std::ptrdiff_t EDBSerialPosix::write(const char* buffer, size_t length) {
    return ::write(fd, buffer, length);
}
