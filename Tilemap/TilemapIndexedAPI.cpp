//
// TilemapIndexedAPI.cpp
// SuperTerminal Framework - Tilemap System
//
// C API implementation for indexed tile rendering with palette banks
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "TilemapIndexedAPI.h"
#include "PaletteBank.h"
#include "TilesetIndexed.h"
#include "TilemapEx.h"
#include "TilemapRenderer.h"
#include "TilemapLayer.h"
#include "Camera.h"
#include <cstdlib>
#include <cstring>

using namespace SuperTerminal;

// =============================================================================
// PaletteBank API Implementation
// =============================================================================

PaletteBankHandle palettebank_create(int32_t paletteCount, 
                                      int32_t colorsPerPalette,
                                      MTLDeviceHandle device) {
    try {
        PaletteBank* bank = new PaletteBank(paletteCount, colorsPerPalette, (MTLDevicePtr)device);
        return static_cast<PaletteBankHandle>(bank);
    } catch (...) {
        return nullptr;
    }
}

void palettebank_destroy(PaletteBankHandle bank) {
    if (bank) {
        delete static_cast<PaletteBank*>(bank);
    }
}

bool palettebank_initialize(PaletteBankHandle bank, MTLDeviceHandle device) {
    if (!bank) return false;
    PaletteBank* pb = static_cast<PaletteBank*>(bank);
    return pb->initialize((MTLDevicePtr)device);
}

bool palettebank_set_color(PaletteBankHandle bank,
                           int32_t paletteIndex,
                           int32_t colorIndex,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!bank) return false;
    PaletteBank* pb = static_cast<PaletteBank*>(bank);
    PaletteColor color(r, g, b, a);
    return pb->setColor(paletteIndex, colorIndex, color);
}

bool palettebank_get_color(PaletteBankHandle bank,
                           int32_t paletteIndex,
                           int32_t colorIndex,
                           uint8_t* out_r, uint8_t* out_g, 
                           uint8_t* out_b, uint8_t* out_a) {
    if (!bank || !out_r || !out_g || !out_b || !out_a) return false;
    PaletteBank* pb = static_cast<PaletteBank*>(bank);
    PaletteColor color = pb->getColor(paletteIndex, colorIndex);
    *out_r = color.r;
    *out_g = color.g;
    *out_b = color.b;
    *out_a = color.a;
    return true;
}

bool palettebank_load_preset(PaletteBankHandle bank,
                             int32_t paletteIndex,
                             const char* presetName) {
    if (!bank || !presetName) return false;
    PaletteBank* pb = static_cast<PaletteBank*>(bank);
    return pb->loadPreset(paletteIndex, presetName);
}

bool palettebank_copy_palette(PaletteBankHandle bank,
                              int32_t srcIndex,
                              int32_t dstIndex) {
    if (!bank) return false;
    PaletteBank* pb = static_cast<PaletteBank*>(bank);
    return pb->copyPalette(srcIndex, dstIndex);
}

void palettebank_fill_palette(PaletteBankHandle bank,
                              int32_t paletteIndex,
                              uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!bank) return;
    PaletteBank* pb = static_cast<PaletteBank*>(bank);
    PaletteColor color(r, g, b, a);
    pb->fillPalette(paletteIndex, color);
}

void palettebank_clear_palette(PaletteBankHandle bank, int32_t paletteIndex) {
    if (!bank) return;
    PaletteBank* pb = static_cast<PaletteBank*>(bank);
    pb->clearPalette(paletteIndex);
}

void palettebank_enforce_convention(PaletteBankHandle bank, int32_t paletteIndex) {
    if (!bank) return;
    PaletteBank* pb = static_cast<PaletteBank*>(bank);
    pb->enforceConvention(paletteIndex);
}

void palettebank_upload(PaletteBankHandle bank, int32_t paletteIndex) {
    if (!bank) return;
    PaletteBank* pb = static_cast<PaletteBank*>(bank);
    pb->uploadToGPU(paletteIndex);
}

int32_t palettebank_get_palette_count(PaletteBankHandle bank) {
    if (!bank) return 0;
    PaletteBank* pb = static_cast<PaletteBank*>(bank);
    return pb->getPaletteCount();
}

int32_t palettebank_get_colors_per_palette(PaletteBankHandle bank) {
    if (!bank) return 0;
    PaletteBank* pb = static_cast<PaletteBank*>(bank);
    return pb->getColorsPerPalette();
}

// =============================================================================
// Palette Manipulation API Implementation
// =============================================================================

void palettebank_lerp(PaletteBankHandle bank,
                     int32_t paletteA,
                     int32_t paletteB,
                     float t,
                     int32_t outPalette) {
    if (!bank) return;
    PaletteBank* pb = static_cast<PaletteBank*>(bank);
    pb->lerpPalettes(paletteA, paletteB, t, outPalette);
}

void palettebank_rotate(PaletteBankHandle bank,
                       int32_t paletteIndex,
                       int32_t startIndex,
                       int32_t endIndex,
                       int32_t amount) {
    if (!bank) return;
    PaletteBank* pb = static_cast<PaletteBank*>(bank);
    pb->rotatePalette(paletteIndex, startIndex, endIndex, amount);
}

void palettebank_adjust_brightness(PaletteBankHandle bank,
                                   int32_t paletteIndex,
                                   float brightness) {
    if (!bank) return;
    PaletteBank* pb = static_cast<PaletteBank*>(bank);
    pb->adjustBrightness(paletteIndex, brightness);
}

void palettebank_adjust_saturation(PaletteBankHandle bank,
                                   int32_t paletteIndex,
                                   float saturation) {
    if (!bank) return;
    PaletteBank* pb = static_cast<PaletteBank*>(bank);
    pb->adjustSaturation(paletteIndex, saturation);
}

// =============================================================================
// TilesetIndexed API Implementation
// =============================================================================

TilesetIndexedHandle tilesetindexed_create(MTLDeviceHandle device,
                                           int32_t tileWidth,
                                           int32_t tileHeight,
                                           int32_t tileCount) {
    try {
        TilesetIndexed* tileset = new TilesetIndexed();
        if (!tileset->initializeIndexed((MTLDevicePtr)device, tileWidth, tileHeight, tileCount)) {
            delete tileset;
            return nullptr;
        }
        return static_cast<TilesetIndexedHandle>(tileset);
    } catch (...) {
        return nullptr;
    }
}

void tilesetindexed_destroy(TilesetIndexedHandle tileset) {
    if (tileset) {
        delete static_cast<TilesetIndexed*>(tileset);
    }
}

bool tilesetindexed_initialize(TilesetIndexedHandle tileset,
                               MTLDeviceHandle device,
                               int32_t tileWidth,
                               int32_t tileHeight,
                               int32_t tileCount) {
    if (!tileset) return false;
    TilesetIndexed* ts = static_cast<TilesetIndexed*>(tileset);
    return ts->initializeIndexed((MTLDevicePtr)device, tileWidth, tileHeight, tileCount);
}

bool tilesetindexed_set_pixel(TilesetIndexedHandle tileset,
                              int32_t tileId,
                              int32_t x, int32_t y,
                              uint8_t colorIndex) {
    if (!tileset) return false;
    TilesetIndexed* ts = static_cast<TilesetIndexed*>(tileset);
    ts->setTileIndexedPixel(tileId, x, y, colorIndex);
    return true;
}

uint8_t tilesetindexed_get_pixel(TilesetIndexedHandle tileset,
                                 int32_t tileId,
                                 int32_t x, int32_t y) {
    if (!tileset) return 0;
    TilesetIndexed* ts = static_cast<TilesetIndexed*>(tileset);
    return ts->getTileIndexedPixel(tileId, x, y);
}

void tilesetindexed_fill_tile(TilesetIndexedHandle tileset,
                              int32_t tileId,
                              uint8_t colorIndex) {
    if (!tileset) return;
    TilesetIndexed* ts = static_cast<TilesetIndexed*>(tileset);
    ts->fillTile(tileId, colorIndex);
}

void tilesetindexed_clear_tile(TilesetIndexedHandle tileset, int32_t tileId) {
    if (!tileset) return;
    TilesetIndexed* ts = static_cast<TilesetIndexed*>(tileset);
    ts->clearTile(tileId);
}

bool tilesetindexed_copy_tile(TilesetIndexedHandle tileset,
                              int32_t srcTileId,
                              int32_t dstTileId) {
    if (!tileset) return false;
    TilesetIndexed* ts = static_cast<TilesetIndexed*>(tileset);
    ts->copyTile(srcTileId, dstTileId);
    return true;
}

void tilesetindexed_upload(TilesetIndexedHandle tileset) {
    if (!tileset) return;
    TilesetIndexed* ts = static_cast<TilesetIndexed*>(tileset);
    ts->uploadIndexedData();
}

int32_t tilesetindexed_get_tile_width(TilesetIndexedHandle tileset) {
    if (!tileset) return 0;
    TilesetIndexed* ts = static_cast<TilesetIndexed*>(tileset);
    return ts->getTileWidth();
}

int32_t tilesetindexed_get_tile_height(TilesetIndexedHandle tileset) {
    if (!tileset) return 0;
    TilesetIndexed* ts = static_cast<TilesetIndexed*>(tileset);
    return ts->getTileHeight();
}

int32_t tilesetindexed_get_tile_count(TilesetIndexedHandle tileset) {
    if (!tileset) return 0;
    TilesetIndexed* ts = static_cast<TilesetIndexed*>(tileset);
    return ts->getTileCount();
}

// =============================================================================
// TilemapEx API Implementation
// =============================================================================

TilemapExHandle tilemapex_create(int32_t width, int32_t height,
                                 int32_t tileWidth, int32_t tileHeight) {
    try {
        TilemapEx* tilemap = new TilemapEx(width, height, tileWidth, tileHeight);
        return static_cast<TilemapExHandle>(tilemap);
    } catch (...) {
        return nullptr;
    }
}

void tilemapex_destroy(TilemapExHandle tilemap) {
    if (tilemap) {
        delete static_cast<TilemapEx*>(tilemap);
    }
}

void tilemapex_set_tile_indexed(TilemapExHandle tilemap,
                                int32_t x, int32_t y,
                                uint16_t tileId,
                                uint8_t paletteIndex,
                                uint8_t zOrder,
                                bool flipX, bool flipY,
                                uint8_t rotation) {
    if (!tilemap) return;
    TilemapEx* tm = static_cast<TilemapEx*>(tilemap);
    tm->setTileWithPalette(x, y, tileId, paletteIndex, zOrder, flipX, flipY, rotation);
}

bool tilemapex_get_tile_indexed(TilemapExHandle tilemap,
                                int32_t x, int32_t y,
                                uint16_t* out_tileId,
                                uint8_t* out_paletteIndex,
                                uint8_t* out_zOrder,
                                bool* out_flipX,
                                bool* out_flipY,
                                uint8_t* out_rotation) {
    if (!tilemap) return false;
    TilemapEx* tm = static_cast<TilemapEx*>(tilemap);
    TileDataEx tile = tm->getTileEx(x, y);
    
    if (out_tileId) *out_tileId = tile.getTileID();
    if (out_paletteIndex) *out_paletteIndex = tile.getPaletteIndex();
    if (out_zOrder) *out_zOrder = tile.getZOrder();
    if (out_flipX) *out_flipX = tile.getFlipX();
    if (out_flipY) *out_flipY = tile.getFlipY();
    if (out_rotation) *out_rotation = tile.getRotation();
    
    return true;
}

void tilemapex_set_palette(TilemapExHandle tilemap,
                           int32_t x, int32_t y,
                           uint8_t paletteIndex) {
    if (!tilemap) return;
    TilemapEx* tm = static_cast<TilemapEx*>(tilemap);
    tm->setTilePalette(x, y, paletteIndex);
}

uint8_t tilemapex_get_palette(TilemapExHandle tilemap,
                              int32_t x, int32_t y) {
    if (!tilemap) return 0;
    TilemapEx* tm = static_cast<TilemapEx*>(tilemap);
    return tm->getTilePalette(x, y);
}

void tilemapex_set_zorder(TilemapExHandle tilemap,
                          int32_t x, int32_t y,
                          uint8_t zOrder) {
    if (!tilemap) return;
    TilemapEx* tm = static_cast<TilemapEx*>(tilemap);
    tm->setTileZOrder(x, y, zOrder);
}

uint8_t tilemapex_get_zorder(TilemapExHandle tilemap,
                             int32_t x, int32_t y) {
    if (!tilemap) return 0;
    TilemapEx* tm = static_cast<TilemapEx*>(tilemap);
    return tm->getTileZOrder(x, y);
}

void tilemapex_fill_indexed(TilemapExHandle tilemap,
                            uint16_t tileId,
                            uint8_t paletteIndex) {
    if (!tilemap) return;
    TilemapEx* tm = static_cast<TilemapEx*>(tilemap);
    TileDataEx tile;
    tile.setTileID(tileId);
    tile.setPaletteIndex(paletteIndex);
    tile.setZOrder(TILEEX_ZORDER_NORMAL);
    tm->fillEx(tile);
}

void tilemapex_fill_rect_indexed(TilemapExHandle tilemap,
                                 int32_t x, int32_t y,
                                 int32_t width, int32_t height,
                                 uint16_t tileId,
                                 uint8_t paletteIndex) {
    if (!tilemap) return;
    TilemapEx* tm = static_cast<TilemapEx*>(tilemap);
    TileDataEx tile;
    tile.setTileID(tileId);
    tile.setPaletteIndex(paletteIndex);
    tile.setZOrder(TILEEX_ZORDER_NORMAL);
    tm->fillRectEx(x, y, width, height, tile);
}

void tilemapex_clear(TilemapExHandle tilemap) {
    if (!tilemap) return;
    TilemapEx* tm = static_cast<TilemapEx*>(tilemap);
    tm->clearEx();
}

int32_t tilemapex_get_width(TilemapExHandle tilemap) {
    if (!tilemap) return 0;
    TilemapEx* tm = static_cast<TilemapEx*>(tilemap);
    return tm->getWidth();
}

int32_t tilemapex_get_height(TilemapExHandle tilemap) {
    if (!tilemap) return 0;
    TilemapEx* tm = static_cast<TilemapEx*>(tilemap);
    return tm->getHeight();
}

int32_t tilemapex_get_tile_width(TilemapExHandle tilemap) {
    if (!tilemap) return 0;
    TilemapEx* tm = static_cast<TilemapEx*>(tilemap);
    return tm->getTileWidth();
}

int32_t tilemapex_get_tile_height(TilemapExHandle tilemap) {
    if (!tilemap) return 0;
    TilemapEx* tm = static_cast<TilemapEx*>(tilemap);
    return tm->getTileHeight();
}

// =============================================================================
// TilemapRenderer Indexed API Extension Implementation
// =============================================================================

bool tilemaprenderer_render_layer_indexed(void* renderer,
                                          void* layer,
                                          TilesetIndexedHandle tileset,
                                          PaletteBankHandle paletteBank,
                                          void* camera,
                                          float time) {
    if (!renderer || !layer || !tileset || !paletteBank || !camera) {
        return false;
    }
    
    TilemapRenderer* tr = static_cast<TilemapRenderer*>(renderer);
    TilemapLayer* tl = static_cast<TilemapLayer*>(layer);
    TilesetIndexed* ts = static_cast<TilesetIndexed*>(tileset);
    PaletteBank* pb = static_cast<PaletteBank*>(paletteBank);
    Camera* cam = static_cast<Camera*>(camera);
    
    return tr->renderLayerIndexed(*tl, *ts, *pb, *cam, time);
}