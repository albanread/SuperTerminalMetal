# SuperTerminal Logger System

## Overview

The SuperTerminal Logger is a centralized, thread-safe logging system that provides structured, consistent logging across the entire codebase. It supports multiple log levels, configurable output destinations, and formatted messages.

## Features

- ✅ **Thread-Safe**: All operations are protected by mutexes
- ✅ **Multiple Log Levels**: DEBUG, INFO, WARNING, ERROR, CRITICAL
- ✅ **Flexible Output**: stderr, file, or both simultaneously
- ✅ **Structured Format**: Timestamp, level, thread ID, file, function, message
- ✅ **Easy to Use**: Convenience macros for one-line logging
- ✅ **Configurable**: Runtime configuration of levels and output
- ✅ **Singleton Pattern**: Global access from anywhere in the codebase
- ✅ **Performance**: Early filtering to avoid formatting overhead

## Quick Start

### Basic Usage

```cpp
#include "Framework/Debug/Logger.h"

// Simple logging with macros
LOG_INFO("Application started");
LOG_WARNING("Cache size exceeds limit");
LOG_ERROR("Failed to load file");

// Formatted logging
LOG_INFOF("Loading cart: %s", cartPath.c_str());
LOG_ERRORF("Invalid parameter: %d (expected: %d)", value, expected);
```

### Initialization

```cpp
// Initialize with default configuration (stderr output)
Logger::instance().initialize(LoggerConfig());

// Or customize configuration
LoggerConfig config;
config.minLevel = LogLevel::DEBUG;
config.output = LogOutput::FILE;
config.logFilePath = "/path/to/app.log";
config.includeTimestamp = true;
config.includeThreadId = true;
Logger::instance().initialize(config);
```

## Log Levels

| Level | Description | Use Case |
|-------|-------------|----------|
| `DEBUG` | Detailed debugging information | Development, troubleshooting |
| `INFO` | General informational messages | Normal operation tracking |
| `WARNING` | Warning messages | Potential issues, deprecations |
| `ERROR` | Error messages | Recoverable errors |
| `CRITICAL` | Critical errors | Fatal errors, crashes |
| `NONE` | Disable all logging | Production optimization |

## Output Formats

### Standard Format

```
[2024-01-15 10:30:45.123] [INFO    ] [T:123456] [CartManager.cpp::createCart] Cart created successfully
[2024-01-15 10:30:45.456] [ERROR   ] [T:123456] [AssetLoader.cpp::loadSprite] Failed to load sprite.png
[2024-01-15 10:30:45.789] [WARNING ] [T:789012] [Memory.cpp::allocate] Memory usage at 85%
```

### Components

- `[2024-01-15 10:30:45.123]` - Timestamp with milliseconds
- `[INFO    ]` - Log level (8 chars, left-aligned)
- `[T:123456]` - Thread ID (optional)
- `[CartManager.cpp::createCart]` - File and function
- Message text

## API Reference

### Logger Class

#### Singleton Access
```cpp
Logger& logger = Logger::instance();
```

#### Initialization
```cpp
bool initialize(const LoggerConfig& config);
void shutdown();
```

#### Logging Methods
```cpp
// Basic logging
void log(LogLevel level, const char* file, const char* function, const std::string& message);

// Formatted logging (printf-style)
void logf(LogLevel level, const char* file, const char* function, const char* format, ...);
```

#### Configuration
```cpp
void setMinLevel(LogLevel level);
LogLevel getMinLevel() const;

void setOutput(LogOutput output);
void setIncludeTimestamp(bool enable);
void setIncludeThreadId(bool enable);
void setFlushImmediately(bool enable);

bool setLogFilePath(const std::string& path);
```

### Convenience Macros

#### Simple Logging
```cpp
LOG_DEBUG(msg)      // Debug level
LOG_INFO(msg)       // Info level
LOG_WARNING(msg)    // Warning level
LOG_ERROR(msg)      // Error level
LOG_CRITICAL(msg)   // Critical level
```

#### Formatted Logging
```cpp
LOG_DEBUGF(fmt, ...)
LOG_INFOF(fmt, ...)
LOG_WARNINGF(fmt, ...)
LOG_ERRORF(fmt, ...)
LOG_CRITICALF(fmt, ...)
```

## Configuration

### LoggerConfig Structure

```cpp
struct LoggerConfig {
    LogLevel minLevel = LogLevel::INFO;         // Minimum level to log
    LogOutput output = LogOutput::STDERR;       // Output destination
    std::string logFilePath;                    // Log file path
    bool includeTimestamp = true;               // Include timestamp
    bool includeThreadId = true;                // Include thread ID
    bool flushImmediately = true;               // Flush after each log
};
```

### Output Destinations

```cpp
enum class LogOutput {
    STDERR,         // Output to stderr only
    FILE,           // Output to file only
    BOTH            // Output to both stderr and file
};
```

## Usage Examples

### Example 1: Basic Application Logging

```cpp
#include "Framework/Debug/Logger.h"

int main() {
    // Initialize logger
    Logger::instance().initialize(LoggerConfig());
    
    LOG_INFO("Application starting");
    
    // Your application code
    if (!loadResources()) {
        LOG_ERROR("Failed to load resources");
        return 1;
    }
    
    LOG_INFO("Application shutting down");
    Logger::instance().shutdown();
    return 0;
}
```

### Example 2: File Logging

```cpp
#include "Framework/Debug/Logger.h"

void initializeLogging() {
    LoggerConfig config;
    config.minLevel = LogLevel::DEBUG;
    config.output = LogOutput::FILE;
    config.logFilePath = "~/Library/Logs/SuperTerminal/app.log";
    config.flushImmediately = true;
    
    if (!Logger::instance().initialize(config)) {
        std::cerr << "Failed to initialize logger" << std::endl;
    }
}
```

### Example 3: Development vs Production

```cpp
void configureLogger(bool isProduction) {
    LoggerConfig config;
    
    if (isProduction) {
        config.minLevel = LogLevel::WARNING;
        config.output = LogOutput::FILE;
        config.logFilePath = "/var/log/superterminal.log";
        config.includeThreadId = false;
    } else {
        config.minLevel = LogLevel::DEBUG;
        config.output = LogOutput::BOTH;
        config.logFilePath = "./debug.log";
        config.includeThreadId = true;
    }
    
    Logger::instance().initialize(config);
}
```

### Example 4: Class-Based Logging

```cpp
class CartManager {
public:
    CartOperationResult createCart(const std::string& path) {
        LOG_INFOF("Creating cart: %s", path.c_str());
        
        if (path.empty()) {
            LOG_ERROR("Cart path cannot be empty");
            return CartOperationResult::Failure("Invalid path");
        }
        
        if (fileExists(path)) {
            LOG_WARNINGF("Cart already exists: %s", path.c_str());
            return CartOperationResult::Failure("File exists");
        }
        
        LOG_DEBUG("Initializing cart metadata");
        // ... create cart ...
        
        LOG_INFOF("Cart created successfully: %s", path.c_str());
        return CartOperationResult::Success();
    }
};
```

### Example 5: Multi-threaded Logging

```cpp
#include <thread>
#include "Framework/Debug/Logger.h"

void workerThread(int id) {
    LOG_INFOF("Worker %d starting", id);
    
    for (int i = 0; i < 100; i++) {
        LOG_DEBUGF("Worker %d: Processing item %d", id, i);
    }
    
    LOG_INFOF("Worker %d finished", id);
}

int main() {
    Logger::instance().initialize(LoggerConfig());
    
    std::thread t1(workerThread, 1);
    std::thread t2(workerThread, 2);
    std::thread t3(workerThread, 3);
    
    t1.join();
    t2.join();
    t3.join();
    
    Logger::instance().shutdown();
}
```

## Best Practices

### 1. Use Appropriate Log Levels

```cpp
// ✅ Good - Correct level usage
LOG_DEBUG("Cache hit for key: xyz");
LOG_INFO("User logged in");
LOG_WARNING("Deprecated API called");
LOG_ERROR("Database connection failed");
LOG_CRITICAL("Out of memory");

// ❌ Bad - Wrong level usage
LOG_ERROR("User clicked button");  // Should be DEBUG or INFO
LOG_INFO("Application crashed");   // Should be CRITICAL
```

### 2. Use Formatted Logging for Variables

```cpp
// ✅ Good - Formatted logging
LOG_INFOF("Loading %d assets from %s", count, path.c_str());

// ❌ Bad - String concatenation (slow)
LOG_INFO("Loading " + std::to_string(count) + " assets from " + path);
```

### 3. Log Entry/Exit for Complex Functions

```cpp
void complexOperation() {
    LOG_DEBUG("complexOperation() ENTER");
    
    try {
        // ... operation ...
        LOG_DEBUG("complexOperation() EXIT (success)");
    } catch (const std::exception& e) {
        LOG_ERRORF("complexOperation() EXIT (error: %s)", e.what());
        throw;
    }
}
```

### 4. Log State Changes

```cpp
void setState(State newState) {
    LOG_INFOF("State transition: %s -> %s", 
              stateToString(m_state).c_str(),
              stateToString(newState).c_str());
    m_state = newState;
}
```

### 5. Avoid Logging in Tight Loops

```cpp
// ❌ Bad - Logs every iteration
for (int i = 0; i < 1000000; i++) {
    LOG_DEBUG("Processing item");  // Too much output!
}

// ✅ Good - Log milestones
for (int i = 0; i < 1000000; i++) {
    if (i % 100000 == 0) {
        LOG_DEBUGF("Processing progress: %d%%", (i * 100) / 1000000);
    }
}
```

## Performance Considerations

### Early Filtering

The logger performs early filtering before acquiring locks:

```cpp
// This is fast - no lock acquired if level is filtered
if (level < m_config.minLevel) {
    return;
}
```

### Formatted Logging

Use `logf()` or the `*F` macros for better performance with variables:

```cpp
// ✅ Efficient
LOG_INFOF("Count: %d", count);

// ❌ Less efficient
LOG_INFO("Count: " + std::to_string(count));
```

### Conditional Compilation

For debug-only logging, use preprocessor directives:

```cpp
#ifdef DEBUG_LOGGING
    LOG_DEBUGF("Detailed debug info: %s", details.c_str());
#endif
```

## Thread Safety

All Logger methods are thread-safe and can be called from any thread:

- Singleton instance is thread-safe (C++11 guarantee)
- All public methods acquire a mutex
- File I/O is synchronized
- No data races or deadlocks

## File Management

### Log File Location

Default locations:
- **macOS**: `~/Library/Logs/SuperTerminal/app.log`
- **Linux**: `~/.local/share/SuperTerminal/logs/app.log`
- **Custom**: Specify any path via `setLogFilePath()`

### Log File Format

Files include session headers:

```
========================================
Log session started: 2024-01-15 10:30:45.123
========================================

[2024-01-15 10:30:45.124] [INFO    ] [T:123456] [main.cpp::main] Application started
...

========================================
Log session ended: 2024-01-15 10:35:22.456
========================================
```

## Integration with Existing Code

### Replace fprintf/NSLog

```cpp
// Before
fprintf(stderr, "[CartManager] Creating cart: %s\n", path.c_str());
NSLog(@"Loading sprite: %@", spriteName);

// After
LOG_INFOF("Creating cart: %s", path.c_str());
LOG_INFOF("Loading sprite: %s", spriteName);
```

### Conditional Debug Logging

```cpp
// Before
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] Value: %d\n", value);
#endif

// After
LOG_DEBUGF("Value: %d", value);
// Logger handles filtering based on minLevel
```

## Troubleshooting

### Logger Not Outputting

1. Check initialization:
   ```cpp
   if (!Logger::instance().isInitialized()) {
       LOG_WARNING("Logger not initialized!");
   }
   ```

2. Check log level:
   ```cpp
   LOG_DEBUGF("Current min level: %d", 
              static_cast<int>(Logger::instance().getMinLevel()));
   ```

### File Not Created

1. Check path permissions
2. Ensure parent directory exists
3. Check return value of `initialize()` or `setLogFilePath()`

### Performance Issues

1. Set `flushImmediately = false` for better performance
2. Increase minimum log level in production
3. Avoid logging in hot loops


## See Also

- **Logger.h** - Header file with API documentation
- **Logger.cpp** - Implementation details
- **Examples/** - Sample usage code
