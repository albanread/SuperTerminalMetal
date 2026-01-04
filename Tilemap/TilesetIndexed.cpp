//
// TilesetIndexed.cpp
// SuperTerminal Framework - Tilemap System
//
// Implementation of TilesetIndexed class
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "TilesetIndexed.h"
#include "PaletteBank.h"
#include <cstring>
#include <algorithm>
#include <cmath>

#ifdef __OBJC__
#import <Metal/Metal.h>
#endif

namespace SuperTerminal {

// =============================================================================
// Construction / Destruction
// =============================================================================

TilesetIndexed::TilesetIndexed()
    : Tileset()
    , m_format(TileFormat::Indexed4Bit)
    , m_device(nullptr)
    , m_indexTexture(nullptr)
    , m_indexedDirty(true)
    , m_textureWidth(0)
    , m_textureHeight(0)
{
}

TilesetIndexed::~TilesetIndexed() {
#ifdef __OBJC__
    if (m_indexTexture) {
        id<MTLTexture> texture = (__bridge_transfer id<MTLTexture>)m_indexTexture;
        texture = nil;
        m_indexTexture = nullptr;
    }
#endif
}

TilesetIndexed::TilesetIndexed(TilesetIndexed&& other) noexcept
    : Tileset(std::move(other))
    , m_format(other.m_format)
    , m_indexedData(std::move(other.m_indexedData))
    , m_device(other.m_device)
    , m_indexTexture(other.m_indexTexture)
    , m_indexedDirty(other.m_indexedDirty)
    , m_textureWidth(other.m_textureWidth)
    , m_textureHeight(other.m_textureHeight)
{
    other.m_device = nullptr;
    other.m_indexTexture = nullptr;
}

TilesetIndexed& TilesetIndexed::operator=(TilesetIndexed&& other) noexcept {
    if (this != &other) {
        Tileset::operator=(std::move(other));
        
#ifdef __OBJC__
        if (m_indexTexture) {
            id<MTLTexture> texture = (__bridge_transfer id<MTLTexture>)m_indexTexture;
            texture = nil;
        }
#endif
        
        m_format = other.m_format;
        m_indexedData = std::move(other.m_indexedData);
        m_device = other.m_device;
        m_indexTexture = other.m_indexTexture;
        m_indexedDirty = other.m_indexedDirty;
        m_textureWidth = other.m_textureWidth;
        m_textureHeight = other.m_textureHeight;
        
        other.m_device = nullptr;
        other.m_indexTexture = nullptr;
    }
    return *this;
}

// =============================================================================
// Initialization
// =============================================================================

bool TilesetIndexed::initializeIndexed(MTLDevicePtr device,
                                       int32_t tileWidth, 
                                       int32_t tileHeight,
                                       int32_t tileCount,
                                       const std::string& name) {
    if (!device || tileWidth <= 0 || tileHeight <= 0 || tileCount <= 0) {
        return false;
    }
    
    m_device = device;
    m_format = TileFormat::Indexed4Bit;
    
    // Calculate atlas dimensions (16 tiles per row)
    int32_t tilesPerRow = 16;
    int32_t rows = (tileCount + tilesPerRow - 1) / tilesPerRow;
    
    m_textureWidth = tilesPerRow * tileWidth;
    m_textureHeight = rows * tileHeight;
    
    // Allocate indexed data
    m_indexedData.resize(m_textureWidth * m_textureHeight, 0);
    
    // Create GPU texture
    if (!createIndexTexture()) {
        return false;
    }
    
    // Initialize base class (for metadata)
    Tileset::initialize(m_indexTexture, tileWidth, tileHeight, name);
    
    m_indexedDirty = true;
    
    return true;
}

bool TilesetIndexed::initializeFromIndexedData(MTLDevicePtr device,
                                               int32_t tileWidth,
                                               int32_t tileHeight,
                                               const uint8_t* indexedData,
                                               size_t dataSize,
                                               const std::string& name) {
    if (!device || !indexedData || dataSize == 0) {
        return false;
    }
    
    // Calculate dimensions
    int32_t tileCount = (int32_t)(dataSize / (tileWidth * tileHeight));
    
    if (!initializeIndexed(device, tileWidth, tileHeight, tileCount, name)) {
        return false;
    }
    
    // Copy data
    size_t copySize = std::min(dataSize, m_indexedData.size());
    std::memcpy(m_indexedData.data(), indexedData, copySize);
    
    // Clamp indices to valid range
    clampIndices();
    
    m_indexedDirty = true;
    
    return true;
}

// =============================================================================
// Image Loading and Conversion
// =============================================================================

bool TilesetIndexed::loadImageIndexed(MTLDevicePtr device,
                                      const char* imagePath,
                                      int32_t tileWidth,
                                      int32_t tileHeight,
                                      const PaletteColor* referencePalette,
                                      const std::string& name) {
    // TODO: Implement image loading
    // For now, return false - needs image loading library integration
    return false;
}

std::vector<uint8_t> TilesetIndexed::convertRGBAToIndexed(const uint8_t* rgbaData,
                                                          int32_t width,
                                                          int32_t height,
                                                          const PaletteColor* referencePalette) {
    std::vector<uint8_t> indexedData;
    indexedData.resize(width * height);
    
    // Generate default palette if none provided
    PaletteColor defaultPalette[16];
    if (!referencePalette) {
        generateDefaultPalette(defaultPalette);
        referencePalette = defaultPalette;
    }
    
    // Convert each pixel
    for (int32_t i = 0; i < width * height; i++) {
        uint8_t r = rgbaData[i * 4 + 0];
        uint8_t g = rgbaData[i * 4 + 1];
        uint8_t b = rgbaData[i * 4 + 2];
        uint8_t a = rgbaData[i * 4 + 3];
        
        // Find closest palette index
        uint8_t index = findClosestPaletteIndex(r, g, b, a, referencePalette, 16);
        indexedData[i] = index;
    }
    
    return indexedData;
}

// =============================================================================
// Pixel Access
// =============================================================================

uint8_t TilesetIndexed::getIndexedPixel(int32_t x, int32_t y) const {
    if (x < 0 || x >= m_textureWidth || y < 0 || y >= m_textureHeight) {
        return 0;
    }
    
    int32_t index = getPixelIndex(x, y);
    return m_indexedData[index];
}

void TilesetIndexed::setIndexedPixel(int32_t x, int32_t y, uint8_t index) {
    if (x < 0 || x >= m_textureWidth || y < 0 || y >= m_textureHeight) {
        return;
    }
    
    // Clamp to 4-bit range
    index = index & 0x0F;
    
    int32_t pixelIndex = getPixelIndex(x, y);
    m_indexedData[pixelIndex] = index;
    m_indexedDirty = true;
}

uint8_t TilesetIndexed::getTileIndexedPixel(uint16_t tileID, int32_t x, int32_t y) const {
    if (x < 0 || y < 0 || x >= getTileWidth() || y >= getTileHeight()) {
        return 0;
    }
    
    int32_t atlasX, atlasY;
    getTileAtlasPosition(tileID, atlasX, atlasY);
    
    return getIndexedPixel(atlasX + x, atlasY + y);
}

void TilesetIndexed::setTileIndexedPixel(uint16_t tileID, int32_t x, int32_t y, uint8_t index) {
    if (x < 0 || y < 0 || x >= getTileWidth() || y >= getTileHeight()) {
        return;
    }
    
    int32_t atlasX, atlasY;
    getTileAtlasPosition(tileID, atlasX, atlasY);
    
    setIndexedPixel(atlasX + x, atlasY + y, index);
}

// =============================================================================
// GPU Upload
// =============================================================================

bool TilesetIndexed::uploadIndexedData() {
#ifdef __OBJC__
    if (!m_indexTexture || !m_indexedDirty) {
        return false;
    }
    
    id<MTLTexture> texture = (__bridge id<MTLTexture>)m_indexTexture;
    
    MTLRegion region = MTLRegionMake2D(0, 0, m_textureWidth, m_textureHeight);
    
    [texture replaceRegion:region
               mipmapLevel:0
                 withBytes:m_indexedData.data()
               bytesPerRow:m_textureWidth];
    
    m_indexedDirty = false;
    return true;
#else
    return false;
#endif
}

// =============================================================================
// Tile Operations
// =============================================================================

void TilesetIndexed::fillTile(uint16_t tileID, uint8_t index) {
    int32_t atlasX, atlasY;
    getTileAtlasPosition(tileID, atlasX, atlasY);
    
    // Clamp index
    index = index & 0x0F;
    
    // Fill tile
    for (int32_t y = 0; y < getTileHeight(); y++) {
        for (int32_t x = 0; x < getTileWidth(); x++) {
            setIndexedPixel(atlasX + x, atlasY + y, index);
        }
    }
}

void TilesetIndexed::clearTile(uint16_t tileID) {
    fillTile(tileID, 0);  // Index 0 = transparent
}

void TilesetIndexed::copyTile(uint16_t srcTileID, uint16_t dstTileID) {
    int32_t srcX, srcY, dstX, dstY;
    getTileAtlasPosition(srcTileID, srcX, srcY);
    getTileAtlasPosition(dstTileID, dstX, dstY);
    
    // Copy pixels
    for (int32_t y = 0; y < getTileHeight(); y++) {
        for (int32_t x = 0; x < getTileWidth(); x++) {
            uint8_t index = getIndexedPixel(srcX + x, srcY + y);
            setIndexedPixel(dstX + x, dstY + y, index);
        }
    }
}

void TilesetIndexed::flipTileHorizontal(uint16_t tileID) {
    int32_t atlasX, atlasY;
    getTileAtlasPosition(tileID, atlasX, atlasY);
    
    int32_t tileW = getTileWidth();
    int32_t tileH = getTileHeight();
    
    // Flip horizontally
    for (int32_t y = 0; y < tileH; y++) {
        for (int32_t x = 0; x < tileW / 2; x++) {
            uint8_t left = getIndexedPixel(atlasX + x, atlasY + y);
            uint8_t right = getIndexedPixel(atlasX + (tileW - 1 - x), atlasY + y);
            
            setIndexedPixel(atlasX + x, atlasY + y, right);
            setIndexedPixel(atlasX + (tileW - 1 - x), atlasY + y, left);
        }
    }
}

void TilesetIndexed::flipTileVertical(uint16_t tileID) {
    int32_t atlasX, atlasY;
    getTileAtlasPosition(tileID, atlasX, atlasY);
    
    int32_t tileW = getTileWidth();
    int32_t tileH = getTileHeight();
    
    // Flip vertically
    for (int32_t y = 0; y < tileH / 2; y++) {
        for (int32_t x = 0; x < tileW; x++) {
            uint8_t top = getIndexedPixel(atlasX + x, atlasY + y);
            uint8_t bottom = getIndexedPixel(atlasX + x, atlasY + (tileH - 1 - y));
            
            setIndexedPixel(atlasX + x, atlasY + y, bottom);
            setIndexedPixel(atlasX + x, atlasY + (tileH - 1 - y), top);
        }
    }
}

// =============================================================================
// Statistics and Analysis
// =============================================================================

void TilesetIndexed::getTileIndexUsage(uint16_t tileID, int32_t* outUsage) const {
    if (!outUsage) return;
    
    // Clear counts
    std::memset(outUsage, 0, 16 * sizeof(int32_t));
    
    int32_t atlasX, atlasY;
    getTileAtlasPosition(tileID, atlasX, atlasY);
    
    // Count usage
    for (int32_t y = 0; y < getTileHeight(); y++) {
        for (int32_t x = 0; x < getTileWidth(); x++) {
            uint8_t index = getIndexedPixel(atlasX + x, atlasY + y);
            outUsage[index & 0x0F]++;
        }
    }
}

int32_t TilesetIndexed::countUniqueIndices(uint16_t tileID) const {
    int32_t usage[16] = {0};
    getTileIndexUsage(tileID, usage);
    
    int32_t count = 0;
    for (int32_t i = 0; i < 16; i++) {
        if (usage[i] > 0) count++;
    }
    
    return count;
}

bool TilesetIndexed::tileUsesIndex(uint16_t tileID, uint8_t index) const {
    int32_t usage[16] = {0};
    getTileIndexUsage(tileID, usage);
    
    return usage[index & 0x0F] > 0;
}

// =============================================================================
// Validation
// =============================================================================

bool TilesetIndexed::validateIndexedData() const {
    if (m_format != TileFormat::Indexed4Bit) {
        return true;  // Only validate 4-bit
    }
    
    for (uint8_t value : m_indexedData) {
        if (value > 15) {
            return false;
        }
    }
    
    return true;
}

void TilesetIndexed::clampIndices() {
    if (m_format != TileFormat::Indexed4Bit) {
        return;
    }
    
    for (uint8_t& value : m_indexedData) {
        value = value & 0x0F;  // Clamp to 0-15
    }
    
    m_indexedDirty = true;
}

// =============================================================================
// Internal Helpers
// =============================================================================

bool TilesetIndexed::createIndexTexture() {
#ifdef __OBJC__
    if (!m_device) {
        return false;
    }
    
    id<MTLDevice> device = (__bridge id<MTLDevice>)m_device;
    
    // Create texture descriptor
    // Use R8Uint format to store palette indices (0-15)
    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Uint
        width:m_textureWidth
        height:m_textureHeight
        mipmapped:NO];
    
    descriptor.usage = MTLTextureUsageShaderRead;
    descriptor.storageMode = MTLStorageModeShared;
    
    // Create texture
    id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
    if (!texture) {
        return false;
    }
    
    m_indexTexture = (__bridge_retained void*)texture;
    return true;
#else
    return false;
#endif
}

void TilesetIndexed::getTileAtlasPosition(uint16_t tileID, int32_t& outX, int32_t& outY) const {
    int32_t tilesPerRow = m_textureWidth / getTileWidth();
    
    int32_t column = tileID % tilesPerRow;
    int32_t row = tileID / tilesPerRow;
    
    outX = column * getTileWidth();
    outY = row * getTileHeight();
}

uint8_t TilesetIndexed::findClosestPaletteIndex(uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                                const PaletteColor* palette, int32_t paletteSize) {
    // Handle transparent pixels
    if (a < 128) {
        return 0;  // Index 0 = transparent
    }
    
    // Handle black pixels
    if (r < 10 && g < 10 && b < 10) {
        return 1;  // Index 1 = opaque black
    }
    
    // Find closest color in palette (starting from index 2)
    int32_t bestIndex = 2;
    int32_t bestDistance = INT32_MAX;
    
    for (int32_t i = 2; i < paletteSize; i++) {
        int32_t dist = colorDistance(r, g, b, palette[i].r, palette[i].g, palette[i].b);
        
        if (dist < bestDistance) {
            bestDistance = dist;
            bestIndex = i;
        }
    }
    
    return (uint8_t)bestIndex;
}

void TilesetIndexed::generateDefaultPalette(PaletteColor* outPalette) {
    // Default 16-color palette
    outPalette[0] = PaletteColor(0, 0, 0, 0);         // Transparent
    outPalette[1] = PaletteColor(0, 0, 0, 255);       // Black
    outPalette[2] = PaletteColor(255, 255, 255, 255); // White
    outPalette[3] = PaletteColor(255, 0, 0, 255);     // Red
    outPalette[4] = PaletteColor(0, 255, 0, 255);     // Green
    outPalette[5] = PaletteColor(0, 0, 255, 255);     // Blue
    outPalette[6] = PaletteColor(255, 255, 0, 255);   // Yellow
    outPalette[7] = PaletteColor(255, 0, 255, 255);   // Magenta
    outPalette[8] = PaletteColor(0, 255, 255, 255);   // Cyan
    outPalette[9] = PaletteColor(128, 128, 128, 255); // Gray
    outPalette[10] = PaletteColor(192, 192, 192, 255);// Light gray
    outPalette[11] = PaletteColor(128, 0, 0, 255);    // Dark red
    outPalette[12] = PaletteColor(0, 128, 0, 255);    // Dark green
    outPalette[13] = PaletteColor(0, 0, 128, 255);    // Dark blue
    outPalette[14] = PaletteColor(128, 128, 0, 255);  // Olive
    outPalette[15] = PaletteColor(128, 0, 128, 255);  // Purple
}

int32_t TilesetIndexed::colorDistance(uint8_t r1, uint8_t g1, uint8_t b1,
                                      uint8_t r2, uint8_t g2, uint8_t b2) {
    // Euclidean distance in RGB space
    int32_t dr = (int32_t)r1 - (int32_t)r2;
    int32_t dg = (int32_t)g1 - (int32_t)g2;
    int32_t db = (int32_t)b1 - (int32_t)b2;
    
    return dr * dr + dg * dg + db * db;
}

} // namespace SuperTerminal