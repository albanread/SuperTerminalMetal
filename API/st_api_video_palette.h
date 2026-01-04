//
// st_api_video_palette.h
// SuperTerminal v2 - Unified Video Palette API
//
// Provides a consistent, mode-agnostic palette API that works across
// LORES, XRES, and WRES video modes.
//
// THREAD SAFETY:
// - All functions are thread-safe via ST_LOCK macro
// - Safe to call from any thread after initialization
//
// COMPATIBILITY:
// - LORES: Full per-row palette (16 colors × 300 rows)
// - XRES:  Hybrid palette (16 per-row × 240 rows + 240 global)
// - WRES:  Hybrid palette (16 per-row × 240 rows + 240 global)
// - URES:  Direct color (no palette) - functions return errors
//

#ifndef ST_API_VIDEO_PALETTE_H
#define ST_API_VIDEO_PALETTE_H

#include <cstdint>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Palette Preset Types
// =============================================================================

/// Preset palette types for quick palette loading
typedef enum {
    ST_PALETTE_IBM_RGBI,        ///< IBM CGA/EGA 16-color RGBI palette
    ST_PALETTE_C64,             ///< Commodore 64 16-color palette
    ST_PALETTE_GRAYSCALE,       ///< 16-level grayscale palette
    ST_PALETTE_RGB_CUBE_6x8x5   ///< 6×8×5 RGB cube (240 colors for global palette)
} st_video_palette_preset_t;

// =============================================================================
// Palette Information Structure
// =============================================================================

/// Detailed palette information for current video mode
typedef struct {
    bool hasPalette;           ///< Does mode use a palette? (false for URES)
    bool hasPerRowPalette;     ///< Does mode support per-row colors? (true for LORES/XRES/WRES)
    int colorDepth;            ///< Total color indices (16=LORES, 256=XRES/WRES, 4096=URES)
    int perRowColorCount;      ///< Number of colors that can vary per row (0/16)
    int globalColorCount;      ///< Number of shared/global colors (0/240)
    int rowCount;              ///< Number of rows with palettes (0/240/300)
} st_video_palette_info_t;

// =============================================================================
// Query Functions
// =============================================================================

/// Check if current video mode uses a palette
/// @return true if mode uses palette (LORES/XRES/WRES), false otherwise (URES/TEXT)
/// @note Thread Safety: Safe to call from any thread
bool st_video_has_palette(void);

/// Check if current video mode supports per-row palette colors
/// @return true if per-row colors supported (LORES=all, XRES/WRES=indices 0-15)
/// @note Thread Safety: Safe to call from any thread
bool st_video_has_per_row_palette(void);

/// Get detailed palette information for current video mode
/// @param info Pointer to structure to fill with palette information
/// @note Thread Safety: Safe to call from any thread
/// @note info must not be NULL
void st_video_get_palette_info(st_video_palette_info_t* info);

// =============================================================================
// Set Palette Functions
// =============================================================================

/// Set a global palette color (or broadcast to all rows in LORES)
/// @param index Color index to set
/// @param r Red component (0-255)
/// @param g Green component (0-255)
/// @param b Blue component (0-255)
/// @note Thread Safety: Safe to call from any thread
/// @note LORES: Sets color for ALL rows (broadcast)
/// @note XRES/WRES: Sets global color (index must be 16-255)
/// @note URES: Returns error (no palette)
/// @note RGB values are clamped to 0-255
void st_video_set_palette(int index, int r, int g, int b);

/// Set a per-row palette color
/// @param row Row/scanline number
/// @param index Color index within per-row palette
/// @param r Red component (0-255)
/// @param g Green component (0-255)
/// @param b Blue component (0-255)
/// @note Thread Safety: Safe to call from any thread
/// @note LORES: row 0-299, index 0-15
/// @note XRES/WRES: row 0-239, index 0-15 only
/// @note URES: Returns error (no palette)
/// @note RGB values are clamped to 0-255
void st_video_set_palette_row(int row, int index, int r, int g, int b);

// =============================================================================
// Get Palette Functions
// =============================================================================

/// Get a global palette color
/// @param index Color index to retrieve
/// @return Color in ARGB format (0xAARRGGBB), or 0x00000000 on error
/// @note Thread Safety: Safe to call from any thread
/// @note LORES: Returns color from row 0
/// @note XRES/WRES: Returns global color (index 16-255)
/// @note URES: Returns 0 (no palette)
uint32_t st_video_get_palette(int index);

/// Get a per-row palette color
/// @param row Row/scanline number
/// @param index Color index within per-row palette
/// @return Color in ARGB format (0xAARRGGBB), or 0x00000000 on error
/// @note Thread Safety: Safe to call from any thread
/// @note LORES: row 0-299, index 0-15
/// @note XRES/WRES: row 0-239, index 0-15
/// @note URES: Returns 0 (no palette)
uint32_t st_video_get_palette_row(int row, int index);

// =============================================================================
// Batch Operations
// =============================================================================

/// Load entire palette from array
/// @param colors Array of colors in ARGB format (0xAARRGGBB)
/// @param count Number of colors in array
/// @note Thread Safety: Safe to call from any thread
/// @note LORES: Loads up to 16 colors, applies to all rows
/// @note XRES/WRES: Loads up to 256 colors (0-15 per-row, 16-255 global)
/// @note URES: Returns error (no palette)
/// @note Colors array must not be NULL
void st_video_load_palette(const uint32_t* colors, int count);

/// Load per-row palette from array
/// @param row Row/scanline number
/// @param colors Array of colors in ARGB format (0xAARRGGBB)
/// @param count Number of colors in array (max 16)
/// @note Thread Safety: Safe to call from any thread
/// @note LORES: row 0-299, loads up to 16 colors
/// @note XRES/WRES: row 0-239, loads up to 16 colors
/// @note URES: Returns error (no palette)
/// @note Colors array must not be NULL
void st_video_load_palette_row(int row, const uint32_t* colors, int count);

/// Save current palette to array
/// @param colors Output array (caller must allocate)
/// @param maxCount Size of output array
/// @return Number of colors written
/// @note Thread Safety: Safe to call from any thread
/// @note LORES: Saves 16 colors from row 0
/// @note XRES/WRES: Saves up to 256 colors (16 per-row from row 0 + 240 global)
/// @note URES: Returns 0 (no palette)
/// @note Colors array must not be NULL
int st_video_save_palette(uint32_t* colors, int maxCount);

/// Save per-row palette to array
/// @param row Row/scanline number
/// @param colors Output array (caller must allocate, size >= 16)
/// @return Number of colors written (0-16)
/// @note Thread Safety: Safe to call from any thread
/// @note LORES: row 0-299, saves up to 16 colors
/// @note XRES/WRES: row 0-239, saves up to 16 colors
/// @note URES: Returns 0 (no palette)
/// @note Colors array must not be NULL
int st_video_save_palette_row(int row, uint32_t* colors);

// =============================================================================
// Preset Palette Functions
// =============================================================================

/// Load a preset palette
/// @param preset Preset palette type
/// @note Thread Safety: Safe to call from any thread
/// @note LORES: Loads 16-color preset to all rows
/// @note XRES/WRES:
///   - 16-color presets (IBM_RGBI, C64, GRAYSCALE) → per-row colors (0-15, all rows)
///   - RGB_CUBE_6x8x5 → global colors (16-255)
/// @note URES: Returns error (no palette)
void st_video_load_preset_palette(st_video_palette_preset_t preset);

/// Load preset palette to specific row range
/// @param preset Preset palette type (only 16-color presets valid)
/// @param startRow First row to modify
/// @param endRow Last row to modify (inclusive)
/// @note Thread Safety: Safe to call from any thread
/// @note LORES: startRow/endRow 0-299, affects indices 0-15
/// @note XRES/WRES: startRow/endRow 0-239, affects indices 0-15
/// @note URES: Returns error (no palette)
/// @note RGB_CUBE_6x8x5 preset is ignored (not a per-row preset)
void st_video_load_preset_palette_rows(st_video_palette_preset_t preset, 
                                        int startRow, int endRow);

// =============================================================================
// Helper Functions
// =============================================================================

/// Pack RGB components into ARGB format
/// @param r Red component (0-255)
/// @param g Green component (0-255)
/// @param b Blue component (0-255)
/// @return Packed color in ARGB format (0xFFRRGGBB)
/// @note Alpha is always set to 0xFF (fully opaque)
/// @note Components are clamped to 0-255
static inline uint32_t st_video_pack_rgb(int r, int g, int b) {
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;
    return 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/// Unpack ARGB format into RGB components
/// @param color Color in ARGB format (0xAARRGGBB)
/// @param r Pointer to red component output (0-255)
/// @param g Pointer to green component output (0-255)
/// @param b Pointer to blue component output (0-255)
/// @note Pointers must not be NULL
static inline void st_video_unpack_rgb(uint32_t color, int* r, int* g, int* b) {
    if (r) *r = (color >> 16) & 0xFF;
    if (g) *g = (color >> 8) & 0xFF;
    if (b) *b = color & 0xFF;
}

#ifdef __cplusplus
}
#endif

#endif // ST_API_VIDEO_PALETTE_H