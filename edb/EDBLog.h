#pragma once

#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <io.h>
#define EDB_LOG_ISATTY(stream) _isatty(_fileno(stream))
#else
#include <unistd.h>
#define EDB_LOG_ISATTY(stream) isatty(fileno(stream))
#endif

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

    // Accepts the names the CLI advertises; case-insensitive.
    inline bool parseLogLevel(const std::string& text, LogLevel* level) {
        std::string name;
        for (char character : text) {
            name += static_cast<char>(
                std::tolower(static_cast<unsigned char>(character)));
        }
        if (name == "debug") {
            *level = LogLevel::Debug;
        } else if (name == "info") {
            *level = LogLevel::Info;
        } else if (name == "warn" || name == "warning") {
            *level = LogLevel::Warn;
        } else if (name == "error") {
            *level = LogLevel::Error;
        } else {
            return false;
        }
        return true;
    }

    // Every record is rendered on a single line. A trailing newline would leak
    // into the next record as a blank line, and an embedded one would split a
    // record in two and strand the progress line in between.
    inline std::string singleLine(std::string message) {
        while (!message.empty() &&
               (message.back() == '\n' || message.back() == '\r')) {
            message.pop_back();
        }
        for (char& character : message) {
            if (character == '\n' || character == '\r') {
                character = ' ';
            }
        }
        return message;
    }

    class Logger {
    public:
        static void setLevel(LogLevel level) {
            std::lock_guard<std::mutex> lock(outputMutex());
            state().threshold = level;
        }

        static LogLevel level() {
            std::lock_guard<std::mutex> lock(outputMutex());
            return state().threshold;
        }

        static void write(LogLevel level, const char* component,
                          const std::string& rawMessage) {
            const std::string message = singleLine(rawMessage);
            std::lock_guard<std::mutex> lock(outputMutex());
            State& s = state();

            // Drop records below the configured level before they can take part
            // in the folding below, so filtered-out traces stay invisible.
            if (level < s.threshold) {
                return;
            }

            // While a progress indicator is on screen, consecutive identical
            // records are collapsed. A per-page trace would otherwise scroll
            // the progress line away faster than it can be read.
            if (s.progressActive) {
                if (s.hasPending && level == s.pendingLevel &&
                    component == s.pendingComponent &&
                    message == s.pendingMessage) {
                    s.pendingRepeats++;
                    return;
                }
                flushPending();
                s.hasPending = true;
                s.pendingLevel = level;
                s.pendingComponent = component;
                s.pendingMessage = message;
                s.pendingRepeats = 0;
            }

            emit(level, component, message);
        }

        static void progress(const std::string& message) {
            std::lock_guard<std::mutex> lock(outputMutex());
            State& s = state();
            const std::string text = singleLine(message);
            if (!interactive()) {
                // Nothing can be redrawn, so each update becomes its own record.
                std::cout << text << '\n'
                          << std::flush;
                return;
            }
            s.progressActive = true;
            s.progressMessage = text;
            if (s.progressMessage.size() > s.progressWidth) {
                s.progressWidth = s.progressMessage.size();
            }
            drawProgress();
        }

        static void endProgress() {
            std::lock_guard<std::mutex> lock(outputMutex());
            flushPending();
            State& s = state();
            if (!s.progressActive) {
                return;
            }
            s.progressActive = false;
            s.progressMessage.clear();
            // Terminate the progress line so later output starts on a clean one.
            std::cout << '\n'
                      << std::flush;
        }

    private:
        struct State {
            // Info is the default: debug tracing has to be asked for explicitly.
            LogLevel threshold = LogLevel::Info;
            bool progressActive = false;
            std::string progressMessage;
            size_t progressWidth = 0;
            bool hasPending = false;
            LogLevel pendingLevel = LogLevel::Info;
            std::string pendingComponent;
            std::string pendingMessage;
            unsigned long long pendingRepeats = 0;
        };

        // Reports how many identical records were folded into the pending one.
        // Callers must hold the output mutex.
        static void flushPending() {
            State& s = state();
            if (!s.hasPending) {
                return;
            }
            const LogLevel level = s.pendingLevel;
            const std::string component = s.pendingComponent;
            const std::string message = s.pendingMessage;
            const unsigned long long repeats = s.pendingRepeats;
            s.hasPending = false;
            s.pendingRepeats = 0;

            if (repeats == 0) {
                // The record itself was already emitted when it became pending.
                return;
            }
            emit(level, component,
                 "Last message repeated " + std::to_string(repeats) +
                     (repeats == 1 ? " more time: " : " more times: ") + message);
        }

        // Callers must hold the output mutex.
        static void emit(LogLevel level, const std::string& component,
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

            const bool hasProgress = state().progressActive;
            if (hasProgress) {
                eraseProgress();
            }

            std::ostream& output = level == LogLevel::Error ? std::cerr : std::cout;
            output << '[' << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S")
                   << '.' << std::setfill('0') << std::setw(3) << milliseconds
                   << std::setfill(' ') << "] [" << std::left << std::setw(5)
                   << logLevelName(level)
                   << std::right << "] [" << component << "] " << message
                   << std::endl;

            if (hasProgress) {
                // Put the progress line back underneath the record just written.
                drawProgress();
            }
        }

        // Both drawing helpers must be called while holding the output mutex.
        static void drawProgress() {
            const State& s = state();
            std::cout << '\r' << s.progressMessage
                      << std::string(s.progressWidth - s.progressMessage.size(),
                                     ' ')
                      << std::flush;
        }

        static void eraseProgress() {
            const State& s = state();
            std::cout << '\r' << std::string(s.progressWidth, ' ') << '\r'
                      << std::flush;
        }

        // A redrawable progress line only makes sense on a terminal. When the
        // output is redirected, the cursor tricks would be written to the file.
        static bool interactive() {
            static const bool value = EDB_LOG_ISATTY(stdout) != 0;
            return value;
        }

        static State& state() {
            static State value;
            return value;
        }

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
