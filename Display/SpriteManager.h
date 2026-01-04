//
// SpriteManager.h
// SuperTerminal v2
//
// Sprite management system for 2D sprite rendering with transforms.
// Supports loading PNG images, positioning, scaling, rotation, and alpha blending.
// These are 16 million colour sprites in their own layer.

#ifndef SUPERTERMINAL_SPRITE_MANAGER_H
#define SUPERTERMINAL_SPRITE_MANAGER_H

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <mutex>

#ifdef __OBJC__
    @protocol MTLDevice;
    @protocol MTLTexture;
    @protocol MTLRenderCommandEncoder;
    typedef id<MTLDevice> MTLDevicePtr;
    typedef id<MTLTexture> MTLTexturePtr;
    typedef id<MTLRenderCommandEncoder> MTLRenderCommandEncoderPtr;
#else
    typedef void* MTLDevicePtr;
    typedef void* MTLTexturePtr;
    typedef void* MTLRenderCommandEncoderPtr;
#endif

namespace SuperTerminal {

/// Maximum number of sprites supported
constexpr uint16_t MAX_SPRITES = 256;

/// Invalid sprite ID
constexpr uint16_t INVALID_SPRITE_ID = 0;

/// Sprite palette size (16 colors × 4 bytes RGBA)
constexpr size_t SPRITE_PALETTE_SIZE = 64;

/// Sprite palette color count
constexpr size_t SPRITE_PALETTE_COLORS = 16;

/// Sprite command types for thread-safe operations
enum class SpriteCommand {
    Show,
    Hide,
    Move,
    Scale,
    Rotate,
    SetAlpha,
    SetTint
};

/// Individual sprite data
struct Sprite {
    uint16_t spriteId;
    MTLTexturePtr texture;
    float x, y;              // Position in screen coordinates
    float scaleX, scaleY;    // Scale factors (1.0 = normal size)
    float rotation;          // Rotation in radians
    float alpha;             // Alpha transparency (0.0-1.0)
    float tintR, tintG, tintB, tintA; // Color tint (1,1,1,1 = no tint)
    bool visible;
    bool loaded;
    int textureWidth;        // Actual texture dimensions
    int textureHeight;

    Sprite()
        : spriteId(INVALID_SPRITE_ID)
        , texture(nullptr)
        , x(0.0f), y(0.0f)
        , scaleX(1.0f), scaleY(1.0f)
        , rotation(0.0f)
        , alpha(1.0f)
        , tintR(1.0f), tintG(1.0f), tintB(1.0f), tintA(1.0f)
        , visible(false)
        , loaded(false)
        , textureWidth(0)
        , textureHeight(0)
    {}
};

/// Indexed sprite data (4-bit color with per-sprite palette)
struct SpriteIndexed {
    uint16_t spriteId;
    MTLTexturePtr indexTexture;   // R8Uint texture (indices 0-15)
    MTLTexturePtr paletteTexture; // 16x1 RGBA8 texture (palette colors)
    uint8_t palette[SPRITE_PALETTE_SIZE];  // 16 colors × RGBA
    float x, y;              // Position in screen coordinates
    float scaleX, scaleY;    // Scale factors (1.0 = normal size)
    float rotation;          // Rotation in radians
    float alpha;             // Alpha transparency (0.0-1.0)
    bool visible;
    bool loaded;
    int textureWidth;        // Actual texture dimensions
    int textureHeight;

    SpriteIndexed()
        : spriteId(INVALID_SPRITE_ID)
        , indexTexture(nullptr)
        , paletteTexture(nullptr)
        , x(0.0f), y(0.0f)
        , scaleX(1.0f), scaleY(1.0f)
        , rotation(0.0f)
        , alpha(1.0f)
        , visible(false)
        , loaded(false)
        , textureWidth(0)
        , textureHeight(0)
    {
        // Initialize palette to convention: index 0 = transparent, 1 = black
        for (size_t i = 0; i < SPRITE_PALETTE_SIZE; i += 4) {
            palette[i] = 0;     // R
            palette[i+1] = 0;   // G
            palette[i+2] = 0;   // B
            palette[i+3] = (i == 0) ? 0 : 255;  // A: transparent for index 0, opaque otherwise
        }
    }
};

/// SpriteManager: Manages sprite loading, positioning, and rendering
///
/// Responsibilities:
/// - Load sprites from PNG/image files
/// - Manage sprite lifecycle (IDs, textures, state)
/// - Queue sprite commands for thread-safe updates
/// - Render sprites with transforms (position, scale, rotation)
/// - Handle Z-ordering (render order)
/// - Alpha blending and color tinting
///
/// Thread-Safety:
/// - All public API methods are thread-safe
/// - Commands are queued and processed at render time
/// - No locks held during rendering
class SpriteManager {
public:
    /// Constructor
    /// @param device Metal device to use for texture creation
    SpriteManager(MTLDevicePtr device);

    /// Destructor
    ~SpriteManager();

    // Disable copy, allow move
    SpriteManager(const SpriteManager&) = delete;
    SpriteManager& operator=(const SpriteManager&) = delete;
    SpriteManager(SpriteManager&&) noexcept;
    SpriteManager& operator=(SpriteManager&&) noexcept;

    // =========================================================================
    // Dirty Tracking
    // =========================================================================

    /// Check if any sprites have changed
    bool isDirty() const;

    /// Mark manager as dirty (needs buffer rebuild)
    void markDirty();

    /// Clear dirty flag after rendering
    void clearDirty();

    /// Check if specific sprite is dirty
    bool isSpriteDirty(uint16_t spriteId) const;

    /// Begin batch update (defers dirty marking)
    void beginBatchUpdate();

    /// End batch update (marks dirty once)
    void endBatchUpdate();

    // =========================================================================
    // Sprite Loading
    // =========================================================================

    /// Load a sprite from an image file (PNG, JPEG, etc.)
    /// @param filePath Path to image file
    /// @return Sprite ID (1-255), or 0 on failure
    uint16_t loadSprite(const std::string& filePath);

    /// Load a sprite from raw pixel data (RGBA)
    /// @param pixels Pixel data (RGBA, 4 bytes per pixel)
    /// @param width Image width in pixels
    /// @param height Image height in pixels
    /// @return Sprite ID (1-255), or 0 on failure
    uint16_t loadSpriteFromPixels(const uint8_t* pixels, int width, int height);

    /// Load an indexed sprite from 4-bit indexed pixel data
    /// @param indices Index data (0-15 per pixel)
    /// @param width Image width in pixels
    /// @param height Image height in pixels
    /// @param palette 16 colors × 4 bytes RGBA (64 bytes total)
    /// @return Sprite ID (1-255), or 0 on failure
    uint16_t loadSpriteIndexed(const uint8_t* indices, int width, int height, 
                               const uint8_t palette[SPRITE_PALETTE_SIZE]);

    /// Load an indexed sprite from raw pixel data with automatic quantization
    /// @param pixels RGBA pixel data (will be converted to indexed)
    /// @param width Image width in pixels
    /// @param height Image height in pixels
    /// @param outPalette Optional output: generated palette (64 bytes)
    /// @return Sprite ID (1-255), or 0 on failure
    uint16_t loadSpriteIndexedFromRGBA(const uint8_t* pixels, int width, int height,
                                       uint8_t* outPalette = nullptr);

    /// Load an indexed sprite from SPRTZ compressed file
    /// @param filePath Path to .sprtz file
    /// @return Sprite ID (1-255), or 0 on failure
    uint16_t loadSpriteFromSPRTZ(const std::string& filePath);

    /// Set texture for an existing sprite ID (used by DrawIntoSprite)
    /// @param spriteId Existing sprite ID to populate with texture
    /// @param pixels Pixel data (RGBA, 4 bytes per pixel)
    /// @param width Image width in pixels
    /// @param height Image height in pixels
    /// @return true on success, false on failure
    bool setSpriteTexture(uint16_t spriteId, const uint8_t* pixels, int width, int height);

    /// Unload a sprite and free its resources
    /// @param spriteId Sprite ID to unload
    void unloadSprite(uint16_t spriteId);

    /// Check if a sprite is loaded
    /// @param spriteId Sprite ID to check
    /// @return true if sprite is loaded
    bool isSpriteLoaded(uint16_t spriteId) const;

    /// Check if a sprite is indexed (4-bit color)
    /// @param spriteId Sprite ID to check
    /// @return true if sprite is indexed
    bool isSpriteIndexed(uint16_t spriteId) const;

    // =========================================================================
    // Indexed Sprite Palette Operations
    // =========================================================================

    /// Set custom palette for an indexed sprite (creates new GPU texture)
    /// @param spriteId Sprite ID (must be indexed sprite)
    /// @param palette 16 colors × 4 bytes RGBA (64 bytes total)
    /// @return true on success, false if not indexed sprite
    bool setSpritePalette(uint16_t spriteId, const uint8_t palette[SPRITE_PALETTE_SIZE]);

    /// Set sprite to use a standard palette (shared GPU texture)
    /// @param spriteId Sprite ID (must be indexed sprite)
    /// @param standardPaletteID Standard palette ID (0-31)
    /// @return true on success, false if invalid ID or not indexed sprite
    bool setSpriteStandardPalette(uint16_t spriteId, uint8_t standardPaletteID);

    /// Get palette from an indexed sprite
    /// @param spriteId Sprite ID (must be indexed sprite)
    /// @param outPalette Output buffer for palette (64 bytes)
    /// @return true on success, false if not indexed sprite
    bool getSpritePalette(uint16_t spriteId, uint8_t outPalette[SPRITE_PALETTE_SIZE]) const;

    /// Set a single color in an indexed sprite's palette
    /// @param spriteId Sprite ID (must be indexed sprite)
    /// @param colorIndex Color index (0-15)
    /// @param r Red (0-255)
    /// @param g Green (0-255)
    /// @param b Blue (0-255)
    /// @param a Alpha (0-255)
    /// @return true on success
    bool setSpritePaletteColor(uint16_t spriteId, uint8_t colorIndex,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    /// Lerp between two palettes for animation
    /// @param spriteId Sprite ID (must be indexed sprite)
    /// @param paletteA First palette (64 bytes)
    /// @param paletteB Second palette (64 bytes)
    /// @param t Interpolation factor (0.0 = A, 1.0 = B)
    /// @return true on success
    bool lerpSpritePalette(uint16_t spriteId, 
                          const uint8_t paletteA[SPRITE_PALETTE_SIZE],
                          const uint8_t paletteB[SPRITE_PALETTE_SIZE],
                          float t);

    /// Copy palette from one indexed sprite to another
    /// @param srcSpriteId Source sprite ID
    /// @param dstSpriteId Destination sprite ID
    /// @return true on success
    bool copySpritePalette(uint16_t srcSpriteId, uint16_t dstSpriteId);

    /// Adjust indexed sprite palette brightness
    /// @param spriteId Sprite ID (must be indexed sprite)
    /// @param brightness Brightness multiplier (1.0 = normal, 0.5 = darker, 2.0 = brighter)
    /// @return true on success
    bool adjustSpritePaletteBrightness(uint16_t spriteId, float brightness);

    /// Rotate colors in indexed sprite palette (for animation)
    /// @param spriteId Sprite ID (must be indexed sprite)
    /// @param startIndex Start color index (0-15)
    /// @param endIndex End color index (0-15)
    /// @param amount Rotation amount (positive = right, negative = left)
    /// @return true on success
    bool rotateSpritePalette(uint16_t spriteId, int startIndex, int endIndex, int amount);

    // =========================================================================
    // Sprite Visibility & Position
    // =========================================================================

    /// Show a sprite at a position
    /// @param spriteId Sprite ID to show
    /// @param x X position in screen coordinates
    /// @param y Y position in screen coordinates
    void showSprite(uint16_t spriteId, float x, float y);

    /// Hide a sprite
    /// @param spriteId Sprite ID to hide
    void hideSprite(uint16_t spriteId);

    /// Move a sprite to a new position
    /// @param spriteId Sprite ID to move
    /// @param x New X position
    /// @param y New Y position
    void moveSprite(uint16_t spriteId, float x, float y);

    /// Check if a sprite is visible
    /// @param spriteId Sprite ID to check
    /// @return true if sprite is visible
    bool isSpriteVisible(uint16_t spriteId) const;

    // =========================================================================
    // Sprite Transforms
    // =========================================================================

    /// Set sprite scale (uniform)
    /// @param spriteId Sprite ID
    /// @param scale Scale factor (1.0 = normal size, 2.0 = double size)
    void setScale(uint16_t spriteId, float scale);

    /// Set sprite scale (non-uniform)
    /// @param spriteId Sprite ID
    /// @param scaleX Horizontal scale factor
    /// @param scaleY Vertical scale factor
    void setScale(uint16_t spriteId, float scaleX, float scaleY);

    /// Set sprite rotation
    /// @param spriteId Sprite ID
    /// @param rotation Rotation in radians (0 = no rotation)
    void setRotation(uint16_t spriteId, float rotation);

    /// Set sprite rotation in degrees
    /// @param spriteId Sprite ID
    /// @param degrees Rotation in degrees (0-360)
    void setRotationDegrees(uint16_t spriteId, float degrees);

    /// Set sprite alpha transparency
    /// @param spriteId Sprite ID
    /// @param alpha Alpha value (0.0 = invisible, 1.0 = opaque)
    void setAlpha(uint16_t spriteId, float alpha);

    /// Set sprite color tint
    /// @param spriteId Sprite ID
    /// @param r Red component (0.0-1.0)
    /// @param g Green component (0.0-1.0)
    /// @param b Blue component (0.0-1.0)
    /// @param a Alpha component (0.0-1.0, optional)
    void setTint(uint16_t spriteId, float r, float g, float b, float a = 1.0f);

    // =========================================================================
    // Sprite Information
    // =========================================================================

    /// Get sprite dimensions
    /// @param spriteId Sprite ID
    /// @param outWidth Output width in pixels
    /// @param outHeight Output height in pixels
    void getSpriteSize(uint16_t spriteId, int* outWidth, int* outHeight) const;

    /// Get sprite position
    /// @param spriteId Sprite ID
    /// @param outX Output X position
    /// @param outY Output Y position
    void getSpritePosition(uint16_t spriteId, float* outX, float* outY) const;

    /// Get sprite texture (for particle rendering)
    /// @param spriteId Sprite ID
    /// @return Metal texture pointer, or nullptr if not found
    MTLTexturePtr getSpriteTexture(uint16_t spriteId) const;

    /// Get number of loaded sprites
    /// @return Count of loaded sprites
    size_t getLoadedSpriteCount() const;

    // =========================================================================
    // Z-Ordering
    // =========================================================================

    /// Bring sprite to front (top of render order)
    /// @param spriteId Sprite ID
    void bringToFront(uint16_t spriteId);

    /// Send sprite to back (bottom of render order)
    /// @param spriteId Sprite ID
    void sendToBack(uint16_t spriteId);

    // =========================================================================
    // Rendering
    // =========================================================================

    /// Render all visible sprites
    /// @param encoder Metal render command encoder
    /// @param viewportWidth Viewport width in pixels
    /// @param viewportHeight Viewport height in pixels
    void render(MTLRenderCommandEncoderPtr encoder,
                int viewportWidth, int viewportHeight);

    // =========================================================================
    // Utility
    // =========================================================================

    /// Clear all sprites
    void clearAll();

    /// Get next available sprite ID (for advanced users)
    /// @return Available sprite ID, or 0 if all IDs in use
    uint16_t getNextAvailableId() const;

    /// Get memory usage statistics
    /// @param outRGBBytes Output: bytes used by RGB sprites
    /// @param outIndexedBytes Output: bytes used by indexed sprites
    /// @param outRGBCount Output: count of RGB sprites
    /// @param outIndexedCount Output: count of indexed sprites
    void getMemoryStats(size_t* outRGBBytes, size_t* outIndexedBytes,
                       size_t* outRGBCount, size_t* outIndexedCount) const;

private:
    // Initialize standard palette textures (32 palettes loaded from StandardPaletteLibrary)
    void initializeStandardPaletteTextures();

    // Load indexed sprite using a pre-loaded standard palette texture
    uint16_t loadSpriteIndexedWithStandardPalette(const uint8_t* indices, int width, int height,
                                                   uint8_t standardPaletteID);

    struct Impl;
    std::unique_ptr<Impl> m_impl;

    // Dirty tracking
    mutable std::mutex m_dirtyMutex;
    bool m_dirty;
    std::vector<bool> m_dirtySprites;
    bool m_batchUpdate;

    /// Process queued commands (called at render time)
    void processCommandQueue();

    /// Create Metal rendering pipeline
    bool createRenderPipeline();

    /// Create vertex buffer for sprite quad
    void createVertexBuffer();

    /// Create sampler state for texture sampling
    void createSamplerState();

    /// Load texture from file path
    MTLTexturePtr loadTextureFromFile(const std::string& filePath, int* outWidth, int* outHeight);

    /// Create texture from pixel data
    MTLTexturePtr createTextureFromPixels(const uint8_t* pixels, int width, int height);

    /// Create indexed texture from index data (R8Uint format)
    MTLTexturePtr createIndexTextureFromIndices(const uint8_t* indices, int width, int height);

    /// Quantize RGBA pixels to 16-color palette
    void quantizeToIndexed(const uint8_t* pixels, int width, int height,
                          uint8_t* outIndices, uint8_t outPalette[SPRITE_PALETTE_SIZE]);

    /// Create indexed render pipeline
    bool createIndexedRenderPipeline();
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_SPRITE_MANAGER_H
