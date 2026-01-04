//
// TilemapLayer.h
// SuperTerminal Framework - Tilemap System
//
// TilemapLayer class combining tilemap, tileset, and rendering properties
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef TILEMAP_LAYER_H
#define TILEMAP_LAYER_H

#include <string>
#include <memory>
#include <cstdint>

namespace SuperTerminal {

// Forward declarations
class Tilemap;
class Tileset;

/// Blend mode for layer rendering
enum class BlendMode {
    Normal,      // Standard alpha blending
    Additive,    // Additive blending (brightens)
    Multiply,    // Multiply blending (darkens)
    Screen,      // Screen blending (brightens without over-exposing)
    Overlay      // Overlay blending
};

/// TilemapLayer: Rendering layer with tilemap, tileset, and visual properties
///
/// A layer combines:
/// - Tilemap data (which tiles to draw)
/// - Tileset (how to draw them)
/// - Rendering properties (parallax, opacity, etc.)
///
/// Features:
/// - Multi-layer parallax scrolling
/// - Per-layer opacity and blending
/// - Auto-scroll support
/// - Show/hide layers
/// - Z-ordering
///
/// Thread Safety: Not thread-safe. Should be used from render thread only.
///
class TilemapLayer {
public:
    // =================================================================
    // Construction
    // =================================================================
    
    TilemapLayer();
    explicit TilemapLayer(const std::string& name);
    ~TilemapLayer();
    
    // Disable copy, allow move
    TilemapLayer(const TilemapLayer&) = delete;
    TilemapLayer& operator=(const TilemapLayer&) = delete;
    TilemapLayer(TilemapLayer&&) noexcept;
    TilemapLayer& operator=(TilemapLayer&&) noexcept;
    
    // =================================================================
    // Layer Identity
    // =================================================================
    
    /// Get layer name
    inline const std::string& getName() const { return m_name; }
    
    /// Set layer name
    inline void setName(const std::string& name) { m_name = name; }
    
    /// Get layer ID (assigned by manager)
    inline int32_t getID() const { return m_id; }
    
    /// Set layer ID (internal use)
    inline void setID(int32_t id) { m_id = id; }
    
    // =================================================================
    // Tilemap and Tileset
    // =================================================================
    
    /// Set tilemap
    void setTilemap(std::shared_ptr<Tilemap> tilemap);
    
    /// Get tilemap
    inline std::shared_ptr<Tilemap> getTilemap() const { return m_tilemap; }
    
    /// Get tilemap (const pointer for renderer)
    inline const Tilemap* getTilemapPtr() const { return m_tilemap.get(); }
    
    /// Set tileset
    void setTileset(std::shared_ptr<Tileset> tileset);
    
    /// Get tileset
    inline std::shared_ptr<Tileset> getTileset() const { return m_tileset; }
    
    /// Get tileset (const pointer for renderer)
    inline const Tileset* getTilesetPtr() const { return m_tileset.get(); }
    
    /// Check if layer is ready to render
    bool isReady() const;
    
    // =================================================================
    // Rendering Properties
    // =================================================================
    
    /// Set parallax factor (0.0 = static, 1.0 = normal, >1.0 = foreground)
    inline void setParallax(float x, float y) { m_parallaxX = x; m_parallaxY = y; }
    inline void setParallaxX(float x) { m_parallaxX = x; }
    inline void setParallaxY(float y) { m_parallaxY = y; }
    
    /// Get parallax factors
    inline float getParallaxX() const { return m_parallaxX; }
    inline float getParallaxY() const { return m_parallaxY; }
    inline void getParallax(float& x, float& y) const { x = m_parallaxX; y = m_parallaxY; }
    
    /// Set opacity (0.0 = transparent, 1.0 = opaque)
    inline void setOpacity(float opacity) { 
        m_opacity = opacity < 0.0f ? 0.0f : (opacity > 1.0f ? 1.0f : opacity);
    }
    
    /// Get opacity
    inline float getOpacity() const { return m_opacity; }
    
    /// Set visibility
    inline void setVisible(bool visible) { m_visible = visible; }
    
    /// Get visibility
    inline bool isVisible() const { return m_visible; }
    
    /// Set blend mode
    inline void setBlendMode(BlendMode mode) { m_blendMode = mode; }
    
    /// Get blend mode
    inline BlendMode getBlendMode() const { return m_blendMode; }
    
    /// Set Z-order (rendering order, lower = back)
    inline void setZOrder(int32_t z) { m_zOrder = z; }
    
    /// Get Z-order
    inline int32_t getZOrder() const { return m_zOrder; }
    
    // =================================================================
    // Offset and Auto-scroll
    // =================================================================
    
    /// Set layer offset (in pixels)
    inline void setOffset(float x, float y) { m_offsetX = x; m_offsetY = y; }
    inline void setOffsetX(float x) { m_offsetX = x; }
    inline void setOffsetY(float y) { m_offsetY = y; }
    
    /// Get layer offset
    inline float getOffsetX() const { return m_offsetX; }
    inline float getOffsetY() const { return m_offsetY; }
    
    /// Set auto-scroll speed (pixels per second)
    inline void setAutoScroll(float x, float y) { m_autoScrollX = x; m_autoScrollY = y; }
    inline void setAutoScrollX(float x) { m_autoScrollX = x; }
    inline void setAutoScrollY(float y) { m_autoScrollY = y; }
    
    /// Get auto-scroll speed
    inline float getAutoScrollX() const { return m_autoScrollX; }
    inline float getAutoScrollY() const { return m_autoScrollY; }
    
    /// Enable/disable wrapping (for infinite scrolling)
    inline void setWrap(bool wrapX, bool wrapY) { m_wrapX = wrapX; m_wrapY = wrapY; }
    inline void setWrapX(bool wrap) { m_wrapX = wrap; }
    inline void setWrapY(bool wrap) { m_wrapY = wrap; }
    
    /// Get wrap settings
    inline bool getWrapX() const { return m_wrapX; }
    inline bool getWrapY() const { return m_wrapY; }
    
    // =================================================================
    // Animation Time
    // =================================================================
    
    /// Set animation time (for animated tiles)
    inline void setAnimationTime(float time) { m_animationTime = time; }
    
    /// Get animation time
    inline float getAnimationTime() const { return m_animationTime; }
    
    // =================================================================
    // Update
    // =================================================================
    
    /// Update layer (auto-scroll, animations, etc.)
    /// @param dt Delta time in seconds
    void update(float dt);
    
    // =================================================================
    // Rendering Hints
    // =================================================================
    
    /// Mark layer as static (never changes, can be pre-rendered)
    inline void setStatic(bool isStatic) { m_static = isStatic; }
    
    /// Check if layer is static
    inline bool isStatic() const { return m_static; }
    
    /// Mark layer as dirty (needs re-rendering)
    inline void markDirty() { m_dirty = true; }
    
    /// Check if layer is dirty
    inline bool isDirty() const { return m_dirty; }
    
    /// Clear dirty flag
    inline void clearDirty() { m_dirty = false; }
    
    // =================================================================
    // Utilities
    // =================================================================
    
    /// Get effective position with parallax applied
    void getEffectivePosition(float cameraX, float cameraY, 
                             float& outX, float& outY) const;
    
    /// Check if layer should be rendered (visible, opacity > 0, etc.)
    bool shouldRender() const;
    
private:
    // Identity
    std::string m_name;
    int32_t m_id = -1;
    
    // Data
    std::shared_ptr<Tilemap> m_tilemap;
    std::shared_ptr<Tileset> m_tileset;
    
    // Rendering properties
    float m_parallaxX = 1.0f;
    float m_parallaxY = 1.0f;
    float m_opacity = 1.0f;
    bool m_visible = true;
    BlendMode m_blendMode = BlendMode::Normal;
    int32_t m_zOrder = 0;
    
    // Offset and scrolling
    float m_offsetX = 0.0f;
    float m_offsetY = 0.0f;
    float m_autoScrollX = 0.0f;
    float m_autoScrollY = 0.0f;
    bool m_wrapX = false;
    bool m_wrapY = false;
    
    // Animation
    float m_animationTime = 0.0f;
    
    // Optimization flags
    bool m_static = false;
    bool m_dirty = true;
};

} // namespace SuperTerminal

#endif // TILEMAP_LAYER_H