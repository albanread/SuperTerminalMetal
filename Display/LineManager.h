//
//  LineManager.h
//  SuperTerminal Framework - GPU-Accelerated Line Rendering
//
//  High-performance instanced line rendering with gradients
//  Uses Metal instanced drawing for minimal CPU overhead
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef LINE_MANAGER_H
#define LINE_MANAGER_H

#include <Metal/Metal.h>
#include <vector>
#include <map>
#include <cstdint>
#include <mutex>

namespace SuperTerminal {

// =============================================================================
// Line Modes
// =============================================================================

enum class LineMode : uint32_t {
    SOLID = 0,              // Single solid color
    GRADIENT = 1,           // Gradient from start to end (2 colors)
    DASHED = 2,             // Dashed line
    DOTTED = 3,             // Dotted line
};

// =============================================================================
// Line Instance Data (GPU Format)
// =============================================================================

struct LineInstance {
    float x1, y1;           // Start point in pixels
    float x2, y2;           // End point in pixels
    float thickness;        // Line thickness in pixels
    float padding1;         // Alignment padding
    uint32_t color1;        // Start color (RGBA8888)
    uint32_t color2;        // End color for gradients (RGBA8888)
    uint32_t mode;          // LineMode
    float dashLength;       // Dash/dot length in pixels (for dashed/dotted lines)
    float gapLength;        // Gap length in pixels (for dashed/dotted lines)
    float padding2;         // Alignment padding
    
    LineInstance()
        : x1(0), y1(0), x2(0), y2(0)
        , thickness(1.0f), padding1(0)
        , color1(0xFFFFFFFF), color2(0xFFFFFFFF)
        , mode(0)
        , dashLength(10.0f), gapLength(5.0f), padding2(0) {
    }
};

// =============================================================================
// LineManager - High-Performance Line Rendering
// =============================================================================

class LineManager {
public:
    LineManager();
    ~LineManager();
    
    // Initialize with Metal device
    bool initialize(id<MTLDevice> device, int screenWidth, int screenHeight);
    
    // ID-based line management (persistent, updatable)
    int createLine(float x1, float y1, float x2, float y2, uint32_t color, float thickness = 1.0f);
    int createGradientLine(float x1, float y1, float x2, float y2,
                          uint32_t color1, uint32_t color2, float thickness = 1.0f);
    int createDashedLine(float x1, float y1, float x2, float y2,
                        uint32_t color, float thickness = 1.0f,
                        float dashLength = 10.0f, float gapLength = 5.0f);
    int createDottedLine(float x1, float y1, float x2, float y2,
                        uint32_t color, float thickness = 1.0f,
                        float dotSpacing = 5.0f);
    
    // Update line properties
    bool setEndpoints(int id, float x1, float y1, float x2, float y2);
    bool setThickness(int id, float thickness);
    bool setColor(int id, uint32_t color);
    bool setColors(int id, uint32_t color1, uint32_t color2);
    bool setDashPattern(int id, float dashLength, float gapLength);
    bool setVisible(int id, bool visible);
    
    // Query lines
    bool exists(int id) const;
    bool isVisible(int id) const;
    
    // Delete lines
    bool deleteLine(int id);
    void deleteAll();
    
    // Statistics and management
    size_t getLineCount() const;
    bool isEmpty() const;
    
    // Rendering
    void render(id<MTLRenderCommandEncoder> encoder);
    
    // Screen size updates (for coordinate transformation)
    void updateScreenSize(int width, int height);
    
    // Statistics
    size_t getMaxLines() const { return m_maxLines; }
    void setMaxLines(size_t max);
    
    // Thread safety
    void lock() { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }
    
private:
    // Metal resources
    id<MTLDevice> m_device;
    id<MTLRenderPipelineState> m_pipelineState;
    id<MTLBuffer> m_instanceBuffer;
    id<MTLBuffer> m_uniformBuffer;
    
    // Line storage with ID-based management
    struct ManagedLine {
        LineInstance data;
        bool visible;
        
        ManagedLine() : visible(true) {}
    };
    
    std::map<int, ManagedLine> m_managedLines;  // ID-based persistent lines
    int m_nextId;
    size_t m_maxLines;
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

struct LineUniforms {
    float screenWidth;
    float screenHeight;
    float padding[2];
};

} // namespace SuperTerminal

#endif // LINE_MANAGER_H