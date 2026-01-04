//
//  RectangleManager.h
//  SuperTerminal Framework - GPU-Accelerated Rectangle Rendering
//
//  High-performance instanced rectangle rendering with gradients
//  Uses Metal instanced drawing for minimal CPU overhead
//  Copyright © 2024 SuperTerminal. All rights reserved.
//  These are composited rectangles in their own layer.

#ifndef RECTANGLE_MANAGER_H
#define RECTANGLE_MANAGER_H

#include <Metal/Metal.h>
#include <vector>
#include <map>
#include <cstdint>
#include <mutex>

namespace SuperTerminal {

// =============================================================================
// Gradient Modes
// =============================================================================

enum class RectangleGradientMode : uint32_t {
    SOLID = 0,              // Single solid color
    HORIZONTAL = 1,         // Left to right (2 colors)
    VERTICAL = 2,           // Top to bottom (2 colors)
    DIAGONAL_TL_BR = 3,     // Top-left to bottom-right (2 colors)
    DIAGONAL_TR_BL = 4,     // Top-right to bottom-left (2 colors)
    RADIAL = 5,             // Center outward (2 colors)
    FOUR_CORNER = 6,        // Each corner different (4 colors)
    THREE_POINT = 7,        // Three-point gradient (3 colors)

    // Procedural patterns (starting at 100)
    OUTLINE = 100,          // Outlined rectangle (color1=fill, color2=outline)
    DASHED_OUTLINE = 101,   // Dashed outline (color1=fill, color2=outline)
    HORIZONTAL_STRIPES = 102, // Horizontal stripes (color1, color2 alternate)
    VERTICAL_STRIPES = 103,   // Vertical stripes (color1, color2 alternate)
    DIAGONAL_STRIPES = 104,   // Diagonal stripes
    CHECKERBOARD = 105,       // Checkerboard pattern
    DOTS = 106,               // Dot pattern
    CROSSHATCH = 107,         // Crosshatch pattern
    ROUNDED_CORNERS = 108,    // Rounded corner rectangle
    GRID = 109                // Grid pattern
};

// =============================================================================
// Rectangle Instance Data (GPU Format)
// =============================================================================

struct RectangleInstance {
    float x, y;             // Position in pixels
    float width, height;    // Size in pixels
    uint32_t color1;        // Primary color (RGBA8888)
    uint32_t color2;        // Secondary color for gradients
    uint32_t color3;        // Tertiary color for 3-point gradients
    uint32_t color4;        // Quaternary color for four-corner
    uint32_t mode;          // RectangleGradientMode
    float param1;           // Pattern parameter 1 (e.g., line width, stripe size)
    float param2;           // Pattern parameter 2 (e.g., spacing, corner radius)
    float param3;           // Pattern parameter 3 (e.g., angle, grid size)
    float rotation;         // Rotation in radians

    RectangleInstance()
        : x(0), y(0), width(0), height(0)
        , color1(0xFFFFFFFF), color2(0xFFFFFFFF)
        , color3(0xFFFFFFFF), color4(0xFFFFFFFF)
        , mode(0)
        , param1(0.0f), param2(0.0f), param3(0.0f)
        , rotation(0.0f) {
    }
};

// =============================================================================
// RectangleManager - High-Performance Rectangle Rendering
// =============================================================================

class RectangleManager {
public:
    RectangleManager();
    ~RectangleManager();

    // Initialize with Metal device
    bool initialize(id<MTLDevice> device, int screenWidth, int screenHeight);

    // ID-based rectangle management (persistent, updatable)
    int createRectangle(float x, float y, float width, float height, uint32_t color);
    int createGradient(float x, float y, float width, float height,
                       uint32_t color1, uint32_t color2,
                       RectangleGradientMode mode);
    int createThreePointGradient(float x, float y, float width, float height,
                                 uint32_t color1, uint32_t color2, uint32_t color3,
                                 RectangleGradientMode mode);
    int createFourCornerGradient(float x, float y, float width, float height,
                                 uint32_t topLeft, uint32_t topRight,
                                 uint32_t bottomRight, uint32_t bottomLeft);

    // Procedural pattern creation functions
    int createOutline(float x, float y, float width, float height,
                      uint32_t fillColor, uint32_t outlineColor, float lineWidth = 2.0f);
    int createDashedOutline(float x, float y, float width, float height,
                            uint32_t fillColor, uint32_t outlineColor,
                            float lineWidth = 2.0f, float dashLength = 10.0f);
    int createHorizontalStripes(float x, float y, float width, float height,
                                uint32_t color1, uint32_t color2, float stripeHeight = 10.0f);
    int createVerticalStripes(float x, float y, float width, float height,
                              uint32_t color1, uint32_t color2, float stripeWidth = 10.0f);
    int createDiagonalStripes(float x, float y, float width, float height,
                              uint32_t color1, uint32_t color2, float stripeWidth = 10.0f, float angle = 45.0f);
    int createCheckerboard(float x, float y, float width, float height,
                           uint32_t color1, uint32_t color2, float cellSize = 10.0f);
    int createDots(float x, float y, float width, float height,
                   uint32_t dotColor, uint32_t backgroundColor,
                   float dotRadius = 3.0f, float spacing = 10.0f);
    int createCrosshatch(float x, float y, float width, float height,
                         uint32_t lineColor, uint32_t backgroundColor,
                         float lineWidth = 1.0f, float spacing = 10.0f);
    int createRoundedCorners(float x, float y, float width, float height,
                             uint32_t color, float cornerRadius = 10.0f);
    int createGrid(float x, float y, float width, float height,
                   uint32_t lineColor, uint32_t backgroundColor,
                   float lineWidth = 1.0f, float cellSize = 10.0f);

    // Update existing rectangles by ID
    bool updatePosition(int id, float x, float y);
    bool updateSize(int id, float width, float height);
    bool updateColor(int id, uint32_t color);
    bool updateColors(int id, uint32_t color1, uint32_t color2, uint32_t color3, uint32_t color4);
    bool updateMode(int id, RectangleGradientMode mode);
    bool updateParameters(int id, float param1, float param2, float param3);
    bool setRotation(int id, float angleDegrees);
    bool setVisible(int id, bool visible);

    // Query rectangles
    bool exists(int id) const;
    bool isVisible(int id) const;

    // Delete rectangles
    bool deleteRectangle(int id);
    void deleteAll();

    // Statistics and management
    size_t getRectangleCount() const;
    bool isEmpty() const;

    // Rendering
    void render(id<MTLRenderCommandEncoder> encoder);

    // Screen size updates (for coordinate transformation)
    void updateScreenSize(int width, int height);

    // Statistics
    size_t getMaxRectangles() const { return m_maxRectangles; }
    void setMaxRectangles(size_t max);

    // Thread safety
    void lock() { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }

private:
    // Metal resources
    id<MTLDevice> m_device;
    id<MTLRenderPipelineState> m_pipelineState;
    id<MTLBuffer> m_instanceBuffer;
    id<MTLBuffer> m_uniformBuffer;

    // Rectangle storage with ID-based management
    struct ManagedRectangle {
        RectangleInstance data;
        bool visible;

        ManagedRectangle() : visible(true) {}
    };

    std::map<int, ManagedRectangle> m_managedRectangles;  // ID-based persistent rectangles
    int m_nextId;
    size_t m_maxRectangles;
    bool m_bufferNeedsUpdate;

    // Screen dimensions
    int m_screenWidth;
    int m_screenHeight;

    // Thread safety
    std::mutex m_mutex;

    // Internal methods
    bool createPipeline();
    void updateInstanceBuffer();
    void updateUniforms();
    void rebuildRenderList();
};

// =============================================================================
// Uniform Data (Passed to Shaders)
// =============================================================================

struct RectangleUniforms {
    float screenWidth;
    float screenHeight;
    float padding[2];  // Align to 16 bytes
};

} // namespace SuperTerminal

#endif // RECTANGLE_MANAGER_H
