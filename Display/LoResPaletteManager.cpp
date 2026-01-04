//
// LoResPaletteManager.cpp
// SuperTerminal v2
//
// Implementation of LORES palette management system.
//

#include "LoResPaletteManager.h"
#include <cstring>
#include <algorithm>

namespace SuperTerminal {

// IBM RGBI 16-color palette (CGA/EGA)
// Color 0 is fully transparent for legacy LORES modes
static const uint32_t IBM_PALETTE[16] = {
    0x00000000,  // 0: Transparent (alpha=0)
    0xFF0000AA,  // 1: Blue
    0xFF00AA00,  // 2: Green
    0xFF00AAAA,  // 3: Cyan
    0xFFAA0000,  // 4: Red
    0xFFAA00AA,  // 5: Magenta
    0xFFAA5500,  // 6: Brown
    0xFFAAAAAA,  // 7: Light Gray
    0xFF555555,  // 8: Dark Gray
    0xFF5555FF,  // 9: Light Blue
    0xFF55FF55,  // 10: Light Green
    0xFF55FFFF,  // 11: Light Cyan
    0xFFFF5555,  // 12: Light Red
    0xFFFF55FF,  // 13: Light Magenta
    0xFFFFFF55,  // 14: Yellow
    0xFFFFFFFF   // 15: White
};

// Commodore 64 16-color palette
// Color 0 is fully transparent for legacy LORES modes
static const uint32_t C64_PALETTE[16] = {
    0x00000000,  // 0: Transparent (alpha=0)
    0xFFFFFFFF,  // 1: White
    0xFF880000,  // 2: Red
    0xFFAAFFEE,  // 3: Cyan
    0xFFCC44CC,  // 4: Purple
    0xFF00CC55,  // 5: Green
    0xFF0000AA,  // 6: Blue
    0xFFEEEE77,  // 7: Yellow
    0xFFDD8855,  // 8: Orange
    0xFF664400,  // 9: Brown
    0xFFFF7777,  // 10: Light Red
    0xFF333333,  // 11: Dark Grey
    0xFF777777,  // 12: Grey
    0xFFAAFF66,  // 13: Light Green
    0xFF0088FF,  // 14: Light Blue
    0xFFBBBBBB   // 15: Light Grey
};

LoResPaletteManager::LoResPaletteManager()
    : m_dirty(true)
{
    // Initialize with IBM palette by default
    initIBMPalette();
}

LoResPaletteManager::~LoResPaletteManager() {
    // Nothing to clean up
}

void LoResPaletteManager::setAllPalettes(LoResPaletteType type) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    switch (type) {
        case LoResPaletteType::IBM:
            initIBMPalette();
            break;
        case LoResPaletteType::C64:
            initC64Palette();
            break;
    }
    
    m_dirty = true;
}

void LoResPaletteManager::setPaletteEntry(int row, int index, uint32_t rgba) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clamp to valid ranges
    row = std::clamp(row, 0, 299);
    index = std::clamp(index, 0, 15);
    
    // Unpack ARGB (0xAARRGGBB) to RGBA bytes
    uint8_t a = (rgba >> 24) & 0xFF;
    uint8_t r = (rgba >> 16) & 0xFF;
    uint8_t g = (rgba >> 8) & 0xFF;
    uint8_t b = (rgba >> 0) & 0xFF;
    
    // Store in RGBA order
    m_palettes[row][index][0] = r;
    m_palettes[row][index][1] = g;
    m_palettes[row][index][2] = b;
    m_palettes[row][index][3] = a;
    
    m_dirty = true;
}

uint32_t LoResPaletteManager::getPaletteEntry(int row, int index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Return 0 for invalid parameters
    if (row < 0 || row > 299 || index < 0 || index > 15) {
        return 0x00000000;
    }
    
    // Pack RGBA bytes to ARGB (0xAARRGGBB)
    uint8_t r = m_palettes[row][index][0];
    uint8_t g = m_palettes[row][index][1];
    uint8_t b = m_palettes[row][index][2];
    uint8_t a = m_palettes[row][index][3];
    
    return (a << 24) | (r << 16) | (g << 8) | b;
}

bool LoResPaletteManager::isDirty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_dirty;
}

void LoResPaletteManager::clearDirty() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dirty = false;
}

void LoResPaletteManager::initIBMPalette() {
    // Note: Assumes mutex is already locked by caller
    setAllPalettesTo(IBM_PALETTE);
}

void LoResPaletteManager::initC64Palette() {
    // Note: Assumes mutex is already locked by caller
    setAllPalettesTo(C64_PALETTE);
}

void LoResPaletteManager::setAllPalettesTo(const uint32_t palette[16]) {
    // Note: Assumes mutex is already locked by caller
    
    // Set all 300 rows for LORES/MIDRES/HIRES support
    for (int row = 0; row < 300; row++) {
        for (int index = 0; index < 16; index++) {
            uint32_t rgba = palette[index];
            
            // Unpack ARGB (0xAARRGGBB) to RGBA bytes
            uint8_t a = (rgba >> 24) & 0xFF;
            uint8_t r = (rgba >> 16) & 0xFF;
            uint8_t g = (rgba >> 8) & 0xFF;
            uint8_t b = (rgba >> 0) & 0xFF;
            
            // Store in RGBA order
            m_palettes[row][index][0] = r;
            m_palettes[row][index][1] = g;
            m_palettes[row][index][2] = b;
            m_palettes[row][index][3] = a;
        }
    }
}

} // namespace SuperTerminal