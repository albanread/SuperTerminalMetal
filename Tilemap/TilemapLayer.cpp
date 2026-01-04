//
// TilemapLayer.cpp
// SuperTerminal Framework - Tilemap System
//
// TilemapLayer implementation
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "TilemapLayer.h"
#include "Tilemap.h"
#include "Tileset.h"
#include <cmath>

namespace SuperTerminal {

// =================================================================
// Construction
// =================================================================

TilemapLayer::TilemapLayer()
    : m_name("")
    , m_id(-1)
    , m_parallaxX(1.0f)
    , m_parallaxY(1.0f)
    , m_opacity(1.0f)
    , m_visible(true)
    , m_blendMode(BlendMode::Normal)
    , m_zOrder(0)
    , m_offsetX(0.0f)
    , m_offsetY(0.0f)
    , m_autoScrollX(0.0f)
    , m_autoScrollY(0.0f)
    , m_wrapX(false)
    , m_wrapY(false)
    , m_animationTime(0.0f)
    , m_static(false)
    , m_dirty(true)
{
}

TilemapLayer::TilemapLayer(const std::string& name)
    : m_name(name)
    , m_id(-1)
    , m_parallaxX(1.0f)
    , m_parallaxY(1.0f)
    , m_opacity(1.0f)
    , m_visible(true)
    , m_blendMode(BlendMode::Normal)
    , m_zOrder(0)
    , m_offsetX(0.0f)
    , m_offsetY(0.0f)
    , m_autoScrollX(0.0f)
    , m_autoScrollY(0.0f)
    , m_wrapX(false)
    , m_wrapY(false)
    , m_animationTime(0.0f)
    , m_static(false)
    , m_dirty(true)
{
}

TilemapLayer::~TilemapLayer() {
    // Shared pointers will clean up automatically
}

TilemapLayer::TilemapLayer(TilemapLayer&& other) noexcept
    : m_name(std::move(other.m_name))
    , m_id(other.m_id)
    , m_tilemap(std::move(other.m_tilemap))
    , m_tileset(std::move(other.m_tileset))
    , m_parallaxX(other.m_parallaxX)
    , m_parallaxY(other.m_parallaxY)
    , m_opacity(other.m_opacity)
    , m_visible(other.m_visible)
    , m_blendMode(other.m_blendMode)
    , m_zOrder(other.m_zOrder)
    , m_offsetX(other.m_offsetX)
    , m_offsetY(other.m_offsetY)
    , m_autoScrollX(other.m_autoScrollX)
    , m_autoScrollY(other.m_autoScrollY)
    , m_wrapX(other.m_wrapX)
    , m_wrapY(other.m_wrapY)
    , m_animationTime(other.m_animationTime)
    , m_static(other.m_static)
    , m_dirty(other.m_dirty)
{
    other.m_id = -1;
}

TilemapLayer& TilemapLayer::operator=(TilemapLayer&& other) noexcept {
    if (this != &other) {
        m_name = std::move(other.m_name);
        m_id = other.m_id;
        m_tilemap = std::move(other.m_tilemap);
        m_tileset = std::move(other.m_tileset);
        m_parallaxX = other.m_parallaxX;
        m_parallaxY = other.m_parallaxY;
        m_opacity = other.m_opacity;
        m_visible = other.m_visible;
        m_blendMode = other.m_blendMode;
        m_zOrder = other.m_zOrder;
        m_offsetX = other.m_offsetX;
        m_offsetY = other.m_offsetY;
        m_autoScrollX = other.m_autoScrollX;
        m_autoScrollY = other.m_autoScrollY;
        m_wrapX = other.m_wrapX;
        m_wrapY = other.m_wrapY;
        m_animationTime = other.m_animationTime;
        m_static = other.m_static;
        m_dirty = other.m_dirty;
        
        other.m_id = -1;
    }
    return *this;
}

// =================================================================
// Tilemap and Tileset
// =================================================================

void TilemapLayer::setTilemap(std::shared_ptr<Tilemap> tilemap) {
    m_tilemap = tilemap;
    m_dirty = true;
}

void TilemapLayer::setTileset(std::shared_ptr<Tileset> tileset) {
    m_tileset = tileset;
    m_dirty = true;
}

// =================================================================
// Update
// =================================================================

void TilemapLayer::update(float dt) {
    // Update animation time
    m_animationTime += dt;
    
    // Update auto-scroll
    if (m_autoScrollX != 0.0f || m_autoScrollY != 0.0f) {
        m_offsetX += m_autoScrollX * dt;
        m_offsetY += m_autoScrollY * dt;
        
        // Handle wrapping
        if (m_tilemap && m_wrapX) {
            float mapWidth = m_tilemap->getPixelWidth();
            if (mapWidth > 0.0f) {
                m_offsetX = fmodf(m_offsetX, mapWidth);
                if (m_offsetX < 0.0f) {
                    m_offsetX += mapWidth;
                }
            }
        }
        
        if (m_tilemap && m_wrapY) {
            float mapHeight = m_tilemap->getPixelHeight();
            if (mapHeight > 0.0f) {
                m_offsetY = fmodf(m_offsetY, mapHeight);
                if (m_offsetY < 0.0f) {
                    m_offsetY += mapHeight;
                }
            }
        }
        
        m_dirty = true;
    }
    
    // Check if tilemap changed
    if (m_tilemap && m_tilemap->isDirty()) {
        m_dirty = true;
    }
}

// =================================================================
// Utilities
// =================================================================

bool TilemapLayer::isReady() const {
    return m_tilemap && m_tileset && m_tileset->isValid();
}

void TilemapLayer::getEffectivePosition(float cameraX, float cameraY,
                                        float& outX, float& outY) const
{
    // Apply parallax to camera position
    outX = cameraX * m_parallaxX + m_offsetX;
    outY = cameraY * m_parallaxY + m_offsetY;
}

bool TilemapLayer::shouldRender() const {
    // Check basic conditions
    if (!m_visible) return false;
    if (m_opacity <= 0.0f) return false;
    if (!isReady()) return false;
    
    return true;
}

} // namespace SuperTerminal