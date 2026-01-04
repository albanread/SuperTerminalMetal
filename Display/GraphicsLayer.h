//
// GraphicsLayer.h
// SuperTerminal v2
//
// Graphics layer for 2D primitive drawing.
// Provides immediate-mode API for rectangles, circles, lines, and pixels.
//

#ifndef SUPERTERMINAL_GRAPHICS_LAYER_H
#define SUPERTERMINAL_GRAPHICS_LAYER_H

#include <cstdint>
#include <vector>
#include <mutex>
#include <memory>

namespace SuperTerminal {

/// Draw command types
enum class DrawCommand {
    Clear,
    DrawRect,
    FillRect,
    DrawCircle,
    FillCircle,
    DrawLine,
    DrawPixel,
};

/// Primitive drawing command
struct GraphicsCommand {
    DrawCommand type;
    
    // Position and size
    float x, y;
    float width, height;
    
    // Line endpoints (for lines)
    float x2, y2;
    
    // Radius (for circles)
    float radius;
    
    // Color (RGBA)
    float r, g, b, a;
    
    // Line width
    float lineWidth;
    
    GraphicsCommand()
        : type(DrawCommand::Clear)
        , x(0), y(0)
        , width(0), height(0)
        , x2(0), y2(0)
        , radius(0)
        , r(1), g(1), b(1), a(1)
        , lineWidth(1.0f)
    {}
};

/// GraphicsLayer: Immediate-mode 2D drawing layer with double buffering
///
/// Responsibilities:
/// - Queue draw commands for primitives (back buffer)
/// - Thread-safe command submission
/// - Double buffering with front/back buffer swap
/// - Clear and reset functionality
/// - Generate Metal vertex buffers for rendering
class GraphicsLayer {
public:
    /// Constructor
    GraphicsLayer();
    
    /// Destructor
    ~GraphicsLayer();
    
    // Disable copy, allow move
    GraphicsLayer(const GraphicsLayer&) = delete;
    GraphicsLayer& operator=(const GraphicsLayer&) = delete;
    GraphicsLayer(GraphicsLayer&&) noexcept;
    GraphicsLayer& operator=(GraphicsLayer&&) noexcept;
    
    // =========================================================================
    // Dirty Tracking & Visibility
    // =========================================================================
    
    /// Check if layer has been modified since last render
    /// @return true if layer needs re-rendering
    bool isDirty() const;
    
    /// Mark layer as dirty (needs re-render)
    void markDirty();
    
    /// Clear dirty flag after rendering
    void clearDirty();
    
    /// Check if layer is visible
    /// @return true if layer should be rendered
    bool isVisible() const;
    
    /// Set layer visibility
    /// @param visible true to show layer, false to hide
    void setVisible(bool visible);
    
    /// Get frame number when layer was last rendered
    /// @return Frame number of last render
    uint64_t getLastRenderFrame() const;
    
    /// Set frame number when layer was rendered
    /// @param frame Frame number
    void setLastRenderFrame(uint64_t frame);
    
    // =========================================================================
    // Drawing Commands (Immediate Mode)
    // =========================================================================
    
    /// Clear all draw commands (clears back buffer)
    void clear();
    
    /// Swap front and back buffers for double buffering
    void swapBuffers();
    
    /// Draw a rectangle outline
    /// @param x X coordinate (top-left)
    /// @param y Y coordinate (top-left)
    /// @param width Rectangle width
    /// @param height Rectangle height
    /// @param r Red (0-1)
    /// @param g Green (0-1)
    /// @param b Blue (0-1)
    /// @param a Alpha (0-1)
    /// @param lineWidth Line thickness
    void drawRect(float x, float y, float width, float height,
                  float r, float g, float b, float a, float lineWidth = 1.0f);
    
    /// Fill a rectangle
    /// @param x X coordinate (top-left)
    /// @param y Y coordinate (top-left)
    /// @param width Rectangle width
    /// @param height Rectangle height
    /// @param r Red (0-1)
    /// @param g Green (0-1)
    /// @param b Blue (0-1)
    /// @param a Alpha (0-1)
    void fillRect(float x, float y, float width, float height,
                  float r, float g, float b, float a);
    
    /// Draw a circle outline
    /// @param x Center X coordinate
    /// @param y Center Y coordinate
    /// @param radius Circle radius
    /// @param r Red (0-1)
    /// @param g Green (0-1)
    /// @param b Blue (0-1)
    /// @param a Alpha (0-1)
    /// @param lineWidth Line thickness
    void drawCircle(float x, float y, float radius,
                    float r, float g, float b, float a, float lineWidth = 1.0f);
    
    /// Fill a circle
    /// @param x Center X coordinate
    /// @param y Center Y coordinate
    /// @param radius Circle radius
    /// @param r Red (0-1)
    /// @param g Green (0-1)
    /// @param b Blue (0-1)
    /// @param a Alpha (0-1)
    void fillCircle(float x, float y, float radius,
                    float r, float g, float b, float a);
    
    /// Draw a line
    /// @param x1 Start X coordinate
    /// @param y1 Start Y coordinate
    /// @param x2 End X coordinate
    /// @param y2 End Y coordinate
    /// @param r Red (0-1)
    /// @param g Green (0-1)
    /// @param b Blue (0-1)
    /// @param a Alpha (0-1)
    /// @param lineWidth Line thickness
    void drawLine(float x1, float y1, float x2, float y2,
                  float r, float g, float b, float a, float lineWidth = 1.0f);
    
    /// Draw a single pixel
    /// @param x X coordinate
    /// @param y Y coordinate
    /// @param r Red (0-1)
    /// @param g Green (0-1)
    /// @param b Blue (0-1)
    /// @param a Alpha (0-1)
    void drawPixel(float x, float y, float r, float g, float b, float a);
    
    // =========================================================================
    // Rendering
    // =========================================================================
    
    /// Get the list of draw commands from front buffer (thread-safe)
    /// @return Vector of commands to render from front buffer
    std::vector<GraphicsCommand> getCommands() const;
    
    /// Get the list of draw commands from back buffer (for debugging)
    /// @return Vector of commands in back buffer
    std::vector<GraphicsCommand> getBackBufferCommands() const;
    
    /// Get the number of queued commands in front buffer
    /// @return Front buffer command count
    size_t getCommandCount() const;
    
    /// Get the number of queued commands in back buffer
    /// @return Back buffer command count
    size_t getBackBufferCommandCount() const;
    
    /// Check if the front buffer is empty
    /// @return true if no commands are queued in front buffer
    bool isEmpty() const;
    
    /// Check if the back buffer is empty
    /// @return true if no commands are queued in back buffer
    bool isBackBufferEmpty() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    
    // Dirty tracking
    mutable std::mutex m_dirtyMutex;
    bool m_dirty;
    bool m_visible;
    uint64_t m_lastRenderFrame;
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_GRAPHICS_LAYER_H