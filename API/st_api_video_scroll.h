//
// st_api_video_scroll.h
// SuperTerminal Framework
//
// Hardware-accelerated scrolling and panning API for all video modes
// Provides GPU-based viewport transforms, parallax layers, and smooth sub-pixel scrolling
//
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef SUPERTERMINAL_API_VIDEO_SCROLL_H
#define SUPERTERMINAL_API_VIDEO_SCROLL_H

#include "st_platform.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// SCROLL LAYER SYSTEM
// =============================================================================
// The scroll layer system allows multiple independent scrolling regions per
// video mode. Each layer can reference a different buffer ID and has its own
// scroll offset, scale, and composition parameters.
//
// Typical use cases:
// - Parallax backgrounds (multiple layers scrolling at different speeds)
// - Split-screen effects (different viewport regions)
// - HUD overlays (non-scrolling UI over scrolling game world)
// - Tiled backgrounds (with wrap mode enabled)
// =============================================================================

/// Scroll layer ID type
typedef int32_t STScrollLayerID;

/// Invalid/null layer ID
#define ST_SCROLL_LAYER_INVALID (-1)

/// Maximum number of scroll layers per video mode
#define ST_SCROLL_MAX_LAYERS 8

/// Scroll wrap mode
typedef enum {
    ST_SCROLL_WRAP_NONE = 0,      ///< Clamp to buffer edges (default)
    ST_SCROLL_WRAP_HORIZONTAL,     ///< Wrap horizontally (for tiled backgrounds)
    ST_SCROLL_WRAP_VERTICAL,       ///< Wrap vertically
    ST_SCROLL_WRAP_BOTH            ///< Wrap both axes
} STScrollWrapMode;

/// Scroll blend mode for layer composition
typedef enum {
    ST_SCROLL_BLEND_OPAQUE = 0,   ///< Replace pixels (no blending)
    ST_SCROLL_BLEND_ALPHA,         ///< Alpha blending (requires alpha channel)
    ST_SCROLL_BLEND_ADD,           ///< Additive blending (for glow effects)
    ST_SCROLL_BLEND_MULTIPLY       ///< Multiplicative blending (for shadows)
} STScrollBlendMode;

/// Scroll layer configuration
typedef struct {
    /// Buffer ID to render from (0-7)
    int32_t bufferID;
    
    /// Scroll offset in pixels
    float scrollX;
    float scrollY;
    
    /// Viewport region (in screen coordinates)
    float viewportX;
    float viewportY;
    float viewportWidth;
    float viewportHeight;
    
    /// Source region (in buffer coordinates, for partial buffer rendering)
    float sourceX;
    float sourceY;
    float sourceWidth;
    float sourceHeight;
    
    /// Scale factors (1.0 = no scaling)
    float scaleX;
    float scaleY;
    
    /// Rotation angle in degrees (0-360)
    float rotation;
    
    /// Layer depth for sorting (lower = rendered first/behind)
    int32_t depth;
    
    /// Blend mode for composition
    STScrollBlendMode blendMode;
    
    /// Wrap mode for tiled backgrounds
    STScrollWrapMode wrapMode;
    
    /// Opacity (0.0 = fully transparent, 1.0 = fully opaque)
    float opacity;
    
    /// Enabled flag
    bool enabled;
    
} STScrollLayerConfig;

// =============================================================================
// LAYER MANAGEMENT
// =============================================================================

/// Initialize scrolling system for current video mode
/// Must be called after setting a video mode (LORES/XRES/WRES/PRES/URES)
/// @return true if successful
ST_API bool st_scroll_init(void);

/// Shutdown scrolling system
ST_API void st_scroll_shutdown(void);

/// Create a new scroll layer
/// @param config Layer configuration (can be NULL for defaults)
/// @return Layer ID, or ST_SCROLL_LAYER_INVALID on failure
ST_API STScrollLayerID st_scroll_layer_create(const STScrollLayerConfig* config);

/// Destroy a scroll layer
/// @param layerID Layer ID to destroy
ST_API void st_scroll_layer_destroy(STScrollLayerID layerID);

/// Destroy all scroll layers
ST_API void st_scroll_layer_destroy_all(void);

/// Get the number of active scroll layers
/// @return Number of active layers
ST_API int32_t st_scroll_layer_get_count(void);

/// Check if a layer ID is valid
/// @param layerID Layer ID to check
/// @return true if valid
ST_API bool st_scroll_layer_is_valid(STScrollLayerID layerID);

// =============================================================================
// LAYER CONFIGURATION
// =============================================================================

/// Set which buffer a layer renders from
/// @param layerID Layer ID
/// @param bufferID Buffer ID (0-7)
ST_API void st_scroll_layer_set_buffer(STScrollLayerID layerID, int32_t bufferID);

/// Get which buffer a layer renders from
/// @param layerID Layer ID
/// @return Buffer ID, or -1 if invalid
ST_API int32_t st_scroll_layer_get_buffer(STScrollLayerID layerID);

/// Set layer scroll offset
/// @param layerID Layer ID
/// @param x X offset in pixels
/// @param y Y offset in pixels
ST_API void st_scroll_layer_set_offset(STScrollLayerID layerID, float x, float y);

/// Get layer scroll offset
/// @param layerID Layer ID
/// @param x Output X offset
/// @param y Output Y offset
ST_API void st_scroll_layer_get_offset(STScrollLayerID layerID, float* x, float* y);

/// Move layer scroll offset by delta (relative movement)
/// @param layerID Layer ID
/// @param dx Delta X
/// @param dy Delta Y
ST_API void st_scroll_layer_move(STScrollLayerID layerID, float dx, float dy);

/// Set layer viewport region (where on screen it appears)
/// @param layerID Layer ID
/// @param x Viewport X position
/// @param y Viewport Y position
/// @param width Viewport width
/// @param height Viewport height
ST_API void st_scroll_layer_set_viewport(STScrollLayerID layerID, 
                                          float x, float y, 
                                          float width, float height);

/// Set layer source region (which part of buffer to render)
/// @param layerID Layer ID
/// @param x Source X position in buffer
/// @param y Source Y position in buffer
/// @param width Source width
/// @param height Source height
ST_API void st_scroll_layer_set_source(STScrollLayerID layerID,
                                        float x, float y,
                                        float width, float height);

/// Set layer scale
/// @param layerID Layer ID
/// @param scaleX X scale factor (1.0 = normal)
/// @param scaleY Y scale factor (1.0 = normal)
ST_API void st_scroll_layer_set_scale(STScrollLayerID layerID, float scaleX, float scaleY);

/// Get layer scale
/// @param layerID Layer ID
/// @param scaleX Output X scale
/// @param scaleY Output Y scale
ST_API void st_scroll_layer_get_scale(STScrollLayerID layerID, float* scaleX, float* scaleY);

/// Set layer rotation
/// @param layerID Layer ID
/// @param degrees Rotation angle in degrees (0-360)
ST_API void st_scroll_layer_set_rotation(STScrollLayerID layerID, float degrees);

/// Get layer rotation
/// @param layerID Layer ID
/// @return Rotation angle in degrees
ST_API float st_scroll_layer_get_rotation(STScrollLayerID layerID);

/// Set layer depth (for sorting)
/// @param layerID Layer ID
/// @param depth Depth value (lower = behind)
ST_API void st_scroll_layer_set_depth(STScrollLayerID layerID, int32_t depth);

/// Get layer depth
/// @param layerID Layer ID
/// @return Depth value
ST_API int32_t st_scroll_layer_get_depth(STScrollLayerID layerID);

/// Set layer blend mode
/// @param layerID Layer ID
/// @param blendMode Blend mode for composition
ST_API void st_scroll_layer_set_blend_mode(STScrollLayerID layerID, STScrollBlendMode blendMode);

/// Get layer blend mode
/// @param layerID Layer ID
/// @return Blend mode
ST_API STScrollBlendMode st_scroll_layer_get_blend_mode(STScrollLayerID layerID);

/// Set layer wrap mode
/// @param layerID Layer ID
/// @param wrapMode Wrap mode for tiled backgrounds
ST_API void st_scroll_layer_set_wrap_mode(STScrollLayerID layerID, STScrollWrapMode wrapMode);

/// Get layer wrap mode
/// @param layerID Layer ID
/// @return Wrap mode
ST_API STScrollWrapMode st_scroll_layer_get_wrap_mode(STScrollLayerID layerID);

/// Set layer opacity
/// @param layerID Layer ID
/// @param opacity Opacity value (0.0 = transparent, 1.0 = opaque)
ST_API void st_scroll_layer_set_opacity(STScrollLayerID layerID, float opacity);

/// Get layer opacity
/// @param layerID Layer ID
/// @return Opacity value
ST_API float st_scroll_layer_get_opacity(STScrollLayerID layerID);

/// Enable/disable a layer
/// @param layerID Layer ID
/// @param enabled true to enable, false to disable
ST_API void st_scroll_layer_set_enabled(STScrollLayerID layerID, bool enabled);

/// Check if layer is enabled
/// @param layerID Layer ID
/// @return true if enabled
ST_API bool st_scroll_layer_is_enabled(STScrollLayerID layerID);

/// Get complete layer configuration
/// @param layerID Layer ID
/// @param config Output configuration structure
/// @return true if successful
ST_API bool st_scroll_layer_get_config(STScrollLayerID layerID, STScrollLayerConfig* config);

/// Set complete layer configuration
/// @param layerID Layer ID
/// @param config Configuration structure
ST_API void st_scroll_layer_set_config(STScrollLayerID layerID, const STScrollLayerConfig* config);

// =============================================================================
// SIMPLE/GLOBAL SCROLLING API
// =============================================================================
// For simple use cases where you just want to scroll the entire screen,
// these functions operate on an implicit "global" layer (layer 0)
// =============================================================================

/// Set global scroll offset (affects default rendering)
/// @param x X offset in pixels
/// @param y Y offset in pixels
ST_API void st_scroll_set(float x, float y);

/// Get global scroll offset
/// @param x Output X offset
/// @param y Output Y offset
ST_API void st_scroll_get(float* x, float* y);

/// Move global scroll by delta
/// @param dx Delta X
/// @param dy Delta Y
ST_API void st_scroll_move(float dx, float dy);

/// Reset global scroll to (0, 0)
ST_API void st_scroll_reset(void);

/// Set global wrap mode
/// @param wrapMode Wrap mode for tiled backgrounds
ST_API void st_scroll_set_wrap_mode(STScrollWrapMode wrapMode);

/// Get global wrap mode
/// @return Current wrap mode
ST_API STScrollWrapMode st_scroll_get_wrap_mode(void);

// =============================================================================
// CAMERA/FOLLOW UTILITIES
// =============================================================================
// High-level camera control similar to tilemap system
// =============================================================================

/// Make camera follow a target position with smoothing
/// @param layerID Layer ID (use -1 for global layer)
/// @param targetX Target X position in world coordinates
/// @param targetY Target Y position in world coordinates
/// @param smoothness Smoothing factor (0.0 = instant, 1.0 = very smooth)
ST_API void st_scroll_camera_follow(STScrollLayerID layerID,
                                     float targetX, float targetY,
                                     float smoothness);

/// Set camera bounds (prevents scrolling outside area)
/// @param layerID Layer ID (use -1 for global layer)
/// @param x Bounds X position
/// @param y Bounds Y position
/// @param width Bounds width
/// @param height Bounds height
ST_API void st_scroll_camera_set_bounds(STScrollLayerID layerID,
                                         float x, float y,
                                         float width, float height);

/// Clear camera bounds (allow unlimited scrolling)
/// @param layerID Layer ID (use -1 for global layer)
ST_API void st_scroll_camera_clear_bounds(STScrollLayerID layerID);

/// Apply camera shake effect
/// @param layerID Layer ID (use -1 for global layer)
/// @param magnitude Shake magnitude in pixels
/// @param duration Shake duration in seconds
ST_API void st_scroll_camera_shake(STScrollLayerID layerID, 
                                    float magnitude, 
                                    float duration);

/// Center camera on a position
/// @param layerID Layer ID (use -1 for global layer)
/// @param worldX World X position
/// @param worldY World Y position
ST_API void st_scroll_camera_center_on(STScrollLayerID layerID, 
                                        float worldX, 
                                        float worldY);

// =============================================================================
// PARALLAX HELPERS
// =============================================================================
// Convenience functions for common parallax scrolling patterns
// =============================================================================

/// Create a parallax layer group (returns ID of first layer)
/// Creates multiple layers with automatic depth and speed scaling
/// @param numLayers Number of layers to create (2-8)
/// @param bufferIDs Array of buffer IDs for each layer
/// @param speedFactors Array of speed multipliers (1.0 = normal, 0.5 = half speed for background)
/// @return ID of first layer, or ST_SCROLL_LAYER_INVALID on failure
ST_API STScrollLayerID st_scroll_parallax_create(int32_t numLayers,
                                                  const int32_t* bufferIDs,
                                                  const float* speedFactors);

/// Update all parallax layers with a scroll delta
/// @param firstLayerID ID of first layer in parallax group
/// @param numLayers Number of layers in group
/// @param dx Delta X
/// @param dy Delta Y
ST_API void st_scroll_parallax_update(STScrollLayerID firstLayerID,
                                       int32_t numLayers,
                                       float dx, float dy);

// =============================================================================
// SYSTEM UPDATE
// =============================================================================

/// Update scroll system (camera smoothing, shake effects, animations)
/// Should be called once per frame
/// @param dt Delta time in seconds
ST_API void st_scroll_update(float dt);

/// Get scroll system statistics
/// @param layerCount Output: number of active layers
/// @param gpuMemoryBytes Output: GPU memory used by scroll system
ST_API void st_scroll_get_stats(int32_t* layerCount, size_t* gpuMemoryBytes);

// =============================================================================
// COORDINATE TRANSFORMATION
// =============================================================================
// Convert between screen coordinates and world coordinates
// =============================================================================

/// Convert screen coordinates to world coordinates
/// @param layerID Layer ID (use -1 for global layer)
/// @param screenX Screen X position
/// @param screenY Screen Y position
/// @param worldX Output world X position
/// @param worldY Output world Y position
ST_API void st_scroll_screen_to_world(STScrollLayerID layerID,
                                       float screenX, float screenY,
                                       float* worldX, float* worldY);

/// Convert world coordinates to screen coordinates
/// @param layerID Layer ID (use -1 for global layer)
/// @param worldX World X position
/// @param worldY World Y position
/// @param screenX Output screen X position
/// @param screenY Output screen Y position
ST_API void st_scroll_world_to_screen(STScrollLayerID layerID,
                                       float worldX, float worldY,
                                       float* screenX, float* screenY);

#ifdef __cplusplus
}
#endif

#endif // SUPERTERMINAL_API_VIDEO_SCROLL_H