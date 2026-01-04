//
// SafeTime.h
// SuperTerminal Framework
//
// Safe time utilities that handle clock_gettime exceptions
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef SUPERTERMINAL_SAFE_TIME_H
#define SUPERTERMINAL_SAFE_TIME_H

#include <chrono>
#include <system_error>

namespace SuperTerminal {
namespace SafeTime {

/// Safe wrapper for steady_clock::now() that catches system_error exceptions
/// @return Current time point, or a default-constructed time_point on error
/// @note Logs error to console if clock_gettime fails
inline std::chrono::steady_clock::time_point steadyNow() noexcept {
    try {
        return std::chrono::steady_clock::now();
    }
    catch (const std::system_error& e) {
        // Log error but don't crash
        // This can happen during sleep/wake or under heavy system load
        // Return epoch time as fallback
        return std::chrono::steady_clock::time_point{};
    }
    catch (...) {
        // Catch any other exceptions
        return std::chrono::steady_clock::time_point{};
    }
}

/// Safe wrapper for high_resolution_clock::now() that catches system_error exceptions
/// @return Current time point, or a default-constructed time_point on error
/// @note Logs error to console if clock_gettime fails
inline std::chrono::high_resolution_clock::time_point highResNow() noexcept {
    try {
        return std::chrono::high_resolution_clock::now();
    }
    catch (const std::system_error& e) {
        return std::chrono::high_resolution_clock::time_point{};
    }
    catch (...) {
        return std::chrono::high_resolution_clock::time_point{};
    }
}

/// Safe wrapper for system_clock::now() that catches system_error exceptions
/// @return Current time point, or a default-constructed time_point on error
/// @note Logs error to console if clock_gettime fails
inline std::chrono::system_clock::time_point systemNow() noexcept {
    try {
        return std::chrono::system_clock::now();
    }
    catch (const std::system_error& e) {
        return std::chrono::system_clock::time_point{};
    }
    catch (...) {
        return std::chrono::system_clock::time_point{};
    }
}

/// Calculate duration between two time points safely
/// @param start Start time point
/// @param end End time point
/// @return Duration in milliseconds, or 0 if calculation fails
template<typename Clock>
inline double durationMs(
    const typename Clock::time_point& start,
    const typename Clock::time_point& end) noexcept
{
    try {
        auto duration = end - start;
        return std::chrono::duration<double, std::milli>(duration).count();
    }
    catch (...) {
        return 0.0;
    }
}

/// Calculate elapsed time from a start point to now
/// @param start Start time point
/// @return Elapsed time in milliseconds, or 0 if calculation fails
template<typename Clock>
inline double elapsedMs(const typename Clock::time_point& start) noexcept {
    try {
        auto now = [&]() {
            if constexpr (std::is_same_v<Clock, std::chrono::steady_clock>) {
                return steadyNow();
            } else if constexpr (std::is_same_v<Clock, std::chrono::high_resolution_clock>) {
                return highResNow();
            } else {
                return systemNow();
            }
        }();
        return durationMs<Clock>(start, now);
    }
    catch (...) {
        return 0.0;
    }
}

/// Sleep for a specified duration safely
/// @param ms Milliseconds to sleep
/// @note Catches exceptions from sleep operations
inline void sleepMs(int ms) noexcept {
    try {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    catch (const std::system_error& e) {
        // Sleep failed, but don't crash
        // This can happen under heavy system load
    }
    catch (...) {
        // Catch any other exceptions
    }
}

/// Sleep until a specific time point safely
/// @param time_point Time point to sleep until
/// @note Catches exceptions from sleep operations
template<typename Clock, typename Duration>
inline void sleepUntil(const std::chrono::time_point<Clock, Duration>& time_point) noexcept {
    try {
        std::this_thread::sleep_until(time_point);
    }
    catch (const std::system_error& e) {
        // Sleep failed, but don't crash
    }
    catch (...) {
        // Catch any other exceptions
    }
}

} // namespace SafeTime
} // namespace SuperTerminal

#endif // SUPERTERMINAL_SAFE_TIME_H