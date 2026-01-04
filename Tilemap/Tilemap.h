//
// Tilemap.h
// SuperTerminal Framework - Tilemap System
//
// Tilemap class for storing tile grid data
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef TILEMAP_H
#define TILEMAP_H

#include "TileData.h"
#include <vector>
#include <string>
#include <cstdint>

namespace SuperTerminal {

/// Tilemap: 2D grid of tiles
///
/// Stores a rectangular grid of tiles with efficient row-major storage.
/// Supports both dense and sparse (chunked) storage for large maps.
///
/// Coordinate system:
/// - Origin (0,0) is at top-left
/// - X increases right
/// - Y increases down
///
class Tilemap {
public:
    // =================================================================
    // Construction
    // =================================================================
    
    Tilemap();
    Tilemap(int32_t width, int32_t height, int32_t tileWidth, int32_t tileHeight);
    ~Tilemap();
    
    // Disable copy, allow move
    Tilemap(const Tilemap&) = delete;
    Tilemap& operator=(const Tilemap&) = delete;
    Tilemap(Tilemap&&) noexcept;
    Tilemap& operator=(Tilemap&&) noexcept;
    
    // =================================================================
    // Initialization
    // =================================================================
    
    /// Initialize or resize tilemap
    /// @param width Width in tiles
    /// @param height Height in tiles
    /// @param tileWidth Tile width in pixels
    /// @param tileHeight Tile height in pixels
    void initialize(int32_t width, int32_t height, int32_t tileWidth, int32_t tileHeight);
    
    /// Clear all tiles to empty
    void clear();
    
    /// Fill entire map with tile
    void fill(TileData tile);
    
    /// Fill rectangle with tile
    void fillRect(int32_t x, int32_t y, int32_t width, int32_t height, TileData tile);
    
    // =================================================================
    // Tile Access
    // =================================================================
    
    /// Get tile at position (returns empty if out of bounds)
    TileData getTile(int32_t x, int32_t y) const;
    
    /// Set tile at position (does nothing if out of bounds)
    void setTile(int32_t x, int32_t y, TileData tile);
    
    /// Get tile by linear index (faster, no bounds check)
    inline TileData getTileByIndex(int32_t index) const {
        return m_tiles[index];
    }
    
    /// Set tile by linear index (faster, no bounds check)
    inline void setTileByIndex(int32_t index, TileData tile) {
        m_tiles[index] = tile;
        m_dirty = true;
    }
    
    /// Convert 2D coordinates to linear index
    inline int32_t coordsToIndex(int32_t x, int32_t y) const {
        return y * m_width + x;
    }
    
    /// Convert linear index to 2D coordinates
    inline void indexToCoords(int32_t index, int32_t& outX, int32_t& outY) const {
        outX = index % m_width;
        outY = index / m_width;
    }
    
    // =================================================================
    // Bounds Checking
    // =================================================================
    
    /// Check if tile coordinates are in bounds
    inline bool isInBounds(int32_t x, int32_t y) const {
        return x >= 0 && x < m_width && y >= 0 && y < m_height;
    }
    
    /// Check if linear index is valid
    inline bool isValidIndex(int32_t index) const {
        return index >= 0 && index < (int32_t)m_tiles.size();
    }
    
    // =================================================================
    // Bulk Operations
    // =================================================================
    
    /// Copy region from another tilemap
    void copyRegion(const Tilemap& src, 
                    int32_t srcX, int32_t srcY,
                    int32_t dstX, int32_t dstY,
                    int32_t width, int32_t height);
    
    /// Copy region within this tilemap
    void copyRegionSelf(int32_t srcX, int32_t srcY,
                        int32_t dstX, int32_t dstY,
                        int32_t width, int32_t height);
    
    /// Get tile data pointer (for direct access)
    TileData* getTileData() { return m_tiles.data(); }
    const TileData* getTileData() const { return m_tiles.data(); }
    
    /// Get tile count
    inline size_t getTileCount() const { return m_tiles.size(); }
    
    // =================================================================
    // Properties
    // =================================================================
    
    /// Get map width in tiles
    inline int32_t getWidth() const { return m_width; }
    
    /// Get map height in tiles
    inline int32_t getHeight() const { return m_height; }
    
    /// Get tile width in pixels
    inline int32_t getTileWidth() const { return m_tileWidth; }
    
    /// Get tile height in pixels
    inline int32_t getTileHeight() const { return m_tileHeight; }
    
    /// Get map width in pixels
    inline int32_t getPixelWidth() const { return m_width * m_tileWidth; }
    
    /// Get map height in pixels
    inline int32_t getPixelHeight() const { return m_height * m_tileHeight; }
    
    /// Get tilemap name
    inline const std::string& getName() const { return m_name; }
    
    /// Set tilemap name
    inline void setName(const std::string& name) { m_name = name; }
    
    // =================================================================
    // Coordinate Conversion
    // =================================================================
    
    /// Convert world coordinates to tile coordinates
    inline void worldToTile(float worldX, float worldY, int32_t& tileX, int32_t& tileY) const {
        tileX = (int32_t)(worldX / m_tileWidth);
        tileY = (int32_t)(worldY / m_tileHeight);
    }
    
    /// Convert tile coordinates to world coordinates (top-left corner)
    inline void tileToWorld(int32_t tileX, int32_t tileY, float& worldX, float& worldY) const {
        worldX = tileX * m_tileWidth;
        worldY = tileY * m_tileHeight;
    }
    
    /// Convert tile coordinates to world coordinates (center)
    inline void tileToWorldCenter(int32_t tileX, int32_t tileY, float& worldX, float& worldY) const {
        worldX = tileX * m_tileWidth + m_tileWidth * 0.5f;
        worldY = tileY * m_tileHeight + m_tileHeight * 0.5f;
    }
    
    // =================================================================
    // Dirty Tracking
    // =================================================================
    
    /// Check if tilemap has been modified
    inline bool isDirty() const { return m_dirty; }
    
    /// Clear dirty flag
    inline void clearDirty() { m_dirty = false; }
    
    /// Mark as dirty
    inline void markDirty() { m_dirty = true; }
    
    // =================================================================
    // Serialization
    // =================================================================
    
    /// Get memory size in bytes
    size_t getMemorySize() const;
    
    /// Export to raw tile data (for serialization)
    std::vector<uint16_t> exportRawData() const;
    
    /// Import from raw tile data
    void importRawData(const std::vector<uint16_t>& data);
    
private:
    // Map dimensions
    int32_t m_width = 0;
    int32_t m_height = 0;
    
    // Tile dimensions
    int32_t m_tileWidth = 16;
    int32_t m_tileHeight = 16;
    
    // Tile data (row-major: tiles[y * width + x])
    std::vector<TileData> m_tiles;
    
    // Metadata
    std::string m_name;
    
    // Dirty flag for optimization
    bool m_dirty = true;
};

} // namespace SuperTerminal

#endif // TILEMAP_H