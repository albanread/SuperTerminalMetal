//
// TileData.h
// SuperTerminal Framework - Tilemap System
//
// Efficient 16-bit tile data structure with flip/rotation support
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef TILEDATA_H
#define TILEDATA_H

#include <cstdint>

namespace SuperTerminal {

/// TileData: 16-bit packed tile information
///
/// Bit Layout (16 bits total):
/// [15:5] - Tile ID (11 bits = 2048 unique tiles)
/// [4:3]  - Rotation (2 bits = 0°, 90°, 180°, 270°)
/// [2]    - Flip Y (vertical flip)
/// [1]    - Flip X (horizontal flip)
/// [0]    - Collision (quick collision flag)
///
/// This compact format allows:
/// - 2 bytes per tile (memory efficient)
/// - 2048 tiles per tileset
/// - Flip and rotation transformations
/// - Fast collision checks
///
struct TileData {
    uint16_t packed = 0;
    
    // =================================================================
    // Constructors
    // =================================================================
    
    TileData() = default;
    
    explicit TileData(uint16_t tileID) {
        setTileID(tileID);
    }
    
    TileData(uint16_t tileID, bool flipX, bool flipY, uint8_t rotation = 0, bool collision = false) {
        setTileID(tileID);
        setFlipX(flipX);
        setFlipY(flipY);
        setRotation(rotation);
        setCollision(collision);
    }
    
    // =================================================================
    // Accessors
    // =================================================================
    
    /// Get tile ID (0-2047)
    inline uint16_t getTileID() const {
        return (packed >> 5) & 0x7FF;
    }
    
    /// Get horizontal flip flag
    inline bool getFlipX() const {
        return (packed >> 1) & 0x1;
    }
    
    /// Get vertical flip flag
    inline bool getFlipY() const {
        return (packed >> 2) & 0x1;
    }
    
    /// Get rotation (0=0°, 1=90°, 2=180°, 3=270°)
    inline uint8_t getRotation() const {
        return (packed >> 3) & 0x3;
    }
    
    /// Get collision flag
    inline bool getCollision() const {
        return packed & 0x1;
    }
    
    /// Get all flags as byte (useful for rendering)
    inline uint8_t getFlags() const {
        return packed & 0x1F;  // Lower 5 bits
    }
    
    // =================================================================
    // Mutators
    // =================================================================
    
    /// Set tile ID (0-2047)
    inline void setTileID(uint16_t tileID) {
        // Clear tile ID bits and set new value
        packed = (packed & 0x001F) | ((tileID & 0x7FF) << 5);
    }
    
    /// Set horizontal flip
    inline void setFlipX(bool flip) {
        if (flip) {
            packed |= (1 << 1);
        } else {
            packed &= ~(1 << 1);
        }
    }
    
    /// Set vertical flip
    inline void setFlipY(bool flip) {
        if (flip) {
            packed |= (1 << 2);
        } else {
            packed &= ~(1 << 2);
        }
    }
    
    /// Set rotation (0-3)
    inline void setRotation(uint8_t rotation) {
        // Clear rotation bits
        packed &= ~(0x3 << 3);
        // Set new rotation
        packed |= ((rotation & 0x3) << 3);
    }
    
    /// Set collision flag
    inline void setCollision(bool collision) {
        if (collision) {
            packed |= 0x1;
        } else {
            packed &= ~0x1;
        }
    }
    
    /// Set all flags at once
    inline void setFlags(uint8_t flags) {
        packed = (packed & 0xFFE0) | (flags & 0x1F);
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
    
    /// Copy properties from another tile (keeps tile ID)
    inline void copyPropertiesFrom(const TileData& other) {
        uint16_t myID = getTileID();
        packed = (other.packed & 0x001F) | (myID << 5);
    }
    
    /// Get rotation angle in degrees
    inline float getRotationDegrees() const {
        return getRotation() * 90.0f;
    }
    
    /// Get rotation angle in radians
    inline float getRotationRadians() const {
        return getRotation() * 1.57079632679f;  // π/2
    }
    
    // =================================================================
    // Comparison
    // =================================================================
    
    bool operator==(const TileData& other) const {
        return packed == other.packed;
    }
    
    bool operator!=(const TileData& other) const {
        return packed != other.packed;
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

constexpr uint16_t TILE_EMPTY = 0;           // Empty tile
constexpr uint16_t TILE_MAX_ID = 2047;       // Maximum tile ID

constexpr uint8_t TILE_ROTATION_0   = 0;     // 0°
constexpr uint8_t TILE_ROTATION_90  = 1;     // 90° clockwise
constexpr uint8_t TILE_ROTATION_180 = 2;     // 180°
constexpr uint8_t TILE_ROTATION_270 = 3;     // 270° clockwise

// Flip flags for convenience
constexpr uint8_t TILE_FLAG_COLLISION = 0x01;
constexpr uint8_t TILE_FLAG_FLIP_X    = 0x02;
constexpr uint8_t TILE_FLAG_FLIP_Y    = 0x04;

} // namespace SuperTerminal

#endif // TILEDATA_H
