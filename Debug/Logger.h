//
//  Logger.h
//  SuperTerminal Framework - Centralized Logging System
//
//  Thread-safe logging with configurable output and filtering
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef SUPERTERMINAL_LOGGER_H
#define SUPERTERMINAL_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <chrono>

// Undefine DEBUG macro if it exists to avoid collision with enum
#ifdef DEBUG
#undef DEBUG
#endif

namespace SuperTerminal {

/// Log level enumeration
enum class LogLevel {
    DEBUG = 0,      // Detailed debugging information
    INFO = 1,       // General informational messages
    WARNING = 2,    // Warning messages
    ERROR = 3,      // Error messages
    CRITICAL = 4,   // Critical errors
    NONE = 5        // Disable logging
};

/// Log output destination
enum class LogOutput {
    STDERR,         // Output to stderr
    FILE,           // Output to file
    BOTH            // Output to both stderr and file
};

/// Logger configuration
struct LoggerConfig {
    LogLevel minLevel = LogLevel::INFO;         // Minimum level to log
    LogOutput output = LogOutput::STDERR;       // Output destination
    std::string logFilePath;                    // Path to log file (if FILE or BOTH)
    bool includeTimestamp = true;               // Include timestamp in output
    bool includeThreadId = true;                // Include thread ID in output
    bool flushImmediately = true;               // Flush after each log
    
    LoggerConfig() = default;
};

/// Centralized thread-safe logging system
///
/// Usage:
///   Logger::instance().log(LogLevel::INFO, __FILE__, __FUNCTION__, "Starting operation");
///   LOG_INFO("Starting operation");  // Using macro
///
/// Thread Safety:
///   All methods are thread-safe and can be called from any thread
///
class Logger {
public:
    /// Get singleton instance
    /// @return Reference to the logger instance
    static Logger& instance();
    
    /// Initialize logger with configuration
    /// @param config Logger configuration
    /// @return true if initialization successful
    bool initialize(const LoggerConfig& config);
    
    /// Shutdown logger (closes file if open)
    void shutdown();
    
    /// Log a message
    /// @param level Log level
    /// @param file Source file name
    /// @param function Function name
    /// @param message Log message
    void log(LogLevel level, const char* file, const char* function, const std::string& message);
    
    /// Log a formatted message
    /// @param level Log level
    /// @param file Source file name
    /// @param function Function name
    /// @param format Printf-style format string
    /// @param ... Format arguments
    void logf(LogLevel level, const char* file, const char* function, const char* format, ...);
    
    /// Set minimum log level
    /// @param level Minimum level to log
    void setMinLevel(LogLevel level);
    
    /// Get minimum log level
    /// @return Current minimum log level
    LogLevel getMinLevel() const;
    
    /// Set output destination
    /// @param output Output destination
    void setOutput(LogOutput output);
    
    /// Enable/disable timestamp in output
    /// @param enable True to include timestamp
    void setIncludeTimestamp(bool enable);
    
    /// Enable/disable thread ID in output
    /// @param enable True to include thread ID
    void setIncludeThreadId(bool enable);
    
    /// Enable/disable immediate flush
    /// @param enable True to flush after each log
    void setFlushImmediately(bool enable);
    
    /// Set log file path (only applies if output is FILE or BOTH)
    /// @param path Path to log file
    /// @return true if file opened successfully
    bool setLogFilePath(const std::string& path);
    
    /// Check if logger is initialized
    /// @return true if initialized
    bool isInitialized() const;
    
    /// Get current configuration
    /// @return Current logger configuration
    const LoggerConfig& getConfig() const;
    
    // Disable copy and move
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;
    
private:
    /// Private constructor (singleton)
    Logger();
    
    /// Destructor
    ~Logger();
    
    /// Format log message with metadata
    /// @param level Log level
    /// @param file Source file
    /// @param function Function name
    /// @param message Log message
    /// @return Formatted log string
    std::string formatMessage(LogLevel level, const char* file, const char* function, 
                             const std::string& message);
    
    /// Get log level name
    /// @param level Log level
    /// @return String representation of level
    const char* getLevelName(LogLevel level);
    
    /// Get current timestamp as string
    /// @return Formatted timestamp
    std::string getTimestamp();
    
    /// Get base filename from full path
    /// @param path Full file path
    /// @return Base filename
    std::string getBasename(const char* path);
    
    /// Write to output destinations
    /// @param message Formatted message to write
    void writeOutput(const std::string& message);
    
    /// Open log file
    /// @return true if opened successfully
    bool openLogFile();
    
    /// Close log file
    void closeLogFile();
    
    // Members
    mutable std::mutex m_mutex;                 // Mutex for thread safety
    LoggerConfig m_config;                      // Current configuration
    std::ofstream m_logFile;                    // Log file stream
    bool m_initialized;                         // Initialization flag
    std::chrono::steady_clock::time_point m_startTime; // Start time for relative timestamps
};

} // namespace SuperTerminal

// =============================================================================
// Convenience Macros
// =============================================================================

// Log with automatic file and function
#define LOG_DEBUG(msg) \
    SuperTerminal::Logger::instance().log(SuperTerminal::LogLevel::DEBUG, __FILE__, __FUNCTION__, msg)

#define LOG_INFO(msg) \
    SuperTerminal::Logger::instance().log(SuperTerminal::LogLevel::INFO, __FILE__, __FUNCTION__, msg)

#define LOG_WARNING(msg) \
    SuperTerminal::Logger::instance().log(SuperTerminal::LogLevel::WARNING, __FILE__, __FUNCTION__, msg)

#define LOG_ERROR(msg) \
    SuperTerminal::Logger::instance().log(SuperTerminal::LogLevel::ERROR, __FILE__, __FUNCTION__, msg)

#define LOG_CRITICAL(msg) \
    SuperTerminal::Logger::instance().log(SuperTerminal::LogLevel::CRITICAL, __FILE__, __FUNCTION__, msg)

// Formatted logging macros
#define LOG_DEBUGF(fmt, ...) \
    SuperTerminal::Logger::instance().logf(SuperTerminal::LogLevel::DEBUG, __FILE__, __FUNCTION__, fmt, ##__VA_ARGS__)

#define LOG_INFOF(fmt, ...) \
    SuperTerminal::Logger::instance().logf(SuperTerminal::LogLevel::INFO, __FILE__, __FUNCTION__, fmt, ##__VA_ARGS__)

#define LOG_WARNINGF(fmt, ...) \
    SuperTerminal::Logger::instance().logf(SuperTerminal::LogLevel::WARNING, __FILE__, __FUNCTION__, fmt, ##__VA_ARGS__)

#define LOG_ERRORF(fmt, ...) \
    SuperTerminal::Logger::instance().logf(SuperTerminal::LogLevel::ERROR, __FILE__, __FUNCTION__, fmt, ##__VA_ARGS__)

#define LOG_CRITICALF(fmt, ...) \
    SuperTerminal::Logger::instance().logf(SuperTerminal::LogLevel::CRITICAL, __FILE__, __FUNCTION__, fmt, ##__VA_ARGS__)

#endif // SUPERTERMINAL_LOGGER_H