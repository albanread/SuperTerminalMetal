//
// MetalRenderer.h
// SuperTerminal v2
//
// Metal-based text and graphics renderer for terminal display.
// Manages the Metal pipeline, buffers, and rendering state.
//

#ifndef SUPERTERMINAL_METAL_RENDERER_H
#define SUPERTERMINAL_METAL_RENDERER_H

#include <cstdint>
#include <memory>

// Forward declarations
namespace SuperTerminal {
    class TextGrid;
    class FontAtlas;
    class GraphicsLayer;
    class TilemapManager;
    class TilemapRenderer;
    class TextDisplayManager;
    class PolygonManager;
    class StarManager;
}

#ifdef __OBJC__
    @class MTLDevice;
    @class MTLCommandQueue;
    @class MTLRenderPipelineState;
    @class MTLBuffer;
    @class MTLTexture;
    @class MTLSamplerState;
    @class CAMetalLayer;
    @protocol MTLDevice;
    @protocol MTLCommandQueue;
    @protocol MTLRenderPipelineState;
    @protocol MTLBuffer;
    @protocol MTLTexture;
    @protocol MTLSamplerState;
    typedef id<MTLDevice> MTLDevicePtr;
    typedef id<MTLCommandQueue> MTLCommandQueuePtr;
    typedef id<MTLRenderPipelineState> MTLRenderPipelineStatePtr;
    typedef id<MTLBuffer> MTLBufferPtr;
    typedef id<MTLTexture> MTLTexturePtr;
    typedef id<MTLSamplerState> MTLSamplerStatePtr;
#else
    typedef void* MTLDevicePtr;
    typedef void* MTLCommandQueuePtr;
    typedef void* MTLRenderPipelineStatePtr;
    typedef void* MTLBufferPtr;
    typedef void* MTLTexturePtr;
    typedef void* MTLSamplerStatePtr;
    typedef void* CAMetalLayer;
#endif

namespace SuperTerminal {

/// Vertex structure for text rendering (interleaved position, texcoord, color)
struct TextVertex {
    float position[2];  // x, y in screen space
    float texCoord[2];  // u, v in font atlas
    float color[4];     // r, g, b, a
};

/// Uniform data passed to shaders each frame
struct TextUniforms {
    float projectionMatrix[16];  // Orthographic projection
    float viewportSize[2];       // Width, height
    float cellSize[2];           // Cell width, height in pixels
    float time;                  // Elapsed time for animations
    float cursorBlink;           // Cursor blink phase [0, 1]
    uint32_t renderMode;         // 0=normal, 1=subpixel, 2=CRT, 3=glow
    uint32_t loresHeight;        // LORES height in pixels (75, 150, or 300)
};

/// Rendering configuration options
struct RenderConfig {
    uint32_t renderMode;         // Normal, subpixel, CRT, glow
    bool enableCursor;           // Draw cursor
    bool enableBlink;            // Enable cursor blink
    float cursorBlinkRate;       // Blinks per second
    float crtCurvature;          // CRT effect strength
    float glowIntensity;         // Glow effect intensity
    bool vsync;                  // V-sync enabled

    RenderConfig()
        : renderMode(0)
        , enableCursor(true)
        , enableBlink(true)
        , cursorBlinkRate(1.5f)
        , crtCurvature(0.1f)
        , glowIntensity(0.3f)
        , vsync(true)
    {}
};

/// MetalRenderer: Core rendering engine for SuperTerminal
///
/// Responsibilities:
/// - Initialize Metal device, command queue, pipeline state
/// - Build vertex buffers from TextGrid content
/// - Bind font atlas texture and render text
/// - Support multiple render modes (normal, subpixel AA, CRT, glow)
/// - Handle frame presentation and vsync
class MetalRenderer {
public:
    /// Create a new MetalRenderer
    /// @param device Metal device (optional, will create default if null)
    explicit MetalRenderer(MTLDevicePtr device = nullptr);

    /// Destructor
    ~MetalRenderer();

    // Disable copy, allow move
    MetalRenderer(const MetalRenderer&) = delete;
    MetalRenderer& operator=(const MetalRenderer&) = delete;
    MetalRenderer(MetalRenderer&&) noexcept;
    MetalRenderer& operator=(MetalRenderer&&) noexcept;

    /// Initialize the renderer with a Metal layer
    /// @param layer CAMetalLayer to render into
    /// @param width Viewport width in pixels
    /// @param height Viewport height in pixels
    /// @return true if initialization succeeded
    bool initialize(CAMetalLayer* layer, uint32_t width, uint32_t height);

    /// Set the font atlas to use for rendering
    /// @param atlas FontAtlas instance
    void setFontAtlas(std::shared_ptr<FontAtlas> atlas);

    /// Set the text display manager for overlay text rendering
    /// @param textDisplay TextDisplayManager instance
    void setTextDisplayManager(std::shared_ptr<TextDisplayManager> textDisplay);

    /// Set rendering configuration
    /// @param config Rendering options
    void setConfig(const RenderConfig& config);

    /// Get current rendering configuration
    const RenderConfig& getConfig() const;

    /// Update viewport size (e.g., on window resize)
    /// @param width New width in pixels
    /// @param height New height in pixels
    void setViewportSize(uint32_t width, uint32_t height);

    /// Render a frame from the TextGrid
    /// @param textGrid TextGrid to render
    /// @param deltaTime Time since last frame (seconds)
    /// @return true if render succeeded
    bool renderFrame(TextGrid& textGrid, float deltaTime);
    
    /// Render a frame with TextGrid and GraphicsLayer
    /// @param textGrid TextGrid to render
    /// @param graphicsLayer Graphics primitives to render on top
    /// @param deltaTime Time since last frame (seconds)
    /// @return true if render succeeded
    bool renderFrame(TextGrid& textGrid, GraphicsLayer* graphicsLayer, float deltaTime);
    
    /// Render a frame with TextGrid, GraphicsLayer, and SpriteManager
    /// @param textGrid TextGrid to render
    /// @param graphicsLayer GraphicsLayer to render (optional)
    /// @param spriteManager SpriteManager to render (optional)
    /// @param deltaTime Time since last frame (seconds)
    /// @return true if render succeeded
    bool renderFrame(TextGrid& textGrid, GraphicsLayer* graphicsLayer, 
                     class SpriteManager* spriteManager, float deltaTime);
    
    /// Render a frame with all components including tilemaps and rectangles
    /// @param textGrid TextGrid to render
    /// @param graphicsLayer GraphicsLayer to render (optional)
    /// @param spriteManager SpriteManager to render (optional)
    /// @param rectangleManager RectangleManager to render (optional)
    /// @param tilemapManager TilemapManager to render (optional)
    /// @param tilemapRenderer TilemapRenderer to use for rendering tilemaps (optional)
    /// @param deltaTime Time since last frame (seconds)
    /// @return true if render succeeded
    bool renderFrame(TextGrid& textGrid, GraphicsLayer* graphicsLayer,
                     class SpriteManager* spriteManager,
                     class RectangleManager* rectangleManager,
                     class CircleManager* circleManager,
                     class LineManager* lineManager,
                     class PolygonManager* polygonManager,
                     class StarManager* starManager,
                     TilemapManager* tilemapManager, TilemapRenderer* tilemapRenderer,
                     float deltaTime);

    /// Get the Metal device
    MTLDevicePtr getDevice() const;

    /// Get the Metal command queue
    MTLCommandQueuePtr getCommandQueue() const;

    /// Check if renderer is initialized
    bool isInitialized() const;

    /// Get the actual cell width in pixels (from FontAtlas)
    uint32_t getCellWidth() const;

    /// Get the actual cell height in pixels (from FontAtlas)
    uint32_t getCellHeight() const;

    // =============================================================================
    // LORES GPU operations (Low Resolution 160×300, 16-color palette)
    // =============================================================================
    
    // GPU Blitter operations
    
    /// GPU-accelerated copy blit for LORES mode
    /// @param srcBuffer Source buffer index (0-7)
    /// @param dstBuffer Destination buffer index (0-7)
    /// @param srcX Source X coordinate
    /// @param srcY Source Y coordinate
    /// @param width Width of region to copy
    /// @param height Height of region to copy
    /// @param dstX Destination X coordinate
    /// @param dstY Destination Y coordinate
    void loresBlitGPU(int srcBuffer, int dstBuffer, 
                      int srcX, int srcY, int width, int height,
                      int dstX, int dstY);
    
    /// GPU-accelerated transparent blit for LORES mode
    /// @param srcBuffer Source buffer index (0-7)
    /// @param dstBuffer Destination buffer index (0-7)
    /// @param srcX Source X coordinate
    /// @param srcY Source Y coordinate
    /// @param width Width of region to copy
    /// @param height Height of region to copy
    /// @param dstX Destination X coordinate
    /// @param dstY Destination Y coordinate
    /// @param transparentColor Color index to skip (0-15)
    void loresBlitTransparentGPU(int srcBuffer, int dstBuffer,
                                 int srcX, int srcY, int width, int height,
                                 int dstX, int dstY, uint8_t transparentColor);
    
    // GPU Clear and Primitive Drawing operations
    
    /// GPU-accelerated clear for LORES buffer
    /// @param bufferIndex Buffer index to clear (0-7)
    /// @param colorIndex Color to fill with (0-15)
    void loresClearGPU(int bufferIndex, uint8_t colorIndex);
    
    /// GPU-accelerated filled rectangle for LORES buffer
    /// @param bufferIndex Buffer index (0-7)
    /// @param x X coordinate
    /// @param y Y coordinate
    /// @param width Rectangle width
    /// @param height Rectangle height
    /// @param colorIndex Fill color (0-15)
    void loresRectFillGPU(int bufferIndex, int x, int y, int width, int height, uint8_t colorIndex);
    
    /// GPU-accelerated filled circle for LORES buffer
    /// @param bufferIndex Buffer index (0-7)
    /// @param cx Center X coordinate
    /// @param cy Center Y coordinate
    /// @param radius Circle radius
    /// @param colorIndex Fill color (0-15)
    void loresCircleFillGPU(int bufferIndex, int cx, int cy, int radius, uint8_t colorIndex);
    
    /// GPU-accelerated line drawing for LORES buffer
    /// @param bufferIndex Buffer index (0-7)
    /// @param x0 Start X coordinate
    /// @param y0 Start Y coordinate
    /// @param x1 End X coordinate
    /// @param y1 End Y coordinate
    /// @param colorIndex Line color (0-15)
    void loresLineGPU(int bufferIndex, int x0, int y0, int x1, int y1, uint8_t colorIndex);

    // =============================================================================
    // XRES/WRES GPU operations
    // =============================================================================
    
    // GPU Blitter operations (XRES/WRES GPU-to-GPU blits)
    
    /// GPU-accelerated copy blit for XRES mode
    /// @param srcBuffer Source buffer index (0-3)
    /// @param dstBuffer Destination buffer index (0-3)
    /// @param srcX Source X coordinate
    /// @param srcY Source Y coordinate
    /// @param width Width of region to copy
    /// @param height Height of region to copy
    /// @param dstX Destination X coordinate
    /// @param dstY Destination Y coordinate
    void xresBlitGPU(int srcBuffer, int dstBuffer, 
                     int srcX, int srcY, int width, int height,
                     int dstX, int dstY);
    
    /// GPU-accelerated transparent blit for XRES mode
    /// @param srcBuffer Source buffer index (0-3)
    /// @param dstBuffer Destination buffer index (0-3)
    /// @param srcX Source X coordinate
    /// @param srcY Source Y coordinate
    /// @param width Width of region to copy
    /// @param height Height of region to copy
    /// @param dstX Destination X coordinate
    /// @param dstY Destination Y coordinate
    /// @param transparentColor Color index to skip (0-255)
    void xresBlitTransparentGPU(int srcBuffer, int dstBuffer,
                                int srcX, int srcY, int width, int height,
                                int dstX, int dstY, uint8_t transparentColor);
    
    /// GPU-accelerated copy blit for WRES mode
    /// @param srcBuffer Source buffer index (0-3)
    /// @param dstBuffer Destination buffer index (0-3)
    /// @param srcX Source X coordinate
    /// @param srcY Source Y coordinate
    /// @param width Width of region to copy
    /// @param height Height of region to copy
    /// @param dstX Destination X coordinate
    /// @param dstY Destination Y coordinate
    void wresBlitGPU(int srcBuffer, int dstBuffer,
                     int srcX, int srcY, int width, int height,
                     int dstX, int dstY);
    
    /// GPU-accelerated transparent blit for WRES mode
    /// @param srcBuffer Source buffer index (0-3)
    /// @param dstBuffer Destination buffer index (0-3)
    /// @param srcX Source X coordinate
    /// @param srcY Source Y coordinate
    /// @param width Width of region to copy
    /// @param height Height of region to copy
    /// @param dstX Destination X coordinate
    /// @param dstY Destination Y coordinate
    /// @param transparentColor Color index to skip (0-255)
    void wresBlitTransparentGPU(int srcBuffer, int dstBuffer,
                                int srcX, int srcY, int width, int height,
                                int dstX, int dstY, uint8_t transparentColor);
    
    /// Wait for all pending GPU blitter operations to complete
    /// @note Call this before reading results or flipping buffers after GPU blits
    void syncGPU();
    
    // GPU Blitter Batching (for performance optimization)
    
    /// Begin a GPU blit batch - creates a command buffer for multiple blits
    /// @note All blits between beginBlitBatch() and endBlitBatch() use same command buffer
    /// @note Must call endBlitBatch() to commit the work
    void beginBlitBatch();
    
    /// End a GPU blit batch and commit all queued blits
    /// @note Commits the command buffer created by beginBlitBatch()
    /// @note Does NOT wait for completion - call syncGPU() if needed
    void endBlitBatch();
    
    /// Check if currently in a blit batch
    /// @return true if between beginBlitBatch() and endBlitBatch()
    bool isInBlitBatch() const;
    
    // GPU Clear operations (fast GPU-resident clear)
    
    /// GPU-accelerated clear for XRES buffer
    /// @param bufferIndex Buffer index to clear (0-3)
    /// @param colorIndex Color to fill with (0-255)
    void xresClearGPU(int bufferIndex, uint8_t colorIndex);
    
    /// GPU-accelerated clear for WRES buffer
    /// @param bufferIndex Buffer index to clear (0-3)
    /// @param colorIndex Color to fill with (0-255)
    void wresClearGPU(int bufferIndex, uint8_t colorIndex);

    // GPU Primitive Drawing operations
    
    /// GPU-accelerated filled rectangle for XRES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param x X coordinate
    /// @param y Y coordinate
    /// @param width Rectangle width
    /// @param height Rectangle height
    /// @param colorIndex Fill color (0-255)
    void xresRectFillGPU(int bufferIndex, int x, int y, int width, int height, uint8_t colorIndex);
    
    /// GPU-accelerated filled rectangle for WRES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param x X coordinate
    /// @param y Y coordinate
    /// @param width Rectangle width
    /// @param height Rectangle height
    /// @param colorIndex Fill color (0-255)
    void wresRectFillGPU(int bufferIndex, int x, int y, int width, int height, uint8_t colorIndex);
    
    /// GPU-accelerated filled circle for XRES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param cx Center X coordinate
    /// @param cy Center Y coordinate
    /// @param radius Circle radius
    /// @param colorIndex Fill color (0-255)
    void xresCircleFillGPU(int bufferIndex, int cx, int cy, int radius, uint8_t colorIndex);
    
    /// GPU-accelerated filled circle for WRES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param cx Center X coordinate
    /// @param cy Center Y coordinate
    /// @param radius Circle radius
    /// @param colorIndex Fill color (0-255)
    void wresCircleFillGPU(int bufferIndex, int cx, int cy, int radius, uint8_t colorIndex);
    
    /// GPU-accelerated line drawing for XRES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param x0 Start X coordinate
    /// @param y0 Start Y coordinate
    /// @param x1 End X coordinate
    /// @param y1 End Y coordinate
    /// @param colorIndex Line color (0-255)
    void xresLineGPU(int bufferIndex, int x0, int y0, int x1, int y1, uint8_t colorIndex);
    
    /// GPU-accelerated line drawing for WRES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param x0 Start X coordinate
    /// @param y0 Start Y coordinate
    /// @param x1 End X coordinate
    /// @param y1 End Y coordinate
    /// @param colorIndex Line color (0-255)
    void wresLineGPU(int bufferIndex, int x0, int y0, int x1, int y1, uint8_t colorIndex);
    
    // GPU Anti-Aliased Primitive Drawing operations
    
    /// GPU-accelerated anti-aliased filled circle for XRES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param cx Center X coordinate
    /// @param cy Center Y coordinate
    /// @param radius Circle radius
    /// @param colorIndex Fill color (0-255)
    /// @note Uses smooth distance falloff and dithering for anti-aliased edges
    void xresCircleFillAA(int bufferIndex, int cx, int cy, int radius, uint8_t colorIndex);
    
    /// GPU-accelerated anti-aliased filled circle for WRES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param cx Center X coordinate
    /// @param cy Center Y coordinate
    /// @param radius Circle radius
    /// @param colorIndex Fill color (0-255)
    /// @note Uses smooth distance falloff and dithering for anti-aliased edges
    void wresCircleFillAA(int bufferIndex, int cx, int cy, int radius, uint8_t colorIndex);
    
    /// GPU-accelerated anti-aliased line drawing for XRES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param x0 Start X coordinate
    /// @param y0 Start Y coordinate
    /// @param x1 End X coordinate
    /// @param y1 End Y coordinate
    /// @param colorIndex Line color (0-255)
    /// @param lineWidth Line width in pixels (default 1.0)
    /// @note Uses smooth distance falloff and dithering for anti-aliased edges
    void xresLineAA(int bufferIndex, int x0, int y0, int x1, int y1, uint8_t colorIndex, float lineWidth = 1.0f);
    
    /// GPU-accelerated anti-aliased line drawing for WRES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param x0 Start X coordinate
    /// @param y0 Start Y coordinate
    /// @param x1 End X coordinate
    /// @param y1 End Y coordinate
    /// @param colorIndex Line color (0-255)
    /// @param lineWidth Line width in pixels (default 1.0)
    /// @note Uses smooth distance falloff and dithering for anti-aliased edges
    void wresLineAA(int bufferIndex, int x0, int y0, int x1, int y1, uint8_t colorIndex, float lineWidth = 1.0f);

    // =============================================================================
    // PRES GPU operations (Premium Resolution 1280×720, 256-color palette)
    // =============================================================================
    
    // GPU Blitter operations
    
    /// GPU-accelerated blit from one PRES buffer to another
    /// @param srcBuffer Source buffer index (0-7)
    /// @param dstBuffer Destination buffer index (0-7)
    /// @param srcX Source X coordinate
    /// @param srcY Source Y coordinate
    /// @param width Width in pixels
    /// @param height Height in pixels
    /// @param dstX Destination X coordinate
    /// @param dstY Destination Y coordinate
    void presBlitGPU(int srcBuffer, int dstBuffer, 
                     int srcX, int srcY, int width, int height,
                     int dstX, int dstY);
    
    /// GPU-accelerated transparent blit from one PRES buffer to another
    /// @param srcBuffer Source buffer index (0-7)
    /// @param dstBuffer Destination buffer index (0-7)
    /// @param srcX Source X coordinate
    /// @param srcY Source Y coordinate
    /// @param width Width in pixels
    /// @param height Height in pixels
    /// @param dstX Destination X coordinate
    /// @param dstY Destination Y coordinate
    /// @param transparentColor Color index to skip (typically 0)
    void presBlitTransparentGPU(int srcBuffer, int dstBuffer, 
                                int srcX, int srcY, int width, int height,
                                int dstX, int dstY, uint8_t transparentColor);
    
    // GPU Clear and Primitive Drawing operations
    
    /// GPU-accelerated clear for PRES buffer
    /// @param bufferIndex Buffer index to clear (0-7)
    /// @param colorIndex Color to fill with (0-255)
    void presClearGPU(int bufferIndex, uint8_t colorIndex);
    
    /// GPU-accelerated filled rectangle for PRES buffer
    /// @param bufferIndex Buffer index (0-7)
    /// @param x X coordinate
    /// @param y Y coordinate
    /// @param width Rectangle width
    /// @param height Rectangle height
    /// @param colorIndex Fill color (0-255)
    void presRectFillGPU(int bufferIndex, int x, int y, int width, int height, uint8_t colorIndex);
    
    /// GPU-accelerated filled circle for PRES buffer
    /// @param bufferIndex Buffer index (0-7)
    /// @param cx Center X coordinate
    /// @param cy Center Y coordinate
    /// @param radius Circle radius
    /// @param colorIndex Fill color (0-255)
    void presCircleFillGPU(int bufferIndex, int cx, int cy, int radius, uint8_t colorIndex);
    
    /// GPU-accelerated line drawing for PRES buffer
    /// @param bufferIndex Buffer index (0-7)
    /// @param x0 Start X coordinate
    /// @param y0 Start Y coordinate
    /// @param x1 End X coordinate
    /// @param y1 End Y coordinate
    /// @param colorIndex Line color (0-255)
    void presLineGPU(int bufferIndex, int x0, int y0, int x1, int y1, uint8_t colorIndex);
    
    // GPU Anti-Aliased Primitive Drawing operations
    
    /// GPU-accelerated anti-aliased filled circle for PRES buffer
    /// @param bufferIndex Buffer index (0-7)
    /// @param cx Center X coordinate
    /// @param cy Center Y coordinate
    /// @param radius Circle radius
    /// @param colorIndex Fill color (0-255)
    /// @note Uses smooth distance falloff and dithering for anti-aliased edges
    void presCircleFillAA(int bufferIndex, int cx, int cy, int radius, uint8_t colorIndex);
    
    /// GPU-accelerated anti-aliased line drawing for PRES buffer
    /// @param bufferIndex Buffer index (0-7)
    /// @param x0 Start X coordinate
    /// @param y0 Start Y coordinate
    /// @param x1 End X coordinate
    /// @param y1 End Y coordinate
    /// @param colorIndex Line color (0-255)
    /// @param lineWidth Line width in pixels (default 1.0)
    /// @note Uses smooth distance falloff and dithering for anti-aliased edges
    void presLineAA(int bufferIndex, int x0, int y0, int x1, int y1, uint8_t colorIndex, float lineWidth = 1.0f);

    // =============================================================================
    // URES GPU Blitter operations (Direct Color ARGB4444)
    // =============================================================================
    
    /// GPU-accelerated copy blit for URES buffer
    /// @param srcBufferIndex Source buffer index (0-3)
    /// @param dstBufferIndex Destination buffer index (0-3)
    /// @param srcX Source X coordinate
    /// @param srcY Source Y coordinate
    /// @param width Width in pixels
    /// @param height Height in pixels
    /// @param dstX Destination X coordinate
    /// @param dstY Destination Y coordinate
    void uresBlitCopyGPU(int srcBufferIndex, int dstBufferIndex, int srcX, int srcY, int width, int height, int dstX, int dstY);
    
    /// GPU-accelerated transparent blit for URES buffer (skip alpha=0 pixels)
    /// @param srcBufferIndex Source buffer index (0-3)
    /// @param dstBufferIndex Destination buffer index (0-3)
    /// @param srcX Source X coordinate
    /// @param srcY Source Y coordinate
    /// @param width Width in pixels
    /// @param height Height in pixels
    /// @param dstX Destination X coordinate
    /// @param dstY Destination Y coordinate
    void uresBlitTransparentGPU(int srcBufferIndex, int dstBufferIndex, int srcX, int srcY, int width, int height, int dstX, int dstY);
    
    /// GPU-accelerated alpha composite blit for URES buffer (proper alpha blending)
    /// @param srcBufferIndex Source buffer index (0-3)
    /// @param dstBufferIndex Destination buffer index (0-3)
    /// @param srcX Source X coordinate
    /// @param srcY Source Y coordinate
    /// @param width Width in pixels
    /// @param height Height in pixels
    /// @param dstX Destination X coordinate
    /// @param dstY Destination Y coordinate
    /// @note This performs TRUE alpha compositing, not just transparency check
    void uresBlitAlphaCompositeGPU(int srcBufferIndex, int dstBufferIndex, int srcX, int srcY, int width, int height, int dstX, int dstY);
    
    /// GPU-accelerated clear for URES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param color ARGB4444 color value
    void uresClearGPU(int bufferIndex, uint16_t color);

    // URES GPU Primitive Drawing operations (Direct Color ARGB4444)
    
    /// GPU-accelerated filled rectangle for URES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param x X coordinate
    /// @param y Y coordinate
    /// @param width Rectangle width
    /// @param height Rectangle height
    /// @param color ARGB4444 color value (0xARGB)
    void uresRectFillGPU(int bufferIndex, int x, int y, int width, int height, uint16_t color);
    
    /// GPU-accelerated filled circle for URES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param cx Center X coordinate
    /// @param cy Center Y coordinate
    /// @param radius Circle radius
    /// @param color ARGB4444 color value (0xARGB)
    void uresCircleFillGPU(int bufferIndex, int cx, int cy, int radius, uint16_t color);
    
    /// GPU-accelerated line drawing for URES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param x0 Start X coordinate
    /// @param y0 Start Y coordinate
    /// @param x1 End X coordinate
    /// @param y1 End Y coordinate
    /// @param color ARGB4444 color value (0xARGB)
    void uresLineGPU(int bufferIndex, int x0, int y0, int x1, int y1, uint16_t color);
    
    // URES GPU Anti-Aliased Primitive Drawing operations (with TRUE alpha blending!)
    
    /// GPU-accelerated anti-aliased filled circle for URES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param cx Center X coordinate
    /// @param cy Center Y coordinate
    /// @param radius Circle radius
    /// @param color ARGB4444 color value (0xARGB)
    /// @note Uses TRUE alpha blending (not dithering!) for smooth edges
    void uresCircleFillAA(int bufferIndex, int cx, int cy, int radius, uint16_t color);
    
    /// GPU-accelerated anti-aliased line drawing for URES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param x0 Start X coordinate
    /// @param y0 Start Y coordinate
    /// @param x1 End X coordinate
    /// @param y1 End Y coordinate
    /// @param color ARGB4444 color value (0xARGB)
    /// @param lineWidth Line width in pixels (default 1.0)
    /// @note Uses TRUE alpha blending (not dithering!) for smooth edges
    void uresLineAA(int bufferIndex, int x0, int y0, int x1, int y1, uint16_t color, float lineWidth = 1.0f);

    /// GPU-accelerated gradient rectangle for URES buffer (4-corner bilinear gradient)
    /// @param bufferIndex Buffer index (0-3)
    /// @param x Rectangle X coordinate
    /// @param y Rectangle Y coordinate
    /// @param width Rectangle width
    /// @param height Rectangle height
    /// @param colorTopLeft ARGB4444 color value for top-left corner
    /// @param colorTopRight ARGB4444 color value for top-right corner
    /// @param colorBottomLeft ARGB4444 color value for bottom-left corner
    /// @param colorBottomRight ARGB4444 color value for bottom-right corner
    void uresRectFillGradientGPU(int bufferIndex, int x, int y, int width, int height,
                                 uint16_t colorTopLeft, uint16_t colorTopRight,
                                 uint16_t colorBottomLeft, uint16_t colorBottomRight);

    /// GPU-accelerated radial gradient circle for URES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param cx Circle center X coordinate
    /// @param cy Circle center Y coordinate
    /// @param radius Circle radius in pixels
    /// @param centerColor ARGB4444 color value at center
    /// @param edgeColor ARGB4444 color value at edge
    void uresCircleFillGradientGPU(int bufferIndex, int cx, int cy, int radius,
                                   uint16_t centerColor, uint16_t edgeColor);

    /// GPU-accelerated radial gradient circle with anti-aliasing for URES buffer
    /// @param bufferIndex Buffer index (0-3)
    /// @param cx Circle center X coordinate
    /// @param cy Circle center Y coordinate
    /// @param radius Circle radius in pixels
    /// @param centerColor ARGB4444 color value at center
    /// @param edgeColor ARGB4444 color value at edge
    /// @note Uses TRUE alpha blending for smooth gradient and anti-aliased edges
    void uresCircleFillGradientAA(int bufferIndex, int cx, int cy, int radius,
                                  uint16_t centerColor, uint16_t edgeColor);

    /// Flip URES GPU buffers 0 and 1 (swap GPU texture pointers for double buffering)
    void uresGPUFlip();

    /// Flip PRES GPU buffers 0 and 1 (swap GPU texture pointers for double buffering)
    void presGPUFlip();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    /// Build vertex buffer from TextGrid cells
    void buildVertexBuffer(const TextGrid& textGrid);

    /// Create orthographic projection matrix
    void updateProjectionMatrix();

    /// Update uniform buffer with current state
    void updateUniforms(float deltaTime);
    
    /// Render graphics primitives
    void renderGraphics(void* encoder, const GraphicsLayer& graphicsLayer);
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_METAL_RENDERER_H
