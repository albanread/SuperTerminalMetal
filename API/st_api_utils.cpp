//
// st_api_utils.cpp
// SuperTerminal v2 - C API Utility Implementation
//
// Utility functions: color conversion, frame control, timing, versioning, debug
//

#include "superterminal_api.h"
#include "st_api_context.h"
#include <cmath>
#include <cstdio>
#include <random>
#include <chrono>
#include <thread>

using namespace STApi;

// =============================================================================
// Utility API - Color Functions
// =============================================================================

ST_API STColor st_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return st_rgba(r, g, b, 255);
}

ST_API STColor st_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (static_cast<uint32_t>(r) << 24) |
           (static_cast<uint32_t>(g) << 16) |
           (static_cast<uint32_t>(b) << 8) |
           static_cast<uint32_t>(a);
}

ST_API STColor st_hsv(float h, float s, float v) {
    // HSV to RGB conversion
    // h: 0-360, s: 0-1, v: 0-1
    
    // Normalize hue to [0, 360)
    while (h < 0.0f) h += 360.0f;
    while (h >= 360.0f) h -= 360.0f;
    
    // Clamp saturation and value
    if (s < 0.0f) s = 0.0f;
    if (s > 1.0f) s = 1.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    
    float c = v * s;  // Chroma
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    
    float r_prime, g_prime, b_prime;
    
    if (h < 60.0f) {
        r_prime = c; g_prime = x; b_prime = 0.0f;
    } else if (h < 120.0f) {
        r_prime = x; g_prime = c; b_prime = 0.0f;
    } else if (h < 180.0f) {
        r_prime = 0.0f; g_prime = c; b_prime = x;
    } else if (h < 240.0f) {
        r_prime = 0.0f; g_prime = x; b_prime = c;
    } else if (h < 300.0f) {
        r_prime = x; g_prime = 0.0f; b_prime = c;
    } else {
        r_prime = c; g_prime = 0.0f; b_prime = x;
    }
    
    uint8_t r = static_cast<uint8_t>((r_prime + m) * 255.0f);
    uint8_t g = static_cast<uint8_t>((g_prime + m) * 255.0f);
    uint8_t b = static_cast<uint8_t>((b_prime + m) * 255.0f);
    
    return st_rgb(r, g, b);
}

ST_API void st_color_components(STColor color, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) {
    if (r) *r = (color >> 24) & 0xFF;
    if (g) *g = (color >> 16) & 0xFF;
    if (b) *b = (color >> 8) & 0xFF;
    if (a) *a = color & 0xFF;
}

// =============================================================================
// Frame Control API
// =============================================================================

ST_API void st_wait_frame(void) {
    // Request to wait for 1 frame
    // Script thread blocks, render thread wakes it up
    ST_CONTEXT.requestFrameWait(1);
}

ST_API void st_wait_frames(int count) {
    // Request to wait for N frames
    // Script thread blocks, render thread wakes it up after N frames
    ST_CONTEXT.requestFrameWait(count);
}

ST_API void st_wait_ms(int milliseconds) {
    if (milliseconds <= 0) return;
    
    // For waits > 25ms, split into 25ms chunks and check cancellation
    while (milliseconds > 25) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        
        // Check if script should be cancelled
        if (ST_CONTEXT.shouldStopScript()) {
            return; // Exit early if script is being stopped
        }
        
        milliseconds -= 25;
    }
    
    // Handle remaining milliseconds (0-25ms)
    if (milliseconds > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }
}

ST_API uint64_t st_frame_count(void) {
    ST_LOCK;
    return ST_CONTEXT.frameCount();
}

ST_API double st_time(void) {
    ST_LOCK;
    return ST_CONTEXT.time();
}

ST_API double st_delta_time(void) {
    ST_LOCK;
    return ST_CONTEXT.deltaTime();
}

// =============================================================================
// Version API
// =============================================================================

ST_API void st_version(int* major, int* minor, int* patch) {
    if (major) *major = SUPERTERMINAL_VERSION_MAJOR;
    if (minor) *minor = SUPERTERMINAL_VERSION_MINOR;
    if (patch) *patch = SUPERTERMINAL_VERSION_PATCH;
}

ST_API const char* st_version_string(void) {
    static const char* version_string = "2.0.0-dev";
    return version_string;
}

ST_API double st_timer(void) {
    // Get elapsed time since app start
    static auto start_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - start_time;
    return elapsed.count();
}

// =============================================================================
// Debug API
// =============================================================================

ST_API void st_debug_print(const char* message) {
    if (message) {
        fprintf(stderr, "[SuperTerminal] %s\n", message);
        fflush(stderr);
    }
}

ST_API const char* st_get_last_error(void) {
    return ST_CONTEXT.getLastError();
}

ST_API void st_clear_error(void) {
    ST_CONTEXT.clearError();
}

// =============================================================================
// Random Number Generation API
// =============================================================================

// Thread-local random engine for better performance
static thread_local std::mt19937 g_random_engine(std::random_device{}());

ST_API double st_random(void) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(g_random_engine);
}

ST_API int st_random_int(int min, int max) {
    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }
    std::uniform_int_distribution<int> dist(min, max);
    return dist(g_random_engine);
}

ST_API void st_random_seed(uint32_t seed) {
    g_random_engine.seed(seed);
}