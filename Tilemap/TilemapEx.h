//
// TilemapEx.h
// SuperTerminal Framework - Tilemap System
//
// Extended tilemap class with 32-bit tile data (palette and z-order support)
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef TILEMAPEX_H
#define TILEMAPEX_H

#include "Tilemap.h"
#include "TileDataEx.h"
#include <vector>
#include <string>
#include <cstdint>

namespace SuperTerminal {

/// TilemapEx: Extended 2D grid of tiles with 32-bit tile data
///
/// Extends Tilemap to use TileDataEx (32-bit) instead of TileData (16-bit).
/// Provides support for:
/// - 4096 tile IDs (vs 2048 in base Tilemap)
/// - 256 palette indices per tile
/// - 8 z-order priority levels per tile
/// - All features of base Tilemap
///
/// Memory usage: 4 bytes per tile (2× base Tilemap)
///
/// Coordinate system:
/// - Origin (0,0) is at top-left
/// - X increases right
/// - Y increases down
///
/// Thread Safety: Not thread-safe. Use from render thread only.
///
class TilemapEx : public Tilemap {
public:
    // =================================================================
    // Construction
    // =================================================================
    
    TilemapEx();
    TilemapEx(int32_t width, int32_t height, int32_t tileWidth, int32_t tileHeight);
    ~TilemapEx();
    
    // Disable copy, allow move
    TilemapEx(const TilemapEx&) = delete;
    TilemapEx& operator=(const TilemapEx&) = delete;
    TilemapEx(TilemapEx&&) noexcept;
    TilemapEx& operator=(TilemapEx&&) noexcept;
    
    // =================================================================
    // Initialization
    // =================================================================
    
    /// Initialize or resize tilemap (32-bit tiles)
    /// @param width Width in tiles
    /// @param height Height in tiles
    /// @param tileWidth Tile width in pixels
    /// @param tileHeight Tile height in pixels
    void initializeEx(int32_t width, int32_t height, int32_t tileWidth, int32_t tileHeight);
    
    /// Clear all tiles to empty (32-bit)
    void clearEx();
    
    /// Fill entire map with tile (32-bit)
    void fillEx(TileDataEx tile);
    
    /// Fill rectangle with tile (32-bit)
    void fillRectEx(int32_t x, int32_t y, int32_t width, int32_t height, TileDataEx tile);
    
    // =================================================================
    // Extended Tile Access (32-bit)
    // =================================================================
    
    /// Get tile at position (returns empty if out of bounds)
    TileDataEx getTileEx(int32_t x, int32_t y) const;
    
    /// Set tile at position (does nothing if out of bounds)
    void setTileEx(int32_t x, int32_t y, TileDataEx tile);
    
    /// Get tile by linear index (faster, no bounds check)
    inline TileDataEx getTileExByIndex(int32_t index) const {
        if (index >= 0 && index < (int32_t)m_tilesEx.size()) {
            return m_tilesEx[index];
        }
        return TileDataEx();
    }
    
    /// Set tile by linear index (faster, no bounds check)
    inline void setTileExByIndex(int32_t index, TileDataEx tile) {
        if (index >= 0 && index < (int32_t)m_tilesEx.size()) {
            m_tilesEx[index] = tile;
            markDirty();
        }
    }
    
    // =================================================================
    // Palette-Aware Operations
    // =================================================================
    
    /// Set tile with specific palette and z-order
    /// @param x X coordinate
    /// @param y Y coordinate
    /// @param tileID Tile ID from tileset
    /// @param paletteIndex Which palette to use (0-255)
    /// @param zOrder Z-order priority (0-7, default 3=normal)
    /// @param flipX Horizontal flip
    /// @param flipY Vertical flip
    /// @param rotation Rotation (0-3)
    void setTileWithPalette(int32_t x, int32_t y, 
                           uint16_t tileID, 
                           uint8_t paletteIndex,
                           uint8_t zOrder = TILEEX_ZORDER_NORMAL,
                           bool flipX = false,
                           bool flipY = false,
                           uint8_t rotation = 0);
    
    /// Set palette for tile (keeps tile ID)
    /// @param x X coordinate
    /// @param y Y coordinate
    /// @param paletteIndex New palette index
    void setTilePalette(int32_t x, int32_t y, uint8_t paletteIndex);
    
    /// Get palette index for tile
    /// @param x X coordinate
    /// @param y Y coordinate
    /// @return Palette index, or 0 if invalid
    uint8_t getTilePalette(int32_t x, int32_t y) const;
    
    /// Set z-order for tile (keeps tile ID and palette)
    /// @param x X coordinate
    /// @param y Y coordinate
    /// @param zOrder Z-order priority (0-7)
    void setTileZOrder(int32_t x, int32_t y, uint8_t zOrder);
    
    /// Get z-order for tile
    /// @param x X coordinate
    /// @param y Y coordinate
    /// @return Z-order, or 0 if invalid
    uint8_t getTileZOrder(int32_t x, int32_t y) const;
    
    // =================================================================
    // Bulk Palette Operations
    // =================================================================
    
    /// Set palette for entire region
    /// @param x X coordinate
    /// @param y Y coordinate
    /// @param width Region width
    /// @param height Region height
    /// @param paletteIndex Palette to apply
    void setPaletteForRegion(int32_t x, int32_t y, 
                            int32_t width, int32_t height,
                            uint8_t paletteIndex);
    
    /// Set z-order for entire region
    /// @param x X coordinate
    /// @param y Y coordinate
    /// @param width Region width
    /// @param height Region height
    /// @param zOrder Z-order to apply
    void setZOrderForRegion(int32_t x, int32_t y,
                           int32_t width, int32_t height,
                           uint8_t zOrder);
    
    /// Replace palette (swap all tiles using one palette to another)
    /// @param oldPalette Old palette index
    /// @param newPalette New palette index
    void replacePalette(uint8_t oldPalette, uint8_t newPalette);
    
    // =================================================================
    // Bulk Copy Operations (32-bit)
    // =================================================================
    
    /// Copy region from another TilemapEx
    void copyRegionEx(const TilemapEx& src, 
                     int32_t srcX, int32_t srcY,
                     int32_t dstX, int32_t dstY,
                     int32_t width, int32_t height);
    
    /// Copy region within this tilemap
    void copyRegionSelfEx(int32_t srcX, int32_t srcY,
                         int32_t dstX, int32_t dstY,
                         int32_t width, int32_t height);
    
    // =================================================================
    // Direct Data Access (32-bit)
    // =================================================================
    
    /// Get 32-bit tile data pointer (for direct access)
    TileDataEx* getTileDataEx() { return m_tilesEx.data(); }
    const TileDataEx* getTileDataEx() const { return m_tilesEx.data(); }
    
    /// Get tile count
    inline size_t getTileCountEx() const { return m_tilesEx.size(); }
    
    // =================================================================
    // Format Detection
    // =================================================================
    
    /// Check if tilemap uses indexed color (32-bit tiles)
    bool usesIndexedColor() const { return true; }
    
    /// Check if tilemap uses extended format
    bool isExtended() const { return true; }
    
    // =================================================================
    // Conversion to/from Base Tilemap
    // =================================================================
    
    /// Import from legacy 16-bit Tilemap
    /// @param source Source tilemap
    /// @param defaultPalette Default palette index for all tiles
    void importFromTilemap(const Tilemap& source, uint8_t defaultPalette = 0);
    
    /// Export to legacy 16-bit Tilemap (loses palette and z-order info)
    /// @param dest Destination tilemap
    void exportToTilemap(Tilemap& dest) const;
    
    // =================================================================
    // Serialization (32-bit)
    // =================================================================
    
    /// Get memory size in bytes (32-bit tiles)
    size_t getMemorySizeEx() const;
    
    /// Export to raw 32-bit tile data
    std::vector<uint32_t> exportRawDataEx() const;
    
    /// Import from raw 32-bit tile data
    void importRawDataEx(const std::vector<uint32_t>& data);
    
    // =================================================================
    // Statistics
    // =================================================================
    
    /// Get palette usage statistics
    /// @param outPaletteUsage Array to fill with usage counts (size 256)
    void getPaletteUsage(int32_t* outPaletteUsage) const;
    
    /// Get z-order usage statistics
    /// @param outZOrderUsage Array to fill with usage counts (size 8)
    void getZOrderUsage(int32_t* outZOrderUsage) const;
    
    /// Count tiles using specific palette
    int32_t countTilesWithPalette(uint8_t paletteIndex) const;
    
    /// Count tiles at specific z-order
    int32_t countTilesAtZOrder(uint8_t zOrder) const;
    
private:
    // =================================================================
    // Extended Tile Storage (32-bit)
    // =================================================================
    
    // 32-bit tile data (row-major: tiles[y * width + x])
    std::vector<TileDataEx> m_tilesEx;
    
    // Note: We don't use base class m_tiles vector
    // All tile data is stored in m_tilesEx
};

} // namespace SuperTerminal

#endif // TILEMAPEX_H