//
// TilemapEx.cpp
// SuperTerminal Framework - Tilemap System
//
// Implementation of TilemapEx class
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "TilemapEx.h"
#include <cstring>
#include <algorithm>

namespace SuperTerminal {

// =============================================================================
// Construction / Destruction
// =============================================================================

TilemapEx::TilemapEx()
    : Tilemap()
{
}

TilemapEx::TilemapEx(int32_t width, int32_t height, int32_t tileWidth, int32_t tileHeight)
    : Tilemap()
{
    initializeEx(width, height, tileWidth, tileHeight);
}

TilemapEx::~TilemapEx() {
}

TilemapEx::TilemapEx(TilemapEx&& other) noexcept
    : Tilemap(std::move(other))
    , m_tilesEx(std::move(other.m_tilesEx))
{
}

TilemapEx& TilemapEx::operator=(TilemapEx&& other) noexcept {
    if (this != &other) {
        Tilemap::operator=(std::move(other));
        m_tilesEx = std::move(other.m_tilesEx);
    }
    return *this;
}

// =============================================================================
// Initialization
// =============================================================================

void TilemapEx::initializeEx(int32_t width, int32_t height, int32_t tileWidth, int32_t tileHeight) {
    // Initialize base class (but don't use its tile storage)
    Tilemap::initialize(width, height, tileWidth, tileHeight);
    
    // Allocate 32-bit tile storage
    m_tilesEx.clear();
    m_tilesEx.resize(width * height);
    
    // Initialize all tiles to empty with default palette
    for (auto& tile : m_tilesEx) {
        tile.clear();
        tile.setPaletteIndex(0);
        tile.setZOrder(TILEEX_ZORDER_NORMAL);
    }
    
    markDirty();
}

void TilemapEx::clearEx() {
    for (auto& tile : m_tilesEx) {
        tile.clear();
        tile.setPaletteIndex(0);
        tile.setZOrder(TILEEX_ZORDER_NORMAL);
    }
    markDirty();
}

void TilemapEx::fillEx(TileDataEx tile) {
    std::fill(m_tilesEx.begin(), m_tilesEx.end(), tile);
    markDirty();
}

void TilemapEx::fillRectEx(int32_t x, int32_t y, int32_t width, int32_t height, TileDataEx tile) {
    // Clamp to bounds
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > getWidth()) width = getWidth() - x;
    if (y + height > getHeight()) height = getHeight() - y;
    
    if (width <= 0 || height <= 0) return;
    
    // Fill rectangle
    for (int32_t dy = 0; dy < height; dy++) {
        for (int32_t dx = 0; dx < width; dx++) {
            int32_t index = coordsToIndex(x + dx, y + dy);
            m_tilesEx[index] = tile;
        }
    }
    
    markDirty();
}

// =============================================================================
// Extended Tile Access
// =============================================================================

TileDataEx TilemapEx::getTileEx(int32_t x, int32_t y) const {
    if (!isInBounds(x, y)) {
        return TileDataEx();
    }
    
    int32_t index = coordsToIndex(x, y);
    return m_tilesEx[index];
}

void TilemapEx::setTileEx(int32_t x, int32_t y, TileDataEx tile) {
    if (!isInBounds(x, y)) {
        return;
    }
    
    int32_t index = coordsToIndex(x, y);
    m_tilesEx[index] = tile;
    markDirty();
}

// =============================================================================
// Palette-Aware Operations
// =============================================================================

void TilemapEx::setTileWithPalette(int32_t x, int32_t y, 
                                   uint16_t tileID, 
                                   uint8_t paletteIndex,
                                   uint8_t zOrder,
                                   bool flipX,
                                   bool flipY,
                                   uint8_t rotation) {
    if (!isInBounds(x, y)) {
        return;
    }
    
    TileDataEx tile;
    tile.setTileID(tileID);
    tile.setPaletteIndex(paletteIndex);
    tile.setZOrder(zOrder);
    tile.setFlipX(flipX);
    tile.setFlipY(flipY);
    tile.setRotation(rotation);
    
    int32_t index = coordsToIndex(x, y);
    m_tilesEx[index] = tile;
    markDirty();
}

void TilemapEx::setTilePalette(int32_t x, int32_t y, uint8_t paletteIndex) {
    if (!isInBounds(x, y)) {
        return;
    }
    
    int32_t index = coordsToIndex(x, y);
    m_tilesEx[index].setPaletteIndex(paletteIndex);
    markDirty();
}

uint8_t TilemapEx::getTilePalette(int32_t x, int32_t y) const {
    if (!isInBounds(x, y)) {
        return 0;
    }
    
    int32_t index = coordsToIndex(x, y);
    return m_tilesEx[index].getPaletteIndex();
}

void TilemapEx::setTileZOrder(int32_t x, int32_t y, uint8_t zOrder) {
    if (!isInBounds(x, y)) {
        return;
    }
    
    int32_t index = coordsToIndex(x, y);
    m_tilesEx[index].setZOrder(zOrder);
    markDirty();
}

uint8_t TilemapEx::getTileZOrder(int32_t x, int32_t y) const {
    if (!isInBounds(x, y)) {
        return 0;
    }
    
    int32_t index = coordsToIndex(x, y);
    return m_tilesEx[index].getZOrder();
}

// =============================================================================
// Bulk Palette Operations
// =============================================================================

void TilemapEx::setPaletteForRegion(int32_t x, int32_t y, 
                                    int32_t width, int32_t height,
                                    uint8_t paletteIndex) {
    // Clamp to bounds
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > getWidth()) width = getWidth() - x;
    if (y + height > getHeight()) height = getHeight() - y;
    
    if (width <= 0 || height <= 0) return;
    
    // Set palette for region
    for (int32_t dy = 0; dy < height; dy++) {
        for (int32_t dx = 0; dx < width; dx++) {
            int32_t index = coordsToIndex(x + dx, y + dy);
            m_tilesEx[index].setPaletteIndex(paletteIndex);
        }
    }
    
    markDirty();
}

void TilemapEx::setZOrderForRegion(int32_t x, int32_t y,
                                   int32_t width, int32_t height,
                                   uint8_t zOrder) {
    // Clamp to bounds
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > getWidth()) width = getWidth() - x;
    if (y + height > getHeight()) height = getHeight() - y;
    
    if (width <= 0 || height <= 0) return;
    
    // Set z-order for region
    for (int32_t dy = 0; dy < height; dy++) {
        for (int32_t dx = 0; dx < width; dx++) {
            int32_t index = coordsToIndex(x + dx, y + dy);
            m_tilesEx[index].setZOrder(zOrder);
        }
    }
    
    markDirty();
}

void TilemapEx::replacePalette(uint8_t oldPalette, uint8_t newPalette) {
    for (auto& tile : m_tilesEx) {
        if (tile.getPaletteIndex() == oldPalette) {
            tile.setPaletteIndex(newPalette);
        }
    }
    markDirty();
}

// =============================================================================
// Bulk Copy Operations
// =============================================================================

void TilemapEx::copyRegionEx(const TilemapEx& src, 
                             int32_t srcX, int32_t srcY,
                             int32_t dstX, int32_t dstY,
                             int32_t width, int32_t height) {
    // Clamp source region
    if (srcX < 0) { width += srcX; dstX -= srcX; srcX = 0; }
    if (srcY < 0) { height += srcY; dstY -= srcY; srcY = 0; }
    if (srcX + width > src.getWidth()) width = src.getWidth() - srcX;
    if (srcY + height > src.getHeight()) height = src.getHeight() - srcY;
    
    // Clamp destination region
    if (dstX < 0) { width += dstX; srcX -= dstX; dstX = 0; }
    if (dstY < 0) { height += dstY; srcY -= dstY; dstY = 0; }
    if (dstX + width > getWidth()) width = getWidth() - dstX;
    if (dstY + height > getHeight()) height = getHeight() - dstY;
    
    if (width <= 0 || height <= 0) return;
    
    // Copy tiles
    for (int32_t dy = 0; dy < height; dy++) {
        for (int32_t dx = 0; dx < width; dx++) {
            int32_t srcIndex = src.coordsToIndex(srcX + dx, srcY + dy);
            int32_t dstIndex = coordsToIndex(dstX + dx, dstY + dy);
            m_tilesEx[dstIndex] = src.m_tilesEx[srcIndex];
        }
    }
    
    markDirty();
}

void TilemapEx::copyRegionSelfEx(int32_t srcX, int32_t srcY,
                                 int32_t dstX, int32_t dstY,
                                 int32_t width, int32_t height) {
    // Clamp source region
    if (srcX < 0) { width += srcX; dstX -= srcX; srcX = 0; }
    if (srcY < 0) { height += srcY; dstY -= srcY; srcY = 0; }
    if (srcX + width > getWidth()) width = getWidth() - srcX;
    if (srcY + height > getHeight()) height = getHeight() - srcY;
    
    // Clamp destination region
    if (dstX < 0) { width += dstX; srcX -= dstX; dstX = 0; }
    if (dstY < 0) { height += dstY; srcY -= dstY; dstY = 0; }
    if (dstX + width > getWidth()) width = getWidth() - dstX;
    if (dstY + height > getHeight()) height = getHeight() - dstY;
    
    if (width <= 0 || height <= 0) return;
    
    // Create temporary buffer to avoid overlap issues
    std::vector<TileDataEx> temp(width * height);
    
    // Copy to temp
    for (int32_t dy = 0; dy < height; dy++) {
        for (int32_t dx = 0; dx < width; dx++) {
            int32_t srcIndex = coordsToIndex(srcX + dx, srcY + dy);
            int32_t tempIndex = dy * width + dx;
            temp[tempIndex] = m_tilesEx[srcIndex];
        }
    }
    
    // Copy from temp to destination
    for (int32_t dy = 0; dy < height; dy++) {
        for (int32_t dx = 0; dx < width; dx++) {
            int32_t dstIndex = coordsToIndex(dstX + dx, dstY + dy);
            int32_t tempIndex = dy * width + dx;
            m_tilesEx[dstIndex] = temp[tempIndex];
        }
    }
    
    markDirty();
}

// =============================================================================
// Conversion to/from Base Tilemap
// =============================================================================

void TilemapEx::importFromTilemap(const Tilemap& source, uint8_t defaultPalette) {
    // Match dimensions
    initializeEx(source.getWidth(), source.getHeight(), 
                source.getTileWidth(), source.getTileHeight());
    
    // Convert each tile
    for (int32_t y = 0; y < source.getHeight(); y++) {
        for (int32_t x = 0; x < source.getWidth(); x++) {
            TileData tile16 = source.getTile(x, y);
            
            TileDataEx tile32;
            tile32.fromTileData16(tile16.packed);
            tile32.setPaletteIndex(defaultPalette);
            
            setTileEx(x, y, tile32);
        }
    }
}

void TilemapEx::exportToTilemap(Tilemap& dest) const {
    // Match dimensions
    dest.initialize(getWidth(), getHeight(), getTileWidth(), getTileHeight());
    
    // Convert each tile (loses palette and z-order info)
    for (int32_t y = 0; y < getHeight(); y++) {
        for (int32_t x = 0; x < getWidth(); x++) {
            TileDataEx tile32 = getTileEx(x, y);
            
            TileData tile16;
            tile16.packed = tile32.toTileData16();
            
            dest.setTile(x, y, tile16);
        }
    }
}

// =============================================================================
// Serialization
// =============================================================================

size_t TilemapEx::getMemorySizeEx() const {
    return m_tilesEx.size() * sizeof(TileDataEx);
}

std::vector<uint32_t> TilemapEx::exportRawDataEx() const {
    std::vector<uint32_t> data;
    data.reserve(m_tilesEx.size());
    
    for (const auto& tile : m_tilesEx) {
        data.push_back(tile.packed);
    }
    
    return data;
}

void TilemapEx::importRawDataEx(const std::vector<uint32_t>& data) {
    if (data.size() != m_tilesEx.size()) {
        return;  // Size mismatch
    }
    
    for (size_t i = 0; i < data.size(); i++) {
        m_tilesEx[i].packed = data[i];
    }
    
    markDirty();
}

// =============================================================================
// Statistics
// =============================================================================

void TilemapEx::getPaletteUsage(int32_t* outPaletteUsage) const {
    if (!outPaletteUsage) return;
    
    // Clear counts
    std::memset(outPaletteUsage, 0, 256 * sizeof(int32_t));
    
    // Count usage
    for (const auto& tile : m_tilesEx) {
        if (!tile.isEmpty()) {
            outPaletteUsage[tile.getPaletteIndex()]++;
        }
    }
}

void TilemapEx::getZOrderUsage(int32_t* outZOrderUsage) const {
    if (!outZOrderUsage) return;
    
    // Clear counts
    std::memset(outZOrderUsage, 0, 8 * sizeof(int32_t));
    
    // Count usage
    for (const auto& tile : m_tilesEx) {
        if (!tile.isEmpty()) {
            outZOrderUsage[tile.getZOrder()]++;
        }
    }
}

int32_t TilemapEx::countTilesWithPalette(uint8_t paletteIndex) const {
    int32_t count = 0;
    
    for (const auto& tile : m_tilesEx) {
        if (!tile.isEmpty() && tile.getPaletteIndex() == paletteIndex) {
            count++;
        }
    }
    
    return count;
}

int32_t TilemapEx::countTilesAtZOrder(uint8_t zOrder) const {
    int32_t count = 0;
    
    for (const auto& tile : m_tilesEx) {
        if (!tile.isEmpty() && tile.getZOrder() == zOrder) {
            count++;
        }
    }
    
    return count;
}

} // namespace SuperTerminal