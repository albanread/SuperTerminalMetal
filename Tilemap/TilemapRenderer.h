//
// TilemapRenderer.h
// SuperTerminal Framework - Tilemap System
//
// Metal-based GPU-accelerated tilemap renderer with instanced drawing
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef TILEMAPRENDERER_H
#define TILEMAPRENDERER_H

#include <memory>
#include <vector>
#include <cstdint>

#ifdef __OBJC__
@protocol MTLDevice;
@protocol MTLCommandQueue;
@protocol MTLRenderPipelineState;
@protocol MTLBuffer;
@protocol MTLTexture;
@protocol MTLSamplerState;
@protocol MTLRenderCommandEncoder;
@class CAMetalLayer;
typedef id<MTLDevice> MTLDevicePtr;
typedef id<MTLCommandQueue> MTLCommandQueuePtr;
typedef id<MTLRenderPipelineState> MTLRenderPipelineStatePtr;
typedef id<MTLBuffer> MTLBufferPtr;
typedef id<MTLTexture> MTLTexturePtr;
typedef id<MTLSamplerState> MTLSamplerStatePtr;
typedef id<MTLRenderCommandEncoder> MTLRenderCommandEncoderPtr;
#else
typedef void* MTLDevicePtr;
typedef void* MTLCommandQueuePtr;
typedef void* MTLRenderPipelineStatePtr;
typedef void* MTLBufferPtr;
typedef void* MTLTexturePtr;
typedef void* MTLSamplerStatePtr;
typedef void* MTLRenderCommandEncoderPtr;
typedef void* CAMetalLayer;
#endif

namespace SuperTerminal {

// Forward declarations
class TilemapLayer;
class Tilemap;
class Tileset;
class TilesetIndexed;
class PaletteBank;
class Camera;

/// Vertex data for explicit vertex buffer (v1-style)
struct TileVertex {
    float position[2];      // Screen position
    float texCoord[2];      // UV coordinates
    uint16_t tileId;        // Tile ID
    uint16_t flags;         // Rendering flags
};

/// Per-instance data for GPU (matches Metal shader struct)
struct TileInstance {
    float position[2];      // World position (top-left corner)
    float texCoordBase[2];  // Base UV coordinate (top-left in atlas)
    float texCoordSize[2];  // UV size (width/height in atlas)
    float tintColor[4];     // RGBA tint/modulation
    uint32_t flags;         // Bit flags: flip X/Y, rotation
    uint32_t padding[3];    // Alignment to 16 bytes
    
    TileInstance();
};

/// Per-instance data for indexed tiles (adds palette index)
struct TileInstanceIndexed {
    float position[2];      // World position (top-left corner)
    float texCoordBase[2];  // Base UV coordinate (top-left in atlas)
    float texCoordSize[2];  // UV size (width/height in atlas)
    float tintColor[4];     // RGBA tint/modulation
    uint32_t flags;         // Bit flags: flip X/Y, rotation
    uint32_t paletteIndex;  // Palette index (0-31)
    uint32_t padding[2];    // Alignment to 16 bytes
    
    TileInstanceIndexed();
};

/// Uniform data passed to shaders (matches Metal shader struct)
struct TilemapUniforms {
    float viewProjectionMatrix[16];  // Combined view-projection matrix
    float viewportSize[2];           // Viewport dimensions
    float tileSize[2];               // Tile size in pixels
    float time;                      // Elapsed time for animations
    float opacity;                   // Layer opacity
    uint32_t flags;                  // Rendering flags
    uint32_t padding;                // Alignment
    
    TilemapUniforms();
};

/// Rendering statistics for performance monitoring
struct TilemapRenderStats {
    uint32_t layersRendered = 0;
    uint32_t tilesRendered = 0;
    uint32_t tilesCulled = 0;
    uint32_t drawCalls = 0;
    float frameTime = 0.0f;
    
    void reset() {
        layersRendered = 0;
        tilesRendered = 0;
        tilesCulled = 0;
        drawCalls = 0;
        frameTime = 0.0f;
    }
};

/// Rendering configuration
struct TilemapRenderConfig {
    bool enableCulling = true;        // Frustum culling
    bool enableAlphaTest = false;     // Alpha testing in shader
    bool enableDebugOverlay = false;  // Grid/wireframe overlay
    bool enableMipmaps = true;        // Mipmap filtering
    uint32_t maxInstancesPerBatch = 2048;  // Max tiles per draw call
    
    TilemapRenderConfig() = default;
};

/// TilemapRenderer: GPU-accelerated tilemap rendering with Metal
///
/// Features:
/// - Instanced rendering (one draw call per layer)
/// - Frustum culling
/// - Per-tile transformations (flip, rotate)
/// - Layer opacity and blending
/// - Parallax scrolling support
/// - Texture atlas support
/// - Animation support (via Tileset)
///
/// Performance:
/// - Uses instanced drawing to minimize draw calls
/// - Builds instance buffers only for visible tiles
/// - Supports thousands of tiles at 60 FPS
///
/// Thread Safety: Must be called from render thread only
///
class TilemapRenderer {
public:
    // =================================================================
    // Construction
    // =================================================================
    
    /// Create renderer with Metal device
    /// @param device Metal device (nullptr = use default)
    explicit TilemapRenderer(MTLDevicePtr device = nullptr);
    
    /// Destructor
    ~TilemapRenderer();
    
    // Disable copy, allow move
    TilemapRenderer(const TilemapRenderer&) = delete;
    TilemapRenderer& operator=(const TilemapRenderer&) = delete;
    TilemapRenderer(TilemapRenderer&&) noexcept;
    TilemapRenderer& operator=(TilemapRenderer&&) noexcept;
    
    // =================================================================
    // Initialization
    // =================================================================
    
    /// Initialize the renderer
    /// @param layer Metal layer to render into
    /// @param viewportWidth Viewport width in pixels
    /// @param viewportHeight Viewport height in pixels
    /// @return true on success
    bool initialize(CAMetalLayer* layer, uint32_t viewportWidth, uint32_t viewportHeight);
    
    /// Check if renderer is initialized
    bool isInitialized() const;
    
    /// Shutdown and cleanup
    void shutdown();
    
    // =================================================================
    // Configuration
    // =================================================================
    
    /// Set rendering configuration
    void setConfig(const TilemapRenderConfig& config);
    
    /// Get current configuration
    const TilemapRenderConfig& getConfig() const;
    
    /// Set viewport size (on window resize)
    void setViewportSize(uint32_t width, uint32_t height);
    
    /// Get viewport size
    void getViewportSize(uint32_t& width, uint32_t& height) const;
    
    // =================================================================
    // Rendering
    // =================================================================
    
    /// Begin rendering frame
    /// @param commandEncoder Active render command encoder (from MetalRenderer)
    /// @return true on success
    bool beginFrame(MTLRenderCommandEncoderPtr commandEncoder);
    
    /// Render a single tilemap layer
    /// @param layer The layer to render
    /// @param camera Camera for view transform
    /// @param time Current time for animations (seconds)
    /// @return true on success
    bool renderLayer(const TilemapLayer& layer, const Camera& camera, float time);
    
    /// Render a single indexed tilemap layer (palette-driven)
    /// @param layer The layer to render
    /// @param tileset Indexed tileset with color indices
    /// @param paletteBank Palette bank containing color palettes
    /// @param camera Camera for view transform
    /// @param time Current time for animations (seconds)
    /// @return true on success
    bool renderLayerIndexed(const TilemapLayer& layer, 
                           const TilesetIndexed& tileset,
                           const PaletteBank& paletteBank,
                           const Camera& camera, 
                           float time);
    
    /// End rendering frame
    void endFrame();
    
    /// Get rendering statistics from last frame
    const TilemapRenderStats& getStats() const;
    
    // =================================================================
    // Resource Management
    // =================================================================
    
    /// Preload tileset texture into GPU memory
    /// @param tileset Tileset to preload
    /// @return true on success
    bool preloadTileset(const Tileset& tileset);
    
    /// Preload indexed tileset texture into GPU memory
    /// @param tileset Indexed tileset to preload
    /// @return true on success
    bool preloadTilesetIndexed(const TilesetIndexed& tileset);
    
    /// Unload tileset texture from GPU
    /// @param tileset Tileset to unload
    void unloadTileset(const Tileset& tileset);
    
    /// Unload indexed tileset texture from GPU
    /// @param tileset Indexed tileset to unload
    void unloadTilesetIndexed(const TilesetIndexed& tileset);
    
    /// Clear all cached resources
    void clearCache();
    
    // =================================================================
    // Debug
    // =================================================================
    
    /// Enable/disable debug overlay
    void setDebugOverlay(bool enabled);
    
    /// Get debug overlay state
    bool getDebugOverlay() const;
    
    /// Print rendering statistics
    void printStats() const;
    
private:
    // Implementation details
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    
    // Helper methods
    void buildInstanceBuffer(const TilemapLayer& layer, const Camera& camera, float time);
    void buildInstanceBufferIndexed(const TilemapLayer& layer, const Camera& camera, float time);
    void updateUniforms(const Camera& camera, const TilemapLayer& layer);
    void createPipelineState();
    void createPipelineStateIndexed();
    void createBuffers();
    void createSamplerState();
    bool validateLayer(const TilemapLayer& layer) const;
};

// =============================================================================
// Inline Implementations
// =============================================================================

inline TileInstance::TileInstance()
    : position{0.0f, 0.0f}
    , texCoordBase{0.0f, 0.0f}
    , texCoordSize{1.0f, 1.0f}
    , tintColor{1.0f, 1.0f, 1.0f, 1.0f}
    , flags(0)
    , padding{0, 0, 0}
{
}

inline TileInstanceIndexed::TileInstanceIndexed()
    : position{0.0f, 0.0f}
    , texCoordBase{0.0f, 0.0f}
    , texCoordSize{1.0f, 1.0f}
    , tintColor{1.0f, 1.0f, 1.0f, 1.0f}
    , flags(0)
    , paletteIndex(0)
    , padding{0, 0}
{
}

inline TilemapUniforms::TilemapUniforms()
    : viewportSize{0.0f, 0.0f}
    , tileSize{16.0f, 16.0f}
    , time(0.0f)
    , opacity(1.0f)
    , flags(0)
    , padding(0)
{
    // Initialize projection matrix to identity
    for (int i = 0; i < 16; i++) {
        viewProjectionMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
}

} // namespace SuperTerminal

#endif // TILEMAPRENDERER_H