// =============================================================================
// fq-compressor - Logger Module
// =============================================================================
// Low-latency asynchronous logging using Quill library.
//
// This module provides a global logger instance with support for:
// - Multiple log levels (trace, debug, info, warning, error, critical)
// - Console and file output
// - Thread-safe logging (Quill is inherently thread-safe)
//
// Usage:
//   fqc::log::init("app.log", fqc::log::Level::kInfo);
//   LOG_INFO(fqc::log::logger(), "Message with {} args", 42);
//
// Requirements: 4.2 (Quill async logging)
// =============================================================================

#ifndef FQC_COMMON_LOGGER_H
#define FQC_COMMON_LOGGER_H

#include <string_view>

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>

namespace fqc::log {

// =============================================================================
// Log Level Enumeration
// =============================================================================

/// @brief Log level enumeration matching Quill's log levels.
/// @note Uses kConstant naming convention per project style guide.
enum class Level {
    kTrace = 0,
    kDebug,
    kInfo,
    kWarning,
    kError,
    kCritical
};

// =============================================================================
// Logger Configuration
// =============================================================================

/// @brief Configuration options for logger initialization.
struct Config {
    /// @brief Log file path. Empty string disables file logging.
    std::string logFile;

    /// @brief Minimum log level to output.
    Level level = Level::kInfo;

    /// @brief Enable console (stderr) output.
    bool enableConsole = true;

    /// @brief Enable colored console output.
    bool enableColors = true;

    /// @brief Logger name for identification.
    std::string loggerName = "fqc";
};

// =============================================================================
// Logger Initialization and Access
// =============================================================================

/// @brief Initialize the global logger with the specified configuration.
/// @param config Logger configuration options.
/// @note This function should be called once at application startup.
/// @note Thread-safe: Can be called from any thread, but typically called
///       from main() before spawning worker threads.
void init(const Config& config);

/// @brief Initialize the global logger with default settings.
/// @param logFile Path to log file. Empty string disables file logging.
/// @param level Minimum log level to output.
/// @note Convenience overload for simple initialization.
void init(std::string_view logFile = "", Level level = Level::kInfo);

/// @brief Get the global logger instance.
/// @return Pointer to the global Quill logger.
/// @note Returns nullptr if init() has not been called.
/// @note Thread-safe: Can be called from any thread after initialization.
[[nodiscard]] quill::Logger* logger() noexcept;

/// @brief Check if the logger has been initialized.
/// @return true if init() has been called successfully.
[[nodiscard]] bool isInitialized() noexcept;

/// @brief Flush all pending log messages.
/// @note Blocks until all messages are written.
/// @note Useful before program exit or when immediate output is needed.
void flush();

/// @brief Shutdown the logging system.
/// @note Flushes all pending messages and stops the backend thread.
/// @note Should be called at program exit for clean shutdown.
void shutdown();

// =============================================================================
// Level Conversion Utilities
// =============================================================================

/// @brief Convert fqc::log::Level to Quill's LogLevel.
/// @param level The fqc log level.
/// @return Corresponding Quill log level.
[[nodiscard]] quill::LogLevel toQuillLevel(Level level) noexcept;

/// @brief Convert string to log level.
/// @param levelStr String representation (case-insensitive).
/// @return Corresponding log level, defaults to kInfo for unknown strings.
[[nodiscard]] Level levelFromString(std::string_view levelStr) noexcept;

/// @brief Convert log level to string.
/// @param level The log level.
/// @return String representation of the level.
[[nodiscard]] std::string_view levelToString(Level level) noexcept;

}  // namespace fqc::log

// =============================================================================
// Convenience Macros
// =============================================================================
// These macros provide a convenient interface for logging with automatic
// source location information.

/// @brief Log a trace message.
#define FQC_LOG_TRACE(fmt, ...) \
    LOG_TRACE_L1(fqc::log::logger(), fmt __VA_OPT__(, ) __VA_ARGS__)

/// @brief Log a debug message.
#define FQC_LOG_DEBUG(fmt, ...) \
    LOG_DEBUG(fqc::log::logger(), fmt __VA_OPT__(, ) __VA_ARGS__)

/// @brief Log an info message.
#define FQC_LOG_INFO(fmt, ...) \
    LOG_INFO(fqc::log::logger(), fmt __VA_OPT__(, ) __VA_ARGS__)

/// @brief Log a warning message.
#define FQC_LOG_WARNING(fmt, ...) \
    LOG_WARNING(fqc::log::logger(), fmt __VA_OPT__(, ) __VA_ARGS__)

/// @brief Log an error message.
#define FQC_LOG_ERROR(fmt, ...) \
    LOG_ERROR(fqc::log::logger(), fmt __VA_OPT__(, ) __VA_ARGS__)

/// @brief Log a critical message.
#define FQC_LOG_CRITICAL(fmt, ...) \
    LOG_CRITICAL(fqc::log::logger(), fmt __VA_OPT__(, ) __VA_ARGS__)

#endif  // FQC_COMMON_LOGGER_H
