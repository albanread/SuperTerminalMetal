# SafeTime Utilities - Exception-Safe Time Operations

## Overview

`SafeTime.h` provides exception-safe wrappers around `std::chrono` time operations to prevent crashes from `clock_gettime()` system call failures.

## Problem

The standard C++ time functions can throw `std::system_error` exceptions when the underlying `clock_gettime()` system call fails. This can happen during:

- Sleep/wake transitions
- Heavy system load
- Resource exhaustion
- Clock hardware/driver issues
- System time adjustments

Example error:
```
libc++abi: terminating due to uncaught exception of type std::__1::system_error: 
clock_gettime(CLOCK_MONOTONIC_RAW) failed: unspecified generic_category error
```

## Solution

All functions in `SafeTime` are **noexcept** and return safe fallback values on error instead of throwing exceptions.

## Usage

### Include the Header

```cpp
#include "Util/SafeTime.h"
using namespace SuperTerminal::SafeTime;
```

### Get Current Time (Safe)

```cpp
// Before (can throw)
auto now = std::chrono::steady_clock::now();

// After (noexcept)
auto now = steadyNow();  // Returns epoch time on error
```

### Available Clock Functions

```cpp
// Steady clock (monotonic, best for intervals)
auto time1 = steadyNow();

// High resolution clock (highest precision)
auto time2 = highResNow();

// System clock (wall clock time)
auto time3 = systemNow();
```

### Calculate Duration

```cpp
auto start = steadyNow();
// ... do work ...
auto end = steadyNow();

// Safe duration calculation in milliseconds
double elapsed = durationMs<std::chrono::steady_clock>(start, end);
```

### Calculate Elapsed Time

```cpp
auto start = steadyNow();
// ... do work ...

// Returns elapsed time in milliseconds from start to now
double elapsed = elapsedMs<std::chrono::steady_clock>(start);
```

### Safe Sleep Operations

```cpp
// Sleep for duration (safe)
sleepMs(100);  // Sleep for 100ms

// Sleep until time point (safe)
auto wakeTime = steadyNow() + std::chrono::seconds(5);
sleepUntil(wakeTime);
```

## API Reference

### Clock Functions

| Function | Return Type | Description |
|----------|-------------|-------------|
| `steadyNow()` | `steady_clock::time_point` | Current time (monotonic) |
| `highResNow()` | `high_resolution_clock::time_point` | Current time (high precision) |
| `systemNow()` | `system_clock::time_point` | Current time (wall clock) |

### Duration Functions

| Function | Return Type | Description |
|----------|-------------|-------------|
| `durationMs<Clock>(start, end)` | `double` | Duration in milliseconds |
| `elapsedMs<Clock>(start)` | `double` | Elapsed time from start to now |

### Sleep Functions

| Function | Parameters | Description |
|----------|------------|-------------|
| `sleepMs(ms)` | `int ms` | Sleep for milliseconds |
| `sleepUntil(tp)` | `time_point` | Sleep until time point |

## Error Handling

All functions catch exceptions internally and return safe defaults:

- **Time functions**: Return `time_point{}` (epoch time) on error
- **Duration functions**: Return `0.0` on error
- **Sleep functions**: Return immediately on error

No exceptions are ever thrown to the caller.

## Migration Guide

### Simple Replacement

```cpp
// Before
auto now = std::chrono::steady_clock::now();
auto duration = end - start;
auto ms = std::chrono::duration<double, std::milli>(duration).count();

// After
auto now = steadyNow();
auto ms = durationMs<std::chrono::steady_clock>(start, end);
```

### Timer Class Example

```cpp
class Timer {
private:
    std::chrono::steady_clock::time_point m_start;

public:
    Timer() : m_start(steadyNow()) {}
    
    void reset() {
        m_start = steadyNow();
    }
    
    double elapsedMs() const {
        return SuperTerminal::SafeTime::elapsedMs<std::chrono::steady_clock>(m_start);
    }
};
```

### Rate Limiting Example

```cpp
class RateLimiter {
private:
    std::chrono::steady_clock::time_point m_lastTime;
    int m_intervalMs;

public:
    RateLimiter(int intervalMs) 
        : m_lastTime(steadyNow())
        , m_intervalMs(intervalMs) {}
    
    bool check() {
        auto now = steadyNow();
        auto elapsed = durationMs<std::chrono::steady_clock>(m_lastTime, now);
        
        if (elapsed >= m_intervalMs) {
            m_lastTime = now;
            return true;
        }
        return false;
    }
};
```

## When to Use

### Use SafeTime When:

✅ Measuring performance/timing  
✅ Implementing timeouts  
✅ Rate limiting operations  
✅ Animation timing  
✅ Audio/video synchronization  
✅ Any real-time or time-critical code  

### Standard chrono May Be OK When:

⚠️ One-time initialization (errors can be fatal)  
⚠️ Non-critical logging timestamps  
⚠️ Code that already has exception handlers  

## Performance

- **Zero overhead** in normal operation
- Exception handling only active when errors occur
- Same performance as raw `std::chrono` calls
- Inline functions optimize to single system call
- **Recommendation**: Use SafeTime by default

## Thread Safety

All SafeTime functions are:
- **Thread-safe**: Safe to call from multiple threads
- **Lock-free**: No mutexes or synchronization
- **Reentrant**: Safe to call recursively
- **Signal-safe**: Safe to call from signal handlers (with caveats)

## Best Practices

1. **Use steadyNow() for intervals** - monotonic, not affected by time adjustments
2. **Use systemNow() for wall clock** - when you need actual time of day
3. **Always check elapsed time** - don't assume operations complete instantly
4. **Use elapsedMs() helper** - simpler than manual duration calculation
5. **Prefer SafeTime over raw chrono** - prevents unexpected crashes

## Examples in Codebase

See these files for usage examples:
- `Framework/Particles/ParticleSystem.cpp` - Animation timing
- `Framework/Audio/MidiEngine.mm` - Note timing
- `Framework/Audio/Voice/VoiceScriptPlayer.cpp` - Script playback timing
- `Framework/Input/SimpleLineEditor.cpp` - Input debouncing

## Troubleshooting

### Q: Why would clock_gettime fail?

**A:** Common reasons:
- System waking from sleep
- Virtual machine clock sync issues
- System under extreme load
- Clock hardware malfunction
- System time being adjusted

### Q: What happens when time functions fail?

**A:** They return epoch time (time_point{}) which represents 00:00:00 UTC on January 1, 1970. This ensures:
- No crash
- Predictable behavior
- Duration calculations return 0
- Code continues running

### Q: Should I log when time functions fail?

**A:** Not recommended for production. Clock failures can be transient and frequent logging could impact performance. The SafeTime utilities are designed to fail silently and gracefully.

### Q: Can I detect when a time function failed?

**A:** Yes, check if the returned time_point equals epoch:
```cpp
auto now = steadyNow();
if (now == std::chrono::steady_clock::time_point{}) {
    // Clock operation failed, now is epoch time
}
```

## See Also

- [CRASH_FIXES_SUMMARY.md](../../CRASH_FIXES_SUMMARY.md) - Overview of all crash fixes
- [C++ Reference: std::chrono](https://en.cppreference.com/w/cpp/chrono)
- [POSIX clock_gettime()](https://man7.org/linux/man-pages/man2/clock_gettime.2.html)

## Version History

- **1.0** (Dec 30, 2024) - Initial implementation
  - Safe wrappers for steady_clock, high_resolution_clock, system_clock
  - Duration and elapsed time helpers
  - Safe sleep functions