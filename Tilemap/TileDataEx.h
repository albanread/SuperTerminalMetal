//
// TileDataEx.h
// SuperTerminal Framework - Tilemap System
//
// Enhanced 32-bit tile data structure with palette and z-order support
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef TILEDATAEX_H
#define TILEDATAEX_H

#include <cstdint>

namespace SuperTerminal {

/// TileDataEx: 32-bit packed tile information with palette support
///
/// Bit Layout (32 bits total):
/// [31:20] - Tile ID (12 bits = 4096 unique tiles)
/// [19:12] - Palette Index (8 bits = 256 palettes)
/// [11:9]  - Z-Order (3 bits = 8 priority levels: 0-7)
/// [8]     - Flip Y (vertical flip)
/// [7]     - Flip X (horizontal flip)
/// [6:5]   - Rotation (2 bits = 0°, 90°, 180°, 270°)
/// [4]     - Collision (quick collision flag)
/// [3:0]   - Reserved (future use)
///
/// This format allows:
/// - 4 bytes per tile (still compact)
/// - 4096 tiles per tileset (vs 2048 in TileData)
/// - 256 palette selections per tile
/// - 8 z-order priority levels
/// - Flip and rotation transformations
/// - Fast collision checks
///
/// Palette Convention (for 16-color palettes):
///   Index 0:  Transparent black (RGBA: 0,0,0,0)
///   Index 1:  Opaque black (RGBA: 0,0,0,255)
///   Index 2-15: 14 usable colors
///
struct TileDataEx {
    uint32_t packed = 0;
    
    // =================================================================
    // Constructors
    // =================================================================
    
    TileDataEx() = default;
    
    explicit TileDataEx(uint16_t tileID) {
        setTileID(tileID);
    }
    
    TileDataEx(uint16_t tileID, uint8_t paletteIndex, int8_t zOrder = 0) {
        setTileID(tileID);
        setPaletteIndex(paletteIndex);
        setZOrder(zOrder);
    }
    
    TileDataEx(uint16_t tileID, uint8_t paletteIndex, int8_t zOrder,
               bool flipX, bool flipY, uint8_t rotation = 0, bool collision = false) {
        setTileID(tileID);
        setPaletteIndex(paletteIndex);
        setZOrder(zOrder);
        setFlipX(flipX);
        setFlipY(flipY);
        setRotation(rotation);
        setCollision(collision);
    }
    
    // =================================================================
    // Tile ID Accessors
    // =================================================================
    
    /// Get tile ID (0-4095)
    inline uint16_t getTileID() const {
        return (packed >> 20) & 0xFFF;
    }
    
    /// Set tile ID (0-4095)
    inline void setTileID(uint16_t tileID) {
        // Clear tile ID bits and set new value
        packed = (packed & 0x000FFFFF) | ((uint32_t)(tileID & 0xFFF) << 20);
    }
    
    // =================================================================
    // Palette Accessors
    // =================================================================
    
    /// Get palette index (0-255)
    inline uint8_t getPaletteIndex() const {
        return (packed >> 12) & 0xFF;
    }
    
    /// Set palette index (0-255)
    inline void setPaletteIndex(uint8_t paletteIndex) {
        // Clear palette bits and set new value
        packed = (packed & 0xFFF00FFF) | ((uint32_t)paletteIndex << 12);
    }
    
    // =================================================================
    // Z-Order Accessors
    // =================================================================
    
    /// Get z-order priority (0-7, where 0=back, 7=front)
    inline uint8_t getZOrder() const {
        return (packed >> 9) & 0x7;
    }
    
    /// Set z-order priority (0-7)
    inline void setZOrder(uint8_t zOrder) {
        // Clear z-order bits and set new value
        packed = (packed & 0xFFFFF1FF) | ((uint32_t)(zOrder & 0x7) << 9);
    }
    
    // =================================================================
    // Transform Accessors
    // =================================================================
    
    /// Get horizontal flip flag
    inline bool getFlipX() const {
        return (packed >> 7) & 0x1;
    }
    
    /// Set horizontal flip
    inline void setFlipX(bool flip) {
        if (flip) {
            packed |= (1 << 7);
        } else {
            packed &= ~(1 << 7);
        }
    }
    
    /// Get vertical flip flag
    inline bool getFlipY() const {
        return (packed >> 8) & 0x1;
    }
    
    /// Set vertical flip
    inline void setFlipY(bool flip) {
        if (flip) {
            packed |= (1 << 8);
        } else {
            packed &= ~(1 << 8);
        }
    }
    
    /// Get rotation (0=0°, 1=90°, 2=180°, 3=270°)
    inline uint8_t getRotation() const {
        return (packed >> 5) & 0x3;
    }
    
    /// Set rotation (0-3)
    inline void setRotation(uint8_t rotation) {
        // Clear rotation bits
        packed &= ~(0x3 << 5);
        // Set new rotation
        packed |= ((rotation & 0x3) << 5);
    }
    
    // =================================================================
    // Collision and Flags
    // =================================================================
    
    /// Get collision flag
    inline bool getCollision() const {
        return (packed >> 4) & 0x1;
    }
    
    /// Set collision flag
    inline void setCollision(bool collision) {
        if (collision) {
            packed |= (1 << 4);
        } else {
            packed &= ~(1 << 4);
        }
    }
    
    /// Get all transform flags as byte (for rendering)
    inline uint8_t getTransformFlags() const {
        return (packed >> 4) & 0x1F;  // Bits 4-8
    }
    
    /// Set all transform flags at once
    inline void setTransformFlags(uint8_t flags) {
        packed = (packed & 0xFFFFFE0F) | (((uint32_t)flags & 0x1F) << 4);
    }
    
    // =================================================================
    // Reserved Bits Accessors (for future use)
    // =================================================================
    
    /// Get reserved bits (0-15)
    inline uint8_t getReserved() const {
        return packed & 0xF;
    }
    
    /// Set reserved bits
    inline void setReserved(uint8_t value) {
        packed = (packed & 0xFFFFFFF0) | (value & 0xF);
    }
    
    // =================================================================
    // Utilities
    // =================================================================
    
    /// Check if tile is empty (ID 0)
    inline bool isEmpty() const {
        return getTileID() == 0;
    }
    
    /// Clear tile (set to empty)
    inline void clear() {
        packed = 0;
    }
    
    /// Copy properties from another tile (keeps tile ID and palette)
    inline void copyPropertiesFrom(const TileDataEx& other) {
        uint16_t myID = getTileID();
        uint8_t myPalette = getPaletteIndex();
        packed = other.packed;
        setTileID(myID);
        setPaletteIndex(myPalette);
    }
    
    /// Get rotation angle in degrees
    inline float getRotationDegrees() const {
        return getRotation() * 90.0f;
    }
    
    /// Get rotation angle in radians
    inline float getRotationRadians() const {
        return getRotation() * 1.57079632679f;  // π/2
    }
    
    /// Check if tile uses transparency (palette index 0)
    inline bool hasTransparency() const {
        // Tiles can have transparent pixels if they reference palette index 0
        // This is a rendering hint, actual transparency depends on pixel data
        return true;  // All indexed tiles can have transparency
    }
    
    // =================================================================
    // Comparison
    // =================================================================
    
    bool operator==(const TileDataEx& other) const {
        return packed == other.packed;
    }
    
    bool operator!=(const TileDataEx& other) const {
        return packed != other.packed;
    }
    
    // =================================================================
    // Conversion to/from 16-bit TileData
    // =================================================================
    
    /// Create TileDataEx from TileData object
    static TileDataEx fromTileData(const TileData& tileData) {
        TileDataEx result;
        result.setTileID(tileData.getTileID());
        result.setPaletteIndex(0);  // Default palette
        result.setZOrder(3);  // Default z-order (TILEEX_ZORDER_NORMAL)
        result.setCollision(tileData.getCollision());
        result.setFlipX(tileData.getFlipX());
        result.setFlipY(tileData.getFlipY());
        result.setRotation(tileData.getRotation());
        return result;
    }
    
    /// Convert from legacy 16-bit TileData (loses palette and z-order)
    void fromTileData16(uint16_t tileData16) {
        // Extract from 16-bit format
        uint16_t tileID = (tileData16 >> 5) & 0x7FF;  // 11 bits
        bool collision = tileData16 & 0x1;
        bool flipX = (tileData16 >> 1) & 0x1;
        bool flipY = (tileData16 >> 2) & 0x1;
        uint8_t rotation = (tileData16 >> 3) & 0x3;
        
        // Convert to 32-bit format
        setTileID(tileID);
        setPaletteIndex(0);  // Default palette
        setZOrder(0);        // Default z-order
        setCollision(collision);
        setFlipX(flipX);
        setFlipY(flipY);
        setRotation(rotation);
    }
    
    /// Convert to legacy 16-bit TileData (loses palette and z-order)
    uint16_t toTileData16() const {
        uint16_t result = 0;
        
        // Pack into 16-bit format (tile ID limited to 11 bits)
        uint16_t tileID = getTileID() & 0x7FF;  // Clamp to 11 bits
        result |= (tileID << 5);
        result |= (getCollision() ? 1 : 0);
        result |= (getFlipX() ? (1 << 1) : 0);
        result |= (getFlipY() ? (1 << 2) : 0);
        result |= ((getRotation() & 0x3) << 3);
        
        return result;
    }
    
    // =================================================================
    // Debug
    // =================================================================
    
    /// Get human-readable string representation
    const char* toString() const;
};

// =================================================================
// Constants
// =================================================================

constexpr uint16_t TILEEX_EMPTY = 0;           // Empty tile
constexpr uint16_t TILEEX_MAX_ID = 4095;       // Maximum tile ID (12-bit)

constexpr uint8_t TILEEX_MAX_PALETTE = 255;    // Maximum palette index
constexpr uint8_t TILEEX_MAX_ZORDER = 7;       // Maximum z-order

constexpr uint8_t TILEEX_ROTATION_0   = 0;     // 0°
constexpr uint8_t TILEEX_ROTATION_90  = 1;     // 90° clockwise
constexpr uint8_t TILEEX_ROTATION_180 = 2;     // 180°
constexpr uint8_t TILEEX_ROTATION_270 = 3;     // 270° clockwise

// Z-order priority constants (semantic names)
constexpr uint8_t TILEEX_ZORDER_BACKGROUND  = 0;  // Far background
constexpr uint8_t TILEEX_ZORDER_BACK        = 1;  // Background
constexpr uint8_t TILEEX_ZORDER_MIDBACK     = 2;  // Mid-background
constexpr uint8_t TILEEX_ZORDER_NORMAL      = 3;  // Normal/default
constexpr uint8_t TILEEX_ZORDER_MIDFRONT    = 4;  // Mid-foreground
constexpr uint8_t TILEEX_ZORDER_FRONT       = 5;  // Foreground
constexpr uint8_t TILEEX_ZORDER_TOP         = 6;  // Top layer
constexpr uint8_t TILEEX_ZORDER_UI          = 7;  // UI/overlay

// Transform flags for convenience
constexpr uint8_t TILEEX_FLAG_COLLISION = 0x01;
constexpr uint8_t TILEEX_FLAG_ROTATE_90 = 0x02;
constexpr uint8_t TILEEX_FLAG_ROTATE_180 = 0x04;
constexpr uint8_t TILEEX_FLAG_FLIP_X    = 0x08;
constexpr uint8_t TILEEX_FLAG_FLIP_Y    = 0x10;

} // namespace SuperTerminal

#endif // TILEDATAEX_H