//
// Tileset.cpp
// SuperTerminal Framework - Tilemap System
//
// Tileset implementation
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "Tileset.h"
#include "Camera.h"  // For Rect if needed
#include <cmath>

#ifdef __OBJC__
#import <Metal/Metal.h>
#endif

namespace SuperTerminal {

// =================================================================
// TileAnimation Implementation
// =================================================================

float TileAnimation::getTotalDuration() const {
    float total = 0.0f;
    for (const auto& frame : frames) {
        total += frame.duration;
    }
    return total;
}

uint16_t TileAnimation::getCurrentTile(float time) const {
    if (frames.empty()) {
        return 0;
    }
    
    float totalDuration = getTotalDuration();
    if (totalDuration <= 0.0f) {
        return frames[0].tileID;
    }
    
    // Loop or clamp time
    float animTime = loop ? fmodf(time, totalDuration) : (time < totalDuration ? time : totalDuration);
    
    // Find current frame
    float elapsed = 0.0f;
    for (const auto& frame : frames) {
        elapsed += frame.duration;
        if (animTime < elapsed) {
            return frame.tileID;
        }
    }
    
    // Return last frame
    return frames.back().tileID;
}

// =================================================================
// Construction
// =================================================================

Tileset::Tileset()
    : m_texture(nullptr)
    , m_textureWidth(0)
    , m_textureHeight(0)
    , m_tileWidth(16)
    , m_tileHeight(16)
    , m_columns(0)
    , m_rows(0)
    , m_tileCount(0)
    , m_margin(0)
    , m_spacing(0)
    , m_uvCacheDirty(true)
{
}

Tileset::~Tileset() {
#ifdef __OBJC__
    if (m_texture) {
        m_texture = nil;  // ARC will handle cleanup
    }
#endif
}

Tileset::Tileset(Tileset&& other) noexcept
    : m_texture(other.m_texture)
    , m_textureWidth(other.m_textureWidth)
    , m_textureHeight(other.m_textureHeight)
    , m_tileWidth(other.m_tileWidth)
    , m_tileHeight(other.m_tileHeight)
    , m_columns(other.m_columns)
    , m_rows(other.m_rows)
    , m_tileCount(other.m_tileCount)
    , m_margin(other.m_margin)
    , m_spacing(other.m_spacing)
    , m_name(std::move(other.m_name))
    , m_animations(std::move(other.m_animations))
    , m_properties(std::move(other.m_properties))
    , m_uvCache(std::move(other.m_uvCache))
    , m_uvCacheDirty(other.m_uvCacheDirty)
{
    other.m_texture = nullptr;
    other.m_textureWidth = 0;
    other.m_textureHeight = 0;
    other.m_columns = 0;
    other.m_rows = 0;
    other.m_tileCount = 0;
}

Tileset& Tileset::operator=(Tileset&& other) noexcept {
    if (this != &other) {
#ifdef __OBJC__
        if (m_texture) {
            m_texture = nil;
        }
#endif
        
        m_texture = other.m_texture;
        m_textureWidth = other.m_textureWidth;
        m_textureHeight = other.m_textureHeight;
        m_tileWidth = other.m_tileWidth;
        m_tileHeight = other.m_tileHeight;
        m_columns = other.m_columns;
        m_rows = other.m_rows;
        m_tileCount = other.m_tileCount;
        m_margin = other.m_margin;
        m_spacing = other.m_spacing;
        m_name = std::move(other.m_name);
        m_animations = std::move(other.m_animations);
        m_properties = std::move(other.m_properties);
        m_uvCache = std::move(other.m_uvCache);
        m_uvCacheDirty = other.m_uvCacheDirty;
        
        other.m_texture = nullptr;
        other.m_textureWidth = 0;
        other.m_textureHeight = 0;
        other.m_columns = 0;
        other.m_rows = 0;
        other.m_tileCount = 0;
    }
    return *this;
}

// =================================================================
// Initialization
// =================================================================

void Tileset::initialize(MTLTexturePtr texture, 
                         int32_t tileWidth, int32_t tileHeight,
                         const std::string& name)
{
    m_texture = texture;
    m_tileWidth = tileWidth;
    m_tileHeight = tileHeight;
    m_name = name;
    
    // Get texture dimensions
#ifdef __OBJC__
    if (texture) {
        id<MTLTexture> metalTexture = (id<MTLTexture>)texture;
        m_textureWidth = (int32_t)[metalTexture width];
        m_textureHeight = (int32_t)[metalTexture height];
    }
#else
    // For non-Objective-C contexts, dimensions must be set manually
    m_textureWidth = 0;
    m_textureHeight = 0;
#endif
    
    recalculateLayout();
}

void Tileset::setMargin(int32_t margin) {
    m_margin = margin;
    m_uvCacheDirty = true;
}

void Tileset::setSpacing(int32_t spacing) {
    m_spacing = spacing;
    m_uvCacheDirty = true;
}

void Tileset::recalculateLayout() {
    if (m_textureWidth <= 0 || m_textureHeight <= 0 || 
        m_tileWidth <= 0 || m_tileHeight <= 0) {
        m_columns = 0;
        m_rows = 0;
        m_tileCount = 0;
        return;
    }
    
    // Calculate how many tiles fit
    // Formula: (textureSize - 2*margin - spacing) / (tileSize + spacing)
    int32_t availableWidth = m_textureWidth - 2 * m_margin;
    int32_t availableHeight = m_textureHeight - 2 * m_margin;
    
    m_columns = (availableWidth + m_spacing) / (m_tileWidth + m_spacing);
    m_rows = (availableHeight + m_spacing) / (m_tileHeight + m_spacing);
    
    m_tileCount = m_columns * m_rows;
    
    // Mark UV cache as dirty
    m_uvCacheDirty = true;
}

// =================================================================
// Tile Access
// =================================================================

TexCoords Tileset::getTexCoords(uint16_t tileID) const {
    // Check cache
    if (!m_uvCacheDirty && tileID < m_uvCache.size()) {
        return m_uvCache[tileID];
    }
    
    // Calculate on-demand
    return calculateTexCoords(tileID);
}

uint16_t Tileset::getTileIDAt(int32_t column, int32_t row) const {
    if (column < 0 || column >= m_columns || row < 0 || row >= m_rows) {
        return 0;
    }
    return row * m_columns + column;
}

void Tileset::getTilePosition(uint16_t tileID, int32_t& outColumn, int32_t& outRow) const {
    if (m_columns == 0) {
        outColumn = 0;
        outRow = 0;
        return;
    }
    
    outColumn = tileID % m_columns;
    outRow = tileID / m_columns;
}

// =================================================================
// Animation
// =================================================================

void Tileset::addAnimation(uint16_t baseTileID, const TileAnimation& animation) {
    m_animations[baseTileID] = animation;
}

void Tileset::removeAnimation(uint16_t baseTileID) {
    m_animations.erase(baseTileID);
}

bool Tileset::hasAnimation(uint16_t baseTileID) const {
    return m_animations.count(baseTileID) > 0;
}

const TileAnimation* Tileset::getAnimation(uint16_t baseTileID) const {
    auto it = m_animations.find(baseTileID);
    return (it != m_animations.end()) ? &it->second : nullptr;
}

uint16_t Tileset::getAnimatedTile(uint16_t baseTileID, float time) const {
    auto it = m_animations.find(baseTileID);
    if (it == m_animations.end()) {
        return baseTileID;
    }
    
    return it->second.getCurrentTile(time);
}

// =================================================================
// Tile Properties
// =================================================================

void Tileset::setTileProperties(uint16_t tileID, const TileProperties& properties) {
    m_properties[tileID] = properties;
}

const TileProperties* Tileset::getTileProperties(uint16_t tileID) const {
    auto it = m_properties.find(tileID);
    return (it != m_properties.end()) ? &it->second : nullptr;
}

bool Tileset::hasTileProperties(uint16_t tileID) const {
    return m_properties.count(tileID) > 0;
}

void Tileset::removeTileProperties(uint16_t tileID) {
    m_properties.erase(tileID);
}

// =================================================================
// Bulk Operations
// =================================================================

void Tileset::clearAnimations() {
    m_animations.clear();
}

void Tileset::clearProperties() {
    m_properties.clear();
}

std::vector<uint16_t> Tileset::getAnimatedTiles() const {
    std::vector<uint16_t> result;
    result.reserve(m_animations.size());
    
    for (const auto& pair : m_animations) {
        result.push_back(pair.first);
    }
    
    return result;
}

std::vector<uint16_t> Tileset::getTilesWithProperties() const {
    std::vector<uint16_t> result;
    result.reserve(m_properties.size());
    
    for (const auto& pair : m_properties) {
        result.push_back(pair.first);
    }
    
    return result;
}

// =================================================================
// Private Helpers
// =================================================================

void Tileset::buildUVCache() {
    m_uvCache.clear();
    m_uvCache.reserve(m_tileCount);
    
    for (int32_t i = 0; i < m_tileCount; i++) {
        m_uvCache.push_back(calculateTexCoords(i));
    }
    
    m_uvCacheDirty = false;
}

TexCoords Tileset::calculateTexCoords(uint16_t tileID) const {
    if (tileID >= m_tileCount || m_textureWidth == 0 || m_textureHeight == 0) {
        return TexCoords(0.0f, 0.0f, 0.0f, 0.0f);
    }
    
    // Get tile position in grid
    int32_t column = tileID % m_columns;
    int32_t row = tileID / m_columns;
    
    // Calculate pixel position in texture
    int32_t pixelX = m_margin + column * (m_tileWidth + m_spacing);
    int32_t pixelY = m_margin + row * (m_tileHeight + m_spacing);
    
    // Convert to normalized UV coordinates (0.0 to 1.0)
    float u = (float)pixelX / (float)m_textureWidth;
    float v = (float)pixelY / (float)m_textureHeight;
    float w = (float)m_tileWidth / (float)m_textureWidth;
    float h = (float)m_tileHeight / (float)m_textureHeight;
    
    return TexCoords(u, v, w, h);
}

} // namespace SuperTerminal