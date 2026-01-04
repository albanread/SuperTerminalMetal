//
// st_api_video_mode.h
// SuperTerminal Framework
//
// Unified Video Mode C API - Simplified function signatures for VideoModeManager
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef SUPERTERMINAL_API_VIDEO_MODE_H
#define SUPERTERMINAL_API_VIDEO_MODE_H

#include "superterminal_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Video Mode Management
// =============================================================================

/// Video mode enumeration for C API
typedef enum {
    ST_VIDEO_MODE_NONE = 0,
    ST_VIDEO_MODE_LORES = 1,
    ST_VIDEO_MODE_XRES = 2,
    ST_VIDEO_MODE_WRES = 3,
    ST_VIDEO_MODE_URES = 4,
    ST_VIDEO_MODE_PRES = 5
} STVideoMode;

/// Set the current video mode
/// @param mode Video mode to activate
/// @return 1 on success, 0 on failure
ST_API int st_video_mode_set(STVideoMode mode);

/// Set the current video mode using a string name
/// @param modeName Mode name: "text", "lores", "mres", "hres", "xres", "wres", "ures"
/// @return 1 on success, 0 on failure
ST_API int st_video_mode_name(const char* modeName);

/// Get the current video mode
/// @return Current video mode
ST_API STVideoMode st_video_mode_get(void);

/// Check if a specific video mode is active
/// @param mode Video mode to check
/// @return 1 if active, 0 if not
ST_API int st_video_mode_is_active(STVideoMode mode);

/// Disable video mode (return to text-only mode)
ST_API void st_video_mode_disable(void);

/// Check if any video mode is currently active
/// @return 1 if a video mode is active, 0 if not
ST_API int st_video_mode_has_active(void);

/// Get the resolution of the current video mode
/// @param width Output parameter for width
/// @param height Output parameter for height
ST_API void st_video_mode_get_resolution(int* width, int* height);

/// Get the resolution of a specific video mode
/// @param mode Video mode to query
/// @param width Output parameter for width
/// @param height Output parameter for height
ST_API void st_video_mode_get_mode_resolution(STVideoMode mode, int* width, int* height);

// =============================================================================
// Unified Drawing API - Basic Functions
// =============================================================================

/// Set a pixel in the current video mode
/// @param x X coordinate
/// @param y Y coordinate
/// @param color Color value (format depends on mode)
ST_API void st_video_pset(int x, int y, uint32_t color);

/// Get a pixel from the current video mode
/// @param x X coordinate
/// @param y Y coordinate
/// @return Color value (format depends on mode)
ST_API uint32_t st_video_pget(int x, int y);

/// Clear the current video mode buffer
/// @param color Color to clear with
ST_API void st_video_clear(uint32_t color);

/// Clear a GPU buffer in the current video mode
/// @param buffer_id Buffer to clear
/// @param color Color to clear with
ST_API void st_video_clear_gpu(int buffer_id, uint32_t color);

// =============================================================================
// Unified Drawing API - Rectangles
// =============================================================================

/// Draw a filled rectangle in the current video mode (CPU)
/// @param x Top-left X coordinate
/// @param y Top-left Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param color Fill color
ST_API void st_video_rect(int x, int y, int width, int height, uint32_t color);

/// Draw a filled rectangle in the current video mode (GPU)
/// @param buffer_id Buffer to draw into
/// @param x Top-left X coordinate
/// @param y Top-left Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param color Fill color
ST_API void st_video_rect_gpu(int buffer_id, int x, int y, int width, int height, uint32_t color);

// =============================================================================
// Unified Drawing API - Circles
// =============================================================================

/// Draw a filled circle in the current video mode (CPU)
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Radius in pixels
/// @param color Fill color
ST_API void st_video_circle(int cx, int cy, int radius, uint32_t color);

/// Draw a filled circle in the current video mode (GPU)
/// @param buffer_id Buffer to draw into
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Radius in pixels
/// @param color Fill color
ST_API void st_video_circle_gpu(int buffer_id, int cx, int cy, int radius, uint32_t color);

/// Draw an antialiased filled circle in the current video mode (GPU)
/// @param buffer_id Buffer to draw into
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Radius in pixels
/// @param color Fill color
ST_API void st_video_circle_aa(int buffer_id, int cx, int cy, int radius, uint32_t color);

// =============================================================================
// Unified Drawing API - Lines
// =============================================================================

/// Draw a line in the current video mode (CPU)
/// @param x0 Start X coordinate
/// @param y0 Start Y coordinate
/// @param x1 End X coordinate
/// @param y1 End Y coordinate
/// @param color Line color
ST_API void st_video_line(int x0, int y0, int x1, int y1, uint32_t color);

/// Draw a line in the current video mode (GPU)
/// @param buffer_id Buffer to draw into
/// @param x0 Start X coordinate
/// @param y0 Start Y coordinate
/// @param x1 End X coordinate
/// @param y1 End Y coordinate
/// @param color Line color
ST_API void st_video_line_gpu(int buffer_id, int x0, int y0, int x1, int y1, uint32_t color);

/// Draw an antialiased line in the current video mode (GPU)
/// @param buffer_id Buffer to draw into
/// @param x0 Start X coordinate
/// @param y0 Start Y coordinate
/// @param x1 End X coordinate
/// @param y1 End Y coordinate
/// @param color Line color
ST_API void st_video_line_aa(int buffer_id, int x0, int y0, int x1, int y1, uint32_t color);

// =============================================================================
// Unified Drawing API - Buffer Management
// =============================================================================

/// Get the current back buffer ID (for drawing)
/// @return Back buffer ID (0 or 1)
ST_API int st_video_get_back_buffer(void);

/// Get the current front buffer ID (being displayed)
/// @return Front buffer ID (0 or 1)
ST_API int st_video_get_front_buffer(void);

// =============================================================================
// Unified Drawing API - Auto-Buffering (draws to back buffer automatically)
// =============================================================================

/// Clear back buffer with color
/// @param color Fill color
ST_API void st_video_clear_auto(uint32_t color);

/// Draw filled rectangle to back buffer
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param color Fill color
ST_API void st_video_rect_auto(int x, int y, int width, int height, uint32_t color);

/// Draw filled circle to back buffer
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Radius in pixels
/// @param color Fill color
ST_API void st_video_circle_auto(int cx, int cy, int radius, uint32_t color);

/// Draw line to back buffer
/// @param x0 Start X coordinate
/// @param y0 Start Y coordinate
/// @param x1 End X coordinate
/// @param y1 End Y coordinate
/// @param color Line color
ST_API void st_video_line_auto(int x0, int y0, int x1, int y1, uint32_t color);

/// Draw antialiased circle to back buffer
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Radius in pixels
/// @param color Fill color
ST_API void st_video_circle_aa_auto(int cx, int cy, int radius, uint32_t color);

/// Draw antialiased line to back buffer
/// @param x0 Start X coordinate
/// @param y0 Start Y coordinate
/// @param x1 End X coordinate
/// @param y1 End Y coordinate
/// @param color Line color
ST_API void st_video_line_aa_auto(int x0, int y0, int x1, int y1, uint32_t color);

// =============================================================================
// Unified Drawing API - Gradients (URES only)
// =============================================================================

/// Draw a rectangle with gradient fill (URES only)
/// @param buffer_id Buffer ID
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param topLeft Top-left corner color
/// @param topRight Top-right corner color
/// @param bottomLeft Bottom-left corner color
/// @param bottomRight Bottom-right corner color
ST_API void st_video_rect_gradient_gpu(int buffer_id, int x, int y, int width, int height,
                                       uint32_t topLeft, uint32_t topRight,
                                       uint32_t bottomLeft, uint32_t bottomRight);

/// Draw a circle with radial gradient fill (URES only)
/// @param buffer_id Buffer ID
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Radius in pixels
/// @param centerColor Center color
/// @param edgeColor Edge color
ST_API void st_video_circle_gradient_gpu(int buffer_id, int cx, int cy, int radius,
                                         uint32_t centerColor, uint32_t edgeColor);

/// Check if current video mode supports gradient primitives
/// @return 1 if gradients are supported (URES mode), 0 otherwise
ST_API int st_video_supports_gradients(void);

// =============================================================================
// Unified Drawing API - Anti-Aliasing
// =============================================================================

/// Enable or disable anti-aliasing for drawing operations
/// @param enable 1 to enable AA, 0 to disable
/// @return 1 if current mode supports AA functions, 0 if not supported
/// @note When enabled, circle and line functions will use AA variants if available
/// @note If AA functions are not available for the mode, falls back to non-AA versions
/// @note AA is supported by XRES, WRES, and URES modes (not LORES)
ST_API int st_video_enable_antialias(int enable);

/// Check if current video mode supports anti-aliasing functions
/// @return 1 if AA functions exist for current mode, 0 otherwise
ST_API int st_video_supports_antialias(void);

/// Set line width for anti-aliased line drawing
/// @param width Line width in pixels (1.0 = single pixel)
/// @note Only affects AA line drawing when AA is enabled
/// @note Reset to 1.0 when changing video modes
ST_API void st_video_set_line_width(float width);

/// Get current line width setting
/// @return Current line width in pixels
ST_API float st_video_get_line_width(void);

// =============================================================================
// Unified Drawing API - Command Batching
// =============================================================================

/// Begin batching GPU commands for performance
/// @note All GPU draw commands after this will be batched until video_end_batch()
/// @note Reduces command buffer overhead when issuing many draw calls
ST_API void st_video_begin_batch(void);

/// End batching and submit all queued GPU commands
/// @note Commits the batched commands to the GPU
/// @note Does NOT wait for completion - call video_sync() if needed
ST_API void st_video_end_batch(void);

// =============================================================================
// Unified Drawing API - Gradients (URES only) - continued
// =============================================================================

/// Draw a rectangle with gradient fill to back buffer (URES only)
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param topLeft Top-left corner color
/// @param topRight Top-right corner color
/// @param bottomLeft Bottom-left corner color
/// @param bottomRight Bottom-right corner color
ST_API void st_video_rect_gradient_auto(int x, int y, int width, int height,
                                        uint32_t topLeft, uint32_t topRight,
                                        uint32_t bottomLeft, uint32_t bottomRight);

/// Draw a circle with radial gradient fill to back buffer (URES only)
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Radius in pixels
/// @param centerColor Center color
/// @param edgeColor Edge color
ST_API void st_video_circle_gradient_auto(int cx, int cy, int radius,
                                          uint32_t centerColor, uint32_t edgeColor);

// =============================================================================
// Unified Drawing API - Blitting
// =============================================================================

/// Blit between buffers in the current video mode
/// @param src_buffer Source buffer ID
/// @param dst_buffer Destination buffer ID
/// @param src_x Source X coordinate
/// @param src_y Source Y coordinate
/// @param dst_x Destination X coordinate
/// @param dst_y Destination Y coordinate
/// @param width Width to copy
/// @param height Height to copy
ST_API void st_video_blit(int src_buffer, int dst_buffer, 
                          int src_x, int src_y, 
                          int dst_x, int dst_y,
                          int width, int height);

/// Blit between buffers with transparency
/// @param src_buffer Source buffer ID
/// @param dst_buffer Destination buffer ID
/// @param src_x Source X coordinate
/// @param src_y Source Y coordinate
/// @param dst_x Destination X coordinate
/// @param dst_y Destination Y coordinate
/// @param width Width to copy
/// @param height Height to copy
/// @param transparent_color Color to treat as transparent
ST_API void st_video_blit_trans(int src_buffer, int dst_buffer,
                                 int src_x, int src_y,
                                 int dst_x, int dst_y,
                                 int width, int height,
                                 uint32_t transparent_color);

/// Blit using GPU acceleration
/// @param src_buffer Source buffer ID
/// @param dst_buffer Destination buffer ID
/// @param src_x Source X coordinate
/// @param src_y Source Y coordinate
/// @param dst_x Destination X coordinate
/// @param dst_y Destination Y coordinate
/// @param width Width to copy
/// @param height Height to copy
ST_API void st_video_blit_gpu(int src_buffer, int dst_buffer,
                               int src_x, int src_y,
                               int dst_x, int dst_y,
                               int width, int height);

/// Blit using GPU with transparency
/// @param src_buffer Source buffer ID
/// @param dst_buffer Destination buffer ID
/// @param src_x Source X coordinate
/// @param src_y Source Y coordinate
/// @param dst_x Destination X coordinate
/// @param dst_y Destination Y coordinate
/// @param width Width to copy
/// @param height Height to copy
ST_API void st_video_blit_trans_gpu(int src_buffer, int dst_buffer,
                                     int src_x, int src_y,
                                     int dst_x, int dst_y,
                                     int width, int height);

// =============================================================================
// Unified Buffer Management
// =============================================================================

/// Set the active drawing buffer for the current video mode
/// @param buffer_id Buffer to set as active
ST_API void st_video_buffer(int buffer_id);

/// Get the current active drawing buffer
/// @return Current buffer ID
ST_API int st_video_buffer_get(void);

/// Flip/swap buffers (present to screen)
ST_API void st_video_flip(void);

/// GPU flip (for modes that support it)
ST_API void st_video_gpu_flip(void);

/// Sync with GPU operations for specified buffer
ST_API void st_video_sync(int buffer_id);

/// Swap front/back buffers for specified buffer
ST_API void st_video_swap(int buffer_id);

// =============================================================================
// Helper Functions - LORES Simplified Wrappers
// =============================================================================

/// LORES pget - simplified wrapper (returns just the color index)
/// @param x X coordinate
/// @param y Y coordinate
/// @return Color index (0-15)
ST_API uint8_t st_lores_pget_simple(int x, int y);

/// LORES rect - simplified wrapper (uses black background)
/// @param x Top-left X
/// @param y Top-left Y
/// @param width Width
/// @param height Height
/// @param color_index Color (0-15)
ST_API void st_lores_rect_simple(int x, int y, int width, int height, uint8_t color_index);

/// LORES circle - simplified wrapper (uses black background)
/// @param cx Center X
/// @param cy Center Y
/// @param radius Radius
/// @param color_index Color (0-15)
ST_API void st_lores_circle_simple(int cx, int cy, int radius, uint8_t color_index);

/// LORES line - simplified wrapper (uses black background)
/// @param x0 Start X
/// @param y0 Start Y
/// @param x1 End X
/// @param y1 End Y
/// @param color_index Color (0-15)
ST_API void st_lores_line_simple(int x0, int y0, int x1, int y1, uint8_t color_index);

// =============================================================================
// Helper Functions - XRES Simplified Wrappers
// =============================================================================

/// XRES rect - wrapper for fillrect
/// @param x Top-left X
/// @param y Top-left Y
/// @param width Width
/// @param height Height
/// @param color_index Color (0-255)
ST_API void st_xres_rect_simple(int x, int y, int width, int height, uint8_t color_index);

/// XRES circle - CPU circle drawing (not yet implemented, placeholder)
/// @param cx Center X
/// @param cy Center Y
/// @param radius Radius
/// @param color_index Color (0-255)
ST_API void st_xres_circle_simple(int cx, int cy, int radius, uint8_t color_index);

/// XRES line - CPU line drawing (not yet implemented, placeholder)
/// @param x0 Start X
/// @param y0 Start Y
/// @param x1 End X
/// @param y1 End Y
/// @param color_index Color (0-255)
ST_API void st_xres_line_simple(int x0, int y0, int x1, int y1, uint8_t color_index);

// =============================================================================
// Helper Functions - WRES Simplified Wrappers
// =============================================================================

/// WRES rect - wrapper for fillrect
/// @param x Top-left X
/// @param y Top-left Y
/// @param width Width
/// @param height Height
/// @param color_index Color (0-255)
ST_API void st_wres_rect_simple(int x, int y, int width, int height, uint8_t color_index);

/// WRES circle - CPU circle drawing (not yet implemented, placeholder)
/// @param cx Center X
/// @param cy Center Y
/// @param radius Radius
/// @param color_index Color (0-255)
ST_API void st_wres_circle_simple(int cx, int cy, int radius, uint8_t color_index);

/// WRES line - CPU line drawing (not yet implemented, placeholder)
/// @param x0 Start X
/// @param y0 Start Y
/// @param x1 End X
/// @param y1 End Y
/// @param color_index Color (0-255)
ST_API void st_wres_line_simple(int x0, int y0, int x1, int y1, uint8_t color_index);

// =============================================================================
// Helper Functions - URES Simplified Wrappers
// =============================================================================

/// URES rect - wrapper for fillrect
/// @param x Top-left X
/// @param y Top-left Y
/// @param width Width
/// @param height Height
/// @param color ARGB4444 color
ST_API void st_ures_rect_simple(int x, int y, int width, int height, uint16_t color);

/// URES circle - CPU circle drawing (not yet implemented, placeholder)
/// @param cx Center X
/// @param cy Center Y
/// @param radius Radius
/// @param color ARGB4444 color
ST_API void st_ures_circle_simple(int cx, int cy, int radius, uint16_t color);

/// URES line - CPU line drawing (not yet implemented, placeholder)
/// @param x0 Start X
/// @param y0 Start Y
/// @param x1 End X
/// @param y1 End Y
/// @param color ARGB4444 color
ST_API void st_ures_line_simple(int x0, int y0, int x1, int y1, uint16_t color);

// =============================================================================
// Unified API - Buffer Management (Phase 1)
// =============================================================================

/// Get maximum number of buffers available in current mode
/// @return Max buffer count (varies by mode)
ST_API int st_video_get_max_buffers(void);

/// Check if a buffer ID is valid for current mode
/// @param buffer_id Buffer ID to check
/// @return 1 if valid, 0 if not
ST_API int st_video_is_valid_buffer(int buffer_id);

/// Get current drawing buffer
/// @return Current buffer ID
ST_API int st_video_get_current_buffer(void);

// =============================================================================
// Unified API - Feature Detection (Phase 1)
// =============================================================================

/// Feature flags for capability querying
typedef enum {
    ST_VIDEO_FEATURE_PALETTE        = (1 << 0),  ///< Mode uses palette
    ST_VIDEO_FEATURE_PER_ROW_PALETTE = (1 << 1),  ///< Mode supports per-row palette
    ST_VIDEO_FEATURE_GPU_ACCEL      = (1 << 2),  ///< Mode supports GPU acceleration
    ST_VIDEO_FEATURE_ANTIALIASING   = (1 << 3),  ///< Mode supports antialiasing
    ST_VIDEO_FEATURE_GRADIENTS      = (1 << 4),  ///< Mode supports gradients
    ST_VIDEO_FEATURE_ALPHA_BLEND    = (1 << 5),  ///< Mode supports alpha blending
    ST_VIDEO_FEATURE_DIRECT_COLOR   = (1 << 6)   ///< Mode uses direct color (not palette)
} STVideoFeatureFlags;

/// Get feature flags for current mode
/// @return Bitmask of supported features
ST_API uint32_t st_video_get_feature_flags(void);

/// Check if current mode uses a palette
/// @return 1 if uses palette, 0 otherwise
ST_API int st_video_uses_palette(void);

/// Check if current mode supports GPU acceleration
/// @return 1 if GPU supported, 0 otherwise
ST_API int st_video_has_gpu(void);

/// Get color depth (bits per pixel or number of palette entries)
/// @return Color depth
ST_API int st_video_get_color_depth(void);

// =============================================================================
// Unified API - Memory Queries (Phase 2)
// =============================================================================

/// Get memory used per buffer in current mode
/// @return Bytes per buffer
ST_API size_t st_video_get_memory_per_buffer(void);

/// Get total memory used by all buffers in current mode
/// @return Total bytes used
ST_API size_t st_video_get_memory_usage(void);

/// Get total pixel count in current mode
/// @return Number of pixels
ST_API size_t st_video_get_pixel_count(void);

// =============================================================================
// Unified API - Palette Management (Phase 2)
// =============================================================================

/// Reset palette to default colors for current mode
/// @note Only works for palette modes (LORES, XRES, WRES, PRES)
ST_API void st_video_reset_palette_to_default(void);

#ifdef __cplusplus
}
#endif

#endif // SUPERTERMINAL_API_VIDEO_MODE_H