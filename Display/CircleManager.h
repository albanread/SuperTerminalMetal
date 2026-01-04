//
//  CircleManager.h
//  SuperTerminal Framework - GPU-Accelerated Circle Rendering
//
//  High-performance instanced circle rendering with gradients
//  Uses Metal instanced drawing for minimal CPU overhead
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef CIRCLE_MANAGER_H
#define CIRCLE_MANAGER_H

#include <Metal/Metal.h>
#include <vector>
#include <map>
#include <cstdint>
#include <mutex>

namespace SuperTerminal {

// =============================================================================
// Gradient Modes
// =============================================================================

enum class CircleGradientMode : uint32_t {
    SOLID = 0,              // Single solid color
    RADIAL = 1,             // Center to edge (2 colors)
    RADIAL_3 = 2,           // Three-ring radial gradient
    RADIAL_4 = 3,           // Four-ring radial gradient
    
    // Advanced patterns (starting at 100)
    OUTLINE = 100,          // Outlined circle (color1=fill, color2=outline)
    DASHED_OUTLINE = 101,   // Dashed outline
    RING = 102,             // Hollow ring
    PIE = 103,              // Pie slice (for charts)
    ARC = 104,              // Arc segment
    DOTS_RING = 105,        // Dots arranged in a ring
    STAR_BURST = 106        // Star burst pattern from center
};

// =============================================================================
// Circle Instance Data (GPU Format)
// =============================================================================

struct CircleInstance {
    float x, y;             // Position in pixels (center)
    float radius;           // Radius in pixels
    float padding1;         // Alignment padding
    uint32_t color1;        // Primary color (RGBA8888)
    uint32_t color2;        // Secondary color for gradients
    uint32_t color3;        // Tertiary color
    uint32_t color4;        // Quaternary color
    uint32_t mode;          // CircleGradientMode
    float param1;           // Pattern parameter 1 (e.g., line width, inner radius)
    float param2;           // Pattern parameter 2 (e.g., start angle, dash length)
    float param3;           // Pattern parameter 3 (e.g., end angle, spacing)
    
    CircleInstance()
        : x(0), y(0), radius(0), padding1(0)
        , color1(0xFFFFFFFF), color2(0xFFFFFFFF)
        , color3(0xFFFFFFFF), color4(0xFFFFFFFF)
        , mode(0)
        , param1(0.0f), param2(0.0f), param3(0.0f) {
    }
};

// =============================================================================
// CircleManager - High-Performance Circle Rendering
// =============================================================================

class CircleManager {
public:
    CircleManager();
    ~CircleManager();
    
    // Initialize with Metal device
    bool initialize(id<MTLDevice> device, int screenWidth, int screenHeight);
    
    // ID-based circle management (persistent, updatable)
    int createCircle(float x, float y, float radius, uint32_t color);
    int createRadialGradient(float x, float y, float radius,
                             uint32_t centerColor, uint32_t edgeColor);
    int createRadialGradient3(float x, float y, float radius,
                              uint32_t color1, uint32_t color2, uint32_t color3);
    int createRadialGradient4(float x, float y, float radius,
                              uint32_t color1, uint32_t color2, 
                              uint32_t color3, uint32_t color4);
    
    // Procedural pattern creation functions
    int createOutline(float x, float y, float radius,
                      uint32_t fillColor, uint32_t outlineColor, float lineWidth = 2.0f);
    int createDashedOutline(float x, float y, float radius,
                            uint32_t fillColor, uint32_t outlineColor,
                            float lineWidth = 2.0f, float dashLength = 10.0f);
    int createRing(float x, float y, float outerRadius, float innerRadius,
                   uint32_t color);
    int createPieSlice(float x, float y, float radius,
                       float startAngle, float endAngle, uint32_t color);
    int createArc(float x, float y, float radius,
                  float startAngle, float endAngle, 
                  uint32_t color, float lineWidth = 2.0f);
    int createDotsRing(float x, float y, float radius,
                       uint32_t dotColor, uint32_t backgroundColor,
                       float dotRadius = 3.0f, int numDots = 12);
    int createStarBurst(float x, float y, float radius,
                        uint32_t color1, uint32_t color2, int numRays = 8);
    
    // Update existing circles by ID
    bool updatePosition(int id, float x, float y);
    bool updateRadius(int id, float radius);
    bool updateColor(int id, uint32_t color);
    bool updateColors(int id, uint32_t color1, uint32_t color2, uint32_t color3, uint32_t color4);
    bool updateMode(int id, CircleGradientMode mode);
    bool updateParameters(int id, float param1, float param2, float param3);
    bool setVisible(int id, bool visible);
    
    // Query circles
    bool exists(int id) const;
    bool isVisible(int id) const;
    
    // Delete circles
    bool deleteCircle(int id);
    void deleteAll();
    
    // Statistics and management
    size_t getCircleCount() const;
    bool isEmpty() const;
    
    // Rendering
    void render(id<MTLRenderCommandEncoder> encoder);
    
    // Screen size updates (for coordinate transformation)
    void updateScreenSize(int width, int height);
    
    // Statistics
    size_t getMaxCircles() const { return m_maxCircles; }
    void setMaxCircles(size_t max);
    
    // Thread safety
    void lock() { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }
    
private:
    // Metal resources
    id<MTLDevice> m_device;
    id<MTLRenderPipelineState> m_pipelineState;
    id<MTLBuffer> m_instanceBuffer;
    id<MTLBuffer> m_uniformBuffer;
    
    // Circle storage with ID-based management
    struct ManagedCircle {
        CircleInstance data;
        bool visible;
        
        ManagedCircle() : visible(true) {}
    };
    
    std::map<int, ManagedCircle> m_managedCircles;  // ID-based persistent circles
    int m_nextId;
    size_t m_maxCircles;
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
    void rebuildRenderList();
};

// =============================================================================
// Uniform Data (Passed to Shaders)
// =============================================================================

struct CircleUniforms {
    float screenWidth;
    float screenHeight;
    float padding[2];  // Align to 16 bytes
};

} // namespace SuperTerminal

#endif // CIRCLE_MANAGER_H