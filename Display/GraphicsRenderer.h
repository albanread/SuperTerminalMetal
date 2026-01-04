//
// GraphicsRenderer.h
// SuperTerminal v2
//
// Double-buffered graphics renderer with dirty tracking for optimal performance.
//

#pragma once

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct STGraphicsRenderer;

/// Performance metrics for graphics rendering
struct STGraphicsMetrics {
    uint64_t totalFrames;       ///< Total frames processed
    uint64_t dirtyFrames;       ///< Frames that required rendering
    uint64_t commandsRendered;  ///< Total commands rendered
    uint64_t commandsCulled;    ///< Total commands culled (offscreen)
    double avgRenderTimeMs;     ///< Average render time per dirty frame (milliseconds)
    double avgUploadTimeMs;     ///< Average upload time per dirty frame (milliseconds)
    double dirtyFrameRatio;     ///< Percentage of frames that were dirty (0.0-1.0)
    double cullRatio;           ///< Percentage of commands culled (0.0-1.0)
};

/// Create a new graphics renderer with double buffering
/// @param device Metal device for texture creation
/// @param width Render surface width in pixels
/// @param height Render surface height in pixels
/// @return Opaque renderer handle, or NULL on failure
STGraphicsRenderer* st_graphics_renderer_create(id<MTLDevice> device, uint32_t width, uint32_t height);

/// Destroy graphics renderer and free resources
/// @param renderer Renderer to destroy
void st_graphics_renderer_destroy(STGraphicsRenderer* renderer);

/// Render graphics layer commands to back buffer and swap
/// @param renderer Graphics renderer
/// @param graphicsLayer GraphicsLayer containing commands to render
void st_graphics_renderer_render(STGraphicsRenderer* renderer, void* graphicsLayer);

/// Get the current front buffer texture for display
/// @param renderer Graphics renderer
/// @return Metal texture ready for GPU sampling, or nil if not ready
id<MTLTexture> st_graphics_renderer_get_texture(STGraphicsRenderer* renderer);

/// Check if renderer has new content since last mark_clean()
/// @param renderer Graphics renderer
/// @return true if new rendering has occurred
bool st_graphics_renderer_is_dirty(STGraphicsRenderer* renderer);

/// Check if renderer has any graphics content to display
/// @param renderer Graphics renderer
/// @return true if renderer contains graphics content
bool st_graphics_renderer_has_content(STGraphicsRenderer* renderer);

/// Mark renderer as clean (up-to-date)
/// Call this after uploading/displaying the texture
/// @param renderer Graphics renderer
void st_graphics_renderer_mark_clean(STGraphicsRenderer* renderer);

/// Clear both buffers to transparent
/// @param renderer Graphics renderer
void st_graphics_renderer_clear(STGraphicsRenderer* renderer);

/// Resize renderer buffers
/// @param renderer Graphics renderer
/// @param width New width in pixels
/// @param height New height in pixels
void st_graphics_renderer_resize(STGraphicsRenderer* renderer, uint32_t width, uint32_t height);

/// Explicitly swap front and back buffers (for SWAP command)
/// Waits for vsync, swaps buffers, and uploads new back buffer
/// @param renderer Graphics renderer
void st_graphics_renderer_swap(STGraphicsRenderer* renderer);

/// Get performance metrics for graphics rendering
/// @param renderer Graphics renderer
/// @param metrics Output structure to fill with metrics
void st_graphics_renderer_get_metrics(STGraphicsRenderer* renderer, STGraphicsMetrics* metrics);

#ifdef __cplusplus
}
#endif