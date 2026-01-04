//
//  Logger.cpp
//  SuperTerminal Framework - Centralized Logging System
//
//  Thread-safe logging with configurable output and filtering
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "Logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cstdarg>
#include <thread>
#include <cstring>

// Undefine DEBUG macro if it exists to avoid collision with enum
#ifdef DEBUG
#undef DEBUG
#endif

#ifdef __APPLE__
#include <mach/mach.h>
#include <pthread.h>
#endif

namespace SuperTerminal {

// =============================================================================
// Singleton Instance
// =============================================================================

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

Logger::Logger()
    : m_initialized(false)
    , m_startTime(std::chrono::steady_clock::now())
{
    // Initialize with default configuration
    m_config.minLevel = LogLevel::INFO;
    m_config.output = LogOutput::STDERR;
    m_config.includeTimestamp = true;
    m_config.includeThreadId = true;
    m_config.flushImmediately = true;
}

Logger::~Logger() {
    shutdown();
}

// =============================================================================
// Initialization
// =============================================================================

bool Logger::initialize(const LoggerConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Close existing log file if open
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    
    m_config = config;
    
    // Open log file if needed
    if (m_config.output == LogOutput::FILE || m_config.output == LogOutput::BOTH) {
        if (!m_config.logFilePath.empty()) {
            if (!openLogFile()) {
                std::cerr << "[Logger] ERROR: Failed to open log file: " 
                         << m_config.logFilePath << std::endl;
                return false;
            }
        } else {
            std::cerr << "[Logger] ERROR: Log file path not specified" << std::endl;
            return false;
        }
    }
    
    m_initialized = true;
    m_startTime = std::chrono::steady_clock::now();
    
    // Note: Cannot log here because we hold the mutex lock
    // First log will be from the caller after initialize() returns
    
    return true;
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        // Note: Cannot log here because we hold the mutex lock
        closeLogFile();
        m_initialized = false;
    }
}

// =============================================================================
// Logging Methods
// =============================================================================

void Logger::log(LogLevel level, const char* file, const char* function, const std::string& message) {
    // Quick check without lock for performance
    if (level < m_config.minLevel) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Double-check after acquiring lock
    if (level < m_config.minLevel) {
        return;
    }
    
    std::string formattedMessage = formatMessage(level, file, function, message);
    writeOutput(formattedMessage);
}

void Logger::logf(LogLevel level, const char* file, const char* function, const char* format, ...) {
    // Quick check without lock
    if (level < m_config.minLevel) {
        return;
    }
    
    // Format the message
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    log(level, file, function, std::string(buffer));
}

// =============================================================================
// Configuration Methods
// =============================================================================

void Logger::setMinLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.minLevel = level;
}

LogLevel Logger::getMinLevel() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config.minLevel;
}

void Logger::setOutput(LogOutput output) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    LogOutput oldOutput = m_config.output;
    m_config.output = output;
    
    // Open log file if switching to FILE or BOTH
    if ((output == LogOutput::FILE || output == LogOutput::BOTH) && 
        (oldOutput != LogOutput::FILE && oldOutput != LogOutput::BOTH)) {
        if (!m_config.logFilePath.empty() && !m_logFile.is_open()) {
            openLogFile();
        }
    }
    
    // Close log file if switching away from FILE/BOTH
    if (output == LogOutput::STDERR && 
        (oldOutput == LogOutput::FILE || oldOutput == LogOutput::BOTH)) {
        closeLogFile();
    }
}

void Logger::setIncludeTimestamp(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.includeTimestamp = enable;
}

void Logger::setIncludeThreadId(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.includeThreadId = enable;
}

void Logger::setFlushImmediately(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.flushImmediately = enable;
}

bool Logger::setLogFilePath(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Close existing file
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    
    m_config.logFilePath = path;
    
    // Open new file if output is set to FILE or BOTH
    if (m_config.output == LogOutput::FILE || m_config.output == LogOutput::BOTH) {
        return openLogFile();
    }
    
    return true;
}

bool Logger::isInitialized() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialized;
}

const LoggerConfig& Logger::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

// =============================================================================
// Private Helper Methods
// =============================================================================

std::string Logger::formatMessage(LogLevel level, const char* file, const char* function,
                                  const std::string& message) {
    std::ostringstream oss;
    
    // Timestamp
    if (m_config.includeTimestamp) {
        oss << "[" << getTimestamp() << "]";
    }
    
    // Log level
    oss << " [" << std::setw(8) << std::left << getLevelName(level) << "]";
    
    // Thread ID
    if (m_config.includeThreadId) {
        std::ostringstream tid;
        tid << std::this_thread::get_id();
        oss << " [T:" << tid.str() << "]";
    }
    
    // File and function
    if (file) {
        oss << " [" << getBasename(file);
        if (function) {
            oss << "::" << function;
        }
        oss << "]";
    } else if (function) {
        oss << " [" << function << "]";
    }
    
    // Message
    oss << " " << message;
    
    return oss.str();
}

const char* Logger::getLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO";
        case LogLevel::WARNING:  return "WARNING";
        case LogLevel::ERROR:    return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        case LogLevel::NONE:     return "NONE";
        default:                 return "UNKNOWN";
    }
}

std::string Logger::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &now_time_t);
#else
    localtime_r(&now_time_t, &tm_buf);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << now_ms.count();
    
    return oss.str();
}

std::string Logger::getBasename(const char* path) {
    if (!path) return "";
    
    const char* lastSlash = strrchr(path, '/');
    if (!lastSlash) {
        lastSlash = strrchr(path, '\\');
    }
    
    return lastSlash ? std::string(lastSlash + 1) : std::string(path);
}

void Logger::writeOutput(const std::string& message) {
    // Write to stderr
    if (m_config.output == LogOutput::STDERR || m_config.output == LogOutput::BOTH) {
        std::cerr << message << std::endl;
        if (m_config.flushImmediately) {
            std::cerr.flush();
        }
    }
    
    // Write to file
    if ((m_config.output == LogOutput::FILE || m_config.output == LogOutput::BOTH) && 
        m_logFile.is_open()) {
        m_logFile << message << std::endl;
        if (m_config.flushImmediately) {
            m_logFile.flush();
        }
    }
}

bool Logger::openLogFile() {
    if (m_config.logFilePath.empty()) {
        return false;
    }
    
    m_logFile.open(m_config.logFilePath, std::ios::out | std::ios::app);
    
    if (!m_logFile.is_open()) {
        std::cerr << "[Logger] ERROR: Failed to open log file: " 
                  << m_config.logFilePath << std::endl;
        return false;
    }
    
    // Write header separator
    m_logFile << "\n========================================" << std::endl;
    m_logFile << "Log session started: " << getTimestamp() << std::endl;
    m_logFile << "========================================\n" << std::endl;
    
    return true;
}

void Logger::closeLogFile() {
    if (m_logFile.is_open()) {
        m_logFile << "\n========================================" << std::endl;
        m_logFile << "Log session ended: " << getTimestamp() << std::endl;
        m_logFile << "========================================\n" << std::endl;
        m_logFile.close();
    }
}

} // namespace SuperTerminal