//
// Tilemap.cpp
// SuperTerminal Framework - Tilemap System
//
// Tilemap implementation
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "Tilemap.h"
#include <algorithm>
#include <cstring>

namespace SuperTerminal {

// =================================================================
// Construction
// =================================================================

Tilemap::Tilemap() 
    : m_width(0)
    , m_height(0)
    , m_tileWidth(16)
    , m_tileHeight(16)
    , m_dirty(true)
{
}

Tilemap::Tilemap(int32_t width, int32_t height, int32_t tileWidth, int32_t tileHeight)
    : m_width(0)
    , m_height(0)
    , m_tileWidth(16)
    , m_tileHeight(16)
    , m_dirty(true)
{
    initialize(width, height, tileWidth, tileHeight);
}

Tilemap::~Tilemap() {
    // Vector cleanup is automatic
}

Tilemap::Tilemap(Tilemap&& other) noexcept
    : m_width(other.m_width)
    , m_height(other.m_height)
    , m_tileWidth(other.m_tileWidth)
    , m_tileHeight(other.m_tileHeight)
    , m_tiles(std::move(other.m_tiles))
    , m_name(std::move(other.m_name))
    , m_dirty(other.m_dirty)
{
    other.m_width = 0;
    other.m_height = 0;
    other.m_dirty = false;
}

Tilemap& Tilemap::operator=(Tilemap&& other) noexcept {
    if (this != &other) {
        m_width = other.m_width;
        m_height = other.m_height;
        m_tileWidth = other.m_tileWidth;
        m_tileHeight = other.m_tileHeight;
        m_tiles = std::move(other.m_tiles);
        m_name = std::move(other.m_name);
        m_dirty = other.m_dirty;
        
        other.m_width = 0;
        other.m_height = 0;
        other.m_dirty = false;
    }
    return *this;
}

// =================================================================
// Initialization
// =================================================================

void Tilemap::initialize(int32_t width, int32_t height, int32_t tileWidth, int32_t tileHeight) {
    if (width <= 0 || height <= 0) {
        return;
    }
    
    if (tileWidth <= 0) tileWidth = 16;
    if (tileHeight <= 0) tileHeight = 16;
    
    m_width = width;
    m_height = height;
    m_tileWidth = tileWidth;
    m_tileHeight = tileHeight;
    
    // Allocate tile data
    m_tiles.clear();
    m_tiles.resize(width * height);
    
    m_dirty = true;
}

void Tilemap::clear() {
    std::fill(m_tiles.begin(), m_tiles.end(), TileData());
    m_dirty = true;
}

void Tilemap::fill(TileData tile) {
    std::fill(m_tiles.begin(), m_tiles.end(), tile);
    m_dirty = true;
}

void Tilemap::fillRect(int32_t x, int32_t y, int32_t width, int32_t height, TileData tile) {
    // Clamp to bounds
    int32_t x1 = std::max(0, x);
    int32_t y1 = std::max(0, y);
    int32_t x2 = std::min(m_width, x + width);
    int32_t y2 = std::min(m_height, y + height);
    
    if (x1 >= x2 || y1 >= y2) {
        return;  // Nothing to fill
    }
    
    for (int32_t ty = y1; ty < y2; ty++) {
        for (int32_t tx = x1; tx < x2; tx++) {
            m_tiles[ty * m_width + tx] = tile;
        }
    }
    
    m_dirty = true;
}

// =================================================================
// Tile Access
// =================================================================

TileData Tilemap::getTile(int32_t x, int32_t y) const {
    if (!isInBounds(x, y)) {
        return TileData();  // Return empty tile
    }
    
    return m_tiles[y * m_width + x];
}

void Tilemap::setTile(int32_t x, int32_t y, TileData tile) {
    if (!isInBounds(x, y)) {
        return;
    }
    
    m_tiles[y * m_width + x] = tile;
    m_dirty = true;
}

// =================================================================
// Bulk Operations
// =================================================================

void Tilemap::copyRegion(const Tilemap& src,
                         int32_t srcX, int32_t srcY,
                         int32_t dstX, int32_t dstY,
                         int32_t width, int32_t height)
{
    // Clamp source region
    int32_t srcX1 = std::max(0, srcX);
    int32_t srcY1 = std::max(0, srcY);
    int32_t srcX2 = std::min(src.m_width, srcX + width);
    int32_t srcY2 = std::min(src.m_height, srcY + height);
    
    // Clamp destination region
    int32_t dstX1 = std::max(0, dstX);
    int32_t dstY1 = std::max(0, dstY);
    int32_t dstX2 = std::min(m_width, dstX + width);
    int32_t dstY2 = std::min(m_height, dstY + height);
    
    // Calculate actual copy size
    int32_t copyWidth = std::min(srcX2 - srcX1, dstX2 - dstX1);
    int32_t copyHeight = std::min(srcY2 - srcY1, dstY2 - dstY1);
    
    if (copyWidth <= 0 || copyHeight <= 0) {
        return;
    }
    
    // Copy tile by tile
    for (int32_t y = 0; y < copyHeight; y++) {
        for (int32_t x = 0; x < copyWidth; x++) {
            TileData tile = src.getTile(srcX1 + x, srcY1 + y);
            setTile(dstX1 + x, dstY1 + y, tile);
        }
    }
    
    m_dirty = true;
}

void Tilemap::copyRegionSelf(int32_t srcX, int32_t srcY,
                              int32_t dstX, int32_t dstY,
                              int32_t width, int32_t height)
{
    // Create temporary buffer to avoid overwriting source during copy
    std::vector<TileData> buffer;
    buffer.reserve(width * height);
    
    // Copy to buffer
    for (int32_t y = 0; y < height; y++) {
        for (int32_t x = 0; x < width; x++) {
            buffer.push_back(getTile(srcX + x, srcY + y));
        }
    }
    
    // Copy from buffer to destination
    int32_t idx = 0;
    for (int32_t y = 0; y < height; y++) {
        for (int32_t x = 0; x < width; x++) {
            setTile(dstX + x, dstY + y, buffer[idx++]);
        }
    }
    
    m_dirty = true;
}

// =================================================================
// Serialization
// =================================================================

size_t Tilemap::getMemorySize() const {
    return m_tiles.size() * sizeof(TileData) + sizeof(Tilemap);
}

std::vector<uint16_t> Tilemap::exportRawData() const {
    std::vector<uint16_t> data;
    data.reserve(m_tiles.size());
    
    for (const auto& tile : m_tiles) {
        data.push_back(tile.packed);
    }
    
    return data;
}

void Tilemap::importRawData(const std::vector<uint16_t>& data) {
    if (data.size() != m_tiles.size()) {
        return;  // Size mismatch
    }
    
    for (size_t i = 0; i < data.size(); i++) {
        m_tiles[i].packed = data[i];
    }
    
    m_dirty = true;
}

} // namespace SuperTerminal