//
// Camera.cpp
// SuperTerminal Framework - Tilemap System
//
// Camera implementation
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "Camera.h"
#include <cmath>
#include <cstdlib>

namespace {
    // Helper clamp function for C++14 compatibility
    template<typename T>
    inline T clamp(T value, T min, T max) {
        return value < min ? min : (value > max ? max : value);
    }
}

namespace SuperTerminal {

// =================================================================
// Construction
// =================================================================

Camera::Camera()
    : m_x(0.0f)
    , m_y(0.0f)
    , m_viewportWidth(800.0f)
    , m_viewportHeight(600.0f)
    , m_zoom(1.0f)
    , m_minZoom(0.1f)
    , m_maxZoom(10.0f)
    , m_following(false)
    , m_targetX(0.0f)
    , m_targetY(0.0f)
    , m_smoothness(0.1f)
    , m_mode(CameraMode::Free)
    , m_bounded(false)
    , m_shakeX(0.0f)
    , m_shakeY(0.0f)
    , m_shakeMagnitude(0.0f)
    , m_shakeDecay(5.0f)
    , m_shakeTimer(0.0f)
{
}

Camera::Camera(float viewportWidth, float viewportHeight)
    : m_x(0.0f)
    , m_y(0.0f)
    , m_viewportWidth(viewportWidth)
    , m_viewportHeight(viewportHeight)
    , m_zoom(1.0f)
    , m_minZoom(0.1f)
    , m_maxZoom(10.0f)
    , m_following(false)
    , m_targetX(0.0f)
    , m_targetY(0.0f)
    , m_smoothness(0.1f)
    , m_mode(CameraMode::Free)
    , m_bounded(false)
    , m_shakeX(0.0f)
    , m_shakeY(0.0f)
    , m_shakeMagnitude(0.0f)
    , m_shakeDecay(5.0f)
    , m_shakeTimer(0.0f)
{
}

Camera::~Camera() {
}

// =================================================================
// Position Control
// =================================================================

void Camera::setPosition(float x, float y) {
    m_x = x;
    m_y = y;
    applyBounds();
}

void Camera::move(float dx, float dy) {
    m_x += dx;
    m_y += dy;
    applyBounds();
}

// =================================================================
// Viewport
// =================================================================

void Camera::setViewportSize(float width, float height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
}

Rect Camera::getVisibleBounds() const {
    float halfWidth = m_viewportWidth / (2.0f * m_zoom);
    float halfHeight = m_viewportHeight / (2.0f * m_zoom);
    
    return Rect(
        m_x - halfWidth + m_shakeX,
        m_y - halfHeight + m_shakeY,
        m_viewportWidth / m_zoom,
        m_viewportHeight / m_zoom
    );
}

// =================================================================
// Zoom
// =================================================================

void Camera::setZoom(float zoom) {
    m_zoom = clamp(zoom, m_minZoom, m_maxZoom);
}

void Camera::zoomIn(float factor) {
    setZoom(m_zoom * factor);
}

void Camera::zoomOut(float factor) {
    setZoom(m_zoom / factor);
}

// =================================================================
// Follow Target
// =================================================================

void Camera::follow(float targetX, float targetY) {
    m_following = true;
    m_targetX = targetX;
    m_targetY = targetY;
}

void Camera::setFollowSmoothing(float smoothness) {
    m_smoothness = clamp(smoothness, 0.0f, 1.0f);
}

void Camera::stopFollowing() {
    m_following = false;
}

// =================================================================
// Camera Mode
// =================================================================

void Camera::setMode(CameraMode mode) {
    m_mode = mode;
}

// =================================================================
// Bounds
// =================================================================

void Camera::setBounds(float x, float y, float width, float height) {
    m_worldBounds = Rect(x, y, width, height);
    m_bounded = true;
    applyBounds();
}

void Camera::setBounded(bool bounded) {
    m_bounded = bounded;
    if (bounded) {
        applyBounds();
    }
}

void Camera::applyBounds() {
    if (!m_bounded) {
        return;
    }
    
    // Camera position is now top-left of viewport (not center)
    float viewportWidth = m_viewportWidth / m_zoom;
    float viewportHeight = m_viewportHeight / m_zoom;
    
    // Clamp camera position to keep viewport within world bounds
    float minX = m_worldBounds.x;
    float maxX = m_worldBounds.x + m_worldBounds.width - viewportWidth;
    float minY = m_worldBounds.y;
    float maxY = m_worldBounds.y + m_worldBounds.height - viewportHeight;
    
    // Handle case where viewport is larger than world
    if (m_worldBounds.width < viewportWidth) {
        m_x = m_worldBounds.x;
    } else {
        m_x = clamp(m_x, minX, maxX);
    }
    
    if (m_worldBounds.height < viewportHeight) {
        m_y = m_worldBounds.y;
    } else {
        m_y = clamp(m_y, minY, maxY);
    }
}

// =================================================================
// Screen Shake
// =================================================================

void Camera::shake(float magnitude, float duration) {
    m_shakeMagnitude = magnitude;
    m_shakeTimer = duration;
}

void Camera::updateShake(float dt) {
    if (m_shakeTimer > 0.0f) {
        m_shakeTimer -= dt;
        
        // Random shake offset
        float progress = 1.0f - (m_shakeTimer / (m_shakeTimer + dt));
        float currentMagnitude = m_shakeMagnitude * (1.0f - progress);
        
        m_shakeX = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * currentMagnitude;
        m_shakeY = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * currentMagnitude;
    } else {
        m_shakeX = 0.0f;
        m_shakeY = 0.0f;
        m_shakeMagnitude = 0.0f;
    }
}

// =================================================================
// Update
// =================================================================

void Camera::updateFollow(float dt) {
    if (!m_following) {
        return;
    }
    
    // Smooth interpolation factor
    // Convert smoothness to frame-rate independent interpolation
    float t = 1.0f - std::pow(1.0f - m_smoothness, dt * 60.0f);
    
    switch (m_mode) {
        case CameraMode::Free:
        case CameraMode::Follow:
        case CameraMode::TopDown:
            // Follow on both axes
            m_x = m_x + (m_targetX - m_x) * t;
            m_y = m_y + (m_targetY - m_y) * t;
            break;
            
        case CameraMode::Platform:
            // Only follow X axis, Y is fixed or separate
            m_x = m_x + (m_targetX - m_x) * t;
            break;
            
        case CameraMode::SnapToRoom:
            // Snap to room boundaries (not implemented yet)
            m_x = m_targetX;
            m_y = m_targetY;
            break;
    }
}

void Camera::update(float dt) {
    // Update follow
    updateFollow(dt);
    
    // Apply bounds
    applyBounds();
    
    // Update screen shake
    updateShake(dt);
}

// =================================================================
// Coordinate Conversion
// =================================================================

Point Camera::worldToScreen(float worldX, float worldY) const {
    // Apply camera transform
    float relX = (worldX - m_x) * m_zoom;
    float relY = (worldY - m_y) * m_zoom;
    
    // Convert to screen space (centered)
    float screenX = relX + m_viewportWidth * 0.5f + m_shakeX * m_zoom;
    float screenY = relY + m_viewportHeight * 0.5f + m_shakeY * m_zoom;
    
    return Point(screenX, screenY);
}

Point Camera::screenToWorld(float screenX, float screenY) const {
    // Remove centering
    float relX = screenX - m_viewportWidth * 0.5f - m_shakeX * m_zoom;
    float relY = screenY - m_viewportHeight * 0.5f - m_shakeY * m_zoom;
    
    // Apply inverse camera transform
    float worldX = relX / m_zoom + m_x;
    float worldY = relY / m_zoom + m_y;
    
    return Point(worldX, worldY);
}

void Camera::getTransform(float& offsetX, float& offsetY, float& scale) const {
    offsetX = m_x + m_shakeX;
    offsetY = m_y + m_shakeY;
    scale = m_zoom;
}

} // namespace SuperTerminal