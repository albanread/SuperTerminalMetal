//
//  StarManager.h
//  SuperTerminal Framework - GPU-Accelerated Star Rendering
//
//  High-performance instanced star rendering with gradients
//  Uses Metal instanced drawing for minimal CPU overhead
//  Copyright © 2024-2025 SuperTerminal. All rights reserved.
//

#ifndef STAR_MANAGER_H
#define STAR_MANAGER_H

#include <Metal/Metal.h>
#include <vector>
#include <map>
#include <cstdint>
#include <mutex>

namespace SuperTerminal {

// =============================================================================
// Star Gradient Modes
// =============================================================================

enum class StarGradientMode : uint32_t {
    SOLID = 0,              // Single solid color
    RADIAL = 1,             // Center to points gradient
    ALTERNATING = 2,        // Alternating colors per point
    
    // Advanced patterns (starting at 100)
    OUTLINE = 100,          // Outlined star (color1=fill, color2=outline)
    DASHED_OUTLINE = 101,   // Dashed outline
};

// =============================================================================
// Star Instance Data (GPU Format)
// =============================================================================

struct StarInstance {
    float x, y;             // Position in pixels (center)
    float outerRadius;      // Outer radius in pixels (to points)
    float innerRadius;      // Inner radius in pixels (between points)
    uint32_t color1;        // Primary color (RGBA8888)
    uint32_t color2;        // Secondary color for gradients
    uint32_t mode;          // StarGradientMode
    uint32_t numPoints;     // Number of points (3-12)
    float rotation;         // Rotation in radians
    float param1;           // Pattern parameter 1 (e.g., line width)
    float param2;           // Pattern parameter 2 (e.g., dash length)
    float padding[1];       // Alignment padding
    
    StarInstance()
        : x(0), y(0), outerRadius(0), innerRadius(0)
        , color1(0xFFFFFFFF), color2(0xFFFFFFFF)
        , mode(0), numPoints(5)
        , rotation(0)
        , param1(0.0f), param2(0.0f)
        , padding{0} {
    }
};

// =============================================================================
// StarManager - High-Performance Star Rendering
// =============================================================================

class StarManager {
public:
    StarManager();
    ~StarManager();
    
    // Initialize with Metal device
    bool initialize(id<MTLDevice> device, int screenWidth, int screenHeight);
    
    // ID-based star management (persistent, updatable)
    int createStar(float x, float y, float outerRadius, int numPoints, uint32_t color);
    int createCustomStar(float x, float y, float outerRadius, float innerRadius,
                        int numPoints, uint32_t color);
    int createGradient(float x, float y, float outerRadius, int numPoints,
                      uint32_t color1, uint32_t color2,
                      StarGradientMode mode);
    int createOutline(float x, float y, float outerRadius, int numPoints,
                     uint32_t fillColor, uint32_t outlineColor,
                     float lineWidth = 2.0f);
    
    // Update star properties
    bool setPosition(int id, float x, float y);
    bool setRadius(int id, float outerRadius);
    bool setRadii(int id, float outerRadius, float innerRadius);
    bool setPoints(int id, int numPoints);
    bool setColor(int id, uint32_t color);
    bool setColors(int id, uint32_t color1, uint32_t color2);
    bool setRotation(int id, float angleDegrees);
    bool setVisible(int id, bool visible);
    
    // Query stars
    bool exists(int id) const;
    bool isVisible(int id) const;
    
    // Delete stars
    bool deleteStar(int id);
    void deleteAll();
    
    // Statistics and management
    size_t getStarCount() const;
    bool isEmpty() const;
    
    // Rendering
    void render(id<MTLRenderCommandEncoder> encoder);
    
    // Screen size updates (for coordinate transformation)
    void updateScreenSize(int width, int height);
    
    // Statistics
    size_t getMaxStars() const { return m_maxStars; }
    void setMaxStars(size_t max);
    
    // Thread safety
    void lock() { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }
    
private:
    // Metal resources
    id<MTLDevice> m_device;
    id<MTLRenderPipelineState> m_pipelineState;
    id<MTLBuffer> m_instanceBuffer;
    id<MTLBuffer> m_uniformBuffer;
    
    // Star storage with ID-based management
    struct ManagedStar {
        StarInstance data;
        bool visible;
        
        ManagedStar() : visible(true) {}
    };
    
    std::map<int, ManagedStar> m_managedStars;  // ID-based persistent stars
    int m_nextId;
    size_t m_maxStars;
    bool m_bufferNeedsUpdate;
    
    // Screen dimensions
    int m_screenWidth;
    int m_screenHeight;
    
    // Thread safety
    mutable std::mutex m_mutex;
    
    // Internal methods
    bool createPipeline();
    void updateInstanceBuffer();
    void updateUniforms();
};

// =============================================================================
// Uniform Data (Passed to Shaders)
// =============================================================================

struct StarUniforms {
    float screenWidth;
    float screenHeight;
    float padding[2];  // Align to 16 bytes
};

} // namespace SuperTerminal

#endif // STAR_MANAGER_H