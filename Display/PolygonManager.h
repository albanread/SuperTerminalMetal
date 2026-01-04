//
// PolygonManager.h
// SuperTerminal v2 - GPU-Accelerated Regular Polygon Rendering
//
// Manages rendering of regular polygons (3-12 sides) using Metal instanced rendering.
// Supports solid colors, gradients, and procedural patterns.
//

#ifndef POLYGONMANAGER_H
#define POLYGONMANAGER_H

#import <Metal/Metal.h>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace SuperTerminal {

// Polygon gradient/pattern modes (matches rectangle modes for consistency)
enum class PolygonGradientMode : uint32_t {
    // Basic gradients
    SOLID = 0,
    HORIZONTAL = 1,
    VERTICAL = 2,
    DIAGONAL_TL_BR = 3,
    DIAGONAL_TR_BL = 4,
    RADIAL = 5,
    FOUR_CORNER = 6,
    THREE_POINT = 7,
    
    // Procedural patterns
    OUTLINE = 100,
    DASHED_OUTLINE = 101,
    HORIZONTAL_STRIPES = 102,
    VERTICAL_STRIPES = 103,
    DIAGONAL_STRIPES = 104,
    CHECKERBOARD = 105,
    DOTS = 106,
    CROSSHATCH = 107,
    GRID = 109,
};

// GPU instance data (sent to vertex shader)
struct PolygonInstance {
    float x;                // Center X position
    float y;                // Center Y position
    float radius;           // Radius (distance from center to vertex)
    uint32_t numSides;      // Number of sides (3-12)
    uint32_t color1;        // RGBA8888 - primary/gradient color 1
    uint32_t color2;        // RGBA8888 - gradient color 2
    uint32_t color3;        // RGBA8888 - gradient color 3
    uint32_t color4;        // RGBA8888 - gradient color 4
    uint32_t mode;          // PolygonGradientMode
    float param1;           // Pattern parameter 1 (e.g., line width, stripe size)
    float param2;           // Pattern parameter 2 (e.g., spacing, corner radius)
    float param3;           // Pattern parameter 3 (e.g., angle, grid size)
    float rotation;         // Rotation in radians
    
    PolygonInstance()
        : x(0), y(0), radius(0), numSides(3)
        , color1(0xFFFFFFFF), color2(0xFFFFFFFF)
        , color3(0xFFFFFFFF), color4(0xFFFFFFFF)
        , mode(0)
        , param1(0.0f), param2(0.0f), param3(0.0f)
        , rotation(0.0f) {
    }
};

// CPU-side managed polygon data
struct ManagedPolygon {
    PolygonInstance data;
    bool visible;
    int id;
    
    ManagedPolygon() : visible(true), id(-1) {}
};

class PolygonManager {
public:
    PolygonManager();
    ~PolygonManager();
    
    // Initialization
    bool initialize(id<MTLDevice> device, int screenWidth, int screenHeight);
    
    // =============================================================================
    // Polygon Creation (returns polygon ID)
    // =============================================================================
    
    /// Create a solid color polygon
    int createPolygon(float x, float y, float radius, int numSides, uint32_t color);
    
    /// Create a gradient polygon
    int createGradient(float x, float y, float radius, int numSides,
                      uint32_t color1, uint32_t color2, PolygonGradientMode mode);
    
    /// Create a three-point gradient polygon
    int createThreePointGradient(float x, float y, float radius, int numSides,
                                uint32_t color1, uint32_t color2, uint32_t color3,
                                PolygonGradientMode mode);
    
    /// Create a four-corner gradient polygon
    int createFourCornerGradient(float x, float y, float radius, int numSides,
                                 uint32_t topLeft, uint32_t topRight,
                                 uint32_t bottomRight, uint32_t bottomLeft);
    
    // =============================================================================
    // Pattern Creation
    // =============================================================================
    
    int createOutline(float x, float y, float radius, int numSides,
                     uint32_t fillColor, uint32_t outlineColor, float lineWidth);
    
    int createDashedOutline(float x, float y, float radius, int numSides,
                           uint32_t fillColor, uint32_t outlineColor,
                           float lineWidth, float dashLength);
    
    int createHorizontalStripes(float x, float y, float radius, int numSides,
                               uint32_t color1, uint32_t color2, float stripeHeight);
    
    int createVerticalStripes(float x, float y, float radius, int numSides,
                             uint32_t color1, uint32_t color2, float stripeWidth);
    
    int createDiagonalStripes(float x, float y, float radius, int numSides,
                             uint32_t color1, uint32_t color2,
                             float stripeWidth, float angle);
    
    int createCheckerboard(float x, float y, float radius, int numSides,
                          uint32_t color1, uint32_t color2, float cellSize);
    
    int createDots(float x, float y, float radius, int numSides,
                  uint32_t dotColor, uint32_t backgroundColor,
                  float dotRadius, float spacing);
    
    int createCrosshatch(float x, float y, float radius, int numSides,
                        uint32_t lineColor, uint32_t backgroundColor,
                        float lineWidth, float spacing);
    
    int createGrid(float x, float y, float radius, int numSides,
                  uint32_t lineColor, uint32_t backgroundColor,
                  float lineWidth, float cellSize);
    
    // =============================================================================
    // Polygon Updates
    // =============================================================================
    
    bool updatePosition(int id, float x, float y);
    bool updateRadius(int id, float radius);
    bool updateSides(int id, int numSides);
    bool updateColor(int id, uint32_t color);
    bool updateColors(int id, uint32_t color1, uint32_t color2,
                     uint32_t color3, uint32_t color4);
    bool updateMode(int id, PolygonGradientMode mode);
    bool updateParameters(int id, float param1, float param2, float param3);
    bool setRotation(int id, float angleDegrees);
    bool setVisible(int id, bool visible);
    
    // =============================================================================
    // Polygon Query/Management
    // =============================================================================
    
    bool exists(int id) const;
    bool isVisible(int id) const;
    bool deletePolygon(int id);
    void deleteAll();
    size_t count() const;
    bool isEmpty() const;
    
    void setMaxPolygons(size_t max) { m_maxPolygons = max; }
    size_t getMaxPolygons() const { return m_maxPolygons; }
    
    // Rendering
    void render(id<MTLRenderCommandEncoder> encoder);
    
private:
    // Metal resources
    id<MTLDevice> m_device;
    id<MTLRenderPipelineState> m_pipelineState;
    id<MTLBuffer> m_instanceBuffer;
    id<MTLBuffer> m_uniformBuffer;
    
    // Polygon management
    std::unordered_map<int, ManagedPolygon> m_managedPolygons;
    int m_nextId;
    size_t m_maxPolygons;
    bool m_bufferNeedsUpdate;
    
    // Screen dimensions
    int m_screenWidth;
    int m_screenHeight;
    
    // Thread safety
    mutable std::mutex m_mutex;
    
    // Helper methods
    int createPolygonInternal(const PolygonInstance& instance);
    void updateInstanceBuffer();
};

} // namespace SuperTerminal

#endif // POLYGONMANAGER_H