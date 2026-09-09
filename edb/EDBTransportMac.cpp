#include "EDBSerialPosix.h"
#include "EDBTransport.h"
#include "EDBLog.h"

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <limits.h>
#include <string>
#include <unistd.h>

namespace {
    struct DiskOperationContext {
        CFRunLoopRef runLoop;
        bool success;
    };

    void diskOperationCallback(DADiskRef, DADissenterRef dissenter, void* context) {
        DiskOperationContext* operation = static_cast<DiskOperationContext*>(context);
        operation->success = dissenter == nullptr;
        CFRunLoopStop(operation->runLoop);
    }

    bool diskOperation(const std::string& devicePath, bool mount) {
        DASessionRef session = DASessionCreate(kCFAllocatorDefault);
        if (!session) {
            return false;
        }
        DADiskRef disk = DADiskCreateFromBSDName(
            kCFAllocatorDefault, session, devicePath.c_str());
        if (!disk) {
            CFRelease(session);
            return false;
        }

        DiskOperationContext context = {CFRunLoopGetCurrent(), false};
        DASessionScheduleWithRunLoop(session, context.runLoop, kCFRunLoopDefaultMode);
        if (mount) {
            DADiskMount(disk, nullptr, kDADiskMountOptionDefault,
                        diskOperationCallback, &context);
        } else {
            DADiskUnmount(disk, kDADiskUnmountOptionDefault,
                          diskOperationCallback, &context);
        }
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 15.0, false);
        DASessionUnscheduleFromRunLoop(session, context.runLoop, kCFRunLoopDefaultMode);
        CFRelease(disk);
        CFRelease(session);
        return context.success;
    }

    std::string cfStringValue(CFTypeRef value) {
        if (!value || CFGetTypeID(value) != CFStringGetTypeID()) {
            return std::string();
        }
        char buffer[PATH_MAX];
        if (!CFStringGetCString(static_cast<CFStringRef>(value), buffer,
                                sizeof(buffer), kCFStringEncodingUTF8)) {
            return std::string();
        }
        return buffer;
    }

    CFDictionaryRef diskDescription(DADiskRef disk) {
        return DADiskCopyDescription(disk);
    }

    std::string descriptionString(DADiskRef disk, CFStringRef key) {
        CFDictionaryRef description = diskDescription(disk);
        if (!description) {
            return std::string();
        }
        std::string result = cfStringValue(CFDictionaryGetValue(description, key));
        CFRelease(description);
        return result;
    }

    std::string mountPathForDevice(const std::string& devicePath) {
        DASessionRef session = DASessionCreate(kCFAllocatorDefault);
        DADiskRef disk = session ? DADiskCreateFromBSDName(
                                       kCFAllocatorDefault, session, devicePath.c_str())
                                 : nullptr;
        std::string mountPath;
        if (disk) {
            CFDictionaryRef description = diskDescription(disk);
            if (description) {
                CFTypeRef value = CFDictionaryGetValue(
                    description, kDADiskDescriptionVolumePathKey);
                if (value && CFGetTypeID(value) == CFURLGetTypeID()) {
                    CFStringRef path = CFURLCopyFileSystemPath(
                        static_cast<CFURLRef>(value), kCFURLPOSIXPathStyle);
                    mountPath = cfStringValue(path);
                    if (path) {
                        CFRelease(path);
                    }
                }
                CFRelease(description);
            }
            CFRelease(disk);
        }
        if (session) {
            CFRelease(session);
        }
        return mountPath;
    }

    bool isExistOSVolume(const std::string& volumeName) {
        const std::string upper = volumeName;
        return upper.find("ExistOS") != std::string::npos ||
               upper.find("EOSRECDISK") != std::string::npos;
    }

    class EDBTransportMac final : public EDBTransport {
    private:
        int hCMDf = -1;
        int hDATf = -1;
        std::string devicePath;
        bool mounted = false;
        bool serialMode = false;
        EDBSerialPosix serial;

        bool findDevice() {
            io_iterator_t iterator = IO_OBJECT_NULL;
            if (IOServiceGetMatchingServices(kIOMainPortDefault,
                                             IOServiceMatching(kIOMediaClass),
                                             &iterator) != KERN_SUCCESS) {
                return false;
            }

            io_object_t media = IO_OBJECT_NULL;
            while ((media = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
                CFStringRef key = CFStringCreateWithCString(
                    kCFAllocatorDefault, kIOBSDNameKey, kCFStringEncodingUTF8);
                CFTypeRef bsdName = IORegistryEntryCreateCFProperty(
                    media, key, kCFAllocatorDefault, 0);
                CFRelease(key);
                const std::string name = cfStringValue(bsdName);
                if (bsdName) {
                    CFRelease(bsdName);
                }
                if (name.empty()) {
                    IOObjectRelease(media);
                    continue;
                }

                const std::string path = "/dev/" + name;
                DASessionRef session = DASessionCreate(kCFAllocatorDefault);
                DADiskRef disk = session ? DADiskCreateFromBSDName(
                                               kCFAllocatorDefault, session, path.c_str())
                                         : nullptr;
                if (disk) {
                    const std::string volumeName = descriptionString(
                        disk, kDADiskDescriptionVolumeNameKey);
                    if (isExistOSVolume(volumeName)) {
                        devicePath = path;
                        CFRelease(disk);
                        if (session) {
                            CFRelease(session);
                        }
                        IOObjectRelease(media);
                        IOObjectRelease(iterator);
                        return true;
                    }
                    CFRelease(disk);
                }
                if (session) {
                    CFRelease(session);
                }
                IOObjectRelease(media);
            }
            IOObjectRelease(iterator);
            return false;
        }

    public:
        ~EDBTransportMac() override { close(); }

        int open(EDBTransportMode mode, const char* preferredPath) override {
            if (mode == EDBTransportMode::Serial) {
                EDB_LOG_INFO("Transport", "Opening serial transport...");
                if (serial.open(preferredPath) != 0) {
                    EDB_LOG_ERROR("Transport", "Unable to open serial transport.");
                    return -1;
                }
                serialMode = true;
                return 0;
            }
            EDB_LOG_INFO("Transport", "Waiting for USB CDC connection...");
            for (int retry = 0; retry < 5; retry++) {
                if (findDevice()) {
                    break;
                }
                if (retry == 4) {
                    EDB_LOG_ERROR("Transport", "Timed out waiting for USB CDC connection.");
                    return -1;
                }
                sleep(2);
            }

            EDB_LOG_INFO("Transport", "USB CDC connected: " << devicePath);
            EDB_LOG_INFO("Transport", "Mounting USB device...");
            std::string mountPath = mountPathForDevice(devicePath);
            if (mountPath.empty()) {
                if (!diskOperation(devicePath, true)) {
                    EDB_LOG_ERROR("Transport", "Mounting failed.");
                    return -1;
                }
                mountPath = mountPathForDevice(devicePath);
            }
            if (mountPath.empty()) {
                EDB_LOG_ERROR("Transport", "Mounting failed: mount point unavailable.");
                return -1;
            }
            mounted = true;

            const std::string commandPath = mountPath + "/cmd_port";
            const std::string dataPath = mountPath + "/dat_port";
            hCMDf = ::open(commandPath.c_str(), O_RDWR | O_CREAT | O_SYNC, 0666);
            hDATf = ::open(dataPath.c_str(), O_RDWR | O_CREAT | O_SYNC, 0666);
            if (hCMDf >= 0) {
                fcntl(hCMDf, F_NOCACHE, 1);
            }
            if (hDATf >= 0) {
                fcntl(hDATf, F_NOCACHE, 1);
            }
            if (hCMDf < 0 || hDATf < 0) {
                EDB_LOG_ERROR("Transport", "Unable to open device files: " << strerror(errno));
                close();
                return -1;
            }
            return 0;
        }

        void reset(bool binaryMode) override {
            if (serialMode) {
                serial.reset(binaryMode);
            }
        }

        void close() override {
            if (serialMode) {
                serial.close();
                serialMode = false;
                return;
            }
            if (hCMDf >= 0) {
                ::close(hCMDf);
                hCMDf = -1;
            }
            if (hDATf >= 0) {
                ::close(hDATf);
                hDATf = -1;
            }
            if (mounted && !devicePath.empty()) {
                EDB_LOG_INFO("Transport", "Unmounting USB device.");
                diskOperation(devicePath, false);
                mounted = false;
            }
            devicePath.clear();
        }

        std::ptrdiff_t readCommand(char* buffer, size_t length) override {
            if (serialMode) {
                return serial.read(buffer, length);
            }
            lseek(hCMDf, 0, SEEK_SET);
            return read(hCMDf, buffer, length);
        }

        std::ptrdiff_t writeCommand(const char* buffer, size_t length) override {
            if (serialMode) {
                return serial.write(buffer, length);
            }
            lseek(hCMDf, 0, SEEK_SET);
            const ssize_t result = write(hCMDf, buffer, length);
            if (result == static_cast<ssize_t>(length)) {
                fsync(hCMDf);
            }
            return result;
        }

        std::ptrdiff_t readData(char* buffer, size_t length) override {
            if (serialMode) {
                return serial.read(buffer, length);
            }
            lseek(hDATf, 0, SEEK_SET);
            return read(hDATf, buffer, length);
        }

        std::ptrdiff_t writeData(const char* buffer, size_t length) override {
            if (serialMode) {
                return serial.write(buffer, length);
            }
            lseek(hDATf, 0, SEEK_SET);
            const ssize_t result = write(hDATf, buffer, length);
            if (result == static_cast<ssize_t>(length)) {
                fsync(hDATf);
            }
            return result;
        }
    };
} // namespace

EDBTransport* createEDBTransport() {
    return new EDBTransportMac();
}
