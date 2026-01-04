#ifndef SUPERTERMINAL_API_H
#define SUPERTERMINAL_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// =============================================================================
// SuperTerminal v2.0 C API
// =============================================================================
// This is the language-agnostic C API that all scripting language runtimes
// bind to. The framework implements this API, and each language runtime
// wraps it in an idiomatic interface for that language.
//
// Design Principles:
// - Simple C types only (no C++ objects)
// - Thread-safe (can be called from any thread)
// - Immediate mode (functions take effect on next frame)
// - No retained state in API (all state in framework)
// =============================================================================

// API Version
#define SUPERTERMINAL_VERSION_MAJOR 2
#define SUPERTERMINAL_VERSION_MINOR 0
#define SUPERTERMINAL_VERSION_PATCH 0

// Export macro for shared library
#ifdef _WIN32
    #ifdef SUPERTERMINAL_BUILD
        #define ST_API __declspec(dllexport)
    #else
        #define ST_API __declspec(dllimport)
    #endif
#else
    #define ST_API __attribute__((visibility("default")))
#endif

// =============================================================================
// Core Types
// =============================================================================

// Color type (32-bit RGBA)
typedef uint32_t STColor;

// Handle types (opaque IDs)
typedef int32_t STSoundID;
typedef int32_t STSpriteID;
typedef int32_t STAssetID;
typedef int32_t STLayerID;
typedef int32_t STTilemapID;
typedef int32_t STTilesetID;

// Key codes (subset of common keys, matches USB HID)
typedef enum {
    ST_KEY_UNKNOWN = 0,

    // Letters
    ST_KEY_A = 4, ST_KEY_B, ST_KEY_C, ST_KEY_D, ST_KEY_E, ST_KEY_F, ST_KEY_G,
    ST_KEY_H, ST_KEY_I, ST_KEY_J, ST_KEY_K, ST_KEY_L, ST_KEY_M, ST_KEY_N,
    ST_KEY_O, ST_KEY_P, ST_KEY_Q, ST_KEY_R, ST_KEY_S, ST_KEY_T, ST_KEY_U,
    ST_KEY_V, ST_KEY_W, ST_KEY_X, ST_KEY_Y, ST_KEY_Z,

    // Numbers
    ST_KEY_1 = 30, ST_KEY_2, ST_KEY_3, ST_KEY_4, ST_KEY_5,
    ST_KEY_6, ST_KEY_7, ST_KEY_8, ST_KEY_9, ST_KEY_0,

    // Special keys
    ST_KEY_ENTER = 40,
    ST_KEY_ESCAPE = 41,
    ST_KEY_BACKSPACE = 42,
    ST_KEY_TAB = 43,
    ST_KEY_SPACE = 44,

    // Navigation keys
    ST_KEY_INSERT = 73,
    ST_KEY_HOME = 74,
    ST_KEY_DELETE = 76,
    ST_KEY_END = 77,

    // Arrow keys
    ST_KEY_RIGHT = 79,
    ST_KEY_LEFT = 80,
    ST_KEY_DOWN = 81,
    ST_KEY_UP = 82,

    // Function keys
    ST_KEY_F1 = 58, ST_KEY_F2, ST_KEY_F3, ST_KEY_F4, ST_KEY_F5, ST_KEY_F6,
    ST_KEY_F7, ST_KEY_F8, ST_KEY_F9, ST_KEY_F10, ST_KEY_F11, ST_KEY_F12,
} STKeyCode;

// Mouse buttons
typedef enum {
    ST_MOUSE_LEFT = 0,
    ST_MOUSE_RIGHT = 1,
    ST_MOUSE_MIDDLE = 2,
} STMouseButton;

// Layer IDs
typedef enum {
    ST_LAYER_TEXT = 0,
    ST_LAYER_GRAPHICS = 1,
    ST_LAYER_SPRITES = 2,
    ST_LAYER_PARTICLES = 3,
} STLayer;

// Asset types
typedef enum {
    ST_ASSET_IMAGE = 0,
    ST_ASSET_SOUND = 1,
    ST_ASSET_MUSIC = 2,
    ST_ASSET_FONT = 3,
    ST_ASSET_SPRITE = 4,
    ST_ASSET_DATA = 5,
} STAssetType;

// =============================================================================
// Display API - Text Layer
// =============================================================================

// Put a character at grid position
ST_API void st_text_putchar(int x, int y, uint32_t character, STColor fg, STColor bg);

// Put a string at grid position
ST_API void st_text_put(int x, int y, const char* text, STColor fg, STColor bg);

// Clear the entire text grid
ST_API void st_text_clear(void);

// Clear a specific region
ST_API void st_text_clear_region(int x, int y, int width, int height);

// Set text grid size (in characters)
ST_API void st_text_set_size(int width, int height);

// Get text grid size
ST_API void st_text_get_size(int* width, int* height);

// Set text scroll region
ST_API void st_text_scroll(int lines);

// =============================================================================
// Free-form Text Display API
// =============================================================================
// Display text at arbitrary pixel positions with transformations (scale, 
// rotation, shear). Renders on top of all other content for game titles,
// scores, HUD elements, and overlays.
//
// Usage:
//   st_text_display_at(400, 50, "GAME OVER", 2.0, 2.0, 45.0, 0xFF0000FF);
//   st_text_display_at(400, 100, "Score: 12345", 1.5, 1.5, 0.0, 0xFFFFFFFF);
//   st_text_clear_displayed();

/// Text alignment options for free-form text display
typedef enum {
    ST_ALIGN_LEFT = 0,
    ST_ALIGN_CENTER = 1, 
    ST_ALIGN_RIGHT = 2
} STTextAlignment;

// Text effect types for enhanced DisplayText rendering
typedef enum {
    ST_EFFECT_NONE = 0,
    ST_EFFECT_DROP_SHADOW = 1,
    ST_EFFECT_OUTLINE = 2,
    ST_EFFECT_GLOW = 3,
    ST_EFFECT_GRADIENT = 4,
    ST_EFFECT_WAVE = 5,
    ST_EFFECT_NEON = 6
} STTextEffect;

/// Display text at pixel position with transformations
/// @param x X coordinate in pixels
/// @param y Y coordinate in pixels  
/// @param text Text content to display
/// @param scale_x Horizontal scale factor (1.0 = normal)
/// @param scale_y Vertical scale factor (1.0 = normal)
/// @param rotation Rotation in degrees (0 = no rotation)
/// @param color Text color (RGBA format)
/// @param alignment Text alignment (ST_ALIGN_LEFT, ST_ALIGN_CENTER, ST_ALIGN_RIGHT)
/// @param layer Rendering layer (higher = on top, default: 0)
/// @return Text item ID for later modification/removal
ST_API int st_text_display_at(float x, float y, const char* text,
                              float scale_x, float scale_y, float rotation,
                              STColor color, STTextAlignment alignment, int layer);

/// Display text with shear transformation  
/// @param x X coordinate in pixels
/// @param y Y coordinate in pixels
/// @param text Text content to display
/// @param scale_x Horizontal scale factor
/// @param scale_y Vertical scale factor
/// @param rotation Rotation in degrees
/// @param shear_x Horizontal shear factor
/// @param shear_y Vertical shear factor
/// @param color Text color (RGBA format)
/// @param alignment Text alignment
/// @param layer Rendering layer
/// @return Text item ID
ST_API int st_text_display_shear(float x, float y, const char* text,
                                 float scale_x, float scale_y, float rotation,
                                 float shear_x, float shear_y,
                                 STColor color, STTextAlignment alignment, int layer);

/// Display text with visual effects
/// @param x X coordinate in pixels
/// @param y Y coordinate in pixels
/// @param text Text content to display
/// @param scale_x Horizontal scale factor
/// @param scale_y Vertical scale factor
/// @param rotation Rotation in degrees
/// @param color Text color (RGBA format)
/// @param alignment Text alignment
/// @param layer Rendering layer
/// @param effect Effect type to apply
/// @param effect_color Effect color (for shadow, outline, glow)
/// @param effect_intensity Effect intensity (0.0 - 1.0)
/// @param effect_size Effect size (outline width, shadow distance, glow radius)
/// @return Text item ID for later modification/removal
ST_API int st_text_display_with_effects(float x, float y, const char* text,
                                        float scale_x, float scale_y, float rotation,
                                        STColor color, STTextAlignment alignment, int layer,
                                        STTextEffect effect, STColor effect_color,
                                        float effect_intensity, float effect_size);

/// Update existing text item
/// @param item_id Text item ID returned by st_text_display_at
/// @param text New text content (NULL = no change)
/// @param x New X position (-1 = no change)
/// @param y New Y position (-1 = no change)
/// @return 1 if item was found and updated, 0 otherwise
ST_API int st_text_update_item(int item_id, const char* text, float x, float y);

/// Remove specific text item
/// @param item_id Text item ID to remove
/// @return 1 if item was found and removed, 0 otherwise
ST_API int st_text_remove_item(int item_id);

/// Clear all displayed text
ST_API void st_text_clear_displayed(void);

/// Set visibility of text item
/// @param item_id Text item ID
/// @param visible Visibility state (1 = visible, 0 = hidden)
/// @return 1 if item was found, 0 otherwise
ST_API int st_text_set_item_visible(int item_id, int visible);

/// Set layer of text item (for z-ordering)
/// @param item_id Text item ID
/// @param layer New layer value (higher = on top)
/// @return 1 if item was found, 0 otherwise
ST_API int st_text_set_item_layer(int item_id, int layer);

/// Set color of text item
/// @param item_id Text item ID
/// @param color New color (RGBA format)
/// @return 1 if item was found, 0 otherwise
ST_API int st_text_set_item_color(int item_id, uint32_t color);

/// Get number of active text items
/// @return Number of text items in display list
ST_API int st_text_get_item_count(void);

/// Get number of visible text items
/// @return Number of visible text items  
ST_API int st_text_get_visible_count(void);

// =============================================================================
// Display API - Chunky Pixel Graphics (Sextants)
// =============================================================================
// Low-resolution 16-color graphics using Unicode sextant characters.
// Each character cell = 2×3 grid of sub-pixels (chunky pixels).
// Resolution = (text_width × 2) by (text_height × 3) pixels.

/// Set a single pixel in LORES graphics mode
/// @param pixel_x Pixel X coordinate (0 to width*2-1)
/// @param pixel_y Pixel Y coordinate (0 to height*3-1)
/// @param color_index Palette color index (0-15)
/// @param background Background color for OFF pixels
ST_API void st_lores_pset(int pixel_x, int pixel_y, uint8_t color_index, STColor background);

/// Draw a line in LORES graphics mode
/// @param x1 Start X coordinate
/// @param y1 Start Y coordinate
/// @param x2 End X coordinate
/// @param y2 End Y coordinate
/// @param color_index Palette color index (0-15)
/// @param background Background color
ST_API void st_lores_line(int x1, int y1, int x2, int y2, uint8_t color_index, STColor background);

/// Draw a rectangle outline in LORES graphics mode
/// @param x Top-left X coordinate
/// @param y Top-left Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param color_index Palette color index (0-15)
/// @param background Background color
ST_API void st_lores_rect(int x, int y, int width, int height, uint8_t color_index, STColor background);

/// Draw a filled rectangle in LORES graphics mode
/// @param x Top-left X coordinate
/// @param y Top-left Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param color_index Palette color index (0-15)
/// @param background Background color
ST_API void st_lores_fillrect(int x, int y, int width, int height, uint8_t color_index, STColor background);

/// Draw a horizontal line in LORES graphics mode (solid, 1 pixel tall)
/// @param x Starting x coordinate (pixel)
/// @param y Y coordinate (pixel)
/// @param width Width in pixels
/// @param color_index Palette color index (0-15)
/// @param background Background color
ST_API void st_lores_hline(int x, int y, int width, uint8_t color_index, STColor background);

/// Draw a vertical line in LORES graphics mode (solid, 1 pixel wide)
/// @param x X coordinate (pixel)
/// @param y Starting y coordinate (pixel)
/// @param height Height in pixels
/// @param color_index Palette color index (0-15)
/// @param background Background color
ST_API void st_lores_vline(int x, int y, int height, uint8_t color_index, STColor background);

/// Clear all LORES graphics
/// @param background Background color
ST_API void st_lores_clear(STColor background);

/// Get LORES graphics resolution in pixels
/// @param width Pointer to store pixel width (cols × 2)
/// @param height Pointer to store pixel height (rows × 3)
ST_API void st_lores_resolution(int* width, int* height);

// =============================================================================
// Display API - LORES Buffer Management
// =============================================================================

/// Select active buffer for drawing operations
/// @param bufferID Buffer ID (0 = front buffer, 1 = back buffer)
ST_API void st_lores_buffer(int bufferID);

/// Get current active buffer ID
/// @return Active buffer ID (0 or 1)
ST_API int st_lores_buffer_get(void);

/// Swap front and back buffers (instant flip for double buffering)
ST_API void st_lores_flip(void);

// =============================================================================
// Display API - LORES Blitter Functions
// =============================================================================

/// Copy rectangular region within current buffer
/// @param src_x Source X coordinate
/// @param src_y Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dst_x Destination X coordinate
/// @param dst_y Destination Y coordinate
ST_API void st_lores_blit(int src_x, int src_y, int width, int height, int dst_x, int dst_y);

/// Copy rectangular region with transparency (cookie-cut blitting)
/// @param src_x Source X coordinate
/// @param src_y Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dst_x Destination X coordinate
/// @param dst_y Destination Y coordinate
/// @param transparent_color Color index to skip (0-15)
ST_API void st_lores_blit_trans(int src_x, int src_y, int width, int height, 
                                  int dst_x, int dst_y, uint8_t transparent_color);

/// Copy rectangular region between buffers
/// @param src_buffer Source buffer ID (0 or 1)
/// @param dst_buffer Destination buffer ID (0 or 1)
/// @param src_x Source X coordinate
/// @param src_y Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dst_x Destination X coordinate
/// @param dst_y Destination Y coordinate
ST_API void st_lores_blit_buffer(int src_buffer, int dst_buffer,
                                   int src_x, int src_y, int width, int height,
                                   int dst_x, int dst_y);

/// Copy rectangular region between buffers with transparency
/// @param src_buffer Source buffer ID (0 or 1)
/// @param dst_buffer Destination buffer ID (0 or 1)
/// @param src_x Source X coordinate
/// @param src_y Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dst_x Destination X coordinate
/// @param dst_y Destination Y coordinate
/// @param transparent_color Color index to skip (0-15)
ST_API void st_lores_blit_buffer_trans(int src_buffer, int dst_buffer,
                                         int src_x, int src_y, int width, int height,
                                         int dst_x, int dst_y, uint8_t transparent_color);

// =============================================================================
// Display API - LORES Mode Management
// =============================================================================

/// Switch to LORES graphics mode
/// Switch display mode
/// @param mode Display mode: 0 = TEXT (default), 1 = LORES
/// In TEXT mode, the screen shows the standard text grid.
/// In LORES mode, text rendering is disabled and the screen shows a dedicated
/// 160×75 pixel buffer. Use DISPLAYTEXT for text overlays.
ST_API void st_mode(int mode);

/// Get current display mode
/// @return Current mode: 0 = TEXT, 1 = LORES, 2 = XRES, 3 = WRES, 4 = URES, 5 = PRES
ST_API int st_mode_get(void);

// =============================================================================
// Display API - LORES GPU-Accelerated Operations
// =============================================================================

/// GPU-accelerated blit from one LORES buffer to another
/// @param srcBufferID Source buffer index (0-7)
/// @param dstBufferID Destination buffer index (0-7)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
/// @note Uses Metal compute shaders for GPU-to-GPU transfer (10-50x faster than CPU)
ST_API void st_lores_blit_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY);

/// GPU-accelerated transparent blit from one LORES buffer to another
/// @param srcBufferID Source buffer index (0-7)
/// @param dstBufferID Destination buffer index (0-7)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
/// @param transparentColor Color index to skip (0-15)
/// @note Uses Metal compute shaders for GPU-to-GPU transfer with transparency
ST_API void st_lores_blit_trans_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY, int transparentColor);

/// GPU-accelerated clear for LORES buffer
/// @param bufferID Buffer index to clear (0-7)
/// @param colorIndex Color to fill with (0-15)
/// @note Uses Metal compute shader for fast GPU-resident clear (no CPU upload)
/// @note Much faster than st_lores_clear() when doing GPU blits
ST_API void st_lores_clear_gpu(int bufferID, int colorIndex);

/// GPU-accelerated filled rectangle for LORES buffer
/// @param bufferID Buffer index (0-7)
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Rectangle width
/// @param height Rectangle height
/// @param colorIndex Fill color (0-15)
/// @note Uses Metal compute shader - no CPU upload, supports batching
ST_API void st_lores_rect_fill_gpu(int bufferID, int x, int y, int width, int height, int colorIndex);

/// GPU-accelerated filled circle for LORES buffer
/// @param bufferID Buffer index (0-7)
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Circle radius
/// @param colorIndex Fill color (0-15)
/// @note Uses Metal compute shader with distance test - no CPU upload, supports batching
ST_API void st_lores_circle_fill_gpu(int bufferID, int cx, int cy, int radius, int colorIndex);

/// GPU-accelerated line drawing for LORES buffer
/// @param bufferID Buffer index (0-7)
/// @param x0 Start X coordinate
/// @param y0 Start Y coordinate
/// @param x1 End X coordinate
/// @param y1 End Y coordinate
/// @param colorIndex Line color (0-15)
/// @note Uses Metal compute shader with parallel distance test - no CPU upload, supports batching
ST_API void st_lores_line_gpu(int bufferID, int x0, int y0, int x1, int y1, int colorIndex);

// =============================================================================
// Display API - URES Mode (Ultra Resolution 1280×720 direct color)
// =============================================================================

/// Set a pixel in URES mode (1280×720 direct color)
/// @param x X coordinate (0-1279)
/// @param y Y coordinate (0-719)
/// @param color 16-bit ARGB4444 color (0xARGB, 4 bits per channel)
/// @note Use st_urgb() or st_urgba() helpers to create colors
ST_API void st_ures_pset(int x, int y, int color);

/// Get a pixel color in URES mode
/// @param x X coordinate (0-1279)
/// @param y Y coordinate (0-719)
/// @return 16-bit ARGB4444 color (0xARGB)
ST_API int st_ures_pget(int x, int y);

/// Clear URES buffer to a specific color
/// @param color 16-bit ARGB4444 color (0xARGB)
ST_API void st_ures_clear(int color);

/// Fill a rectangle in URES mode
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param color 16-bit ARGB4444 color (0xARGB)
ST_API void st_ures_fillrect(int x, int y, int width, int height, int color);

/// Draw a horizontal line in URES mode
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Length in pixels
/// @param color 16-bit ARGB4444 color (0xARGB)
ST_API void st_ures_hline(int x, int y, int width, int color);

/// Draw a vertical line in URES mode
/// @param x X coordinate
/// @param y Y coordinate
/// @param height Length in pixels
/// @param color 16-bit ARGB4444 color (0xARGB)
ST_API void st_ures_vline(int x, int y, int height, int color);

/// Select active URES buffer for drawing (0=front, 1=back, 2-3=scratch)
/// @param bufferID Buffer ID (0-3)
ST_API void st_ures_buffer(int bufferID);

/// Get current active URES buffer ID
/// @return Active buffer ID (0-3)
ST_API int st_ures_buffer_get(void);

/// Flip URES front and back buffers (swap buffers 0 and 1)
ST_API void st_ures_flip(void);

/// Flip URES GPU buffers 0 and 1 (swap GPU texture pointers for double buffering)
ST_API void st_ures_gpu_flip(void);

/// Sync URES GPU operations for specified buffer
ST_API void st_ures_sync(int bufferID);

/// Swap URES buffer to display (make specified buffer visible)
ST_API void st_ures_swap(int bufferID);

/// Blit from another URES buffer to the active buffer
/// @param srcBufferID Source buffer ID (0-3)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
ST_API void st_ures_blit_from(int srcBufferID, int srcX, int srcY, 
                               int width, int height, int dstX, int dstY);

/// Blit from another URES buffer with transparency (skip alpha=0 pixels)
/// @param srcBufferID Source buffer ID (0-3)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
ST_API void st_ures_blit_from_trans(int srcBufferID, int srcX, int srcY, 
                                     int width, int height, int dstX, int dstY);

/// Create ARGB4444 color from RGB components (opaque)
/// @param r Red component (0-15)
/// @param g Green component (0-15)
/// @param b Blue component (0-15)
/// @return 16-bit ARGB4444 color with full opacity (0xFRGB)
ST_API int st_urgb(int r, int g, int b);

/// Create ARGB4444 color from RGBA components
/// @param r Red component (0-15)
/// @param g Green component (0-15)
/// @param b Blue component (0-15)
/// @param a Alpha component (0-15, 0=transparent, 15=opaque)
/// @return 16-bit ARGB4444 color (0xARGB)
ST_API int st_urgba(int r, int g, int b, int a);

// =============================================================================
// Display API - XRES Buffer Operations (Mode X: 320×240, 256-color palette)
// =============================================================================

/// Set a pixel in XRES mode (320×240, 8-bit palette index)
/// @param x X coordinate (0-319)
/// @param y Y coordinate (0-239)
/// @param colorIndex 8-bit palette index (0-255)
/// @note Indices 0-15 use per-row palette, 16-255 use global palette
ST_API void st_xres_pset(int x, int y, int colorIndex);

/// Get a pixel color index in XRES mode
/// @param x X coordinate (0-319)
/// @param y Y coordinate (0-239)
/// @return 8-bit palette index (0-255)
ST_API int st_xres_pget(int x, int y);

/// Clear XRES buffer to a specific palette index
/// @param colorIndex 8-bit palette index (0-255)
ST_API void st_xres_clear(int colorIndex);

/// Fill a rectangle in XRES mode
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param colorIndex 8-bit palette index (0-255)
ST_API void st_xres_fillrect(int x, int y, int width, int height, int colorIndex);

/// Draw a horizontal line in XRES mode
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Length in pixels
/// @param colorIndex 8-bit palette index (0-255)
ST_API void st_xres_hline(int x, int y, int width, int colorIndex);

/// Draw a vertical line in XRES mode
/// @param x X coordinate
/// @param y Y coordinate
/// @param height Length in pixels
/// @param colorIndex 8-bit palette index (0-255)
ST_API void st_xres_vline(int x, int y, int height, int colorIndex);

/// Set active XRES buffer for drawing (0-3)
/// @param bufferID Buffer ID (0=front, 1=back, 2-3=atlas/scratch)
ST_API void st_xres_buffer(int bufferID);

/// Flip front and back XRES buffers (swap 0 and 1)
ST_API void st_xres_flip();

/// Blit rectangle within the same XRES buffer
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
ST_API void st_xres_blit(int srcX, int srcY, int width, int height, int dstX, int dstY);

/// Blit rectangle with transparency (skip index 0)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
ST_API void st_xres_blit_trans(int srcX, int srcY, int width, int height, int dstX, int dstY);

/// Blit from another XRES buffer
/// @param srcBufferID Source buffer ID (0-3)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
ST_API void st_xres_blit_from(int srcBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY);

/// Blit from another XRES buffer with transparency
/// @param srcBufferID Source buffer ID (0-3)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
ST_API void st_xres_blit_from_trans(int srcBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY);

/// GPU-accelerated blit from one XRES buffer to another (much faster than CPU blit)
/// @param srcBufferID Source buffer ID (0-3)
/// @param dstBufferID Destination buffer ID (0-3)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
/// @note Uses Metal compute shaders for GPU-to-GPU transfer (10-50x faster than CPU)
ST_API void st_xres_blit_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY);

/// GPU-accelerated transparent blit from one XRES buffer to another
/// @param srcBufferID Source buffer ID (0-3)
/// @param dstBufferID Destination buffer ID (0-3)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
/// @param transparentColor Color index to skip (typically 0, range 0-255)
/// @note Uses Metal compute shaders for GPU-to-GPU transfer with transparency
ST_API void st_xres_blit_trans_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY, int transparentColor);

/// Set per-row palette color (indices 0-15)
/// @param row Row number (0-239)
/// @param index Color index within row (0-15)
/// @param r Red component (0-255)
/// @param g Green component (0-255)
/// @param b Blue component (0-255)
ST_API void st_xres_palette_row(int row, int index, int r, int g, int b);

/// Set global palette color (indices 16-255)
/// @param index Global color index (16-255)
/// @param r Red component (0-255)
/// @param g Green component (0-255)
/// @param b Blue component (0-255)
ST_API void st_xres_palette_global(int index, int r, int g, int b);

/// Calculate XRES global palette index from RGB (for 6×8×5 cube)
/// @param r Red component (0-255)
/// @param g Green component (0-255)
/// @param b Blue component (0-255)
/// @return Palette index (16-255) for the closest matching color
/// @note Maps to 6 red × 8 green × 5 blue = 240 color cube
ST_API int st_xrgb(int r, int g, int b);

/// Reset XRES palette to default (IBM RGBI for 0-15, RGB cube for 16-255)
ST_API void st_xres_palette_reset();

// =============================================================================
// Display API - WRES Buffer (Wide Resolution 432×240, 256-color palette)
// =============================================================================

/// Set a pixel in WRES mode
/// @param x X coordinate (0-431)
/// @param y Y coordinate (0-239)
/// @param colorIndex 8-bit palette index (0-255)
/// @note Indices 0-15 use per-row palette, 16-255 use global palette
ST_API void st_wres_pset(int x, int y, int colorIndex);

/// Get a pixel color index in WRES mode
/// @param x X coordinate (0-431)
/// @param y Y coordinate (0-239)
/// @return 8-bit palette index (0-255)
ST_API int st_wres_pget(int x, int y);

/// Clear WRES buffer to a specific palette index
/// @param colorIndex 8-bit palette index (0-255)
ST_API void st_wres_clear(int colorIndex);

/// Fill a rectangle in WRES mode
/// @param x X coordinate (0-431)
/// @param y Y coordinate (0-239)
/// @param width Width in pixels
/// @param height Height in pixels
/// @param colorIndex 8-bit palette index (0-255)
ST_API void st_wres_fillrect(int x, int y, int width, int height, int colorIndex);

/// Draw a horizontal line in WRES mode
/// @param x X coordinate (0-431)
/// @param y Y coordinate (0-239)
/// @param width Length in pixels
/// @param colorIndex 8-bit palette index (0-255)
ST_API void st_wres_hline(int x, int y, int width, int colorIndex);

/// Draw a vertical line in WRES mode
/// @param x X coordinate (0-431)
/// @param y Y coordinate (0-239)
/// @param height Length in pixels
/// @param colorIndex 8-bit palette index (0-255)
ST_API void st_wres_vline(int x, int y, int height, int colorIndex);

/// Set active WRES buffer for drawing (0-3)
/// @param bufferID Buffer ID (0=front, 1=back, 2-3=atlas/scratch)
ST_API void st_wres_buffer(int bufferID);

/// Flip front and back WRES buffers (swap 0 and 1)
ST_API void st_wres_flip();

/// Blit rectangle within the same WRES buffer
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
ST_API void st_wres_blit(int srcX, int srcY, int width, int height, int dstX, int dstY);

/// Blit rectangle with transparency (skip index 0)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
ST_API void st_wres_blit_trans(int srcX, int srcY, int width, int height, int dstX, int dstY);

/// Blit from another WRES buffer
/// @param srcBufferID Source buffer ID (0-3)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
ST_API void st_wres_blit_from(int srcBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY);

/// Blit from another WRES buffer with transparency
/// @param srcBufferID Source buffer ID (0-3)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
ST_API void st_wres_blit_from_trans(int srcBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY);

/// GPU-accelerated blit from one WRES buffer to another (much faster than CPU blit)
/// @param srcBufferID Source buffer ID (0-3)
/// @param dstBufferID Destination buffer ID (0-3)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
/// @note Uses Metal compute shaders for GPU-to-GPU transfer (10-50x faster than CPU)
ST_API void st_wres_blit_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY);

/// GPU-accelerated transparent blit from one WRES buffer to another
/// @param srcBufferID Source buffer ID (0-3)
/// @param dstBufferID Destination buffer ID (0-3)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
/// @param transparentColor Color index to skip (typically 0, range 0-255)
/// @note Uses Metal compute shaders for GPU-to-GPU transfer with transparency
ST_API void st_wres_blit_trans_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY, int transparentColor);

/// Set per-row palette color (indices 0-15)
/// @param row Row number (0-239)
/// @param index Color index (0-15)
/// @param r Red component (0-255)
/// @param g Green component (0-255)
/// @param b Blue component (0-255)
ST_API void st_wres_palette_row(int row, int index, int r, int g, int b);

/// Set global palette color (indices 16-255)
/// @param index Global color index (16-255)
/// @param r Red component (0-255)
/// @param g Green component (0-255)
/// @param b Blue component (0-255)
ST_API void st_wres_palette_global(int index, int r, int g, int b);

/// Calculate WRES global palette index from RGB (for 6×8×5 cube)
/// @param r Red component (0-255)
/// @param g Green component (0-255)
/// @param b Blue component (0-255)
/// @return Palette index (16-255) for the closest matching color
/// @note Maps to 6 red × 8 green × 5 blue = 240 color cube (same as XRES)
ST_API int st_wrgb(int r, int g, int b);

/// Reset WRES palette to default (IBM RGBI for 0-15, RGB cube for 16-255)
ST_API void st_wres_palette_reset();

// =============================================================================
// --- WRES Palette Animation Helpers ---

/// Rotate per-row palette colors (palette cycling effect)
/// @param row Row number (0-239)
/// @param startIndex Start of range (0-15)
/// @param endIndex End of range (0-15)
/// @param direction 1 for forward, -1 for backward
/// @note Colors in range [startIndex, endIndex] are rotated
/// @note Example: rotate(100, 0, 7, 1) cycles colors 0-7 forward on row 100
ST_API void st_wres_palette_rotate_row(int row, int startIndex, int endIndex, int direction);

/// Rotate global palette colors (palette cycling effect)
/// @param startIndex Start of range (16-255)
/// @param endIndex End of range (16-255)
/// @param direction 1 for forward, -1 for backward
/// @note Colors in range [startIndex, endIndex] are rotated
/// @note Example: rotate(16, 31, 1) cycles colors 16-31 forward
ST_API void st_wres_palette_rotate_global(int startIndex, int endIndex, int direction);

/// Copy per-row palette from one row to another
/// @param srcRow Source row (0-239)
/// @param dstRow Destination row (0-239)
/// @note Copies all 16 per-row colors from srcRow to dstRow
ST_API void st_wres_palette_copy_row(int srcRow, int dstRow);

/// Interpolate between two per-row palette colors
/// @param row Row number (0-239)
/// @param index Color index (0-15)
/// @param r1 Start red (0-255)
/// @param g1 Start green (0-255)
/// @param b1 Start blue (0-255)
/// @param r2 End red (0-255)
/// @param g2 End green (0-255)
/// @param b2 End blue (0-255)
/// @param t Interpolation factor (0.0-1.0)
/// @note Sets color to interpolated value between (r1,g1,b1) and (r2,g2,b2)
ST_API void st_wres_palette_lerp_row(int row, int index, int r1, int g1, int b1, int r2, int g2, int b2, float t);

/// Interpolate between two global palette colors
/// @param index Color index (16-255)
/// @param r1 Start red (0-255)
/// @param g1 Start green (0-255)
/// @param b1 Start blue (0-255)
/// @param r2 End red (0-255)
/// @param g2 End green (0-255)
/// @param b2 End blue (0-255)
/// @param t Interpolation factor (0.0-1.0)
/// @note Sets color to interpolated value between (r1,g1,b1) and (r2,g2,b2)
ST_API void st_wres_palette_lerp_global(int index, int r1, int g1, int b1, int r2, int g2, int b2, float t);

// --- WRES Palette Automation (Copper-style effects) ---

/// Enable automatic gradient effect on a WRES palette index
/// @param paletteIndex Which palette index to modify (0-15)
/// @param startRow First scanline (0-239)
/// @param endRow Last scanline (0-239, inclusive)
/// @param startR Start color red (0-255)
/// @param startG Start color green (0-255)
/// @param startB Start color blue (0-255)
/// @param endR End color red (0-255)
/// @param endG End color green (0-255)
/// @param endB End color blue (0-255)
/// @param speed Animation speed (0.0 = static, > 0 = animated)
/// @note Thread Safety: Safe to call from any thread
/// @note Applies a smooth color gradient across scanlines automatically
ST_API void st_wres_palette_auto_gradient(int paletteIndex, int startRow, int endRow,
                                           int startR, int startG, int startB,
                                           int endR, int endG, int endB,
                                           float speed);

/// Enable automatic color bars effect on a WRES palette index
/// @param paletteIndex Which palette index to modify (0-15)
/// @param startRow First scanline (0-239)
/// @param endRow Last scanline (0-239, inclusive)
/// @param barHeight Height of each bar in scanlines
/// @param numColors Number of colors (1-4)
/// @param r1 Color 1 red (0-255)
/// @param g1 Color 1 green (0-255)
/// @param b1 Color 1 blue (0-255)
/// @param r2 Color 2 red (0-255)
/// @param g2 Color 2 green (0-255)
/// @param b2 Color 2 blue (0-255)
/// @param r3 Color 3 red (0-255, ignored if numColors < 3)
/// @param g3 Color 3 green (0-255, ignored if numColors < 3)
/// @param b3 Color 3 blue (0-255, ignored if numColors < 3)
/// @param r4 Color 4 red (0-255, ignored if numColors < 4)
/// @param g4 Color 4 green (0-255, ignored if numColors < 4)
/// @param b4 Color 4 blue (0-255, ignored if numColors < 4)
/// @param speed Animation speed (0.0 = static, > 0 = animated scrolling)
/// @note Thread Safety: Safe to call from any thread
/// @note Creates moving or static color bars (like Amiga copper bars)
ST_API void st_wres_palette_auto_bars(int paletteIndex, int startRow, int endRow,
                                       int barHeight, int numColors,
                                       int r1, int g1, int b1,
                                       int r2, int g2, int b2,
                                       int r3, int g3, int b3,
                                       int r4, int g4, int b4,
                                       float speed);

/// Disable all WRES palette automation
/// @note Thread Safety: Safe to call from any thread
ST_API void st_wres_palette_auto_stop();

/// Update WRES palette automation (call once per frame)
/// @param deltaTime Time elapsed since last update (in seconds)
/// @note Thread Safety: Safe to call from any thread
/// @note This applies the automation effects to the palette
ST_API void st_wres_palette_auto_update(float deltaTime);

// --- XRES Palette Animation Helpers ---

/// Rotate per-row palette colors (palette cycling effect)
/// @param row Row number (0-239)
/// @param startIndex Start of range (0-15)
/// @param endIndex End of range (0-15)
/// @param direction 1 for forward, -1 for backward
/// @note Colors in range [startIndex, endIndex] are rotated
/// @note Example: rotate(100, 0, 7, 1) cycles colors 0-7 forward on row 100
ST_API void st_xres_palette_rotate_row(int row, int startIndex, int endIndex, int direction);

/// Rotate global palette colors (palette cycling effect)
/// @param startIndex Start of range (16-255)
/// @param endIndex End of range (16-255)
/// @param direction 1 for forward, -1 for backward
/// @note Colors in range [startIndex, endIndex] are rotated
/// @note Example: rotate(16, 31, 1) cycles colors 16-31 forward
ST_API void st_xres_palette_rotate_global(int startIndex, int endIndex, int direction);

/// Copy per-row palette from one row to another
/// @param srcRow Source row (0-239)
/// @param dstRow Destination row (0-239)
/// @note Copies all 16 per-row colors from srcRow to dstRow
ST_API void st_xres_palette_copy_row(int srcRow, int dstRow);

/// Interpolate between two per-row palette colors
/// @param row Row number (0-239)
/// @param index Color index (0-15)
/// @param r1 Start red (0-255)
/// @param g1 Start green (0-255)
/// @param b1 Start blue (0-255)
/// @param r2 End red (0-255)
/// @param g2 End green (0-255)
/// @param b2 End blue (0-255)
/// @param t Interpolation factor (0.0-1.0)
/// @note Sets color to interpolated value between (r1,g1,b1) and (r2,g2,b2)
ST_API void st_xres_palette_lerp_row(int row, int index, int r1, int g1, int b1, int r2, int g2, int b2, float t);

/// Interpolate between two global palette colors
/// @param index Color index (16-255)
/// @param r1 Start red (0-255)
/// @param g1 Start green (0-255)
/// @param b1 Start blue (0-255)
/// @param r2 End red (0-255)
/// @param g2 End green (0-255)
/// @param b2 End blue (0-255)
/// @param t Interpolation factor (0.0-1.0)
/// @note Sets color to interpolated value between (r1,g1,b1) and (r2,g2,b2)
ST_API void st_xres_palette_lerp_global(int index, int r1, int g1, int b1, int r2, int g2, int b2, float t);

// --- XRES Palette Automation (Copper-style effects) ---

/// Enable automatic gradient effect on an XRES palette index
/// @param paletteIndex Which palette index to modify (0-15)
/// @param startRow First scanline (0-239)
/// @param endRow Last scanline (0-239, inclusive)
/// @param startR Start color red (0-255)
/// @param startG Start color green (0-255)
/// @param startB Start color blue (0-255)
/// @param endR End color red (0-255)
/// @param endG End color green (0-255)
/// @param endB End color blue (0-255)
/// @param speed Animation speed (0.0 = static, > 0 = animated)
/// @note Thread Safety: Safe to call from any thread
/// @note Applies a smooth color gradient across scanlines automatically
ST_API void st_xres_palette_auto_gradient(int paletteIndex, int startRow, int endRow,
                                           int startR, int startG, int startB,
                                           int endR, int endG, int endB,
                                           float speed);

/// Enable automatic color bars effect on an XRES palette index
/// @param paletteIndex Which palette index to modify (0-15)
/// @param startRow First scanline (0-239)
/// @param endRow Last scanline (0-239, inclusive)
/// @param barHeight Height of each bar in scanlines
/// @param numColors Number of colors (1-4)
/// @param r1 Color 1 red (0-255)
/// @param g1 Color 1 green (0-255)
/// @param b1 Color 1 blue (0-255)
/// @param r2 Color 2 red (0-255)
/// @param g2 Color 2 green (0-255)
/// @param b2 Color 2 blue (0-255)
/// @param r3 Color 3 red (0-255, ignored if numColors < 3)
/// @param g3 Color 3 green (0-255, ignored if numColors < 3)
/// @param b3 Color 3 blue (0-255, ignored if numColors < 3)
/// @param r4 Color 4 red (0-255, ignored if numColors < 4)
/// @param g4 Color 4 green (0-255, ignored if numColors < 4)
/// @param b4 Color 4 blue (0-255, ignored if numColors < 4)
/// @param speed Animation speed (0.0 = static, > 0 = animated scrolling)
/// @note Thread Safety: Safe to call from any thread
/// @note Creates moving or static color bars (like Amiga copper bars)
ST_API void st_xres_palette_auto_bars(int paletteIndex, int startRow, int endRow,
                                       int barHeight, int numColors,
                                       int r1, int g1, int b1,
                                       int r2, int g2, int b2,
                                       int r3, int g3, int b3,
                                       int r4, int g4, int b4,
                                       float speed);

/// Disable all XRES palette automation
/// @note Thread Safety: Safe to call from any thread
ST_API void st_xres_palette_auto_stop();

/// Update XRES palette automation (call once per frame)
/// @param deltaTime Time elapsed since last update (in seconds)
/// @note Thread Safety: Safe to call from any thread
/// @note This applies the automation effects to the palette
ST_API void st_xres_palette_auto_update(float deltaTime);

// --- XRES Gradient Drawing Functions ---

/// Create a palette ramp (fills palette indices with a color gradient)
/// @param row Row number (0-239) for per-row palette, or -1 for global palette
/// @param startIndex Start palette index (0-255)
/// @param endIndex End palette index (0-255)
/// @param r1 Start red (0-255)
/// @param g1 Start green (0-255)
/// @param b1 Start blue (0-255)
/// @param r2 End red (0-255)
/// @param g2 End green (0-255)
/// @param b2 End blue (0-255)
/// @note Creates a smooth gradient from color1 to color2 across palette indices
/// @note Use row=-1 for global palette (indices 16-255), or row number for per-row (0-15)
ST_API void st_xres_palette_make_ramp(int row, int startIndex, int endIndex, 
                                       int r1, int g1, int b1, int r2, int g2, int b2);

/// Draw horizontal gradient (left to right)
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param startIndex Left edge palette index (0-255)
/// @param endIndex Right edge palette index (0-255)
/// @note Fills rectangle with gradient from startIndex (left) to endIndex (right)
ST_API void st_xres_gradient_h(int x, int y, int width, int height, 
                                uint8_t startIndex, uint8_t endIndex);

/// Draw vertical gradient (top to bottom)
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param startIndex Top edge palette index (0-255)
/// @param endIndex Bottom edge palette index (0-255)
/// @note Fills rectangle with gradient from startIndex (top) to endIndex (bottom)
ST_API void st_xres_gradient_v(int x, int y, int width, int height, 
                                uint8_t startIndex, uint8_t endIndex);

/// Draw radial gradient (center to edge)
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Radius in pixels
/// @param centerIndex Center palette index (0-255)
/// @param edgeIndex Edge palette index (0-255)
/// @note Fills circle with gradient from centerIndex (center) to edgeIndex (edge)
ST_API void st_xres_gradient_radial(int cx, int cy, int radius, 
                                     uint8_t centerIndex, uint8_t edgeIndex);

/// Draw four-corner gradient (bilinear interpolation)
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param tlIndex Top-left corner palette index (0-255)
/// @param trIndex Top-right corner palette index (0-255)
/// @param blIndex Bottom-left corner palette index (0-255)
/// @param brIndex Bottom-right corner palette index (0-255)
/// @note Fills rectangle with bilinear gradient between four corner colors
ST_API void st_xres_gradient_corners(int x, int y, int width, int height, 
                                      uint8_t tlIndex, uint8_t trIndex, 
                                      uint8_t blIndex, uint8_t brIndex);

// --- WRES Gradient Drawing Functions ---

/// Create a palette ramp (fills palette indices with a color gradient)
/// @param row Row number (0-239) for per-row palette, or -1 for global palette
/// @param startIndex Start palette index (0-255)
/// @param endIndex End palette index (0-255)
/// @param r1 Start red (0-255)
/// @param g1 Start green (0-255)
/// @param b1 Start blue (0-255)
/// @param r2 End red (0-255)
/// @param g2 End green (0-255)
/// @param b2 End blue (0-255)
/// @note Creates a smooth gradient from color1 to color2 across palette indices
/// @note Use row=-1 for global palette (indices 16-255), or row number for per-row (0-15)
ST_API void st_wres_palette_make_ramp(int row, int startIndex, int endIndex, 
                                       int r1, int g1, int b1, int r2, int g2, int b2);

/// Draw horizontal gradient (left to right)
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param startIndex Left edge palette index (0-255)
/// @param endIndex Right edge palette index (0-255)
/// @note Fills rectangle with gradient from startIndex (left) to endIndex (right)
ST_API void st_wres_gradient_h(int x, int y, int width, int height, 
                                uint8_t startIndex, uint8_t endIndex);

/// Draw vertical gradient (top to bottom)
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param startIndex Top edge palette index (0-255)
/// @param endIndex Bottom edge palette index (0-255)
/// @note Fills rectangle with gradient from startIndex (top) to endIndex (bottom)
ST_API void st_wres_gradient_v(int x, int y, int width, int height, 
                                uint8_t startIndex, uint8_t endIndex);

/// Draw radial gradient (center to edge)
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Radius in pixels
/// @param centerIndex Center palette index (0-255)
/// @param edgeIndex Edge palette index (0-255)
/// @note Fills circle with gradient from centerIndex (center) to edgeIndex (edge)
ST_API void st_wres_gradient_radial(int cx, int cy, int radius, 
                                     uint8_t centerIndex, uint8_t edgeIndex);

/// Draw four-corner gradient (bilinear interpolation)
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param tlIndex Top-left corner palette index (0-255)
/// @param trIndex Top-right corner palette index (0-255)
/// @param blIndex Bottom-left corner palette index (0-255)
/// @param brIndex Bottom-right corner palette index (0-255)
/// @note Fills rectangle with bilinear gradient between four corner colors
ST_API void st_wres_gradient_corners(int x, int y, int width, int height, 
                                      uint8_t tlIndex, uint8_t trIndex, 
                                      uint8_t blIndex, uint8_t brIndex);

/// Wait for all pending GPU blitter operations to complete
/// @note Call this before reading results or flipping buffers after GPU blits
/// @note GPU blits are asynchronous by default for performance
ST_API void st_gpu_sync();

/// Begin a GPU blit batch for performance optimization
/// @note All GPU blits between st_begin_blit_batch() and st_end_blit_batch() will share one command buffer
/// @note This dramatically reduces overhead when doing many blits (e.g. 100+ sprites per frame)
/// @note Must call st_end_blit_batch() to commit the work
ST_API void st_begin_blit_batch();

/// End a GPU blit batch and commit all queued blits
/// @note Commits the command buffer created by st_begin_blit_batch()
/// @note Does NOT wait for completion - call st_gpu_sync() after if needed
ST_API void st_end_blit_batch();

/// GPU-accelerated clear for XRES buffer
/// @param bufferID Buffer index to clear (0-3)
/// @param colorIndex Color to fill with (0-255)
/// @note Uses Metal compute shader for fast GPU-resident clear (no CPU upload)
/// @note Much faster than st_xres_clear() when doing GPU blits
ST_API void st_xres_clear_gpu(int bufferID, int colorIndex);

/// GPU-accelerated clear for WRES buffer
/// @param bufferID Buffer index to clear (0-3)
/// @param colorIndex Color to fill with (0-255)
/// @note Uses Metal compute shader for fast GPU-resident clear (no CPU upload)
/// @note Much faster than st_wres_clear() when doing GPU blits
ST_API void st_wres_clear_gpu(int bufferID, int colorIndex);

/// GPU-accelerated filled rectangle for XRES buffer
/// @param bufferID Buffer index (0-3)
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Rectangle width
/// @param height Rectangle height
/// @param colorIndex Fill color (0-255)
/// @note Uses Metal compute shader - no CPU upload, supports batching
ST_API void st_xres_rect_fill_gpu(int bufferID, int x, int y, int width, int height, int colorIndex);

/// GPU-accelerated filled rectangle for WRES buffer
/// @param bufferID Buffer index (0-3)
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Rectangle width
/// @param height Rectangle height
/// @param colorIndex Fill color (0-255)
/// @note Uses Metal compute shader - no CPU upload, supports batching
ST_API void st_wres_rect_fill_gpu(int bufferID, int x, int y, int width, int height, int colorIndex);

/// GPU-accelerated filled circle for XRES buffer
/// @param bufferID Buffer index (0-3)
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Circle radius
/// @param colorIndex Fill color (0-255)
/// @note Uses Metal compute shader with distance test - no CPU upload, supports batching
ST_API void st_xres_circle_fill_gpu(int bufferID, int cx, int cy, int radius, int colorIndex);

/// GPU-accelerated filled circle for WRES buffer
/// @param bufferID Buffer index (0-3)
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Circle radius
/// @param colorIndex Fill color (0-255)
/// @note Uses Metal compute shader with distance test - no CPU upload, supports batching
ST_API void st_wres_circle_fill_gpu(int bufferID, int cx, int cy, int radius, int colorIndex);

/// GPU-accelerated line drawing for XRES buffer
/// @param bufferID Buffer index (0-3)
/// @param x0 Start X coordinate
/// @param y0 Start Y coordinate
/// @param x1 End X coordinate
/// @param y1 End Y coordinate
/// @param colorIndex Line color (0-255)
/// @note Uses Metal compute shader with parallel distance test - no CPU upload, supports batching
ST_API void st_xres_line_gpu(int bufferID, int x0, int y0, int x1, int y1, int colorIndex);

/// GPU-accelerated line drawing for WRES buffer
/// @param bufferID Buffer index (0-3)
/// @param x0 Start X coordinate
/// @param y0 Start Y coordinate
/// @param x1 End X coordinate
/// @param y1 End Y coordinate
/// @param colorIndex Line color (0-255)
/// @note Uses Metal compute shader with parallel distance test - no CPU upload, supports batching
ST_API void st_wres_line_gpu(int bufferID, int x0, int y0, int x1, int y1, int colorIndex);

/// GPU-accelerated anti-aliased filled circle for XRES buffer
/// @param bufferID Buffer index (0-3)
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Circle radius
/// @param colorIndex Fill color (0-255)
/// @note Uses smooth distance falloff and dithering for anti-aliased edges - no CPU upload, supports batching
ST_API void st_xres_circle_fill_aa(int bufferID, int cx, int cy, int radius, int colorIndex);

/// GPU-accelerated anti-aliased filled circle for WRES buffer
/// @param bufferID Buffer index (0-3)
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Circle radius
/// @param colorIndex Fill color (0-255)
/// @note Uses smooth distance falloff and dithering for anti-aliased edges - no CPU upload, supports batching
ST_API void st_wres_circle_fill_aa(int bufferID, int cx, int cy, int radius, int colorIndex);

/// GPU-accelerated anti-aliased line drawing for XRES buffer
/// @param bufferID Buffer index (0-3)
/// @param x0 Start X coordinate
/// @param y0 Start Y coordinate
/// @param x1 End X coordinate
/// @param y1 End Y coordinate
/// @param colorIndex Line color (0-255)
/// @param lineWidth Line width in pixels (1.0 = single pixel)
/// @note Uses smooth distance falloff and dithering for anti-aliased edges - no CPU upload, supports batching
ST_API void st_xres_line_aa(int bufferID, int x0, int y0, int x1, int y1, int colorIndex, float lineWidth);

/// GPU-accelerated anti-aliased line drawing for WRES buffer
/// @param bufferID Buffer index (0-3)
/// @param x0 Start X coordinate
/// @param y0 Start Y coordinate
/// @param x1 End X coordinate
/// @param y1 End Y coordinate
/// @param colorIndex Line color (0-255)
/// @param lineWidth Line width in pixels (1.0 = single pixel)
/// @note Uses smooth distance falloff and dithering for anti-aliased edges - no CPU upload, supports batching
ST_API void st_wres_line_aa(int bufferID, int x0, int y0, int x1, int y1, int colorIndex, float lineWidth);

// =============================================================================
// Display API - PRES Buffer (Premium Resolution 1280×720, 256-color palette)
// =============================================================================

/// Set a pixel in PRES mode
/// @param x X coordinate (0-1279)
/// @param y Y coordinate (0-719)
/// @param colorIndex 8-bit palette index (0-255)
/// @note Indices 0-15 use per-row palette, 16-255 use global palette
ST_API void st_pres_pset(int x, int y, int colorIndex);

/// Get a pixel color index in PRES mode
/// @param x X coordinate (0-1279)
/// @param y Y coordinate (0-719)
/// @return 8-bit palette index (0-255)
ST_API int st_pres_pget(int x, int y);

/// Clear PRES buffer to a specific palette index
/// @param colorIndex 8-bit palette index (0-255)
ST_API void st_pres_clear(int colorIndex);

/// Fill a rectangle in PRES mode
/// @param x X coordinate (0-1279)
/// @param y Y coordinate (0-719)
/// @param width Width in pixels
/// @param height Height in pixels
/// @param colorIndex 8-bit palette index (0-255)
ST_API void st_pres_fillrect(int x, int y, int width, int height, int colorIndex);

/// Draw a horizontal line in PRES mode
/// @param x X coordinate (0-1279)
/// @param y Y coordinate (0-719)
/// @param width Length in pixels
/// @param colorIndex 8-bit palette index (0-255)
ST_API void st_pres_hline(int x, int y, int width, int colorIndex);

/// Draw a vertical line in PRES mode
/// @param x X coordinate (0-1279)
/// @param y Y coordinate (0-719)
/// @param height Length in pixels
/// @param colorIndex 8-bit palette index (0-255)
ST_API void st_pres_vline(int x, int y, int height, int colorIndex);

/// Draw filled circle in PRES mode (CPU)
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Radius in pixels
/// @param colorIndex 8-bit palette index (0-255)
ST_API void st_pres_circle_simple(int cx, int cy, int radius, int colorIndex);

/// Draw line in PRES mode (CPU)
/// @param x0 Start X coordinate
/// @param y0 Start Y coordinate
/// @param x1 End X coordinate
/// @param y1 End Y coordinate
/// @param colorIndex 8-bit palette index (0-255)
ST_API void st_pres_line_simple(int x0, int y0, int x1, int y1, int colorIndex);

/// Set active PRES buffer for drawing (0-7)
/// @param bufferID Buffer ID (0=front, 1=back, 2-7=atlas/scratch)
ST_API void st_pres_buffer(int bufferID);

/// Flip front and back PRES buffers (swap 0 and 1)
ST_API void st_pres_flip();

/// Blit rectangle within the same PRES buffer
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
ST_API void st_pres_blit(int srcX, int srcY, int width, int height, int dstX, int dstY);

/// Blit rectangle with transparency (skip index 0)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
ST_API void st_pres_blit_trans(int srcX, int srcY, int width, int height, int dstX, int dstY);

/// Blit from another PRES buffer
/// @param srcBufferID Source buffer ID (0-7)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
ST_API void st_pres_blit_from(int srcBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY);

/// Blit from another PRES buffer with transparency
/// @param srcBufferID Source buffer ID (0-7)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
ST_API void st_pres_blit_from_trans(int srcBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY);

/// GPU-accelerated blit from one PRES buffer to another (much faster than CPU blit)
/// @param srcBufferID Source buffer ID (0-7)
/// @param dstBufferID Destination buffer ID (0-7)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
/// @note Uses Metal compute shaders for GPU-to-GPU transfer (10-50x faster than CPU)
ST_API void st_pres_blit_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY);

/// GPU-accelerated transparent blit from one PRES buffer to another
/// @param srcBufferID Source buffer ID (0-7)
/// @param dstBufferID Destination buffer ID (0-7)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
/// @param transparentColor Color index to skip (typically 0, range 0-255)
/// @note Uses Metal compute shaders for GPU-to-GPU transfer with transparency
ST_API void st_pres_blit_trans_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY, int transparentColor);

/// Set per-row palette color (indices 0-15)
/// @param row Row number (0-719)
/// @param index Color index (0-15)
/// @param r Red component (0-255)
/// @param g Green component (0-255)
/// @param b Blue component (0-255)
ST_API void st_pres_palette_row(int row, int index, int r, int g, int b);

/// Set global palette color (indices 16-255)
/// @param index Global color index (16-255)
/// @param r Red component (0-255)
/// @param g Green component (0-255)
/// @param b Blue component (0-255)
ST_API void st_pres_palette_global(int index, int r, int g, int b);

/// Calculate PRES global palette index from RGB (for 6×8×5 cube)
/// @param r Red component (0-255)
/// @param g Green component (0-255)
/// @param b Blue component (0-255)
/// @return Palette index (16-255) for the closest matching color
/// @note Maps to 6 red × 8 green × 5 blue = 240 color cube (same as XRES/WRES)
ST_API int st_prgb(int r, int g, int b);

/// Reset PRES palette to default (IBM RGBI for 0-15, RGB cube for 16-255)
ST_API void st_pres_palette_reset();

// --- PRES Palette Automation (Copper-style effects) ---

/// Enable automatic gradient effect on a PRES palette index
/// @param paletteIndex Which palette index to modify (0-15)
/// @param startRow First scanline (0-719)
/// @param endRow Last scanline (0-719, inclusive)
/// @param startR Start color red (0-255)
/// @param startG Start color green (0-255)
/// @param startB Start color blue (0-255)
/// @param endR End color red (0-255)
/// @param endG End color green (0-255)
/// @param endB End color blue (0-255)
/// @param speed Animation speed (0.0 = static, > 0 = animated)
/// @note Thread Safety: Safe to call from any thread
/// @note Applies a smooth color gradient across scanlines automatically
ST_API void st_pres_palette_auto_gradient(int paletteIndex, int startRow, int endRow,
                                           int startR, int startG, int startB,
                                           int endR, int endG, int endB,
                                           float speed);

/// Enable automatic color bars effect on a PRES palette index
/// @param paletteIndex Which palette index to modify (0-15)
/// @param startRow First scanline (0-719)
/// @param endRow Last scanline (0-719, inclusive)
/// @param barHeight Height of each bar in scanlines
/// @param numColors Number of colors (1-4)
/// @param r1 Color 1 red (0-255)
/// @param g1 Color 1 green (0-255)
/// @param b1 Color 1 blue (0-255)
/// @param r2 Color 2 red (0-255)
/// @param g2 Color 2 green (0-255)
/// @param b2 Color 2 blue (0-255)
/// @param r3 Color 3 red (0-255, ignored if numColors < 3)
/// @param g3 Color 3 green (0-255, ignored if numColors < 3)
/// @param b3 Color 3 blue (0-255, ignored if numColors < 3)
/// @param r4 Color 4 red (0-255, ignored if numColors < 4)
/// @param g4 Color 4 green (0-255, ignored if numColors < 4)
/// @param b4 Color 4 blue (0-255, ignored if numColors < 4)
/// @param speed Animation speed (0.0 = static, > 0 = animated scrolling)
/// @note Thread Safety: Safe to call from any thread
/// @note Creates moving or static color bars (like Amiga copper bars)
ST_API void st_pres_palette_auto_bars(int paletteIndex, int startRow, int endRow,
                                       int barHeight, int numColors,
                                       int r1, int g1, int b1,
                                       int r2, int g2, int b2,
                                       int r3, int g3, int b3,
                                       int r4, int g4, int b4,
                                       float speed);

/// Disable all PRES palette automation
/// @note Thread Safety: Safe to call from any thread
ST_API void st_pres_palette_auto_stop();

/// Update PRES palette automation (call once per frame)
/// @param deltaTime Time elapsed since last update (in seconds)
/// @note Thread Safety: Safe to call from any thread
/// @note This applies the automation effects to the palette
ST_API void st_pres_palette_auto_update(float deltaTime);

// =============================================================================
// --- PRES Palette Animation Helpers ---

/// Rotate per-row palette colors (palette cycling effect)
/// @param row Row number (0-719)
/// @param startIndex Start of range (0-15)
/// @param endIndex End of range (0-15)
/// @param direction 1 for forward, -1 for backward
/// @note Colors in range [startIndex, endIndex] are rotated
/// @note Example: rotate(360, 0, 7, 1) cycles colors 0-7 forward on row 360
ST_API void st_pres_palette_rotate_row(int row, int startIndex, int endIndex, int direction);

/// Rotate global palette colors (palette cycling effect)
/// @param startIndex Start of range (16-255)
/// @param endIndex End of range (16-255)
/// @param direction 1 for forward, -1 for backward
/// @note Colors in range [startIndex, endIndex] are rotated
/// @note Example: rotate(16, 31, 1) cycles colors 16-31 forward
ST_API void st_pres_palette_rotate_global(int startIndex, int endIndex, int direction);

/// Copy per-row palette from one row to another
/// @param srcRow Source row (0-719)
/// @param dstRow Destination row (0-719)
/// @note Copies all 16 per-row colors from srcRow to dstRow
ST_API void st_pres_palette_copy_row(int srcRow, int dstRow);

/// Interpolate between two per-row palette colors
/// @param row Row number (0-719)
/// @param index Color index (0-15)
/// @param r1 Start red (0-255)
/// @param g1 Start green (0-255)
/// @param b1 Start blue (0-255)
/// @param r2 End red (0-255)
/// @param g2 End green (0-255)
/// @param b2 End blue (0-255)
/// @param t Interpolation factor (0.0-1.0)
/// @note Sets color to interpolated value between (r1,g1,b1) and (r2,g2,b2)
ST_API void st_pres_palette_lerp_row(int row, int index, int r1, int g1, int b1, int r2, int g2, int b2, float t);

/// Interpolate between two global palette colors
/// @param index Color index (16-255)
/// @param r1 Start red (0-255)
/// @param g1 Start green (0-255)
/// @param b1 Start blue (0-255)
/// @param r2 End red (0-255)
/// @param g2 End green (0-255)
/// @param b2 End blue (0-255)
/// @param t Interpolation factor (0.0-1.0)
/// @note Sets color to interpolated value between (r1,g1,b1) and (r2,g2,b2)
ST_API void st_pres_palette_lerp_global(int index, int r1, int g1, int b1, int r2, int g2, int b2, float t);

// --- PRES Gradient Drawing Functions ---

/// Create a palette ramp (fills palette indices with a color gradient)
/// @param row Row number (0-719) for per-row palette, or -1 for global palette
/// @param startIndex Start palette index (0-255)
/// @param endIndex End palette index (0-255)
/// @param r1 Start red (0-255)
/// @param g1 Start green (0-255)
/// @param b1 Start blue (0-255)
/// @param r2 End red (0-255)
/// @param g2 End green (0-255)
/// @param b2 End blue (0-255)
/// @note Creates a smooth gradient from color1 to color2 across palette indices
/// @note Use row=-1 for global palette (indices 16-255), or row number for per-row (0-15)
ST_API void st_pres_palette_make_ramp(int row, int startIndex, int endIndex, 
                                       int r1, int g1, int b1, int r2, int g2, int b2);

/// Draw horizontal gradient (left to right)
/// @param bufferID Buffer ID (0-7)
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param startIndex Left edge palette index (0-255)
/// @param endIndex Right edge palette index (0-255)
/// @note Fills rectangle with gradient from startIndex (left) to endIndex (right)
ST_API void st_pres_gradient_h(int bufferID, int x, int y, int width, int height, 
                                uint8_t startIndex, uint8_t endIndex);

/// Draw vertical gradient (top to bottom)
/// @param bufferID Buffer ID (0-7)
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param startIndex Top edge palette index (0-255)
/// @param endIndex Bottom edge palette index (0-255)
/// @note Fills rectangle with gradient from startIndex (top) to endIndex (bottom)
ST_API void st_pres_gradient_v(int bufferID, int x, int y, int width, int height, 
                                uint8_t startIndex, uint8_t endIndex);

/// Draw radial gradient (center to edge)
/// @param bufferID Buffer ID (0-7)
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Radius in pixels
/// @param centerIndex Center palette index (0-255)
/// @param edgeIndex Edge palette index (0-255)
/// @note Fills circle with gradient from centerIndex (center) to edgeIndex (edge)
ST_API void st_pres_gradient_radial(int bufferID, int cx, int cy, int radius, 
                                     uint8_t centerIndex, uint8_t edgeIndex);

/// Draw four-corner gradient (bilinear interpolation)
/// @param bufferID Buffer ID (0-7)
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param tlIndex Top-left corner palette index (0-255)
/// @param trIndex Top-right corner palette index (0-255)
/// @param blIndex Bottom-left corner palette index (0-255)
/// @param brIndex Bottom-right corner palette index (0-255)
/// @note Fills rectangle with bilinear gradient between four corner colors
ST_API void st_pres_gradient_corners(int bufferID, int x, int y, int width, int height, 
                                      uint8_t tlIndex, uint8_t trIndex, 
                                      uint8_t blIndex, uint8_t brIndex);

/// GPU-accelerated clear for PRES buffer
/// @param bufferID Buffer index to clear (0-7)
/// @param colorIndex Color to fill with (0-255)
/// @note Uses Metal compute shader for fast GPU-resident clear (no CPU upload)
/// @note Much faster than st_pres_clear() when doing GPU blits
ST_API void st_pres_clear_gpu(int bufferID, int colorIndex);

/// GPU-accelerated filled rectangle for PRES buffer
/// @param bufferID Buffer index (0-7)
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Rectangle width
/// @param height Rectangle height
/// @param colorIndex Fill color (0-255)
/// @note Uses Metal compute shader - no CPU upload, supports batching
ST_API void st_pres_rect_fill_gpu(int bufferID, int x, int y, int width, int height, int colorIndex);

/// GPU-accelerated filled circle for PRES buffer
/// @param bufferID Buffer index (0-7)
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Circle radius
/// @param colorIndex Fill color (0-255)
/// @note Uses Metal compute shader with distance test - no CPU upload, supports batching
ST_API void st_pres_circle_fill_gpu(int bufferID, int cx, int cy, int radius, int colorIndex);

/// GPU-accelerated line drawing for PRES buffer
/// @param bufferID Buffer index (0-7)
/// @param x0 Start X coordinate
/// @param y0 Start Y coordinate
/// @param x1 End X coordinate
/// @param y1 End Y coordinate
/// @param colorIndex Line color (0-255)
/// @note Uses Bresenham algorithm on GPU - no CPU upload, supports batching
ST_API void st_pres_line_gpu(int bufferID, int x0, int y0, int x1, int y1, int colorIndex);

/// GPU-accelerated anti-aliased filled circle for PRES buffer
/// @param bufferID Buffer index (0-7)
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Circle radius
/// @param colorIndex Fill color (0-255)
/// @note Uses smooth distance falloff and dithering for anti-aliased edges - no CPU upload, supports batching
ST_API void st_pres_circle_fill_aa(int bufferID, int cx, int cy, int radius, int colorIndex);

/// GPU-accelerated anti-aliased line drawing for PRES buffer
/// @param bufferID Buffer index (0-7)
/// @param x0 Start X coordinate
/// @param y0 Start Y coordinate
/// @param x1 End X coordinate
/// @param y1 End Y coordinate
/// @param colorIndex Line color (0-255)
/// @param lineWidth Line width in pixels (1.0 = single pixel)
/// @note Uses smooth distance falloff and dithering for anti-aliased edges - no CPU upload, supports batching
ST_API void st_pres_line_aa(int bufferID, int x0, int y0, int x1, int y1, int colorIndex, float lineWidth);

// =============================================================================
// Display API - URES GPU Blitter (Direct Color ARGB4444)
// =============================================================================

/// GPU-accelerated copy blit for URES buffer
/// @param srcBufferID Source buffer index (0-3)
/// @param dstBufferID Destination buffer index (0-3)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
/// @note Pure GPU operation - no CPU upload, supports batching
ST_API void st_ures_blit_copy_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY);

/// GPU-accelerated transparent blit for URES buffer (skip alpha=0 pixels)
/// @param srcBufferID Source buffer index (0-3)
/// @param dstBufferID Destination buffer index (0-3)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
/// @note Pure GPU operation - skips pixels with alpha=0
ST_API void st_ures_blit_transparent_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY);

/// GPU-accelerated alpha composite blit for URES buffer (proper alpha blending)
/// @param srcBufferID Source buffer index (0-3)
/// @param dstBufferID Destination buffer index (0-3)
/// @param srcX Source X coordinate
/// @param srcY Source Y coordinate
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dstX Destination X coordinate
/// @param dstY Destination Y coordinate
/// @note This performs TRUE alpha compositing - blends colors directly, not just transparency check
ST_API void st_ures_blit_alpha_composite_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY);

/// GPU-accelerated clear for URES buffer
/// @param bufferID Buffer index (0-3)
/// @param color ARGB4444 color value (0xARGB)
/// @note Pure GPU operation - extremely fast clear, supports batching
ST_API void st_ures_clear_gpu(int bufferID, int color);

// =============================================================================
// Display API - URES GPU Primitive Drawing (Direct Color ARGB4444)
// =============================================================================

/// GPU-accelerated filled rectangle for URES buffer
/// @param bufferID Buffer index (0-3)
/// @param x X coordinate
/// @param y Y coordinate
/// @param width Rectangle width
/// @param height Rectangle height
/// @param color ARGB4444 color value (0xARGB)
/// @note Pure GPU operation - no CPU upload, supports batching
ST_API void st_ures_rect_fill_gpu(int bufferID, int x, int y, int width, int height, int color);

/// GPU-accelerated filled circle for URES buffer
/// @param bufferID Buffer index (0-3)
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Circle radius
/// @param color ARGB4444 color value (0xARGB)
/// @note Pure GPU operation - no CPU upload, supports batching
ST_API void st_ures_circle_fill_gpu(int bufferID, int cx, int cy, int radius, int color);

/// GPU-accelerated line drawing for URES buffer
/// @param bufferID Buffer index (0-3)
/// @param x0 Start X coordinate
/// @param y0 Start Y coordinate
/// @param x1 End X coordinate
/// @param y1 End Y coordinate
/// @param color ARGB4444 color value (0xARGB)
/// @note Pure GPU operation - no CPU upload, supports batching
ST_API void st_ures_line_gpu(int bufferID, int x0, int y0, int x1, int y1, int color);

// =============================================================================
// Display API - URES GPU Anti-Aliased Primitive Drawing (TRUE alpha blending!)
// =============================================================================

/// GPU-accelerated anti-aliased filled circle for URES buffer
/// @param bufferID Buffer index (0-3)
/// @param cx Center X coordinate
/// @param cy Center Y coordinate
/// @param radius Circle radius
/// @param color ARGB4444 color value (0xARGB)
/// @note Uses TRUE alpha blending (not dithering!) for smooth edges - no CPU upload, supports batching
ST_API void st_ures_circle_fill_aa(int bufferID, int cx, int cy, int radius, int color);

/// GPU-accelerated anti-aliased line drawing for URES buffer
/// @param bufferID Buffer index (0-3)
/// @param x0 Start X coordinate
/// @param y0 Start Y coordinate
/// @param x1 End X coordinate
/// @param y1 End Y coordinate
/// @param color ARGB4444 color value (0xARGB)
/// @param lineWidth Line width in pixels (1.0 = single pixel)
/// @note Uses TRUE alpha blending (not dithering!) for smooth edges - no CPU upload, supports batching
ST_API void st_ures_line_aa(int bufferID, int x0, int y0, int x1, int y1, int color, float lineWidth);

/// GPU-accelerated gradient rectangle for URES buffer (4-corner bilinear gradient)
/// @param bufferID Buffer index (0-3)
/// @param x Rectangle X coordinate
/// @param y Rectangle Y coordinate
/// @param width Rectangle width
/// @param height Rectangle height
/// @param colorTopLeft ARGB4444 color value for top-left corner
/// @param colorTopRight ARGB4444 color value for top-right corner
/// @param colorBottomLeft ARGB4444 color value for bottom-left corner
/// @param colorBottomRight ARGB4444 color value for bottom-right corner
/// @note Pure GPU operation with bilinear color interpolation - no CPU upload, supports batching
ST_API void st_ures_rect_fill_gradient_gpu(int bufferID, int x, int y, int width, int height,
                                           int colorTopLeft, int colorTopRight,
                                           int colorBottomLeft, int colorBottomRight);

/// GPU-accelerated radial gradient circle for URES buffer
/// @param bufferID Buffer index (0-3)
/// @param cx Circle center X coordinate
/// @param cy Circle center Y coordinate
/// @param radius Circle radius in pixels
/// @param centerColor ARGB4444 color value at center
/// @param edgeColor ARGB4444 color value at edge
/// @note Pure GPU operation with radial color interpolation - no CPU upload, supports batching
ST_API void st_ures_circle_fill_gradient_gpu(int bufferID, int cx, int cy, int radius,
                                             int centerColor, int edgeColor);

/// GPU-accelerated radial gradient circle with anti-aliasing for URES buffer
/// @param bufferID Buffer index (0-3)
/// @param cx Circle center X coordinate
/// @param cy Circle center Y coordinate
/// @param radius Circle radius in pixels
/// @param centerColor ARGB4444 color value at center
/// @param edgeColor ARGB4444 color value at edge
/// @note Uses TRUE alpha blending for smooth gradient and anti-aliased edges - no CPU upload, supports batching
ST_API void st_ures_circle_fill_gradient_aa(int bufferID, int cx, int cy, int radius,
                                            int centerColor, int edgeColor);

// =============================================================================
// Display API - URES Color Utilities (ARGB4444 helpers)
// =============================================================================

/// Pack ARGB components (0-15 each) into ARGB4444 format
/// @param a Alpha component (0-15, where 15=opaque, 0=transparent)
/// @param r Red component (0-15)
/// @param g Green component (0-15)
/// @param b Blue component (0-15)
/// @return Packed ARGB4444 color value (0xARGB)
/// @note Each component is 4 bits. Use this instead of bit shifts in Lua.
ST_API int st_ures_pack_argb4(int a, int r, int g, int b);

/// Pack ARGB components from 8-bit values (0-255) into ARGB4444 format
/// @param a Alpha component (0-255)
/// @param r Red component (0-255)
/// @param g Green component (0-255)
/// @param b Blue component (0-255)
/// @return Packed ARGB4444 color value (0xARGB)
/// @note Converts 8-bit components to 4-bit by dividing by 17 (or right-shifting by 4)
ST_API int st_ures_pack_argb8(int a, int r, int g, int b);

/// Unpack ARGB4444 color into 4-bit components
/// @param color Packed ARGB4444 color value
/// @param out_a Pointer to receive alpha (0-15), can be NULL
/// @param out_r Pointer to receive red (0-15), can be NULL
/// @param out_g Pointer to receive green (0-15), can be NULL
/// @param out_b Pointer to receive blue (0-15), can be NULL
/// @note Returns 4-bit values. Use st_ures_unpack_argb8 for 8-bit values.
ST_API void st_ures_unpack_argb4(int color, int* out_a, int* out_r, int* out_g, int* out_b);

/// Unpack ARGB4444 color into 8-bit components (0-255)
/// @param color Packed ARGB4444 color value
/// @param out_a Pointer to receive alpha (0-255), can be NULL
/// @param out_r Pointer to receive red (0-255), can be NULL
/// @param out_g Pointer to receive green (0-255), can be NULL
/// @param out_b Pointer to receive blue (0-255), can be NULL
/// @note Converts 4-bit components to 8-bit by multiplying by 17 (scales 0-15 to 0-255)
ST_API void st_ures_unpack_argb8(int color, int* out_a, int* out_r, int* out_g, int* out_b);

/// Blend two ARGB4444 colors using alpha compositing (Porter-Duff "over")
/// @param src Source color (foreground)
/// @param dst Destination color (background)
/// @return Blended ARGB4444 color
/// @note Uses formula: out = src + dst * (1 - src.alpha)
ST_API int st_ures_blend_colors(int src, int dst);

/// Linearly interpolate between two ARGB4444 colors
/// @param color1 First color
/// @param color2 Second color
/// @param t Interpolation factor (0.0 = color1, 1.0 = color2)
/// @return Interpolated ARGB4444 color
/// @note Clamps t to [0.0, 1.0] range
ST_API int st_ures_lerp_colors(int color1, int color2, float t);

/// Create an ARGB4444 color from HSV values
/// @param h Hue (0.0 - 360.0 degrees)
/// @param s Saturation (0.0 - 1.0)
/// @param v Value/brightness (0.0 - 1.0)
/// @param a Alpha (0-15)
/// @return ARGB4444 color
/// @note Useful for color wheels, gradients, and procedural palettes
ST_API int st_ures_color_from_hsv(float h, float s, float v, int a);

/// Adjust the brightness of an ARGB4444 color
/// @param color Input color
/// @param factor Brightness multiplier (1.0 = unchanged, 2.0 = twice as bright, 0.5 = half as bright)
/// @return Adjusted ARGB4444 color (alpha preserved)
/// @note Clamps result to valid 4-bit range
ST_API int st_ures_adjust_brightness(int color, float factor);

/// Adjust the alpha of an ARGB4444 color
/// @param color Input color
/// @param alpha New alpha value (0-15)
/// @return Color with modified alpha (RGB components unchanged)
ST_API int st_ures_set_alpha(int color, int alpha);

/// Get the alpha component of an ARGB4444 color
/// @param color Input color
/// @return Alpha value (0-15)
ST_API int st_ures_get_alpha(int color);

// =============================================================================
// Display API - LORES Palette Management
// =============================================================================

/// Set all 32 palettes to a preset configuration
/// @param mode Palette mode: "IBM" (RGBI 16-color) or "C64" (Commodore 64)
ST_API void st_lores_palette_set(const char* mode);

/// Set a specific palette entry
/// @param row Palette row (0-31)
/// @param index Color index within palette (0-15)
/// @param rgba Color in ARGB format (0xAARRGGBB)
ST_API void st_lores_palette_poke(int row, int index, uint32_t rgba);

/// Get a specific palette entry
/// @param row Palette row (0-31)
/// @param index Color index within palette (0-15)
/// @return Color in ARGB format (0xAARRGGBB), or 0 if invalid parameters
ST_API uint32_t st_lores_palette_peek(int row, int index);

// =============================================================================
// Display API - Sixel Graphics (Legacy - Deprecated)
// =============================================================================

/// Put a sixel character at grid position with 6 RGBI colors (one per horizontal stripe)
/// @param x Column position (0-based)
/// @param y Row position (0-based)
/// @param sixel_char Unicode sixel character (U+1FB00 - U+1FB3B)
/// @param colors Array of 6 color indices (0-15, RGBI palette), indexed top to bottom
/// @param bg Background color (RGBA)
///
/// The sixel character is rendered as 6 horizontal stripes, each with its own color
/// from the 16-color RGBI palette (classic CGA/EGA colors).
///
/// Example:
///   uint8_t colors[6] = {15, 14, 12, 10, 9, 1}; // Rainbow gradient
///   st_text_putsixel(10, 5, 0x1FB00, colors, 0xFF000000);
ST_API void st_text_putsixel(int x, int y, uint32_t sixel_char, const uint8_t colors[6], STColor bg);

/// Helper function to pack 6 RGBI color indices into a single uint32_t
/// @param colors Array of 6 color indices (0-15), indexed top to bottom
/// @return Packed color value suitable for st_text_putchar with sixel characters
///
/// This packing format:
///   bits 0-3:   color 5 (bottom stripe)
///   bits 4-7:   color 4
///   bits 8-11:  color 3
///   bits 12-15: color 2
///   bits 16-19: color 1
///   bits 20-23: color 0 (top stripe)
///   bits 24-31: alpha (255)
ST_API uint32_t st_sixel_pack_colors(const uint8_t colors[6]);

/// Put a sixel character using pre-packed color data (advanced usage)
/// @param x Column position (0-based)
/// @param y Row position (0-based)
/// @param sixel_char Unicode sixel character (U+1FB00 - U+1FB3B)
/// @param packed_colors Packed color data from st_sixel_pack_colors()
/// @param bg Background color (RGBA)
ST_API void st_text_putsixel_packed(int x, int y, uint32_t sixel_char, uint32_t packed_colors, STColor bg);

/// Set a single stripe color in an existing sixel character (optimized for ARM BFI)
/// @param x Column position (0-based)
/// @param y Row position (0-based)
/// @param stripe_index Stripe index (0-5, where 0=top, 5=bottom)
/// @param color_index Color palette index (0-15)
///
/// This is optimized for ARM's Bit Field Insert (BFI) instruction for efficient
/// single-stripe updates without reading the entire packed color value.
///
/// Example:
///   st_sixel_set_stripe(10, 5, 0, 15);  // Set top stripe to white
///   st_sixel_set_stripe(10, 5, 5, 1);   // Set bottom stripe to blue
ST_API void st_sixel_set_stripe(int x, int y, int stripe_index, uint8_t color_index);

/// Get a single stripe color from a sixel character
/// @param x Column position (0-based)
/// @param y Row position (0-based)
/// @param stripe_index Stripe index (0-5, where 0=top, 5=bottom)
/// @return Color palette index (0-15), or 0 if out of bounds
ST_API uint8_t st_sixel_get_stripe(int x, int y, int stripe_index);

/// Create a vertical gradient sixel (helper function)
/// @param x Column position (0-based)
/// @param y Row position (0-based)
/// @param top_color Color index for top stripes (0-15)
/// @param bottom_color Color index for bottom stripes (0-15)
/// @param bg Background color (RGBA)
///
/// Creates a smooth 6-stripe gradient from top_color to bottom_color
ST_API void st_sixel_gradient(int x, int y, uint8_t top_color, uint8_t bottom_color, STColor bg);

/// Fill a horizontal line with sixels
/// @param x Starting column
/// @param y Row position
/// @param width Number of cells to fill
/// @param colors Array of 6 color indices
/// @param bg Background color
ST_API void st_sixel_hline(int x, int y, int width, const uint8_t colors[6], STColor bg);

/// Fill a rectangular region with sixels
/// @param x Starting column
/// @param y Starting row
/// @param width Width in cells
/// @param height Height in cells
/// @param colors Array of 6 color indices
/// @param bg Background color
ST_API void st_sixel_fill_rect(int x, int y, int width, int height, const uint8_t colors[6], STColor bg);

// =============================================================================
// Display API - Graphics Layer
// =============================================================================

// Clear graphics layer
ST_API void st_gfx_clear(void);

// Swap graphics buffers (double buffering)
ST_API void st_gfx_swap(void);

// Clear all layers (text, graphics, sprites, rectangles, circles, particles, tilemaps)
ST_API void st_clear_all_layers(void);

// Draw filled rectangle
ST_API void st_gfx_rect(int x, int y, int width, int height, STColor color);

// Draw rectangle outline
ST_API void st_gfx_rect_outline(int x, int y, int width, int height, STColor color, int thickness);

// Draw filled circle
ST_API void st_gfx_circle(int x, int y, int radius, STColor color);

// Draw circle outline
ST_API void st_gfx_circle_outline(int x, int y, int radius, STColor color, int thickness);

// Draw arc outline
ST_API void st_gfx_arc(int x, int y, int radius, float start_angle, float end_angle, STColor color);

// Draw filled arc (pie slice)
ST_API void st_gfx_arc_filled(int x, int y, int radius, float start_angle, float end_angle, STColor color);

// Draw line
ST_API void st_gfx_line(int x1, int y1, int x2, int y2, STColor color, int thickness);

// Draw point/pixel
ST_API void st_gfx_point(int x, int y, STColor color);

// =============================================================================
// Display API - Sprites
// =============================================================================

// Load sprite from file
ST_API STSpriteID st_sprite_load(const char* path);

// Load sprite from built-in asset
ST_API STSpriteID st_sprite_load_builtin(const char* name);

// Show sprite at position
ST_API void st_sprite_show(STSpriteID sprite, int x, int y);

// Hide sprite
ST_API void st_sprite_hide(STSpriteID sprite);

// Set sprite transform (position, rotation, scale)
ST_API void st_sprite_transform(STSpriteID sprite, int x, int y, float rotation, float scale_x, float scale_y);

// Set sprite tint color
ST_API void st_sprite_tint(STSpriteID sprite, STColor color);

// Unload sprite
ST_API void st_sprite_unload(STSpriteID sprite);

// Unload all sprites
ST_API void st_sprite_unload_all();

// =============================================================================
// Indexed Sprite API (4-bit color with 16-color palettes)
// =============================================================================

// Load indexed sprite from index data and palette
// @param indices Index data (0-15 per pixel, width*height bytes)
// @param width Sprite width in pixels
// @param height Sprite height in pixels
// @param palette 16 colors × 4 bytes RGBA (64 bytes total)
// @return Sprite ID, or -1 on failure
ST_API STSpriteID st_sprite_load_indexed(const uint8_t* indices, int width, int height,
                                         const uint8_t* palette);

// Load indexed sprite from RGBA with automatic quantization
// @param pixels RGBA pixel data (width*height*4 bytes)
// @param width Sprite width in pixels
// @param height Sprite height in pixels
// @param out_palette Optional output: generated palette (64 bytes)
// @return Sprite ID, or -1 on failure
ST_API STSpriteID st_sprite_load_indexed_from_rgba(const uint8_t* pixels, int width, int height,
                                                    uint8_t* out_palette);

// Load indexed sprite from SPRTZ compressed file
// @param path Path to .sprtz file
// @return Sprite ID, or -1 on failure
ST_API STSpriteID st_sprite_load_sprtz(const char* path);

// Check if sprite is indexed (4-bit color)
ST_API bool st_sprite_is_indexed(STSpriteID sprite);

// Set custom palette for indexed sprite (creates new GPU texture)
// @param sprite Sprite ID
// @param palette 16 colors × 4 bytes RGBA (64 bytes total)
ST_API bool st_sprite_set_palette(STSpriteID sprite, const uint8_t* palette);

// Set sprite to use a standard palette (shared GPU texture)
// @param sprite Sprite ID
// @param standard_palette_id Standard palette ID (0-31)
ST_API bool st_sprite_set_standard_palette(STSpriteID sprite, uint8_t standard_palette_id);

// Get palette from indexed sprite
// @param sprite Sprite ID
// @param out_palette Output buffer for palette (64 bytes)
ST_API bool st_sprite_get_palette(STSpriteID sprite, uint8_t* out_palette);

// Set single color in indexed sprite palette
// @param sprite Sprite ID
// @param color_index Color index (0-15)
// @param r Red (0-255)
// @param g Green (0-255)
// @param b Blue (0-255)
// @param a Alpha (0-255)
ST_API bool st_sprite_set_palette_color(STSpriteID sprite, int color_index,
                                        uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// Lerp between two palettes for animation
// @param sprite Sprite ID
// @param palette_a First palette (64 bytes)
// @param palette_b Second palette (64 bytes)
// @param t Interpolation factor (0.0 = A, 1.0 = B)
ST_API bool st_sprite_lerp_palette(STSpriteID sprite, const uint8_t* palette_a,
                                   const uint8_t* palette_b, float t);

// Rotate colors in palette for animation
// @param sprite Sprite ID
// @param start_index Start color index (0-15)
// @param end_index End color index (0-15)
// @param amount Rotation amount (positive = right, negative = left)
ST_API bool st_sprite_rotate_palette(STSpriteID sprite, int start_index, int end_index, int amount);

// Adjust indexed sprite palette brightness
// @param sprite Sprite ID
// @param brightness Brightness multiplier (1.0 = normal, 0.5 = darker, 2.0 = brighter)
ST_API bool st_sprite_adjust_brightness(STSpriteID sprite, float brightness);

// Copy palette from one indexed sprite to another
// @param src_sprite Source sprite ID
// @param dst_sprite Destination sprite ID
ST_API bool st_sprite_copy_palette(STSpriteID src_sprite, STSpriteID dst_sprite);

// Begin drawing into a sprite (returns sprite ID)
ST_API STSpriteID st_sprite_begin_draw(int width, int height);

// End drawing into a sprite (finalizes and uploads texture)
ST_API void st_sprite_end_draw();

// Begin drawing to a file (redirects graphics to PNG file)
ST_API bool st_draw_to_file_begin(const char* filename, int width, int height);

// End drawing to file (saves PNG and closes file context)
ST_API bool st_draw_to_file_end();

// Begin drawing to a tileset (creates atlas for procedural tile generation)
ST_API STTilesetID st_tileset_begin_draw(int tileWidth, int tileHeight, int columns, int rows);

// Select which tile to draw into (sets drawing region and coordinate transform)
ST_API bool st_tileset_draw_tile(int tileIndex);

// End drawing to tileset (finalizes and uploads texture atlas)
ST_API bool st_tileset_end_draw();

// =============================================================================
// Display API - Layers
// =============================================================================

// Set layer visibility
ST_API void st_layer_set_visible(STLayer layer, bool visible);

// Set layer alpha (0.0 = transparent, 1.0 = opaque)
ST_API void st_layer_set_alpha(STLayer layer, float alpha);

// Set layer order (lower numbers drawn first)
ST_API void st_layer_set_order(STLayer layer, int order);

// =============================================================================
// Display API - Screen
// =============================================================================

// Get display size in pixels
ST_API void st_display_size(int* width, int* height);

// Get cell size in pixels
ST_API void st_cell_size(int* width, int* height);

// =============================================================================
// Audio API - Sound Effects
// =============================================================================

// Load sound from file
ST_API STSoundID st_sound_load(const char* path);

// Load sound from built-in asset
ST_API STSoundID st_sound_load_builtin(const char* name);

// Play sound (volume: 0.0 to 1.0)
ST_API void st_sound_play(STSoundID sound, float volume);

// Play sound with optional fade-out cap
// @param sound_id Sound slot ID
// @param volume Volume (0.0 to 1.0)
// @param cap_duration Maximum duration in seconds (-1.0 = no cap, play full duration)
//                     If cap_duration < actual duration, fade out and stop
ST_API void st_sound_play_with_fade(uint32_t sound_id, float volume, float cap_duration);

// Stop sound
ST_API void st_sound_stop(STSoundID sound);

// Unload sound
ST_API void st_sound_unload(STSoundID sound);

// =============================================================================
// Audio API - Music
// =============================================================================

// Play music from ABC notation string
ST_API void st_music_play(const char* abc_notation);

// Play ABC notation string with escape sequence processing (\n becomes newline)
ST_API void st_play_abc(const char* abc_text);

// Play music from file
ST_API void st_music_play_file(const char* path);

// Play music from file with format override
ST_API void st_music_play_file_with_format(const char* path, const char* format);

// Render music file to WAV
// @param path Path to the music file (supports .abc, .sid, .vscript)
// @param outputPath Path for the output WAV file
// @param format Optional format override (null for auto-detect)
// @param fastRender Enable fast rendering mode (coarser update rate)
// @return true if successful, false on error
ST_API bool st_music_render_to_wav(const char* path, const char* outputPath, const char* format, bool fastRender);

// Render music file to sound slot
// @param path Path to the music file (supports .abc, .sid, .vscript)
// @param slotNumber Sound slot number to store the rendered audio
// @param format Optional format override (null for auto-detect)
// @param fastRender Enable fast rendering mode (coarser update rate)
// @return Slot number on success, 0 on error
ST_API uint32_t st_music_render_to_slot(const char* path, uint32_t slotNumber, const char* format, bool fastRender);

// Stop music
ST_API void st_music_stop(void);

// Pause music
ST_API void st_music_pause(void);

// Resume music
ST_API void st_music_resume(void);

// Check if music is playing
ST_API bool st_music_is_playing(void);

// Set music volume (0.0 to 1.0)
ST_API void st_music_set_volume(float volume);

// =============================================================================
// Audio API - Music Bank (ID-based Music Management)
// =============================================================================

// Load music from ABC notation string
// Returns music ID (0 = error)
ST_API uint32_t st_music_load_string(const char* abc_notation);

// Load music from ABC file
// Returns music ID (0 = error)
ST_API uint32_t st_music_load_file(const char* filename);

// Play music by ID
ST_API void st_music_play_id(uint32_t music_id, float volume);

// Check if music exists in bank
ST_API bool st_music_exists(uint32_t music_id);

// Get music title by ID
// Returns empty string if not found
ST_API const char* st_music_get_title(uint32_t music_id);

// Get music composer by ID
// Returns empty string if not found
ST_API const char* st_music_get_composer(uint32_t music_id);

// Get music key signature by ID
// Returns empty string if not found
ST_API const char* st_music_get_key(uint32_t music_id);

// Get music tempo by ID
// Returns 0.0 if not found
ST_API float st_music_get_tempo(uint32_t music_id);

// Free a music piece by ID
ST_API bool st_music_free(uint32_t music_id);

// Free all music from bank
ST_API void st_music_free_all(void);

// Get number of music pieces in bank
ST_API uint32_t st_music_get_count(void);

// Get total memory usage of music bank (bytes)
ST_API uint32_t st_music_get_memory(void);

// =============================================================================
// Audio API - Synthesis
// =============================================================================

// Play synthesized note (note: MIDI note number 0-127, duration: seconds)
ST_API void st_synth_note(int note, float duration, float volume);

// Set synthesizer instrument (0-127, General MIDI)
ST_API void st_synth_set_instrument(int instrument);

// Play synthesized frequency (frequency: Hz, duration: seconds)
ST_API void st_synth_frequency(float frequency, float duration, float volume);

// =============================================================================
// Audio API - Sound Bank (ID-based Sound Management)
// =============================================================================

// Create a beep sound and store in sound bank
// Returns sound ID (0 = error)
ST_API uint32_t st_sound_create_beep(float frequency, float duration);

// Predefined sound effects (Phase 2) - Returns sound ID (0 = error)
ST_API uint32_t st_sound_create_zap(float frequency, float duration);
ST_API uint32_t st_sound_create_explode(float size, float duration);
ST_API uint32_t st_sound_create_coin(float pitch, float duration);
ST_API uint32_t st_sound_create_jump(float power, float duration);
ST_API uint32_t st_sound_create_shoot(float power, float duration);
ST_API uint32_t st_sound_create_click(float sharpness, float duration);
ST_API uint32_t st_sound_create_blip(float pitch, float duration);
ST_API uint32_t st_sound_create_pickup(float brightness, float duration);
ST_API uint32_t st_sound_create_powerup(float intensity, float duration);
ST_API uint32_t st_sound_create_hurt(float severity, float duration);
ST_API uint32_t st_sound_create_sweep_up(float start_freq, float end_freq, float duration);
ST_API uint32_t st_sound_create_sweep_down(float start_freq, float end_freq, float duration);
ST_API uint32_t st_sound_create_big_explosion(float size, float duration);
ST_API uint32_t st_sound_create_small_explosion(float intensity, float duration);
ST_API uint32_t st_sound_create_distant_explosion(float distance, float duration);
ST_API uint32_t st_sound_create_metal_explosion(float shrapnel, float duration);
ST_API uint32_t st_sound_create_bang(float intensity, float duration);
ST_API uint32_t st_sound_create_random_beep(uint32_t seed, float duration);

// Phase 3: Custom Synthesis APIs
// Create a tone with specified frequency, duration, and waveform
// waveform: 0=SINE, 1=SQUARE, 2=SAWTOOTH, 3=TRIANGLE, 4=NOISE, 5=PULSE
// Returns sound ID (0 = error)
ST_API uint32_t st_sound_create_tone(float frequency, float duration, int waveform);

// Create a musical note with ADSR envelope
// note: MIDI note number (0-127, where 60=middle C)
// duration: total duration in seconds
// waveform: 0=SINE, 1=SQUARE, 2=SAWTOOTH, 3=TRIANGLE, 4=NOISE, 5=PULSE
// attack, decay, sustain_level, release: ADSR envelope parameters (seconds and 0-1 level)
// Returns sound ID (0 = error)
ST_API uint32_t st_sound_create_note(int note, float duration, int waveform,
                                      float attack, float decay, float sustain_level, float release);

// Create noise with specified type and duration
// noise_type: 0=WHITE (default), 1=PINK, 2=BROWN/RED
// Returns sound ID (0 = error)
ST_API uint32_t st_sound_create_noise(int noise_type, float duration);

// Phase 4: Advanced Synthesis APIs
// Create FM synthesized sound
// carrier_freq: carrier frequency in Hz
// modulator_freq: modulator frequency in Hz
// mod_index: modulation index (depth of modulation, typically 0.5-10.0)
// duration: duration in seconds
// Returns sound ID (0 = error)
ST_API uint32_t st_sound_create_fm(float carrier_freq, float modulator_freq, 
                                    float mod_index, float duration);

// Create a tone with filter applied
// frequency: frequency in Hz
// duration: duration in seconds
// waveform: 0=SINE, 1=SQUARE, 2=SAWTOOTH, 3=TRIANGLE, 4=NOISE, 5=PULSE
// filter_type: 0=NONE, 1=LOW_PASS, 2=HIGH_PASS, 3=BAND_PASS
// cutoff: filter cutoff frequency in Hz
// resonance: filter resonance (0.0-1.0)
// Returns sound ID (0 = error)
ST_API uint32_t st_sound_create_filtered_tone(float frequency, float duration, int waveform,
                                               int filter_type, float cutoff, float resonance);

// Create a musical note with ADSR envelope and filter
// note: MIDI note number (0-127, where 60=middle C)
// duration: total duration in seconds
// waveform: 0=SINE, 1=SQUARE, 2=SAWTOOTH, 3=TRIANGLE, 4=NOISE, 5=PULSE
// attack, decay, sustain_level, release: ADSR envelope parameters
// filter_type: 0=NONE, 1=LOW_PASS, 2=HIGH_PASS, 3=BAND_PASS
// cutoff: filter cutoff frequency in Hz
// resonance: filter resonance (0.0-1.0)
// Returns sound ID (0 = error)
ST_API uint32_t st_sound_create_filtered_note(int note, float duration, int waveform,
                                               float attack, float decay, float sustain_level, float release,
                                               int filter_type, float cutoff, float resonance);

// Phase 5: Effects Chain APIs
// Create a tone with reverb effect
// frequency: frequency in Hz
// duration: duration in seconds
// waveform: 0=SINE, 1=SQUARE, 2=SAWTOOTH, 3=TRIANGLE, 4=NOISE, 5=PULSE
// room_size: reverb room size (0.0-1.0)
// damping: high frequency damping (0.0-1.0)
// wet: wet signal level (0.0-1.0)
// Returns sound ID (0 = error)
ST_API uint32_t st_sound_create_with_reverb(float frequency, float duration, int waveform,
                                             float room_size, float damping, float wet);

// Create a tone with delay/echo effect
// frequency: frequency in Hz
// duration: duration in seconds
// waveform: 0=SINE, 1=SQUARE, 2=SAWTOOTH, 3=TRIANGLE, 4=NOISE, 5=PULSE
// delay_time: delay time in seconds
// feedback: feedback amount (0.0-1.0)
// mix: dry/wet mix (0.0-1.0)
// Returns sound ID (0 = error)
ST_API uint32_t st_sound_create_with_delay(float frequency, float duration, int waveform,
                                            float delay_time, float feedback, float mix);

// Create a tone with distortion effect
// frequency: frequency in Hz
// duration: duration in seconds
// waveform: 0=SINE, 1=SQUARE, 2=SAWTOOTH, 3=TRIANGLE, 4=NOISE, 5=PULSE
// drive: distortion drive amount (0.0-10.0)
// tone: tone control (0.0-1.0)
// level: output level (0.0-1.0)
// Returns sound ID (0 = error)
ST_API uint32_t st_sound_create_with_distortion(float frequency, float duration, int waveform,
                                                 float drive, float tone, float level);

// Play a sound from the sound bank by ID
// volume: 0.0 to 1.0 (default 1.0)
// pan: -1.0 (left) to 1.0 (right), 0.0 = center (default 0.0)
ST_API void st_sound_play_id(uint32_t sound_id, float volume, float pan);

// Free a sound from the sound bank
// Returns true if sound was found and freed
ST_API bool st_sound_free_id(uint32_t sound_id);

// Free all sounds from the sound bank
ST_API void st_sound_free_all(void);

// Check if a sound exists in the sound bank
ST_API bool st_sound_exists(uint32_t sound_id);

// Get number of sounds in the sound bank
ST_API size_t st_sound_get_count(void);

// Get approximate memory usage of sound bank (bytes)
ST_API size_t st_sound_get_memory_usage(void);

// =============================================================================
// Audio API - SID Player (Commodore 64 Music)
// =============================================================================

// Load SID file from disk
// Returns SID ID (0 = error)
ST_API uint32_t st_sid_load_file(const char* filename);

// Load SID file from memory buffer
// Returns SID ID (0 = error)
ST_API uint32_t st_sid_load_memory(const uint8_t* data, size_t size);

// Play SID tune by ID
// subtune: subtune number (1-based, 0 = default subtune)
// volume: playback volume (0.0-1.0, default 1.0)
ST_API void st_sid_play(uint32_t sid_id, int subtune, float volume);

// Stop SID playback
ST_API void st_sid_stop(void);

// Pause SID playback
ST_API void st_sid_pause(void);

// Resume SID playback
ST_API void st_sid_resume(void);

// Check if SID is currently playing
ST_API bool st_sid_is_playing(void);

// Set SID playback volume (0.0-1.0)
ST_API void st_sid_set_volume(float volume);

// Get SID title/name
// Returns empty string if not found
ST_API const char* st_sid_get_title(uint32_t sid_id);

// Get SID author/composer
// Returns empty string if not found
ST_API const char* st_sid_get_author(uint32_t sid_id);

// Get SID copyright/released info
// Returns empty string if not found
ST_API const char* st_sid_get_copyright(uint32_t sid_id);

// Get number of subtunes in SID
// Returns 0 if SID not found
ST_API int st_sid_get_subtune_count(uint32_t sid_id);

// Get default subtune number (1-based)
// Returns 0 if SID not found
ST_API int st_sid_get_default_subtune(uint32_t sid_id);

// Set SID emulation quality
// quality: 0=FAST, 1=GOOD (default), 2=BEST
ST_API void st_sid_set_quality(int quality);

// Set SID chip model
// model: 0=MOS6581 (old), 1=MOS8580 (new), 2=AUTO (default)
ST_API void st_sid_set_chip_model(int model);

// Set playback speed multiplier
// speed: 1.0 = normal, 2.0 = double speed, 0.5 = half speed
ST_API void st_sid_set_speed(float speed);

// Set maximum number of SID chips to emulate (1-3)
// maxSids: Number of SID chips (default 3)
// Note: Requires SID player re-initialization to take effect
ST_API void st_sid_set_max_sids(int maxSids);

// Get maximum number of SID chips
// Returns: Maximum SID chips (1-3)
ST_API int st_sid_get_max_sids(void);

// Get current playback time in seconds
ST_API float st_sid_get_time(void);

// Free a SID from the bank by ID
// Returns true if SID was found and freed
ST_API bool st_sid_free(uint32_t sid_id);

// Free all SIDs from the bank
ST_API void st_sid_free_all(void);

// Check if a SID exists in the bank
ST_API bool st_sid_exists(uint32_t sid_id);

// Get number of SIDs in the bank
ST_API uint32_t st_sid_get_count(void);

// Get total memory usage of SID bank (bytes)
ST_API uint32_t st_sid_get_memory(void);

// =============================================================================
// Voice Controller API - Persistent Voice Synthesis for Advanced Audio Programming
// =============================================================================

// Set voice waveform
// voiceNum: Voice number (1-based, 1-8)
// waveform: Waveform type (0=Silence, 1=Sine, 2=Square, 3=Sawtooth, 4=Triangle, 5=Noise, 6=Pulse)
ST_API void st_voice_set_waveform(int voiceNum, int waveform);

// Set voice frequency in Hz
// voiceNum: Voice number (1-based)
// frequencyHz: Frequency in Hz
ST_API void st_voice_set_frequency(int voiceNum, float frequencyHz);

// Set voice note by MIDI note number
// voiceNum: Voice number (1-based)
// midiNote: MIDI note number (0-127, middle C = 60)
ST_API void st_voice_set_note(int voiceNum, int midiNote);

// Set voice note by note name
// voiceNum: Voice number (1-based)
// noteName: Note name (e.g., "C-4", "A#3", "Gb5")
ST_API void st_voice_set_note_name(int voiceNum, const char* noteName);

// Set voice envelope (ADSR)
// voiceNum: Voice number (1-based)
// attackMs: Attack time in milliseconds
// decayMs: Decay time in milliseconds
// sustainLevel: Sustain level (0.0 to 1.0)
// releaseMs: Release time in milliseconds
ST_API void st_voice_set_envelope(int voiceNum, float attackMs, float decayMs, float sustainLevel, float releaseMs);

// Set voice gate (on = start/sustain note, off = release)
// voiceNum: Voice number (1-based)
// gateOn: Gate state (0 = off, 1 = on)
ST_API void st_voice_set_gate(int voiceNum, int gateOn);

// Set voice volume
// voiceNum: Voice number (1-based)
// volume: Volume level (0.0 to 1.0)
ST_API void st_voice_set_volume(int voiceNum, float volume);

// Set pulse width for pulse waveform
// voiceNum: Voice number (1-based)
// pulseWidth: Pulse width (0.0 to 1.0, 0.5 = square wave)
ST_API void st_voice_set_pulse_width(int voiceNum, float pulseWidth);

// Enable/disable filter routing for voice
// voiceNum: Voice number (1-based)
// enabled: Route through global filter (0 = disabled, 1 = enabled)
ST_API void st_voice_set_filter_routing(int voiceNum, int enabled);

// Set global filter type
// filterType: Filter type (0=None, 1=LowPass, 2=HighPass, 3=BandPass)
ST_API void st_voice_set_filter_type(int filterType);

// Set global filter cutoff frequency
// cutoffHz: Cutoff frequency in Hz
ST_API void st_voice_set_filter_cutoff(float cutoffHz);

// Set global filter resonance
// resonance: Resonance (1.0 = none, higher = more resonant)
ST_API void st_voice_set_filter_resonance(float resonance);

// Enable/disable global filter
// enabled: Filter enabled (0 = disabled, 1 = enabled)
ST_API void st_voice_set_filter_enabled(int enabled);

// Set voice master volume
// volume: Master volume (0.0 to 1.0)
ST_API void st_voice_set_master_volume(float volume);

// Get voice master volume
// Returns: Master volume (0.0 to 1.0)
ST_API float st_voice_get_master_volume(void);

// Reset all voices (gate off, clear state)
ST_API void st_voice_reset_all(void);

// Wait for N beats (advances beat cursor in timeline mode)
ST_API void st_voice_wait(float beats);

// VOICES Timeline System - Record and render voice sequences
ST_API void st_voices_start(void);
ST_API void st_voices_set_tempo(float bpm);
ST_API void st_voices_end_slot(int slot, float volume);
ST_API uint32_t st_voices_next_slot(float volume);
ST_API void st_voices_end_play(void);
ST_API void st_voices_end_save(const char* filename);

// Get number of active voices (gate on)
// Returns: Number of active voices
ST_API int st_voice_get_active_count(void);

// Check if VOICES_END_PLAY is currently playing
// Returns: 1 if voices playback buffer is currently playing, 0 otherwise
ST_API int st_voices_are_playing(void);

// Direct voice output to WAV file or live playback
// @param destination WAV filename for rendering, or empty string for live playback
ST_API void st_voice_direct(const char* destination);

// Direct voice output to sound slot (render audio to memory)
// @param slotNum Sound slot number (1-based)
// @param volume Playback volume (0.0 to 1.0)
// @param duration Duration in seconds (0 = auto-detect from gate off)
// Returns: Sound ID that was stored in the slot
ST_API uint32_t st_voice_direct_slot(int slotNum, float volume, float duration);

// =============================================================================
// VoiceScript WAV Export
// =============================================================================

// Render a VoiceScript to WAV file and save to cart
// @param scriptName Name of the VoiceScript to render
// @param assetName Asset name for the WAV file (without extension)
// @param duration Duration in seconds (0 = auto-detect from script)
ST_API void st_music_save_to_wav(const char* scriptName, const char* assetName, float duration);

// Render a VoiceScript and save to sound bank
// @param scriptName Name of the VoiceScript to render
// @param duration Duration in seconds (0 = auto-detect from script)
// @return Sound ID (> 0 on success, 0 on error)
ST_API uint32_t st_vscript_save_to_bank(const char* scriptName, float duration);

// =============================================================================
// Voice Controller Extended API - Stereo & Spatial
// =============================================================================

// Set voice stereo pan position
// voiceNum: Voice number (1-based, 1-8)
// pan: Pan position (-1.0 = left, 0.0 = center, 1.0 = right)
ST_API void st_voice_set_pan(int voiceNum, float pan);

// =============================================================================
// Voice Controller Extended API - SID-Style Modulation
// =============================================================================

// Enable ring modulation from source voice
// voiceNum: Voice number (1-based, 1-8)
// sourceVoice: Source voice for modulation (1-8, 0 = off)
ST_API void st_voice_set_ring_mod(int voiceNum, int sourceVoice);

// Enable hard sync from source voice
// voiceNum: Voice number (1-based, 1-8)
// sourceVoice: Source voice for sync (1-8, 0 = off)
ST_API void st_voice_set_sync(int voiceNum, int sourceVoice);

// Set portamento (pitch glide) time
// voiceNum: Voice number (1-based, 1-8)
// time: Glide time in seconds
ST_API void st_voice_set_portamento(int voiceNum, float time);

// Set voice detuning in cents
// voiceNum: Voice number (1-based, 1-8)
// cents: Detuning in cents (-100 to +100)
ST_API void st_voice_set_detune(int voiceNum, float cents);

// =============================================================================
// Voice Controller Extended API - Delay Effects
// =============================================================================

// Enable/disable delay effect for voice
// voiceNum: Voice number (1-based, 1-8)
// enabled: Enable state (0 = off, 1 = on)
ST_API void st_voice_set_delay_enable(int voiceNum, int enabled);

// Set delay time
// voiceNum: Voice number (1-based, 1-8)
// time: Delay time in seconds
ST_API void st_voice_set_delay_time(int voiceNum, float time);

// Set delay feedback amount
// voiceNum: Voice number (1-based, 1-8)
// feedback: Feedback (0.0 to 0.95)
ST_API void st_voice_set_delay_feedback(int voiceNum, float feedback);

// Set delay wet/dry mix
// voiceNum: Voice number (1-based, 1-8)
// mix: Wet mix (0.0 = dry, 1.0 = wet)
ST_API void st_voice_set_delay_mix(int voiceNum, float mix);

// =============================================================================
// Voice Controller Extended API - LFO Controls
// =============================================================================

// Set LFO waveform type
// lfoNum: LFO number (1-based, 1-4)
// waveform: Waveform type (0=sine, 1=triangle, 2=square, 3=sawtooth, 4=random)
ST_API void st_lfo_set_waveform(int lfoNum, int waveform);

// Set LFO rate in Hz
// lfoNum: LFO number (1-based, 1-4)
// rateHz: Rate in Hz
ST_API void st_lfo_set_rate(int lfoNum, float rateHz);

// Reset LFO phase to start
// lfoNum: LFO number (1-based, 1-4)
ST_API void st_lfo_reset(int lfoNum);

// Route LFO to pitch (vibrato)
// voiceNum: Voice number (1-based, 1-8)
// lfoNum: LFO number (1-4, 0 = off)
// depthCents: Modulation depth in cents
ST_API void st_lfo_to_pitch(int voiceNum, int lfoNum, float depthCents);

// Route LFO to volume (tremolo)
// voiceNum: Voice number (1-based, 1-8)
// lfoNum: LFO number (1-4, 0 = off)
// depth: Modulation depth (0.0 to 1.0)
ST_API void st_lfo_to_volume(int voiceNum, int lfoNum, float depth);

// Route LFO to filter cutoff (auto-wah)
// voiceNum: Voice number (1-based, 1-8)
// lfoNum: LFO number (1-4, 0 = off)
// depthHz: Modulation depth in Hz
ST_API void st_lfo_to_filter(int voiceNum, int lfoNum, float depthHz);

// Route LFO to pulse width (auto-PWM)
// voiceNum: Voice number (1-based, 1-8)
// lfoNum: LFO number (1-4, 0 = off)
// depth: Modulation depth (0.0 to 1.0)
ST_API void st_lfo_to_pulsewidth(int voiceNum, int lfoNum, float depth);

// =============================================================================
// Voice Controller Extended API - Physical Modeling
// =============================================================================

// Set physical modeling type
// voiceNum: Voice number (1-based, 1-8)
// modelType: Model type (0=plucked_string, 1=struck_bar, 2=blown_tube, 3=drumhead, 4=glass)
ST_API void st_voice_set_physical_model(int voiceNum, int modelType);

// Set physical model damping
// voiceNum: Voice number (1-based, 1-8)
// damping: Damping (0.0 = none, 1.0 = max)
ST_API void st_voice_set_physical_damping(int voiceNum, float damping);

// Set physical model brightness
// voiceNum: Voice number (1-based, 1-8)
// brightness: Brightness (0.0 = dark, 1.0 = bright)
ST_API void st_voice_set_physical_brightness(int voiceNum, float brightness);

// Set physical model excitation strength
// voiceNum: Voice number (1-based, 1-8)
// excitation: Excitation (0.0 = gentle, 1.0 = violent)
ST_API void st_voice_set_physical_excitation(int voiceNum, float excitation);

// Set physical model resonance
// voiceNum: Voice number (1-based, 1-8)
// resonance: Resonance (0.0 = damped, 1.0 = max)
ST_API void st_voice_set_physical_resonance(int voiceNum, float resonance);

// Set string tension (string models only)
// voiceNum: Voice number (1-based, 1-8)
// tension: Tension (0.0 = loose, 1.0 = tight)
ST_API void st_voice_set_physical_tension(int voiceNum, float tension);

// Set air pressure (wind models only)
// voiceNum: Voice number (1-based, 1-8)
// pressure: Pressure (0.0 = gentle, 1.0 = strong)
ST_API void st_voice_set_physical_pressure(int voiceNum, float pressure);

// Trigger physical model excitation
// voiceNum: Voice number (1-based, 1-8)
ST_API void st_voice_physical_trigger(int voiceNum);

// =============================================================================
// Input API - Keyboard
// =============================================================================

// Check if key is currently pressed
ST_API bool st_key_pressed(STKeyCode key);

// Check if key was just pressed this frame
ST_API bool st_key_just_pressed(STKeyCode key);

// Check if key was just released this frame
ST_API bool st_key_just_released(STKeyCode key);

// Get next character from input buffer (returns 0 if empty)
ST_API uint32_t st_key_get_char(void);

// Clear input buffer
ST_API void st_key_clear_buffer(void);

// Clear all key states (useful when switching modes to prevent stuck keys)
ST_API void st_key_clear_all(void);

// =============================================================================
// Input API - Mouse
// =============================================================================

// Get mouse position in pixels
ST_API void st_mouse_position(int* x, int* y);

// Get mouse position in grid coordinates
ST_API void st_mouse_grid_position(int* x, int* y);

// Check if mouse button is pressed
ST_API bool st_mouse_button(STMouseButton button);

// Check if mouse button was just pressed this frame
ST_API bool st_mouse_button_just_pressed(STMouseButton button);

// Check if mouse button was just released this frame
ST_API bool st_mouse_button_just_released(STMouseButton button);

// Get mouse wheel delta
ST_API void st_mouse_wheel(float* dx, float* dy);

// =============================================================================
// Asset API
// =============================================================================
// Asset management system with SQLite-backed storage, caching, and lifecycle
// management. Supports images, sounds, music, fonts, sprites, and data files.
//
// Usage:
// 1. Initialize asset database with st_asset_init()
// 2. Import assets with st_asset_import() or st_asset_import_directory()
// 3. Load assets with st_asset_load() - returns handle
// 4. Access asset data with st_asset_get_data() and st_asset_get_size()
// 5. Unload with st_asset_unload() when done
// 6. Shutdown with st_asset_shutdown()
//
// Thread Safety: All functions are thread-safe
// =============================================================================

// === INITIALIZATION ===

/// Initialize asset manager with database file
/// @param db_path Path to SQLite database file (will be created if doesn't exist)
/// @param max_cache_size Maximum cache size in bytes (0 = default 100MB)
/// @return true if successful
ST_API bool st_asset_init(const char* db_path, size_t max_cache_size);

/// Shutdown asset manager and close database
ST_API void st_asset_shutdown(void);

/// Check if asset manager is initialized
/// @return true if initialized
ST_API bool st_asset_is_initialized(void);

// === LOADING / UNLOADING ===

/// Load asset by name (from database)
/// @param name Asset name
/// @return Asset handle or -1 on error
ST_API STAssetID st_asset_load(const char* name);

/// Load asset from file (legacy - use st_asset_import + st_asset_load instead)
/// @param path File path
/// @param type Asset type
/// @return Asset handle or -1 on error
ST_API STAssetID st_asset_load_file(const char* path, STAssetType type);

/// Load built-in asset
/// @param name Built-in asset name
/// @param type Asset type
/// @return Asset handle or -1 on error
ST_API STAssetID st_asset_load_builtin(const char* name, STAssetType type);

/// Unload asset (decrements reference count)
/// @param asset Asset handle
ST_API void st_asset_unload(STAssetID asset);

/// Check if asset is loaded in cache
/// @param name Asset name
/// @return true if loaded
ST_API bool st_asset_is_loaded(const char* name);

// === IMPORT / EXPORT ===

/// Import asset from file into database
/// @param file_path Path to file on disk
/// @param asset_name Name to give the asset in database
/// @param type Asset type (use -1 for auto-detect)
/// @return true if successful
ST_API bool st_asset_import(const char* file_path, const char* asset_name, int type);

/// Import all assets from directory
/// @param directory Directory path
/// @param recursive Include subdirectories
/// @return Number of assets imported, or -1 on error
ST_API int st_asset_import_directory(const char* directory, bool recursive);

/// Export asset to file
/// @param asset_name Asset name
/// @param file_path Output file path
/// @return true if successful
ST_API bool st_asset_export(const char* asset_name, const char* file_path);

/// Delete asset from database
/// @param asset_name Asset name
/// @return true if successful
ST_API bool st_asset_delete(const char* asset_name);

// === DATA ACCESS ===

/// Get asset data pointer (valid while asset is loaded)
/// @param asset Asset handle
/// @return Pointer to asset data or NULL on error
ST_API const void* st_asset_get_data(STAssetID asset);

/// Get asset data size in bytes
/// @param asset Asset handle
/// @return Size in bytes or 0 on error
ST_API size_t st_asset_get_size(STAssetID asset);

/// Get asset type
/// @param asset Asset handle
/// @return Asset type or -1 on error
ST_API int st_asset_get_type(STAssetID asset);

/// Get asset name from handle
/// @param asset Asset handle
/// @return Asset name or NULL on error (string valid until next API call)
ST_API const char* st_asset_get_name(STAssetID asset);

// === QUERIES ===

/// Check if asset exists in database
/// @param name Asset name
/// @return true if exists
ST_API bool st_asset_exists(const char* name);

/// List all assets of a specific type
/// @param type Asset type (-1 for all types)
/// @param names Output array for asset names (can be NULL to get count)
/// @param max_count Maximum number of names to return
/// @return Total count of matching assets
ST_API int st_asset_list(int type, const char** names, int max_count);

/// List built-in assets of a type (returns count, fills array if provided)
ST_API int st_asset_list_builtin(STAssetType type, const char** names, int max_count);

/// Search assets by name pattern (SQL LIKE syntax: % = wildcard)
/// @param pattern Search pattern (e.g., "player%", "%coin%")
/// @param names Output array for asset names (can be NULL to get count)
/// @param max_count Maximum number of names to return
/// @return Total count of matching assets
ST_API int st_asset_search(const char* pattern, const char** names, int max_count);

/// Get total asset count
/// @param type Asset type (-1 for all types)
/// @return Number of assets
ST_API int st_asset_get_count(int type);

// === CACHE MANAGEMENT ===

/// Clear asset cache (unloads all assets)
ST_API void st_asset_clear_cache(void);

/// Get current cache size in bytes
/// @return Cache size in bytes
ST_API size_t st_asset_get_cache_size(void);

/// Get number of assets in cache
/// @return Number of cached assets
ST_API int st_asset_get_cached_count(void);

/// Set maximum cache size
/// @param max_size Maximum size in bytes
ST_API void st_asset_set_max_cache_size(size_t max_size);

// === STATISTICS ===

/// Get cache hit rate (0.0 to 1.0)
/// @return Hit rate as percentage
ST_API double st_asset_get_hit_rate(void);

/// Get total database size in bytes
/// @return Database size in bytes
ST_API size_t st_asset_get_database_size(void);

// === ERROR HANDLING ===

/// Get last asset error message
/// @return Error message or NULL if no error
ST_API const char* st_asset_get_error(void);

/// Clear last asset error
ST_API void st_asset_clear_error(void);

// =============================================================================
// Tilemap API
// =============================================================================
// Multi-layer tilemap system with parallax scrolling, camera control, and
// efficient tile-based rendering.
//
// Usage:
// 1. Initialize with st_tilemap_init()
// 2. Create tilemap with st_tilemap_create()
// 3. Create layer with st_tilemap_create_layer()
// 4. Assign tilemap to layer with st_tilemap_layer_set_tilemap()
// 5. Set tiles with st_tilemap_set_tile()
// 6. Control camera with st_tilemap_set_camera()
// 7. Update with st_tilemap_update()
//
// Thread Safety: All functions are thread-safe
// =============================================================================

// === INITIALIZATION ===

/// Initialize tilemap system
/// @param viewportWidth Viewport width in pixels
/// @param viewportHeight Viewport height in pixels
/// @return true if successful
ST_API bool st_tilemap_init(float viewportWidth, float viewportHeight);

/// Shutdown tilemap system
ST_API void st_tilemap_shutdown(void);

// === TILEMAP MANAGEMENT ===

/// Create tilemap
/// @param width Width in tiles
/// @param height Height in tiles
/// @param tileWidth Tile width in pixels
/// @param tileHeight Tile height in pixels
/// @return Tilemap ID or -1 on error
ST_API STTilemapID st_tilemap_create(int32_t width, int32_t height,
                                      int32_t tileWidth, int32_t tileHeight);

/// Destroy tilemap
/// @param tilemap Tilemap ID
ST_API void st_tilemap_destroy(STTilemapID tilemap);

/// Get tilemap size
/// @param tilemap Tilemap ID
/// @param width Output width in tiles
/// @param height Output height in tiles
ST_API void st_tilemap_get_size(STTilemapID tilemap, int32_t* width, int32_t* height);

// === FILE I/O ===

/// Load tilemap from file
/// @param filePath File path (.stmap, .json, .csv)
/// @param outLayerIDs Output array for layer IDs (can be NULL)
/// @param maxLayers Maximum number of layer IDs to write
/// @param outLayerCount Output number of layers created
/// @return Tilemap ID or -1 on error
ST_API STTilemapID st_tilemap_load_file(const char* filePath, STLayerID* outLayerIDs,
                                         int32_t maxLayers, int32_t* outLayerCount);

/// Save tilemap to file
/// @param tilemap Tilemap ID
/// @param filePath File path (.stmap, .json, .csv)
/// @param layerIDs Array of layer IDs to save (NULL = all layers)
/// @param layerCount Number of layers in array
/// @param saveCamera Whether to save camera state
/// @return true on success
ST_API bool st_tilemap_save_file(STTilemapID tilemap, const char* filePath,
                                  const STLayerID* layerIDs, int32_t layerCount,
                                  bool saveCamera);

/// Load tilemap from asset database
/// @param assetName Asset name
/// @return Tilemap ID or -1 on error
ST_API STTilemapID st_tilemap_load_asset(const char* assetName);

/// Save tilemap to asset database
/// @param tilemap Tilemap ID
/// @param assetName Asset name
/// @param layerIDs Array of layer IDs to save (NULL = all layers)
/// @param layerCount Number of layers in array
/// @param saveCamera Whether to save camera state
/// @return true on success
ST_API bool st_tilemap_save_asset(STTilemapID tilemap, const char* assetName,
                                   const STLayerID* layerIDs, int32_t layerCount,
                                   bool saveCamera);

// === TILESET MANAGEMENT ===

/// Load tileset from image file
/// @param imagePath Path to image file (PNG, JPG, etc.)
/// @param tileWidth Tile width in pixels
/// @param tileHeight Tile height in pixels
/// @param margin Margin around entire tileset in pixels (default 0)
/// @param spacing Spacing between tiles in pixels (default 0)
/// @return Tileset ID or -1 on error
ST_API STTilesetID st_tileset_load(const char* imagePath,
                                    int32_t tileWidth, int32_t tileHeight,
                                    int32_t margin, int32_t spacing);

/// Load tileset from asset database
/// @param assetName Asset name
/// @param tileWidth Tile width in pixels
/// @param tileHeight Tile height in pixels
/// @param margin Margin around entire tileset in pixels (default 0)
/// @param spacing Spacing between tiles in pixels (default 0)
/// @return Tileset ID or -1 on error
ST_API STTilesetID st_tileset_load_asset(const char* assetName,
                                          int32_t tileWidth, int32_t tileHeight,
                                          int32_t margin, int32_t spacing);

/// Destroy tileset
/// @param tileset Tileset ID
ST_API void st_tileset_destroy(STTilesetID tileset);

/// Get tileset tile count
/// @param tileset Tileset ID
/// @return Number of tiles in tileset or 0 on error
ST_API int32_t st_tileset_get_tile_count(STTilesetID tileset);

/// Get tileset dimensions (columns and rows)
/// @param tileset Tileset ID
/// @param columns Output columns
/// @param rows Output rows
ST_API void st_tileset_get_dimensions(STTilesetID tileset, int32_t* columns, int32_t* rows);

// === LAYER MANAGEMENT ===

/// Create layer
/// @param name Layer name (can be NULL)
/// @return Layer ID or -1 on error
ST_API STLayerID st_tilemap_create_layer(const char* name);

/// Destroy layer
/// @param layer Layer ID
ST_API void st_tilemap_destroy_layer(STLayerID layer);

/// Assign tilemap to layer
/// @param layer Layer ID
/// @param tilemap Tilemap ID
ST_API void st_tilemap_layer_set_tilemap(STLayerID layer, STTilemapID tilemap);

/// Assign tileset to layer
/// @param layer Layer ID
/// @param tileset Tileset ID
ST_API void st_tilemap_layer_set_tileset(STLayerID layer, STTilesetID tileset);

/// Set layer parallax factor
/// @param layer Layer ID
/// @param parallaxX Horizontal parallax (0.0 = static, 1.0 = normal)
/// @param parallaxY Vertical parallax
ST_API void st_tilemap_layer_set_parallax(STLayerID layer, float parallaxX, float parallaxY);

/// Set layer opacity
/// @param layer Layer ID
/// @param opacity Opacity (0.0 = transparent, 1.0 = opaque)
ST_API void st_tilemap_layer_set_opacity(STLayerID layer, float opacity);

/// Set layer visibility
/// @param layer Layer ID
/// @param visible Visibility flag
ST_API void st_tilemap_layer_set_visible(STLayerID layer, bool visible);

/// Set layer Z-order (rendering order)
/// @param layer Layer ID
/// @param zOrder Z-order (lower = back)
ST_API void st_tilemap_layer_set_z_order(STLayerID layer, int32_t zOrder);

/// Set layer auto-scroll
/// @param layer Layer ID
/// @param scrollX Horizontal scroll speed (pixels/second)
/// @param scrollY Vertical scroll speed (pixels/second)
ST_API void st_tilemap_layer_set_auto_scroll(STLayerID layer, float scrollX, float scrollY);

// === TILE MANIPULATION ===

/// Set tile at position
/// @param layer Layer ID
/// @param x Tile X coordinate
/// @param y Tile Y coordinate
/// @param tileID Tile ID from tileset
ST_API void st_tilemap_set_tile(STLayerID layer, int32_t x, int32_t y, uint16_t tileID);

/// Get tile at position
/// @param layer Layer ID
/// @param x Tile X coordinate
/// @param y Tile Y coordinate
/// @return Tile ID or 0 if empty
ST_API uint16_t st_tilemap_get_tile(STLayerID layer, int32_t x, int32_t y);

/// Fill rectangle with tile
/// @param layer Layer ID
/// @param x Starting X coordinate
/// @param y Starting Y coordinate
/// @param width Width in tiles
/// @param height Height in tiles
/// @param tileID Tile ID to fill with
ST_API void st_tilemap_fill_rect(STLayerID layer, int32_t x, int32_t y,
                                   int32_t width, int32_t height, uint16_t tileID);

/// Clear layer (set all tiles to 0)
/// @param layer Layer ID
ST_API void st_tilemap_clear(STLayerID layer);

// === CAMERA CONTROL ===

/// Set camera position
/// @param x X position in world coordinates
/// @param y Y position in world coordinates
ST_API void st_tilemap_set_camera(float x, float y);

/// Move camera by offset
/// @param dx Delta X
/// @param dy Delta Y
ST_API void st_tilemap_move_camera(float dx, float dy);

/// Get camera position
/// @param x Output X position
/// @param y Output Y position
ST_API void st_tilemap_get_camera(float* x, float* y);

/// Set camera zoom
/// @param zoom Zoom level (1.0 = 100%, 2.0 = 200%)
ST_API void st_tilemap_set_zoom(float zoom);

/// Camera follow target with smoothing
/// @param targetX Target X position
/// @param targetY Target Y position
/// @param smoothness Smoothing factor (0.0 = instant, 1.0 = very smooth)
ST_API void st_tilemap_camera_follow(float targetX, float targetY, float smoothness);

/// Set camera bounds
/// @param x Bounds X
/// @param y Bounds Y
/// @param width Bounds width
/// @param height Bounds height
ST_API void st_tilemap_set_camera_bounds(float x, float y, float width, float height);

/// Camera shake effect
/// @param magnitude Shake magnitude in pixels
/// @param duration Shake duration in seconds
ST_API void st_tilemap_camera_shake(float magnitude, float duration);

// === UPDATE ===

/// Update tilemap system (animations, camera, auto-scroll)
/// @param dt Delta time in seconds
ST_API void st_tilemap_update(float dt);

// === COORDINATE CONVERSION ===

/// Convert world coordinates to tile coordinates
/// @param layer Layer ID
/// @param worldX World X position
/// @param worldY World Y position
/// @param tileX Output tile X
/// @param tileY Output tile Y
ST_API void st_tilemap_world_to_tile(STLayerID layer, float worldX, float worldY,
                                      int32_t* tileX, int32_t* tileY);

/// Convert tile coordinates to world coordinates
/// @param layer Layer ID
/// @param tileX Tile X coordinate
/// @param tileY Tile Y coordinate
/// @param worldX Output world X
/// @param worldY Output world Y
ST_API void st_tilemap_tile_to_world(STLayerID layer, int32_t tileX, int32_t tileY,
                                      float* worldX, float* worldY);

// =============================================================================
// Frame Control API
// =============================================================================

// Wait for next frame (60 FPS vsync)
ST_API void st_wait_frame(void);

// Wait for multiple frames
ST_API void st_wait_frames(int count);

// Wait for milliseconds (with script cancellation support)
ST_API void st_wait_ms(int milliseconds);

// Get current frame count
ST_API uint64_t st_frame_count(void);

// Get elapsed time in seconds
ST_API double st_time(void);

// Get delta time (time since last frame)
ST_API double st_delta_time(void);

// =============================================================================
// Random Number Generation API
// =============================================================================

// Get random float between 0.0 and 1.0
ST_API double st_random(void);

// Get random integer between min and max (inclusive)
ST_API int st_random_int(int min, int max);

// Seed the random number generator
ST_API void st_random_seed(uint32_t seed);

// =============================================================================
// Utility API
// =============================================================================

// Create color from RGB (0-255)
ST_API STColor st_rgb(uint8_t r, uint8_t g, uint8_t b);

// Create color from RGBA (0-255)
ST_API STColor st_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// Create color from HSV (h: 0-360, s: 0-1, v: 0-1)
ST_API STColor st_hsv(float h, float s, float v);

// Extract color components
ST_API void st_color_components(STColor color, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);

// Get API version
ST_API void st_version(int* major, int* minor, int* patch);

// Get version string
ST_API const char* st_version_string(void);

// Get elapsed time in seconds since app start
// Returns: Time in seconds (double precision)
ST_API double st_timer(void);

// =============================================================================
// Rectangle Rendering API
// =============================================================================

// Rectangle gradient modes and procedural patterns
typedef enum {
    ST_GRADIENT_SOLID = 0,              // Single solid color
    ST_GRADIENT_HORIZONTAL = 1,         // Left to right (2 colors)
    ST_GRADIENT_VERTICAL = 2,           // Top to bottom (2 colors)
    ST_GRADIENT_DIAGONAL_TL_BR = 3,     // Top-left to bottom-right (2 colors)
    ST_GRADIENT_DIAGONAL_TR_BL = 4,     // Top-right to bottom-left (2 colors)
    ST_GRADIENT_RADIAL = 5,             // Center outward (2 colors)
    ST_GRADIENT_FOUR_CORNER = 6,        // Each corner different (4 colors)
    ST_GRADIENT_THREE_POINT = 7,        // Three-point gradient (3 colors)
    
    // Procedural patterns (starting at 100)
    ST_PATTERN_OUTLINE = 100,           // Outlined rectangle (color1=fill, color2=outline)
    ST_PATTERN_DASHED_OUTLINE = 101,    // Dashed outline (color1=fill, color2=outline)
    ST_PATTERN_HORIZONTAL_STRIPES = 102,// Horizontal stripes (color1, color2 alternate)
    ST_PATTERN_VERTICAL_STRIPES = 103,  // Vertical stripes (color1, color2 alternate)
    ST_PATTERN_DIAGONAL_STRIPES = 104,  // Diagonal stripes
    ST_PATTERN_CHECKERBOARD = 105,      // Checkerboard pattern
    ST_PATTERN_DOTS = 106,              // Dot pattern
    ST_PATTERN_CROSSHATCH = 107,        // Crosshatch pattern
    ST_PATTERN_ROUNDED_CORNERS = 108,   // Rounded corner rectangle
    ST_PATTERN_GRID = 109               // Grid pattern
} STRectangleGradientMode;

// ID-Based Rectangle Management (persistent, updatable)
ST_API int st_rect_create(float x, float y, float width, float height, uint32_t color);
ST_API int st_rect_create_gradient(float x, float y, float width, float height,
                                     uint32_t color1, uint32_t color2,
                                     STRectangleGradientMode mode);
ST_API int st_rect_create_three_point(float x, float y, float width, float height,
                                        uint32_t color1, uint32_t color2, uint32_t color3,
                                        STRectangleGradientMode mode);
ST_API int st_rect_create_four_corner(float x, float y, float width, float height,
                                        uint32_t topLeft, uint32_t topRight,
                                        uint32_t bottomRight, uint32_t bottomLeft);

// Update existing rectangles by ID
ST_API bool st_rect_set_position(int id, float x, float y);
ST_API bool st_rect_set_size(int id, float width, float height);
ST_API bool st_rect_set_color(int id, uint32_t color);
ST_API bool st_rect_set_colors(int id, uint32_t color1, uint32_t color2, 
                                 uint32_t color3, uint32_t color4);
ST_API bool st_rect_set_mode(int id, STRectangleGradientMode mode);
ST_API bool st_rect_set_visible(int id, bool visible);

// Query rectangles
ST_API bool st_rect_exists(int id);
ST_API bool st_rect_is_visible(int id);

// Delete rectangles
ST_API bool st_rect_delete(int id);
ST_API void st_rect_delete_all(void);

// Legacy Queue-Based Rectangle API (backward compatibility)
ST_API void st_rect_add(float x, float y, float width, float height, uint32_t color);
ST_API void st_rect_add_gradient(float x, float y, float width, float height,
                                   uint32_t color1, uint32_t color2,
                                   STRectangleGradientMode mode);
ST_API void st_rect_add_three_point(float x, float y, float width, float height,
                                      uint32_t color1, uint32_t color2, uint32_t color3,
                                      STRectangleGradientMode mode);
ST_API void st_rect_add_four_corner(float x, float y, float width, float height,
                                      uint32_t topLeft, uint32_t topRight,
                                      uint32_t bottomRight, uint32_t bottomLeft);
ST_API void st_rect_clear(void);
ST_API size_t st_rect_count(void);
ST_API bool st_rect_is_empty(void);
ST_API void st_rect_set_max(size_t max);
ST_API size_t st_rect_get_max(void);

// =============================================================================
// Circle System API
// =============================================================================

// Circle gradient modes
typedef enum {
    ST_CIRCLE_SOLID = 0,              // Single solid color
    ST_CIRCLE_RADIAL = 1,             // Center to edge (2 colors)
    ST_CIRCLE_RADIAL_3 = 2,           // Three-ring radial gradient
    ST_CIRCLE_RADIAL_4 = 3,           // Four-ring radial gradient
    
    // Advanced patterns (starting at 100)
    ST_CIRCLE_OUTLINE = 100,          // Outlined circle
    ST_CIRCLE_DASHED_OUTLINE = 101,   // Dashed outline
    ST_CIRCLE_RING = 102,             // Hollow ring
    ST_CIRCLE_PIE = 103,              // Pie slice
    ST_CIRCLE_ARC = 104,              // Arc segment
    ST_CIRCLE_DOTS_RING = 105,        // Dots arranged in a ring
    ST_CIRCLE_STAR_BURST = 106        // Star burst pattern
} STCircleGradientMode;

// ID-Based Circle Management (persistent, updatable)
ST_API int st_circle_create(float x, float y, float radius, uint32_t color);
ST_API int st_circle_create_radial(float x, float y, float radius,
                                    uint32_t centerColor, uint32_t edgeColor);
ST_API int st_circle_create_radial_3(float x, float y, float radius,
                                      uint32_t color1, uint32_t color2, uint32_t color3);
ST_API int st_circle_create_radial_4(float x, float y, float radius,
                                      uint32_t color1, uint32_t color2,
                                      uint32_t color3, uint32_t color4);

// Procedural Pattern Creation
ST_API int st_circle_create_outline(float x, float y, float radius,
                                     uint32_t fillColor, uint32_t outlineColor, float lineWidth);
ST_API int st_circle_create_dashed_outline(float x, float y, float radius,
                                            uint32_t fillColor, uint32_t outlineColor,
                                            float lineWidth, float dashLength);
ST_API int st_circle_create_ring(float x, float y, float outerRadius, float innerRadius,
                                  uint32_t color);
ST_API int st_circle_create_pie_slice(float x, float y, float radius,
                                       float startAngle, float endAngle, uint32_t color);
ST_API int st_circle_create_arc(float x, float y, float radius,
                                 float startAngle, float endAngle,
                                 uint32_t color, float lineWidth);
ST_API int st_circle_create_dots_ring(float x, float y, float radius,
                                       uint32_t dotColor, uint32_t backgroundColor,
                                       float dotRadius, int numDots);
ST_API int st_circle_create_star_burst(float x, float y, float radius,
                                        uint32_t color1, uint32_t color2, int numRays);

// Circle Update API
ST_API bool st_circle_set_position(int id, float x, float y);
ST_API bool st_circle_set_radius(int id, float radius);
ST_API bool st_circle_set_color(int id, uint32_t color);
ST_API bool st_circle_set_colors(int id, uint32_t color1, uint32_t color2,
                                  uint32_t color3, uint32_t color4);
ST_API bool st_circle_set_parameters(int id, float param1, float param2, float param3);
ST_API bool st_circle_set_visible(int id, bool visible);

// Circle Query/Management
ST_API bool st_circle_exists(int id);
ST_API bool st_circle_is_visible(int id);
// Management
ST_API bool st_rect_delete(int id);
ST_API void st_rect_delete_all(void);
ST_API size_t st_rect_count(void);
ST_API bool st_rect_is_empty(void);
ST_API void st_rect_set_max(size_t max);
ST_API size_t st_rect_get_max(void);

// Rotation
ST_API bool st_rect_set_rotation(int id, float angleDegrees);

// =============================================================================
// Line API - ID-Based GPU-Accelerated Line Management
// =============================================================================

// Line Creation (returns line ID)
ST_API int st_line_create(float x1, float y1, float x2, float y2, uint32_t color, float thickness);
ST_API int st_line_create_gradient(float x1, float y1, float x2, float y2,
                                    uint32_t color1, uint32_t color2, float thickness);
ST_API int st_line_create_dashed(float x1, float y1, float x2, float y2,
                                  uint32_t color, float thickness,
                                  float dashLength, float gapLength);
ST_API int st_line_create_dotted(float x1, float y1, float x2, float y2,
                                  uint32_t color, float thickness,
                                  float dotSpacing);

// Line Updates
ST_API bool st_line_set_endpoints(int id, float x1, float y1, float x2, float y2);
ST_API bool st_line_set_thickness(int id, float thickness);
ST_API bool st_line_set_color(int id, uint32_t color);
ST_API bool st_line_set_colors(int id, uint32_t color1, uint32_t color2);
ST_API bool st_line_set_dash_pattern(int id, float dashLength, float gapLength);
ST_API bool st_line_set_visible(int id, bool visible);

// Line Query/Management
ST_API bool st_line_exists(int id);
ST_API bool st_line_is_visible(int id);
ST_API bool st_line_delete(int id);
ST_API void st_line_delete_all(void);
ST_API size_t st_line_count(void);
ST_API bool st_line_is_empty(void);
ST_API void st_line_set_max(size_t max);
ST_API size_t st_line_get_max(void);



// =============================================================================
// Star Shape API - GPU-Accelerated Star Rendering
// =============================================================================

// Star gradient modes
typedef enum {
    ST_STAR_SOLID = 0,                // Single solid color
    ST_STAR_RADIAL = 1,               // Center to points gradient
    ST_STAR_ALTERNATING = 2,          // Alternating colors per point
    ST_STAR_OUTLINE = 100,            // Outlined star
    ST_STAR_DASHED_OUTLINE = 101,     // Dashed outline
} STStarGradientMode;

// Star Creation (returns star ID)
// numPoints: 3 to 12 points (default 5)
// innerRadius: ratio of inner to outer radius (0.0 to 1.0, default 0.382 for classic 5-point star)
ST_API int st_star_create(float x, float y, float outerRadius, int numPoints, uint32_t color);
ST_API int st_star_create_custom(float x, float y, float outerRadius, float innerRadius,
                                  int numPoints, uint32_t color);
ST_API int st_star_create_gradient(float x, float y, float outerRadius, int numPoints,
                                    uint32_t color1, uint32_t color2,
                                    STStarGradientMode mode);
ST_API int st_star_create_outline(float x, float y, float outerRadius, int numPoints,
                                   uint32_t fillColor, uint32_t outlineColor,
                                   float lineWidth);

// Star Updates
ST_API bool st_star_set_position(int id, float x, float y);
ST_API bool st_star_set_radius(int id, float outerRadius);
ST_API bool st_star_set_radii(int id, float outerRadius, float innerRadius);
ST_API bool st_star_set_points(int id, int numPoints);
ST_API bool st_star_set_color(int id, uint32_t color);
ST_API bool st_star_set_colors(int id, uint32_t color1, uint32_t color2);
ST_API bool st_star_set_rotation(int id, float angleDegrees);
ST_API bool st_star_set_visible(int id, bool visible);

// Star Query/Management
ST_API bool st_star_exists(int id);
ST_API bool st_star_is_visible(int id);
ST_API bool st_star_delete(int id);
ST_API void st_star_delete_all(void);
ST_API size_t st_star_count(void);
ST_API bool st_star_is_empty(void);

// =============================================================================
// Particle System API
// =============================================================================

// Initialize particle system (called by framework)
ST_API bool st_particle_system_initialize(uint32_t maxParticles);

// Shutdown particle system (called by framework)
ST_API void st_particle_system_shutdown(void);

// Check if particle system is ready
ST_API bool st_particle_system_is_ready(void);

// Create sprite explosion at position (basic)
ST_API bool st_sprite_explode(float x, float y, uint16_t particleCount, STColor color);

// Create sprite explosion with advanced parameters
ST_API bool st_sprite_explode_advanced(float x, float y, uint16_t particleCount, STColor color,
                                       float force, float gravity, float fadeTime);

// Create directional sprite explosion
ST_API bool st_sprite_explode_directional(float x, float y, uint16_t particleCount, STColor color,
                                          float forceX, float forceY);

// Clear all particles
ST_API void st_particle_clear(void);

// Pause particle simulation
ST_API void st_particle_pause(void);

// Resume particle simulation
ST_API void st_particle_resume(void);

// Set time scale for particle simulation
ST_API void st_particle_set_time_scale(float scale);

// Set world bounds for particle culling
ST_API void st_particle_set_world_bounds(float width, float height);

// Enable or disable particle system
ST_API void st_particle_set_enabled(bool enabled);

// Get active particle count
ST_API uint32_t st_particle_get_active_count(void);

// Get total particles created
ST_API uint64_t st_particle_get_total_created(void);

// Dump particle system statistics
ST_API void st_particle_dump_stats(void);

// Update particle system (called by framework each frame)
ST_API void st_particle_system_update(float deltaTime);

// =============================================================================
// Voice Timeline API (for _AT commands with explicit beat positions)
// =============================================================================

// Set voice waveform at specific beat position
ST_API void st_voice_waveform_at(int voiceNum, float beat, int waveform);

// Set voice frequency at specific beat position
ST_API void st_voice_frequency_at(int voiceNum, float beat, float frequencyHz);

// Set voice envelope at specific beat position
ST_API void st_voice_envelope_at(int voiceNum, float beat, float attackMs, float decayMs, float sustainLevel, float releaseMs);

// Set voice gate at specific beat position
ST_API void st_voice_gate_at(int voiceNum, float beat, int gateOn);

// Set voice volume at specific beat position
ST_API void st_voice_volume_at(int voiceNum, float beat, float volume);

// Set voice pan at specific beat position
ST_API void st_voice_pan_at(int voiceNum, float beat, float pan);

// Set voice filter parameters at specific beat position
ST_API void st_voice_filter_at(int voiceNum, float beat, float cutoffHz, float resonance, int filterType);

// =============================================================================
// Debug API
// =============================================================================

// Print debug message to console
ST_API void st_debug_print(const char* message);

// Get last error message (returns NULL if no error)
ST_API const char* st_get_last_error(void);

// Clear last error
ST_API void st_clear_error(void);

// =============================================================================
// Unified Video Palette API
// =============================================================================
// Mode-agnostic palette functions that work across LORES/XRES/WRES
// See st_api_video_palette.h for full documentation

#include "st_api_video_palette.h"

// =============================================================================
// VIDEO MODE IMAGE LOADING/SAVING
// =============================================================================

/// Load image file into video buffer (XRES/WRES/PRES modes only)
/// @param filePath Path to PNG image file (must be 8-bit indexed)
/// @param bufferID Buffer ID (0-7)
/// @param destX Destination X coordinate in buffer
/// @param destY Destination Y coordinate in buffer
/// @param maxWidth Maximum width to copy (0 = entire image width)
/// @param maxHeight Maximum height to copy (0 = entire image height)
/// @return true on success, false on failure
/// @note Image must match current mode depth (8-bit indexed for XRES/WRES/PRES)
ST_API bool st_video_load_image(const char* filePath, int bufferID,
                                 int destX, int destY, int maxWidth, int maxHeight);

/// Save video buffer to PNG file (XRES/WRES/PRES modes only)
/// @param filePath Path to output PNG file
/// @param bufferID Buffer ID (0-7)
/// @return true on success, false on failure
/// @note Saves as 8-bit indexed PNG matching current mode resolution
ST_API bool st_video_save_image(const char* filePath, int bufferID);

/// Load palette from .pal file (XRES/WRES/PRES modes only)
/// @param filePath Path to .pal palette file
/// @return true on success, false on failure
/// @note Loads global palette colors 16-255 (240 colors)
ST_API bool st_video_load_palette_file(const char* filePath);

/// Save current palette to .pal file (XRES/WRES/PRES modes only)
/// @param filePath Path to output .pal file
/// @return true on success, false on failure
/// @note Saves global palette colors 16-255 (240 colors)
ST_API bool st_video_save_palette_file(const char* filePath);

#ifdef __cplusplus
}
#endif

#endif // SUPERTERMINAL_API_H
