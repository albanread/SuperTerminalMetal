//
// Camera.h
// SuperTerminal Framework - Tilemap System
//
// Camera class for viewport control and smooth movement
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef CAMERA_H
#define CAMERA_H

#include <cstdint>

namespace SuperTerminal {

/// Rectangle structure for bounds
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    
    Rect() = default;
    Rect(float x, float y, float w, float h) : x(x), y(y), width(w), height(h) {}
    
    inline bool contains(float px, float py) const {
        return px >= x && px < (x + width) && py >= y && py < (y + height);
    }
    
    inline bool intersects(const Rect& other) const {
        return !(x >= other.x + other.width || 
                 x + width <= other.x ||
                 y >= other.y + other.height ||
                 y + height <= other.y);
    }
};

/// Point structure
struct Point {
    float x = 0.0f;
    float y = 0.0f;
    
    Point() = default;
    Point(float x, float y) : x(x), y(y) {}
};

/// Camera mode
enum class CameraMode {
    Free,           // Manual control only
    Follow,         // Smooth follow target
    Platform,       // Side-scroller (only X axis, Y fixed or smoothed differently)
    TopDown,        // Zelda-style (both axes with equal smoothing)
    SnapToRoom      // Room-based (snap to room boundaries)
};

/// Camera: Viewport control for tilemap rendering
///
/// Features:
/// - Smooth position interpolation
/// - Zoom support
/// - Bounded movement (keep in world)
/// - Follow target with configurable smoothness
/// - Screen shake effects
/// - Multiple camera modes
///
/// Thread Safety: Not thread-safe. Should be used from render thread only.
///
class Camera {
public:
    // =================================================================
    // Construction
    // =================================================================
    
    Camera();
    Camera(float viewportWidth, float viewportHeight);
    ~Camera();
    
    // =================================================================
    // Position Control
    // =================================================================
    
    /// Set camera position (world coordinates)
    void setPosition(float x, float y);
    
    /// Move camera by offset
    void move(float dx, float dy);
    
    /// Get camera position
    inline float getX() const { return m_x; }
    inline float getY() const { return m_y; }
    inline Point getPosition() const { return Point(m_x, m_y); }
    
    // =================================================================
    // Viewport
    // =================================================================
    
    /// Set viewport size (screen size in pixels)
    void setViewportSize(float width, float height);
    
    /// Get viewport size
    inline float getViewportWidth() const { return m_viewportWidth; }
    inline float getViewportHeight() const { return m_viewportHeight; }
    
    /// Get visible world bounds (rectangle in world space)
    Rect getVisibleBounds() const;
    
    // =================================================================
    // Zoom
    // =================================================================
    
    /// Set zoom level (1.0 = 100%, 2.0 = 200%, 0.5 = 50%)
    void setZoom(float zoom);
    
    /// Get current zoom
    inline float getZoom() const { return m_zoom; }
    
    /// Zoom in (multiply by factor)
    void zoomIn(float factor = 1.2f);
    
    /// Zoom out (divide by factor)
    void zoomOut(float factor = 1.2f);
    
    // =================================================================
    // Follow Target
    // =================================================================
    
    /// Enable follow mode with target position
    void follow(float targetX, float targetY);
    
    /// Set follow smoothness (0.0 = instant, 1.0 = very smooth)
    void setFollowSmoothing(float smoothness);
    
    /// Get follow smoothness
    inline float getFollowSmoothing() const { return m_smoothness; }
    
    /// Disable follow mode
    void stopFollowing();
    
    /// Check if following
    inline bool isFollowing() const { return m_following; }
    
    // =================================================================
    // Camera Mode
    // =================================================================
    
    /// Set camera mode
    void setMode(CameraMode mode);
    
    /// Get camera mode
    inline CameraMode getMode() const { return m_mode; }
    
    // =================================================================
    // Bounds
    // =================================================================
    
    /// Set world bounds (limits camera movement)
    void setBounds(float x, float y, float width, float height);
    
    /// Enable/disable bounds
    void setBounded(bool bounded);
    
    /// Check if bounded
    inline bool isBounded() const { return m_bounded; }
    
    /// Get world bounds
    inline Rect getWorldBounds() const { return m_worldBounds; }
    
    // =================================================================
    // Screen Shake
    // =================================================================
    
    /// Trigger screen shake
    /// @param magnitude Shake intensity in pixels
    /// @param duration Shake duration in seconds
    void shake(float magnitude, float duration);
    
    /// Get current shake offset
    inline float getShakeX() const { return m_shakeX; }
    inline float getShakeY() const { return m_shakeY; }
    
    // =================================================================
    // Update
    // =================================================================
    
    /// Update camera (call once per frame)
    /// @param dt Delta time in seconds
    void update(float dt);
    
    // =================================================================
    // Coordinate Conversion
    // =================================================================
    
    /// Convert world coordinates to screen coordinates
    Point worldToScreen(float worldX, float worldY) const;
    
    /// Convert screen coordinates to world coordinates
    Point screenToWorld(float screenX, float screenY) const;
    
    /// Get camera transform matrix values (for rendering)
    void getTransform(float& offsetX, float& offsetY, float& scale) const;
    
private:
    // Position
    float m_x = 0.0f;
    float m_y = 0.0f;
    
    // Viewport
    float m_viewportWidth = 800.0f;
    float m_viewportHeight = 600.0f;
    
    // Zoom
    float m_zoom = 1.0f;
    float m_minZoom = 0.1f;
    float m_maxZoom = 10.0f;
    
    // Follow target
    bool m_following = false;
    float m_targetX = 0.0f;
    float m_targetY = 0.0f;
    float m_smoothness = 0.1f;  // 0.0 = instant, 1.0 = very smooth
    
    // Camera mode
    CameraMode m_mode = CameraMode::Free;
    
    // Bounds
    bool m_bounded = false;
    Rect m_worldBounds;
    
    // Screen shake
    float m_shakeX = 0.0f;
    float m_shakeY = 0.0f;
    float m_shakeMagnitude = 0.0f;
    float m_shakeDecay = 5.0f;  // Decay rate per second
    float m_shakeTimer = 0.0f;
    
    // Helper methods
    void applyBounds();
    void updateShake(float dt);
    void updateFollow(float dt);
};

} // namespace SuperTerminal

#endif // CAMERA_H