//
// TilemapIndexedAPI.h
// SuperTerminal Framework - Tilemap System
//
// C API for indexed tile rendering with palette banks
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef TILEMAPINDEXEDAPI_H
#define TILEMAPINDEXEDAPI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Opaque Handle Types
// =============================================================================

typedef void* PaletteBankHandle;
typedef void* TilesetIndexedHandle;
typedef void* TilemapExHandle;
typedef void* MTLDeviceHandle;

// =============================================================================
// PaletteBank API
// =============================================================================

/// Create a new palette bank
/// @param paletteCount Number of palettes (default 32)
/// @param colorsPerPalette Colors per palette (default 16)
/// @param device Metal device (NULL = use default)
/// @return Palette bank handle, or NULL on failure
PaletteBankHandle palettebank_create(int32_t paletteCount, 
                                      int32_t colorsPerPalette,
                                      MTLDeviceHandle device);

/// Destroy palette bank
void palettebank_destroy(PaletteBankHandle bank);

/// Initialize GPU resources
bool palettebank_initialize(PaletteBankHandle bank, MTLDeviceHandle device);

/// Set a single color in a palette
/// @param bank Palette bank handle
/// @param paletteIndex Palette index (0-31)
/// @param colorIndex Color index (0-15)
/// @param r Red (0-255)
/// @param g Green (0-255)
/// @param b Blue (0-255)
/// @param a Alpha (0-255)
/// @return true on success
bool palettebank_set_color(PaletteBankHandle bank,
                           int32_t paletteIndex,
                           int32_t colorIndex,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/// Get a single color from a palette
/// @param bank Palette bank handle
/// @param paletteIndex Palette index (0-31)
/// @param colorIndex Color index (0-15)
/// @param out_r Output red (0-255)
/// @param out_g Output green (0-255)
/// @param out_b Output blue (0-255)
/// @param out_a Output alpha (0-255)
/// @return true on success
bool palettebank_get_color(PaletteBankHandle bank,
                           int32_t paletteIndex,
                           int32_t colorIndex,
                           uint8_t* out_r, uint8_t* out_g, 
                           uint8_t* out_b, uint8_t* out_a);

/// Load a preset palette
/// @param bank Palette bank handle
/// @param paletteIndex Palette index to load into
/// @param presetName Preset name (e.g., "desert", "forest", "cave")
/// @return true on success
bool palettebank_load_preset(PaletteBankHandle bank,
                             int32_t paletteIndex,
                             const char* presetName);

/// Copy palette from one index to another
bool palettebank_copy_palette(PaletteBankHandle bank,
                              int32_t srcIndex,
                              int32_t dstIndex);

/// Fill entire palette with a single color
void palettebank_fill_palette(PaletteBankHandle bank,
                              int32_t paletteIndex,
                              uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/// Clear palette (set all to transparent black)
void palettebank_clear_palette(PaletteBankHandle bank, int32_t paletteIndex);

/// Enforce palette convention (index 0=transparent, 1=black)
void palettebank_enforce_convention(PaletteBankHandle bank, int32_t paletteIndex);

/// Upload palette data to GPU
/// @param paletteIndex Palette to upload (-1 = all palettes)
void palettebank_upload(PaletteBankHandle bank, int32_t paletteIndex);

/// Get number of palettes
int32_t palettebank_get_palette_count(PaletteBankHandle bank);

/// Get colors per palette
int32_t palettebank_get_colors_per_palette(PaletteBankHandle bank);

// =============================================================================
// Palette Manipulation API
// =============================================================================

/// Lerp between two palettes
/// @param bank Palette bank handle
/// @param paletteA First palette index
/// @param paletteB Second palette index
/// @param t Interpolation factor (0.0 = A, 1.0 = B)
/// @param outPalette Output palette index
void palettebank_lerp(PaletteBankHandle bank,
                     int32_t paletteA,
                     int32_t paletteB,
                     float t,
                     int32_t outPalette);

/// Rotate palette colors (for animation)
/// @param bank Palette bank handle
/// @param paletteIndex Palette index
/// @param startIndex Start color index
/// @param endIndex End color index
/// @param amount Rotation amount (positive = right, negative = left)
void palettebank_rotate(PaletteBankHandle bank,
                       int32_t paletteIndex,
                       int32_t startIndex,
                       int32_t endIndex,
                       int32_t amount);

/// Adjust palette brightness
/// @param bank Palette bank handle
/// @param paletteIndex Palette index
/// @param brightness Brightness multiplier (1.0 = normal, 0.5 = darker, 2.0 = brighter)
void palettebank_adjust_brightness(PaletteBankHandle bank,
                                   int32_t paletteIndex,
                                   float brightness);

/// Adjust palette saturation
/// @param bank Palette bank handle
/// @param paletteIndex Palette index
/// @param saturation Saturation multiplier (0.0 = grayscale, 1.0 = normal, 2.0 = vibrant)
void palettebank_adjust_saturation(PaletteBankHandle bank,
                                   int32_t paletteIndex,
                                   float saturation);

// =============================================================================
// TilesetIndexed API
// =============================================================================

/// Create a new indexed tileset
/// @param device Metal device (NULL = use default)
/// @param tileWidth Tile width in pixels
/// @param tileHeight Tile height in pixels
/// @param tileCount Number of tiles
/// @return Tileset handle, or NULL on failure
TilesetIndexedHandle tilesetindexed_create(MTLDeviceHandle device,
                                           int32_t tileWidth,
                                           int32_t tileHeight,
                                           int32_t tileCount);

/// Destroy indexed tileset
void tilesetindexed_destroy(TilesetIndexedHandle tileset);

/// Initialize indexed tileset
bool tilesetindexed_initialize(TilesetIndexedHandle tileset,
                               MTLDeviceHandle device,
                               int32_t tileWidth,
                               int32_t tileHeight,
                               int32_t tileCount);

/// Set a single pixel in a tile
/// @param tileset Tileset handle
/// @param tileId Tile ID (0-based)
/// @param x Pixel X coordinate
/// @param y Pixel Y coordinate
/// @param colorIndex Color index (0-15)
/// @return true on success
bool tilesetindexed_set_pixel(TilesetIndexedHandle tileset,
                              int32_t tileId,
                              int32_t x, int32_t y,
                              uint8_t colorIndex);

/// Get a single pixel from a tile
/// @param tileset Tileset handle
/// @param tileId Tile ID (0-based)
/// @param x Pixel X coordinate
/// @param y Pixel Y coordinate
/// @return Color index (0-15), or 0 on error
uint8_t tilesetindexed_get_pixel(TilesetIndexedHandle tileset,
                                 int32_t tileId,
                                 int32_t x, int32_t y);

/// Fill entire tile with a single color index
/// @param tileset Tileset handle
/// @param tileId Tile ID (0-based)
/// @param colorIndex Color index (0-15)
void tilesetindexed_fill_tile(TilesetIndexedHandle tileset,
                              int32_t tileId,
                              uint8_t colorIndex);

/// Clear tile (set all pixels to 0 = transparent)
void tilesetindexed_clear_tile(TilesetIndexedHandle tileset, int32_t tileId);

/// Copy tile from one ID to another
bool tilesetindexed_copy_tile(TilesetIndexedHandle tileset,
                              int32_t srcTileId,
                              int32_t dstTileId);

/// Upload tileset data to GPU
void tilesetindexed_upload(TilesetIndexedHandle tileset);

/// Get tile width
int32_t tilesetindexed_get_tile_width(TilesetIndexedHandle tileset);

/// Get tile height
int32_t tilesetindexed_get_tile_height(TilesetIndexedHandle tileset);

/// Get tile count
int32_t tilesetindexed_get_tile_count(TilesetIndexedHandle tileset);

// =============================================================================
// TilemapEx API
// =============================================================================

/// Create a new extended tilemap (32-bit tiles with palette support)
/// @param width Width in tiles
/// @param height Height in tiles
/// @param tileWidth Tile width in pixels
/// @param tileHeight Tile height in pixels
/// @return Tilemap handle, or NULL on failure
TilemapExHandle tilemapex_create(int32_t width, int32_t height,
                                 int32_t tileWidth, int32_t tileHeight);

/// Destroy extended tilemap
void tilemapex_destroy(TilemapExHandle tilemap);

/// Set tile with palette and z-order
/// @param tilemap Tilemap handle
/// @param x X coordinate
/// @param y Y coordinate
/// @param tileId Tile ID from tileset
/// @param paletteIndex Palette index (0-255)
/// @param zOrder Z-order priority (0-7, default 3)
/// @param flipX Horizontal flip
/// @param flipY Vertical flip
/// @param rotation Rotation (0-3)
void tilemapex_set_tile_indexed(TilemapExHandle tilemap,
                                int32_t x, int32_t y,
                                uint16_t tileId,
                                uint8_t paletteIndex,
                                uint8_t zOrder,
                                bool flipX, bool flipY,
                                uint8_t rotation);

/// Get tile data
/// @param tilemap Tilemap handle
/// @param x X coordinate
/// @param y Y coordinate
/// @param out_tileId Output tile ID
/// @param out_paletteIndex Output palette index
/// @param out_zOrder Output z-order
/// @param out_flipX Output flip X
/// @param out_flipY Output flip Y
/// @param out_rotation Output rotation
/// @return true on success
bool tilemapex_get_tile_indexed(TilemapExHandle tilemap,
                                int32_t x, int32_t y,
                                uint16_t* out_tileId,
                                uint8_t* out_paletteIndex,
                                uint8_t* out_zOrder,
                                bool* out_flipX,
                                bool* out_flipY,
                                uint8_t* out_rotation);

/// Set palette for tile (keeps tile ID)
void tilemapex_set_palette(TilemapExHandle tilemap,
                           int32_t x, int32_t y,
                           uint8_t paletteIndex);

/// Get palette index for tile
uint8_t tilemapex_get_palette(TilemapExHandle tilemap,
                              int32_t x, int32_t y);

/// Set z-order for tile (keeps tile ID and palette)
void tilemapex_set_zorder(TilemapExHandle tilemap,
                          int32_t x, int32_t y,
                          uint8_t zOrder);

/// Get z-order for tile
uint8_t tilemapex_get_zorder(TilemapExHandle tilemap,
                             int32_t x, int32_t y);

/// Fill entire map with tile
void tilemapex_fill_indexed(TilemapExHandle tilemap,
                            uint16_t tileId,
                            uint8_t paletteIndex);

/// Fill rectangle with tile
void tilemapex_fill_rect_indexed(TilemapExHandle tilemap,
                                 int32_t x, int32_t y,
                                 int32_t width, int32_t height,
                                 uint16_t tileId,
                                 uint8_t paletteIndex);

/// Clear entire map
void tilemapex_clear(TilemapExHandle tilemap);

/// Get map width
int32_t tilemapex_get_width(TilemapExHandle tilemap);

/// Get map height
int32_t tilemapex_get_height(TilemapExHandle tilemap);

/// Get tile width
int32_t tilemapex_get_tile_width(TilemapExHandle tilemap);

/// Get tile height
int32_t tilemapex_get_tile_height(TilemapExHandle tilemap);

// =============================================================================
// TilemapRenderer Indexed API Extension
// =============================================================================

/// Render indexed tilemap layer (extends existing TilemapRenderer)
/// @param renderer TilemapRenderer handle (from existing API)
/// @param layer TilemapLayer handle (from existing API)
/// @param tileset Indexed tileset handle
/// @param paletteBank Palette bank handle
/// @param camera Camera handle (from existing API)
/// @param time Current time for animations
/// @return true on success
bool tilemaprenderer_render_layer_indexed(void* renderer,
                                          void* layer,
                                          TilesetIndexedHandle tileset,
                                          PaletteBankHandle paletteBank,
                                          void* camera,
                                          float time);

#ifdef __cplusplus
}
#endif

#endif // TILEMAPINDEXEDAPI_H