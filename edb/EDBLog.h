#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace edb_log {
    enum class LogLevel { Debug,
                          Info,
                          Warn,
                          Error };

    inline const char* logLevelName(LogLevel level) {
        switch (level) {
            case LogLevel::Debug:
                return "DEBUG";
            case LogLevel::Info:
                return "INFO";
            case LogLevel::Warn:
                return "WARN";
            case LogLevel::Error:
                return "ERROR";
        }
        return "INFO";
    }

    class Logger {
    public:
        static void write(LogLevel level, const char* component,
                          const std::string& message) {
            const std::chrono::system_clock::time_point now =
                std::chrono::system_clock::now();
            const std::time_t time = std::chrono::system_clock::to_time_t(now);
            const long milliseconds = static_cast<long>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch())
                    .count() %
                1000);
            std::tm localTime;
#ifdef _WIN32
            localtime_s(&localTime, &time);
#else
            localtime_r(&time, &localTime);
#endif

            std::lock_guard<std::mutex> lock(outputMutex());
            std::ostream& output = level == LogLevel::Error ? std::cerr : std::cout;
            output << '[' << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S")
                   << '.' << std::setfill('0') << std::setw(3) << milliseconds
                   << std::setfill(' ') << "] [" << std::left << std::setw(5)
                   << logLevelName(level)
                   << std::right << "] [" << component << "] " << message
                   << std::endl;
        }

        static void progress(const std::string& message) {
            std::lock_guard<std::mutex> lock(outputMutex());
            std::cout << '\r' << message << std::string(8, ' ') << std::flush;
        }

        static void endProgress() {
            std::lock_guard<std::mutex> lock(outputMutex());
            std::cout << std::endl;
        }

    private:
        static std::mutex& outputMutex() {
            static std::mutex mutex;
            return mutex;
        }
    };
} // namespace edb_log

#define EDB_LOG(level, component, expression)                         \
    do {                                                              \
        std::ostringstream edbLogStream;                              \
        edbLogStream << expression;                                   \
        edb_log::Logger::write(level, component, edbLogStream.str()); \
    } while (false)

#define EDB_LOG_DEBUG(component, expression) \
    EDB_LOG(edb_log::LogLevel::Debug, component, expression)
#define EDB_LOG_INFO(component, expression) \
    EDB_LOG(edb_log::LogLevel::Info, component, expression)
#define EDB_LOG_WARN(component, expression) \
    EDB_LOG(edb_log::LogLevel::Warn, component, expression)
#define EDB_LOG_ERROR(component, expression) \
    EDB_LOG(edb_log::LogLevel::Error, component, expression)
