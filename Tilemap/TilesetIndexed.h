//
// TilesetIndexed.h
// SuperTerminal Framework - Tilemap System
//
// Indexed color tileset for 4-bit palette-based tiles
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef TILESETINDEXED_H
#define TILESETINDEXED_H

#include "Tileset.h"
#include <vector>
#include <cstdint>
#include <memory>

#ifdef __OBJC__
@protocol MTLDevice;
@protocol MTLTexture;
typedef id<MTLDevice> MTLDevicePtr;
typedef id<MTLTexture> MTLTexturePtr;
#else
typedef void* MTLDevicePtr;
typedef void* MTLTexturePtr;
#endif

namespace SuperTerminal {

// Forward declarations
struct PaletteColor;

/// Tile format for indexed color
enum class TileFormat {
    RGB888,          // 24-bit RGB (legacy)
    RGBA8888,        // 32-bit RGBA (legacy)
    Indexed4Bit,     // 4-bit indexed (16 colors) ⭐
    Indexed8Bit      // 8-bit indexed (256 colors) - future
};

/// TilesetIndexed: Tileset with indexed color support
///
/// Extends Tileset to support 4-bit indexed color (16 colors per tile).
/// Each pixel in the tile stores a palette index (0-15) instead of RGB.
///
/// Features:
/// - 4-bit indexed pixels (16 colors)
/// - 75% memory savings vs RGB textures
/// - GPU texture storage (R8 format)
/// - Conversion from RGB images
/// - Convention: index 0=transparent, index 1=black
///
/// Memory usage:
/// - 256 tiles × 32×32 pixels × 1 byte = 256 KB
/// - Compare to RGB: 1 MB (4× larger)
///
/// Thread Safety: Not thread-safe. Use from render thread only.
///
class TilesetIndexed : public Tileset {
public:
    // =================================================================
    // Construction
    // =================================================================
    
    TilesetIndexed();
    ~TilesetIndexed();
    
    // Disable copy, allow move
    TilesetIndexed(const TilesetIndexed&) = delete;
    TilesetIndexed& operator=(const TilesetIndexed&) = delete;
    TilesetIndexed(TilesetIndexed&&) noexcept;
    TilesetIndexed& operator=(TilesetIndexed&&) noexcept;
    
    // =================================================================
    // Initialization
    // =================================================================
    
    /// Initialize with indexed color format
    /// @param device Metal device
    /// @param tileWidth Tile width in pixels
    /// @param tileHeight Tile height in pixels
    /// @param tileCount Number of tiles
    /// @param name Tileset name
    /// @return true on success
    bool initializeIndexed(MTLDevicePtr device,
                          int32_t tileWidth, 
                          int32_t tileHeight,
                          int32_t tileCount,
                          const std::string& name = "");
    
    /// Initialize from raw 4-bit indexed data
    /// @param device Metal device
    /// @param tileWidth Tile width in pixels
    /// @param tileHeight Tile height in pixels
    /// @param indexedData Raw indexed pixel data (0-15 per pixel)
    /// @param dataSize Size of data in bytes
    /// @param name Tileset name
    /// @return true on success
    bool initializeFromIndexedData(MTLDevicePtr device,
                                   int32_t tileWidth,
                                   int32_t tileHeight,
                                   const uint8_t* indexedData,
                                   size_t dataSize,
                                   const std::string& name = "");
    
    // =================================================================
    // Image Loading and Conversion
    // =================================================================
    
    /// Load tileset from image and convert to indexed color
    /// @param device Metal device
    /// @param imagePath Path to image file (PNG, etc.)
    /// @param tileWidth Tile width in pixels
    /// @param tileHeight Tile height in pixels
    /// @param referencePalette Reference palette for quantization (can be NULL)
    /// @param name Tileset name
    /// @return true on success
    bool loadImageIndexed(MTLDevicePtr device,
                         const char* imagePath,
                         int32_t tileWidth,
                         int32_t tileHeight,
                         const PaletteColor* referencePalette = nullptr,
                         const std::string& name = "");
    
    /// Convert RGB/RGBA image data to indexed
    /// @param rgbaData RGBA pixel data (4 bytes per pixel)
    /// @param width Image width
    /// @param height Image height
    /// @param referencePalette Reference palette (16 colors, can be NULL)
    /// @return Vector of indexed pixel data (1 byte per pixel, values 0-15)
    static std::vector<uint8_t> convertRGBAToIndexed(const uint8_t* rgbaData,
                                                      int32_t width,
                                                      int32_t height,
                                                      const PaletteColor* referencePalette = nullptr);
    
    // =================================================================
    // Pixel Access
    // =================================================================
    
    /// Get indexed pixel value at position
    /// @param x X coordinate (in atlas)
    /// @param y Y coordinate (in atlas)
    /// @return Palette index (0-15), or 0 if out of bounds
    uint8_t getIndexedPixel(int32_t x, int32_t y) const;
    
    /// Set indexed pixel value at position
    /// @param x X coordinate (in atlas)
    /// @param y Y coordinate (in atlas)
    /// @param index Palette index (0-15)
    void setIndexedPixel(int32_t x, int32_t y, uint8_t index);
    
    /// Get indexed pixel for specific tile
    /// @param tileID Tile ID (0-based)
    /// @param x X coordinate within tile
    /// @param y Y coordinate within tile
    /// @return Palette index (0-15), or 0 if out of bounds
    uint8_t getTileIndexedPixel(uint16_t tileID, int32_t x, int32_t y) const;
    
    /// Set indexed pixel for specific tile
    /// @param tileID Tile ID (0-based)
    /// @param x X coordinate within tile
    /// @param y Y coordinate within tile
    /// @param index Palette index (0-15)
    void setTileIndexedPixel(uint16_t tileID, int32_t x, int32_t y, uint8_t index);
    
    // =================================================================
    // Format Information
    // =================================================================
    
    /// Get tile format
    TileFormat getFormat() const { return m_format; }
    
    /// Check if tileset uses indexed color
    bool isIndexed() const { 
        return m_format == TileFormat::Indexed4Bit || 
               m_format == TileFormat::Indexed8Bit; 
    }
    
    /// Check if tileset uses 4-bit indexed color
    bool is4Bit() const { return m_format == TileFormat::Indexed4Bit; }
    
    /// Check if tileset uses 8-bit indexed color
    bool is8Bit() const { return m_format == TileFormat::Indexed8Bit; }
    
    /// Get color depth (bits per pixel)
    int32_t getColorDepth() const {
        switch (m_format) {
            case TileFormat::Indexed4Bit: return 4;
            case TileFormat::Indexed8Bit: return 8;
            case TileFormat::RGB888: return 24;
            case TileFormat::RGBA8888: return 32;
            default: return 0;
        }
    }
    
    /// Get maximum colors (for indexed formats)
    int32_t getMaxColors() const {
        switch (m_format) {
            case TileFormat::Indexed4Bit: return 16;
            case TileFormat::Indexed8Bit: return 256;
            default: return 0;
        }
    }
    
    // =================================================================
    // GPU Access
    // =================================================================
    
    /// Get indexed texture (Metal texture containing palette indices)
    /// Format: MTLPixelFormatR8Uint (each pixel = 0-15)
    MTLTexturePtr getIndexTexture() const { return m_indexTexture; }
    
    /// Upload indexed data to GPU
    /// @return true on success
    bool uploadIndexedData();
    
    /// Check if indexed data needs GPU upload
    bool isIndexedDirty() const { return m_indexedDirty; }
    
    /// Mark indexed data as dirty (needs GPU upload)
    void markIndexedDirty() { m_indexedDirty = true; }
    
    /// Clear indexed dirty flag
    void clearIndexedDirty() { m_indexedDirty = false; }
    
    // =================================================================
    // Direct Data Access
    // =================================================================
    
    /// Get raw indexed data pointer (read-only)
    const uint8_t* getIndexedData() const { return m_indexedData.data(); }
    
    /// Get raw indexed data pointer (mutable - marks dirty)
    uint8_t* getIndexedDataMutable() { 
        m_indexedDirty = true;
        return m_indexedData.data(); 
    }
    
    /// Get indexed data size in bytes
    size_t getIndexedDataSize() const { return m_indexedData.size(); }
    
    /// Get expected data size for dimensions
    size_t getExpectedDataSize() const {
        return m_textureWidth * m_textureHeight;
    }
    
    // =================================================================
    // Tile Operations
    // =================================================================
    
    /// Fill tile with palette index
    /// @param tileID Tile ID
    /// @param index Palette index to fill with
    void fillTile(uint16_t tileID, uint8_t index);
    
    /// Clear tile (set all pixels to index 0 = transparent)
    /// @param tileID Tile ID
    void clearTile(uint16_t tileID);
    
    /// Copy tile to another position
    /// @param srcTileID Source tile ID
    /// @param dstTileID Destination tile ID
    void copyTile(uint16_t srcTileID, uint16_t dstTileID);
    
    /// Flip tile horizontally
    /// @param tileID Tile ID
    void flipTileHorizontal(uint16_t tileID);
    
    /// Flip tile vertically
    /// @param tileID Tile ID
    void flipTileVertical(uint16_t tileID);
    
    // =================================================================
    // Statistics and Analysis
    // =================================================================
    
    /// Get palette index usage for a tile
    /// @param tileID Tile ID
    /// @param outUsage Array to fill with usage counts (size 16)
    void getTileIndexUsage(uint16_t tileID, int32_t* outUsage) const;
    
    /// Count unique indices used in a tile
    /// @param tileID Tile ID
    /// @return Number of unique indices (1-16)
    int32_t countUniqueIndices(uint16_t tileID) const;
    
    /// Check if tile uses specific index
    /// @param tileID Tile ID
    /// @param index Palette index to check
    /// @return true if tile uses this index
    bool tileUsesIndex(uint16_t tileID, uint8_t index) const;
    
    /// Get memory usage
    size_t getMemoryUsage() const {
        return m_indexedData.size() + sizeof(*this);
    }
    
    // =================================================================
    // Validation
    // =================================================================
    
    /// Check if tileset is valid
    bool isValid() const {
        return !m_indexedData.empty() && m_indexTexture != nullptr;
    }
    
    /// Validate indexed data (ensure all values are 0-15 for 4-bit)
    bool validateIndexedData() const;
    
    /// Clamp all indices to valid range
    void clampIndices();
    
private:
    // =================================================================
    // Internal State
    // =================================================================
    
    // Format
    TileFormat m_format;
    
    // CPU data storage
    std::vector<uint8_t> m_indexedData;  // Indexed pixel data (1 byte per pixel)
    
    // GPU resources
    MTLDevicePtr m_device;
    MTLTexturePtr m_indexTexture;  // R8Uint texture
    
    // Dirty tracking
    bool m_indexedDirty;
    
    // Dimensions (stored for convenience)
    int32_t m_textureWidth;
    int32_t m_textureHeight;
    
    // =================================================================
    // Internal Helpers
    // =================================================================
    
    /// Create GPU texture for indexed data
    bool createIndexTexture();
    
    /// Calculate atlas position for tile
    void getTileAtlasPosition(uint16_t tileID, int32_t& outX, int32_t& outY) const;
    
    /// Calculate linear index for pixel
    inline int32_t getPixelIndex(int32_t x, int32_t y) const {
        return y * m_textureWidth + x;
    }
    
    /// Find closest palette index for RGB color
    static uint8_t findClosestPaletteIndex(uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                          const PaletteColor* palette, int32_t paletteSize);
    
    /// Generate default palette (if no reference palette provided)
    static void generateDefaultPalette(PaletteColor* outPalette);
    
    /// Color distance (for palette quantization)
    static int32_t colorDistance(uint8_t r1, uint8_t g1, uint8_t b1,
                                uint8_t r2, uint8_t g2, uint8_t b2);
};

} // namespace SuperTerminal

#endif // TILESETINDEXED_H