//
// TextDisplayManager.h
// SuperTerminal Framework - Free-form Text Display System
//
// Manages overlay text rendering with transformations (scale, rotation, shear)
// at arbitrary pixel positions. Renders on top of all other content.
// These are text items composed in their own layer, for game text, not editing.

#ifndef SUPERTERMINAL_TEXTDISPLAYMANAGER_H
#define SUPERTERMINAL_TEXTDISPLAYMANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

#ifdef __OBJC__
    @protocol MTLDevice;
    @protocol MTLBuffer;
    @protocol MTLRenderCommandEncoder;
    typedef id<MTLDevice> MTLDevicePtr;
    typedef id<MTLBuffer> MTLBufferPtr;
    typedef id<MTLRenderCommandEncoder> MTLRenderCommandEncoderPtr;
#else
    typedef void* MTLDevicePtr;
    typedef void* MTLBufferPtr;
    typedef void* MTLRenderCommandEncoderPtr;
#endif

namespace SuperTerminal {

class FontAtlas;

/// Text alignment options
enum class TextAlignment {
    LEFT = 0,
    CENTER = 1,
    RIGHT = 2
};

/// Vertex structure for transformed text rendering
struct TransformedTextVertex {
    float position[2];  // x, y in screen space (after transformation)
    float texCoord[2];  // u, v in font atlas
    float color[4];     // r, g, b, a
};

/// Extended vertex structure for text effects rendering
struct TextEffectVertex {
    float position[2];      // x, y in screen space (after transformation)
    float texCoord[2];      // u, v in font atlas
    float color[4];         // r, g, b, a (main text color)
    float effectColor[4];   // r, g, b, a (effect color)
    float effectIntensity;  // Effect intensity (0.0 - 1.0)
    float effectSize;       // Effect size parameter
    float animationTime;    // Animation time for dynamic effects
    uint32_t effectType;    // Effect type enum value
};

/// Text effect types
enum class TextEffect {
    NONE = 0,
    DROP_SHADOW = 1,
    OUTLINE = 2,
    GLOW = 3,
    GRADIENT = 4,
    WAVE = 5,
    NEON = 6
};

/// Individual text display item with transformation properties and effects
struct TextDisplayItem {
    std::string text;           // Text content to display
    float x, y;                 // Position in pixels
    float scaleX, scaleY;       // Scale factors
    float rotation;             // Rotation in degrees
    float shearX, shearY;       // Shear factors
    uint32_t color;             // Text color (RGBA)
    TextAlignment alignment;    // Text alignment
    int layer;                  // Rendering layer (higher = on top)
    bool visible;               // Visibility flag

    // Effect parameters
    TextEffect effect;          // Effect type to apply
    uint32_t effectColor;       // Effect color (for shadow, outline, glow)
    float effectIntensity;      // Effect intensity/strength (0.0 - 1.0)
    float effectSize;           // Effect size (outline width, shadow distance, glow radius)
    float animationTime;        // Animation time for animated effects

    // Constructor with defaults
    TextDisplayItem()
        : x(0), y(0)
        , scaleX(1.0f), scaleY(1.0f)
        , rotation(0.0f)
        , shearX(0.0f), shearY(0.0f)
        , color(0xFFFFFFFF)  // White
        , alignment(TextAlignment::LEFT)
        , layer(0)
        , visible(true)
        , effect(TextEffect::NONE)
        , effectColor(0x000000FF)  // Black
        , effectIntensity(0.5f)
        , effectSize(2.0f)
        , animationTime(0.0f)
    {}
};

/// Text display manager for free-form transformed text rendering
///
/// Provides functionality to display text at arbitrary pixel positions
/// with scale, rotation, and shear transformations. All text is rendered
/// on top of other content (TextGrid, graphics, sprites).
///
/// Usage example:
/// ```cpp
/// textDisplayManager->displayTextAt(400, 50, "GAME OVER", 2.0f, 45.0f, 0xFF0000FF);
/// textDisplayManager->displayTextAt(400, 100, "Score: 12345", 1.5f, 0.0f, 0xFFFFFFFF);
/// ```
class TextDisplayManager {
public:
    /// Constructor
    /// @param device Metal device for buffer allocation
    explicit TextDisplayManager(MTLDevicePtr device);

    /// Destructor
    ~TextDisplayManager();

    // Disable copy, allow move
    TextDisplayManager(const TextDisplayManager&) = delete;
    TextDisplayManager& operator=(const TextDisplayManager&) = delete;
    TextDisplayManager(TextDisplayManager&&) noexcept;
    TextDisplayManager& operator=(TextDisplayManager&&) noexcept;

    // =========================================================================
    // Text Display API
    // =========================================================================

    /// Display text at specified position with transformations
    /// @param x X coordinate in pixels
    /// @param y Y coordinate in pixels
    /// @param text Text content to display
    /// @param scaleX Horizontal scale factor (1.0 = normal)
    /// @param scaleY Vertical scale factor (1.0 = normal)
    /// @param rotation Rotation in degrees (0 = no rotation)
    /// @param color Text color (RGBA format)
    /// @param alignment Text alignment (default: LEFT)
    /// @param layer Rendering layer (higher = on top, default: 0)
    /// @return Text item ID for later modification/removal
    int displayTextAt(float x, float y, const std::string& text,
                      float scaleX = 1.0f, float scaleY = 1.0f, float rotation = 0.0f,
                      uint32_t color = 0xFFFFFFFF,
                      TextAlignment alignment = TextAlignment::LEFT,
                      int layer = 0);

    /// Display text with shear transformation
    /// @param x X coordinate in pixels
    /// @param y Y coordinate in pixels
    /// @param text Text content to display
    /// @param scaleX Horizontal scale factor
    /// @param scaleY Vertical scale factor
    /// @param rotation Rotation in degrees
    /// @param shearX Horizontal shear factor
    /// @param shearY Vertical shear factor
    /// @param color Text color (RGBA format)
    /// @param alignment Text alignment
    /// @param layer Rendering layer
    /// @return Text item ID
    int displayTextAtWithShear(float x, float y, const std::string& text,
                               float scaleX, float scaleY, float rotation,
                               float shearX, float shearY,
                               uint32_t color = 0xFFFFFFFF,
                               TextAlignment alignment = TextAlignment::LEFT,
                               int layer = 0);

    /// Display text with visual effects
    /// @param x X coordinate in pixels
    /// @param y Y coordinate in pixels
    /// @param text Text content to display
    /// @param scaleX Horizontal scale factor
    /// @param scaleY Vertical scale factor
    /// @param rotation Rotation in degrees
    /// @param color Text color (RGBA format)
    /// @param alignment Text alignment
    /// @param layer Rendering layer
    /// @param effect Effect type to apply
    /// @param effectColor Effect color (for shadow, outline, glow)
    /// @param effectIntensity Effect intensity (0.0 - 1.0)
    /// @param effectSize Effect size (outline width, shadow distance, glow radius)
    /// @return Text item ID
    int displayTextWithEffects(float x, float y, const std::string& text,
                               float scaleX = 1.0f, float scaleY = 1.0f, float rotation = 0.0f,
                               uint32_t color = 0xFFFFFFFF,
                               TextAlignment alignment = TextAlignment::LEFT,
                               int layer = 0,
                               TextEffect effect = TextEffect::NONE,
                               uint32_t effectColor = 0x000000FF,
                               float effectIntensity = 0.5f,
                               float effectSize = 2.0f);

    /// Update existing text item
    /// @param itemId Text item ID returned by displayTextAt
    /// @param text New text content (empty string = no change)
    /// @param x New X position (-1 = no change)
    /// @param y New Y position (-1 = no change)
    /// @return true if item was found and updated
    bool updateTextItem(int itemId, const std::string& text = "",
                        float x = -1, float y = -1);

    /// Remove specific text item
    /// @param itemId Text item ID to remove
    /// @return true if item was found and removed
    bool removeTextItem(int itemId);

    /// Clear all displayed text
    void clearDisplayedText();

    /// Set visibility of text item
    /// @param itemId Text item ID
    /// @param visible Visibility state
    /// @return true if item was found
    bool setTextItemVisible(int itemId, bool visible);

    /// Set layer of text item (for z-ordering)
    /// @param itemId Text item ID
    /// @param layer New layer value (higher = on top)
    /// @return true if item was found
    bool setTextItemLayer(int itemId, int layer);

    /// Set color of text item
    /// @param itemId Text item ID
    /// @param color New color (RGBA format)
    /// @return true if item was found
    bool setTextItemColor(int itemId, uint32_t color);

    // =========================================================================
    // Rendering Integration
    // =========================================================================

    /// Set font atlas for text rendering
    /// @param fontAtlas Shared pointer to font atlas
    void setFontAtlas(std::shared_ptr<FontAtlas> fontAtlas);

    /// Build vertex buffers for all visible text items
    /// @param viewportWidth Viewport width in pixels
    /// @param viewportHeight Viewport height in pixels
    void buildVertexBuffers(uint32_t viewportWidth, uint32_t viewportHeight);

    /// Render all visible text items
    /// @param encoder Metal render command encoder
    /// @param viewportWidth Viewport width in pixels
    /// @param viewportHeight Viewport height in pixels
    void render(MTLRenderCommandEncoderPtr encoder,
                uint32_t viewportWidth, uint32_t viewportHeight);

    // =========================================================================
    // Status and Debug
    // =========================================================================

    /// Get number of active text items
    /// @return Number of text items in display list
    size_t getTextItemCount() const;

    /// Get number of visible text items
    /// @return Number of visible text items
    size_t getVisibleTextItemCount() const;

    /// Check if any text items need rendering
    /// @return true if there are visible text items
    bool hasContent() const;

    /// Get total vertex count for all visible text
    /// @return Number of vertices to render
    size_t getTotalVertexCount() const;

    /// Check if any text items use visual effects
    /// @return true if any visible text items have effects enabled
    bool hasEffects() const;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    struct Impl;
    std::unique_ptr<Impl> m_impl;

    /// Generate unique text item ID
    int generateItemId();

    /// Calculate transformation matrix for text item
    /// @param item Text display item
    /// @param matrix Output 4x4 transformation matrix (column-major)
    void calculateTransformMatrix(const TextDisplayItem& item, float* matrix);

    /// Transform vertex by matrix
    /// @param x Input X coordinate
    /// @param y Input Y coordinate
    /// @param matrix 4x4 transformation matrix
    /// @param outX Output X coordinate
    /// @param outY Output Y coordinate
    void transformVertex(float x, float y, const float* matrix,
                        float& outX, float& outY);

    /// Calculate text dimensions
    /// @param text Text content
    /// @param outWidth Output width in pixels
    /// @param outHeight Output height in pixels
    void calculateTextDimensions(const std::string& text,
                                float& outWidth, float& outHeight);

    /// Apply text alignment offset
    /// @param text Text content
    /// @param alignment Text alignment
    /// @param outOffsetX Output X offset
    /// @param outOffsetY Output Y offset
    void calculateAlignmentOffset(const std::string& text, TextAlignment alignment,
                                 float& outOffsetX, float& outOffsetY);

    /// Sort text items by layer (for proper z-ordering)
    void sortTextItemsByLayer();
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_TEXTDISPLAYMANAGER_H
