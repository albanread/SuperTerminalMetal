//
//  test_logger.cpp
//  SuperTerminal Framework - Logger Test/Example Program
//
//  Demonstrates usage of the centralized logging system
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "Logger.h"
#include <thread>
#include <chrono>
#include <vector>

using namespace SuperTerminal;

// =============================================================================
// Test Functions
// =============================================================================

void testBasicLogging() {
    LOG_INFO("=== Test 1: Basic Logging ===");
    
    LOG_DEBUG("This is a debug message");
    LOG_INFO("This is an info message");
    LOG_WARNING("This is a warning message");
    LOG_ERROR("This is an error message");
    LOG_CRITICAL("This is a critical message");
    
    LOG_INFO("");
}

void testFormattedLogging() {
    LOG_INFO("=== Test 2: Formatted Logging ===");
    
    int count = 42;
    float value = 3.14159f;
    const char* name = "TestCart";
    
    LOG_INFOF("Integer: %d", count);
    LOG_INFOF("Float: %.2f", value);
    LOG_INFOF("String: %s", name);
    LOG_INFOF("Multiple: %s has %d items worth $%.2f", name, count, value);
    
    LOG_INFO("");
}

void testLogLevels() {
    LOG_INFO("=== Test 3: Log Level Filtering ===");
    
    // Save original level
    LogLevel originalLevel = Logger::instance().getMinLevel();
    
    LOG_INFO("Setting minimum level to WARNING...");
    Logger::instance().setMinLevel(LogLevel::WARNING);
    
    LOG_DEBUG("This DEBUG message should NOT appear");
    LOG_INFO("This INFO message should NOT appear");
    LOG_WARNING("This WARNING message SHOULD appear");
    LOG_ERROR("This ERROR message SHOULD appear");
    
    // Restore original level
    Logger::instance().setMinLevel(originalLevel);
    LOG_INFO("Restored original log level");
    
    LOG_INFO("");
}

void testFunctionLogging() {
    LOG_INFO("=== Test 4: Function Entry/Exit Logging ===");
    
    LOG_DEBUG("testFunctionLogging() ENTER");
    
    LOG_INFO("Performing operation...");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    LOG_DEBUG("testFunctionLogging() EXIT");
    
    LOG_INFO("");
}

void simulateError() {
    LOG_DEBUG("simulateError() ENTER");
    
    try {
        LOG_WARNING("About to trigger error condition");
        throw std::runtime_error("Simulated error for testing");
    } catch (const std::exception& e) {
        LOG_ERRORF("Caught exception: %s", e.what());
    }
    
    LOG_DEBUG("simulateError() EXIT");
}

void testErrorHandling() {
    LOG_INFO("=== Test 5: Error Handling ===");
    
    simulateError();
    LOG_INFO("Error handled gracefully");
    
    LOG_INFO("");
}

void workerThread(int threadId) {
    LOG_INFOF("Worker thread %d starting", threadId);
    
    for (int i = 0; i < 5; i++) {
        LOG_DEBUGF("[Thread %d] Processing item %d", threadId, i);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    LOG_INFOF("Worker thread %d completed", threadId);
}

void testMultithreading() {
    LOG_INFO("=== Test 6: Multi-threaded Logging ===");
    
    std::vector<std::thread> threads;
    
    for (int i = 1; i <= 3; i++) {
        threads.emplace_back(workerThread, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    LOG_INFO("All worker threads completed");
    LOG_INFO("");
}

void testStateChanges() {
    LOG_INFO("=== Test 7: State Transition Logging ===");
    
    enum class State { IDLE, LOADING, RUNNING, STOPPED };
    
    auto stateToString = [](State s) -> const char* {
        switch (s) {
            case State::IDLE: return "IDLE";
            case State::LOADING: return "LOADING";
            case State::RUNNING: return "RUNNING";
            case State::STOPPED: return "STOPPED";
            default: return "UNKNOWN";
        }
    };
    
    State current = State::IDLE;
    State next;
    
    next = State::LOADING;
    LOG_INFOF("State transition: %s -> %s", stateToString(current), stateToString(next));
    current = next;
    
    next = State::RUNNING;
    LOG_INFOF("State transition: %s -> %s", stateToString(current), stateToString(next));
    current = next;
    
    next = State::STOPPED;
    LOG_INFOF("State transition: %s -> %s", stateToString(current), stateToString(next));
    current = next;
    
    LOG_INFO("");
}

void testPerformanceLogging() {
    LOG_INFO("=== Test 8: Performance Metrics ===");
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Simulate operation
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    LOG_INFOF("Operation completed in %lld ms", duration.count());
    
    LOG_INFO("");
}

void testCartOperations() {
    LOG_INFO("=== Test 9: Simulated Cart Operations ===");
    
    const char* cartPath = "test.crt";
    
    LOG_INFOF("Creating cart: %s", cartPath);
    LOG_DEBUG("Validating cart path");
    LOG_DEBUG("Initializing cart metadata");
    LOG_DEBUG("Creating SQLite database");
    LOG_INFOF("Cart created successfully: %s", cartPath);
    
    LOG_INFOF("Loading cart: %s", cartPath);
    LOG_DEBUG("Opening SQLite connection");
    LOG_DEBUG("Reading metadata");
    LOG_DEBUG("Loading program source");
    LOG_INFOF("Cart loaded successfully: %s", cartPath);
    
    LOG_INFOF("Saving cart: %s", cartPath);
    LOG_DEBUG("Writing changes to database");
    LOG_DEBUG("Committing transaction");
    LOG_INFOF("Cart saved successfully: %s", cartPath);
    
    LOG_INFO("");
}

void testResourceManagement() {
    LOG_INFO("=== Test 10: Resource Management ===");
    
    LOG_DEBUG("Allocating resource: texture_001");
    LOG_DEBUG("Allocating resource: sound_bgm");
    LOG_DEBUG("Allocating resource: sprite_player");
    
    LOG_INFOF("Total resources allocated: %d", 3);
    
    LOG_WARNING("Memory usage at 75%");
    
    LOG_DEBUG("Freeing resource: texture_001");
    LOG_DEBUG("Freeing resource: sound_bgm");
    LOG_DEBUG("Freeing resource: sprite_player");
    
    LOG_INFOF("Total resources freed: %d", 3);
    
    LOG_INFO("");
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    // Initialize logger with default configuration
    LoggerConfig config;
    config.minLevel = LogLevel::DEBUG;
    config.output = LogOutput::STDERR;
    config.includeTimestamp = true;
    config.includeThreadId = true;
    config.flushImmediately = true;
    
    if (!Logger::instance().initialize(config)) {
        std::cerr << "Failed to initialize logger" << std::endl;
        return 1;
    }
    
    LOG_INFO("╔══════════════════════════════════════════════════════════════╗");
    LOG_INFO("║       SuperTerminal Logger - Test Suite                     ║");
    LOG_INFO("╚══════════════════════════════════════════════════════════════╝");
    LOG_INFO("");
    
    // Run all tests
    testBasicLogging();
    testFormattedLogging();
    testLogLevels();
    testFunctionLogging();
    testErrorHandling();
    testMultithreading();
    testStateChanges();
    testPerformanceLogging();
    testCartOperations();
    testResourceManagement();
    
    LOG_INFO("╔══════════════════════════════════════════════════════════════╗");
    LOG_INFO("║       All Tests Completed Successfully!                     ║");
    LOG_INFO("╚══════════════════════════════════════════════════════════════╝");
    
    // Shutdown logger
    Logger::instance().shutdown();
    
    return 0;
}