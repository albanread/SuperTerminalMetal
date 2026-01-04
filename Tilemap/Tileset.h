//
// Tileset.h
// SuperTerminal Framework - Tilemap System
//
// Tileset class for texture atlas management
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef TILESET_H
#define TILESET_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <memory>

#ifdef __OBJC__
@protocol MTLTexture;
typedef id<MTLTexture> MTLTexturePtr;
#else
typedef void* MTLTexturePtr;
#endif

namespace SuperTerminal {

// Forward declarations
struct Rect;

/// UV coordinates for texture sampling
struct TexCoords {
    float u = 0.0f;      // Left
    float v = 0.0f;      // Top
    float width = 1.0f;  // Width in UV space
    float height = 1.0f; // Height in UV space
    
    TexCoords() = default;
    TexCoords(float u, float v, float w, float h) : u(u), v(v), width(w), height(h) {}
};

/// Tile animation frame
struct TileAnimationFrame {
    uint16_t tileID = 0;
    float duration = 0.1f;  // Duration in seconds
    
    TileAnimationFrame() = default;
    TileAnimationFrame(uint16_t id, float dur) : tileID(id), duration(dur) {}
};

/// Tile animation sequence
struct TileAnimation {
    std::vector<TileAnimationFrame> frames;
    bool loop = true;
    
    TileAnimation() = default;
    
    /// Get total animation duration
    float getTotalDuration() const;
    
    /// Get tile ID for given time
    uint16_t getCurrentTile(float time) const;
    
    /// Check if animation is valid
    bool isValid() const { return !frames.empty(); }
};

/// Tile properties (collision, gameplay attributes)
struct TileProperties {
    bool collision = false;      // Blocks movement
    bool platform = false;       // One-way platform
    bool ladder = false;         // Climbable
    bool water = false;          // Water/liquid
    bool spike = false;          // Damage
    bool slippery = false;       // Ice, etc.
    
    uint8_t customType = 0;      // Game-specific type ID
    float friction = 1.0f;       // Movement friction multiplier
    
    std::string metadata;        // Custom data (JSON, etc.)
    
    TileProperties() = default;
};

/// Tileset: Texture atlas containing multiple tiles
///
/// Manages a texture atlas with tile layout information.
/// Supports:
/// - Regular grid layout
/// - Margin and spacing
/// - Tile animations
/// - Per-tile properties
/// - UV coordinate generation
///
/// Thread Safety: Not thread-safe. Should be accessed from render thread only.
///
class Tileset {
public:
    // =================================================================
    // Construction
    // =================================================================
    
    Tileset();
    ~Tileset();
    
    // Disable copy, allow move
    Tileset(const Tileset&) = delete;
    Tileset& operator=(const Tileset&) = delete;
    Tileset(Tileset&&) noexcept;
    Tileset& operator=(Tileset&&) noexcept;
    
    // =================================================================
    // Initialization
    // =================================================================
    
    /// Initialize tileset with texture
    /// @param texture Metal texture (atlas)
    /// @param tileWidth Tile width in pixels
    /// @param tileHeight Tile height in pixels
    /// @param name Tileset name
    void initialize(MTLTexturePtr texture, 
                    int32_t tileWidth, int32_t tileHeight,
                    const std::string& name = "");
    
    /// Set margin (pixels around entire atlas)
    void setMargin(int32_t margin);
    
    /// Set spacing (pixels between tiles)
    void setSpacing(int32_t spacing);
    
    /// Recalculate tile layout (after changing margin/spacing)
    void recalculateLayout();
    
    // =================================================================
    // Tile Access
    // =================================================================
    
    /// Get texture coordinates for tile ID
    /// @param tileID Tile ID (0-based)
    /// @return UV coordinates, or default if out of range
    TexCoords getTexCoords(uint16_t tileID) const;
    
    /// Get tile ID at atlas position
    /// @param column Atlas column
    /// @param row Atlas row
    /// @return Tile ID or 0 if out of bounds
    uint16_t getTileIDAt(int32_t column, int32_t row) const;
    
    /// Get atlas position for tile ID
    /// @param tileID Tile ID
    /// @param outColumn Output column
    /// @param outRow Output row
    void getTilePosition(uint16_t tileID, int32_t& outColumn, int32_t& outRow) const;
    
    // =================================================================
    // Properties
    // =================================================================
    
    /// Get tileset name
    inline const std::string& getName() const { return m_name; }
    
    /// Set tileset name
    inline void setName(const std::string& name) { m_name = name; }
    
    /// Get texture
    inline MTLTexturePtr getTexture() const { return m_texture; }
    
    /// Get tile dimensions
    inline int32_t getTileWidth() const { return m_tileWidth; }
    inline int32_t getTileHeight() const { return m_tileHeight; }
    
    /// Get atlas dimensions
    inline int32_t getColumns() const { return m_columns; }
    inline int32_t getRows() const { return m_rows; }
    inline int32_t getTileCount() const { return m_tileCount; }
    
    /// Get texture dimensions
    inline int32_t getTextureWidth() const { return m_textureWidth; }
    inline int32_t getTextureHeight() const { return m_textureHeight; }
    
    /// Get margin and spacing
    inline int32_t getMargin() const { return m_margin; }
    inline int32_t getSpacing() const { return m_spacing; }
    
    /// Check if tileset is valid
    inline bool isValid() const { return m_texture != nullptr && m_tileCount > 0; }
    
    // =================================================================
    // Animation
    // =================================================================
    
    /// Add animation for tile
    /// @param baseTileID Base tile ID
    /// @param animation Animation data
    void addAnimation(uint16_t baseTileID, const TileAnimation& animation);
    
    /// Remove animation
    void removeAnimation(uint16_t baseTileID);
    
    /// Check if tile has animation
    bool hasAnimation(uint16_t baseTileID) const;
    
    /// Get animation for tile
    const TileAnimation* getAnimation(uint16_t baseTileID) const;
    
    /// Get animated tile ID for current time
    /// @param baseTileID Base tile ID
    /// @param time Current time in seconds
    /// @return Current animation frame tile ID, or baseTileID if not animated
    uint16_t getAnimatedTile(uint16_t baseTileID, float time) const;
    
    // =================================================================
    // Tile Properties
    // =================================================================
    
    /// Set properties for tile
    void setTileProperties(uint16_t tileID, const TileProperties& properties);
    
    /// Get properties for tile
    const TileProperties* getTileProperties(uint16_t tileID) const;
    
    /// Check if tile has properties
    bool hasTileProperties(uint16_t tileID) const;
    
    /// Remove properties for tile
    void removeTileProperties(uint16_t tileID);
    
    // =================================================================
    // Bulk Operations
    // =================================================================
    
    /// Clear all animations
    void clearAnimations();
    
    /// Clear all properties
    void clearProperties();
    
    /// Get all animated tile IDs
    std::vector<uint16_t> getAnimatedTiles() const;
    
    /// Get all tiles with properties
    std::vector<uint16_t> getTilesWithProperties() const;
    
private:
    // Texture
    MTLTexturePtr m_texture = nullptr;
    int32_t m_textureWidth = 0;
    int32_t m_textureHeight = 0;
    
    // Tile dimensions
    int32_t m_tileWidth = 16;
    int32_t m_tileHeight = 16;
    
    // Layout
    int32_t m_columns = 0;
    int32_t m_rows = 0;
    int32_t m_tileCount = 0;
    
    // Margin and spacing
    int32_t m_margin = 0;   // Pixels around atlas
    int32_t m_spacing = 0;  // Pixels between tiles
    
    // Metadata
    std::string m_name;
    
    // Animations (tile ID -> animation)
    std::unordered_map<uint16_t, TileAnimation> m_animations;
    
    // Properties (tile ID -> properties)
    std::unordered_map<uint16_t, TileProperties> m_properties;
    
    // Cached UV coordinates (for performance)
    std::vector<TexCoords> m_uvCache;
    bool m_uvCacheDirty = true;
    
    // Helper methods
    void buildUVCache();
    TexCoords calculateTexCoords(uint16_t tileID) const;
};

} // namespace SuperTerminal

#endif // TILESET_H