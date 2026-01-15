// =============================================================================
// fq-compressor - Logger Module Implementation
// =============================================================================
// Implementation of the low-latency asynchronous logging module using Quill.
//
// Requirements: 4.2 (Quill async logging)
// =============================================================================

#include "fqc/common/logger.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace fqc::log {

namespace {

// =============================================================================
// Global State
// =============================================================================

/// @brief Global logger instance pointer.
/// @note Atomic for thread-safe access after initialization.
std::atomic<quill::Logger*> gLogger{nullptr};

/// @brief Flag indicating if the logger has been initialized.
std::atomic<bool> gInitialized{false};

/// @brief Mutex for initialization synchronization.
std::mutex gInitMutex;

/// @brief Convert string to lowercase for case-insensitive comparison.
std::string toLower(std::string_view str) {
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

}  // namespace

// =============================================================================
// Level Conversion Implementation
// =============================================================================

quill::LogLevel toQuillLevel(Level level) noexcept {
    switch (level) {
        case Level::kTrace:
            return quill::LogLevel::TraceL1;
        case Level::kDebug:
            return quill::LogLevel::Debug;
        case Level::kInfo:
            return quill::LogLevel::Info;
        case Level::kWarning:
            return quill::LogLevel::Warning;
        case Level::kError:
            return quill::LogLevel::Error;
        case Level::kCritical:
            return quill::LogLevel::Critical;
        default:
            return quill::LogLevel::Info;
    }
}

Level levelFromString(std::string_view levelStr) noexcept {
    const std::string lower = toLower(levelStr);

    if (lower == "trace") {
        return Level::kTrace;
    }
    if (lower == "debug") {
        return Level::kDebug;
    }
    if (lower == "info") {
        return Level::kInfo;
    }
    if (lower == "warning" || lower == "warn") {
        return Level::kWarning;
    }
    if (lower == "error") {
        return Level::kError;
    }
    if (lower == "critical" || lower == "fatal") {
        return Level::kCritical;
    }

    // Default to info for unknown strings
    return Level::kInfo;
}

std::string_view levelToString(Level level) noexcept {
    switch (level) {
        case Level::kTrace:
            return "trace";
        case Level::kDebug:
            return "debug";
        case Level::kInfo:
            return "info";
        case Level::kWarning:
            return "warning";
        case Level::kError:
            return "error";
        case Level::kCritical:
            return "critical";
        default:
            return "info";
    }
}

// =============================================================================
// Logger Initialization Implementation
// =============================================================================

void init(const Config& config) {
    std::lock_guard<std::mutex> lock(gInitMutex);

    // Prevent double initialization
    if (gInitialized.load(std::memory_order_acquire)) {
        return;
    }

    // Start the Quill backend thread
    quill::BackendOptions backendOptions;
    quill::Backend::start(backendOptions);

    // Collect sinks for the logger
    std::vector<std::shared_ptr<quill::Sink>> sinks;

    // Add console sink if enabled
    if (config.enableConsole) {
        auto consoleSink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");
        sinks.push_back(consoleSink);
    }

    // Add file sink if log file is specified
    if (!config.logFile.empty()) {
        auto fileSink = quill::Frontend::create_or_get_sink<quill::FileSink>(
            config.logFile,
            []() {
                quill::FileSinkConfig fileSinkConfig;
                fileSinkConfig.set_open_mode('w');  // Overwrite mode
                return fileSinkConfig;
            }(),
            quill::FileEventNotifier{});
        sinks.push_back(fileSink);
    }

    // Create the logger with collected sinks
    quill::Logger* loggerPtr = nullptr;

    if (!sinks.empty()) {
        loggerPtr = quill::Frontend::create_or_get_logger(config.loggerName, std::move(sinks));
    } else {
        // If no sinks configured, create with console sink as fallback
        auto consoleSink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");
        loggerPtr = quill::Frontend::create_or_get_logger(config.loggerName, std::move(consoleSink));
    }

    // Set the log level
    loggerPtr->set_log_level(toQuillLevel(config.level));

    // Store the logger pointer
    gLogger.store(loggerPtr, std::memory_order_release);
    gInitialized.store(true, std::memory_order_release);
}

void init(std::string_view logFile, Level level) {
    Config config;
    config.logFile = std::string(logFile);
    config.level = level;
    config.enableConsole = true;
    config.enableColors = true;
    config.loggerName = "fqc";

    init(config);
}

// =============================================================================
// Logger Access Implementation
// =============================================================================

quill::Logger* logger() noexcept {
    return gLogger.load(std::memory_order_acquire);
}

bool isInitialized() noexcept {
    return gInitialized.load(std::memory_order_acquire);
}

void flush() {
    if (isInitialized()) {
        quill::Logger* loggerPtr = logger();
        if (loggerPtr != nullptr) {
            loggerPtr->flush_log();
        }
    }
}

void shutdown() {
    if (isInitialized()) {
        // Flush any pending messages
        flush();

        // Stop the backend thread
        quill::Backend::stop();

        // Reset global state
        gLogger.store(nullptr, std::memory_order_release);
        gInitialized.store(false, std::memory_order_release);
    }
}

}  // namespace fqc::log
