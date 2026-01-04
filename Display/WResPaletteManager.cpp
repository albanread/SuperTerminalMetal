//
// WResPaletteManager.cpp
// SuperTerminal v2
//
// Implementation of hybrid 256-color palette manager for WRES mode.
//

#include "WResPaletteManager.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace SuperTerminal {

// Helper function for clamping (C++14 compatible)
template<typename T>
static inline T clamp(T value, T min, T max) {
    return (value < min) ? min : (value > max) ? max : value;
}

// IBM RGBI 16-color palette (CGA/EGA)
static const uint32_t IBM_RGBI_PALETTE[16] = {
    0xFF000000,  // 0: Black
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
static const uint32_t C64_PALETTE[16] = {
    0xFF000000,  // 0: Black
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
    0xFF333333,  // 11: Dark Gray
    0xFF777777,  // 12: Gray
    0xFFAAFF66,  // 13: Light Green
    0xFF0088FF,  // 14: Light Blue
    0xFFBBBBBB   // 15: Light Gray
};

// Generate grayscale palette (16 levels)
static void generateGrayscalePalette(uint32_t* palette) {
    for (int i = 0; i < 16; i++) {
        uint8_t level = (i * 255) / 15;
        palette[i] = 0xFF000000 | (level << 16) | (level << 8) | level;
    }
}

// Generate 6×8×5 RGB cube (240 colors for indices 16-255)
static void generateRGBCube6x8x5(uint32_t* palette) {
    int idx = 0;
    for (int r = 0; r < 6; r++) {
        for (int g = 0; g < 8; g++) {
            for (int b = 0; b < 5; b++) {
                if (idx >= 240) break;
                
                uint8_t red   = (r * 255) / 5;
                uint8_t green = (g * 255) / 7;
                uint8_t blue  = (b * 255) / 4;
                
                palette[idx++] = 0xFF000000 | (red << 16) | (green << 8) | blue;
            }
        }
    }
}

WResPaletteManager::WResPaletteManager()
    : m_floatData(nullptr)
    , m_floatDataDirty(true)
    , m_dirty(true)
{
    // Initialize all memory to zero
    std::memset(m_perRowPalettes, 0, sizeof(m_perRowPalettes));
    std::memset(m_globalPalette, 0, sizeof(m_globalPalette));
    
    // Set default palette
    initDefaultPalette();
    
    // Allocate float conversion buffer
    size_t floatCount = (ROW_COUNT * PER_ROW_COLORS + GLOBAL_COLORS) * 4;
    m_floatData = std::make_unique<float[]>(floatCount);
}

WResPaletteManager::~WResPaletteManager() {
    // Unique_ptr handles cleanup
}

void WResPaletteManager::initDefaultPalette() {
    // Set IBM RGBI palette for indices 0-15 (all rows)
    for (int i = 0; i < 16; i++) {
        setAllRowsToColor(i, IBM_RGBI_PALETTE[i]);
    }
    
    // Set 6×8×5 RGB cube for indices 16-255
    uint32_t rgbCube[240];
    generateRGBCube6x8x5(rgbCube);
    for (int i = 0; i < 240; i++) {
        setGlobalColor(16 + i, rgbCube[i]);
    }
    
    m_dirty = true;
    m_floatDataDirty = true;
}

void WResPaletteManager::setPerRowColor(int row, int index, uint32_t rgba) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clamp to valid ranges
    row = clamp(row, 0, ROW_COUNT - 1);
    index = clamp(index, 0, PER_ROW_COLORS - 1);
    
    // Unpack ARGB to RGBA
    uint8_t r, g, b, a;
    unpackRGBA(rgba, r, g, b, a);
    
    // Store in per-row palette
    m_perRowPalettes[row][index][0] = r;
    m_perRowPalettes[row][index][1] = g;
    m_perRowPalettes[row][index][2] = b;
    m_perRowPalettes[row][index][3] = a;
    
    m_dirty = true;
    m_floatDataDirty = true;
}

uint32_t WResPaletteManager::getPerRowColor(int row, int index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Return 0 for invalid parameters
    if (row < 0 || row >= ROW_COUNT || index < 0 || index >= PER_ROW_COLORS) {
        return 0x00000000;
    }
    
    // Pack RGBA to ARGB
    uint8_t r = m_perRowPalettes[row][index][0];
    uint8_t g = m_perRowPalettes[row][index][1];
    uint8_t b = m_perRowPalettes[row][index][2];
    uint8_t a = m_perRowPalettes[row][index][3];
    
    return packRGBA(r, g, b, a);
}

void WResPaletteManager::setGlobalColor(int index, uint32_t rgba) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clamp to valid range (16-255)
    index = clamp(index, 16, 255);
    int globalIdx = index - 16;  // Convert to 0-239 range
    
    // Unpack ARGB to RGBA
    uint8_t r, g, b, a;
    unpackRGBA(rgba, r, g, b, a);
    
    // Store in global palette
    m_globalPalette[globalIdx][0] = r;
    m_globalPalette[globalIdx][1] = g;
    m_globalPalette[globalIdx][2] = b;
    m_globalPalette[globalIdx][3] = a;
    
    m_dirty = true;
    m_floatDataDirty = true;
}

uint32_t WResPaletteManager::getGlobalColor(int index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Return 0 for invalid parameters
    if (index < 16 || index > 255) {
        return 0x00000000;
    }
    
    int globalIdx = index - 16;  // Convert to 0-239 range
    
    // Pack RGBA to ARGB
    uint8_t r = m_globalPalette[globalIdx][0];
    uint8_t g = m_globalPalette[globalIdx][1];
    uint8_t b = m_globalPalette[globalIdx][2];
    uint8_t a = m_globalPalette[globalIdx][3];
    
    return packRGBA(r, g, b, a);
}

void WResPaletteManager::setAllRowsToColor(int index, uint32_t rgba) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clamp to valid range
    index = clamp(index, 0, PER_ROW_COLORS - 1);
    
    // Unpack ARGB to RGBA
    uint8_t r, g, b, a;
    unpackRGBA(rgba, r, g, b, a);
    
    // Set color for all rows
    for (int row = 0; row < ROW_COUNT; row++) {
        m_perRowPalettes[row][index][0] = r;
        m_perRowPalettes[row][index][1] = g;
        m_perRowPalettes[row][index][2] = b;
        m_perRowPalettes[row][index][3] = a;
    }
    
    m_dirty = true;
    m_floatDataDirty = true;
}

void WResPaletteManager::loadPresetPalette(WResPalettePreset preset) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    switch (preset) {
        case WResPalettePreset::IBM_RGBI:
            // Apply to all rows, indices 0-15
            for (int i = 0; i < 16; i++) {
                uint8_t r, g, b, a;
                unpackRGBA(IBM_RGBI_PALETTE[i], r, g, b, a);
                for (int row = 0; row < ROW_COUNT; row++) {
                    m_perRowPalettes[row][i][0] = r;
                    m_perRowPalettes[row][i][1] = g;
                    m_perRowPalettes[row][i][2] = b;
                    m_perRowPalettes[row][i][3] = a;
                }
            }
            break;
            
        case WResPalettePreset::C64:
            // Apply to all rows, indices 0-15
            for (int i = 0; i < 16; i++) {
                uint8_t r, g, b, a;
                unpackRGBA(C64_PALETTE[i], r, g, b, a);
                for (int row = 0; row < ROW_COUNT; row++) {
                    m_perRowPalettes[row][i][0] = r;
                    m_perRowPalettes[row][i][1] = g;
                    m_perRowPalettes[row][i][2] = b;
                    m_perRowPalettes[row][i][3] = a;
                }
            }
            break;
            
        case WResPalettePreset::GRAYSCALE: {
            // Generate and apply to all rows, indices 0-15
            uint32_t grayscale[16];
            generateGrayscalePalette(grayscale);
            for (int i = 0; i < 16; i++) {
                uint8_t r, g, b, a;
                unpackRGBA(grayscale[i], r, g, b, a);
                for (int row = 0; row < ROW_COUNT; row++) {
                    m_perRowPalettes[row][i][0] = r;
                    m_perRowPalettes[row][i][1] = g;
                    m_perRowPalettes[row][i][2] = b;
                    m_perRowPalettes[row][i][3] = a;
                }
            }
            break;
        }
            
        case WResPalettePreset::RGB_CUBE_6x8x5: {
            // Generate and apply to global palette (indices 16-255)
            uint32_t rgbCube[240];
            generateRGBCube6x8x5(rgbCube);
            for (int i = 0; i < 240; i++) {
                uint8_t r, g, b, a;
                unpackRGBA(rgbCube[i], r, g, b, a);
                m_globalPalette[i][0] = r;
                m_globalPalette[i][1] = g;
                m_globalPalette[i][2] = b;
                m_globalPalette[i][3] = a;
            }
            break;
        }
    }
    
    m_dirty = true;
    m_floatDataDirty = true;
}

void WResPaletteManager::loadPresetPaletteRows(WResPalettePreset preset, int startRow, int endRow) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clamp row range
    startRow = clamp(startRow, 0, ROW_COUNT - 1);
    endRow = clamp(endRow, 0, ROW_COUNT - 1);
    if (startRow > endRow) std::swap(startRow, endRow);
    
    const uint32_t* paletteData = nullptr;
    uint32_t tempPalette[16];
    
    switch (preset) {
        case WResPalettePreset::IBM_RGBI:
            paletteData = IBM_RGBI_PALETTE;
            break;
            
        case WResPalettePreset::C64:
            paletteData = C64_PALETTE;
            break;
            
        case WResPalettePreset::GRAYSCALE:
            generateGrayscalePalette(tempPalette);
            paletteData = tempPalette;
            break;
            
        case WResPalettePreset::RGB_CUBE_6x8x5:
            // RGB cube is for global palette, not per-row
            // Ignore this preset for row-specific loading
            return;
    }
    
    if (paletteData) {
        // Apply palette to specified row range
        for (int row = startRow; row <= endRow; row++) {
            for (int i = 0; i < 16; i++) {
                uint8_t r, g, b, a;
                unpackRGBA(paletteData[i], r, g, b, a);
                m_perRowPalettes[row][i][0] = r;
                m_perRowPalettes[row][i][1] = g;
                m_perRowPalettes[row][i][2] = b;
                m_perRowPalettes[row][i][3] = a;
            }
        }
        
        m_dirty = true;
        m_floatDataDirty = true;
    }
}

const float* WResPaletteManager::getPaletteDataFloat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Regenerate float data if dirty
    if (m_floatDataDirty) {
        convertToFloat();
        m_floatDataDirty = false;
    }
    
    return m_floatData.get();
}

bool WResPaletteManager::isDirty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_dirty;
}

void WResPaletteManager::clearDirty() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dirty = false;
}

void WResPaletteManager::convertToFloat() const {
    // Must be called with mutex held
    
    float* dst = m_floatData.get();
    
    // Convert per-row palettes (240 rows × 16 colors × 4 floats)
    for (int row = 0; row < ROW_COUNT; row++) {
        for (int i = 0; i < PER_ROW_COLORS; i++) {
            *dst++ = m_perRowPalettes[row][i][0] / 255.0f;  // R
            *dst++ = m_perRowPalettes[row][i][1] / 255.0f;  // G
            *dst++ = m_perRowPalettes[row][i][2] / 255.0f;  // B
            *dst++ = m_perRowPalettes[row][i][3] / 255.0f;  // A
        }
    }
    
    // Convert global palette (240 colors × 4 floats)
    for (int i = 0; i < GLOBAL_COLORS; i++) {
        *dst++ = m_globalPalette[i][0] / 255.0f;  // R
        *dst++ = m_globalPalette[i][1] / 255.0f;  // G
        *dst++ = m_globalPalette[i][2] / 255.0f;  // B
        *dst++ = m_globalPalette[i][3] / 255.0f;  // A
    }
}

void WResPaletteManager::unpackRGBA(uint32_t rgba, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) {
    a = (rgba >> 24) & 0xFF;
    r = (rgba >> 16) & 0xFF;
    g = (rgba >> 8) & 0xFF;
    b = rgba & 0xFF;
}

uint32_t WResPaletteManager::packRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (static_cast<uint32_t>(a) << 24) |
           (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) |
           static_cast<uint32_t>(b);
}

// =============================================================================
// Palette Automation (Copper-style effects)
// =============================================================================

void WResPaletteManager::enableGradientAutomation(int paletteIndex, int startRow, int endRow,
                                                   int startR, int startG, int startB,
                                                   int endR, int endG, int endB,
                                                   float speed) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clamp parameters
    paletteIndex = clamp(paletteIndex, 0, PER_ROW_COLORS - 1);
    startRow = clamp(startRow, 0, ROW_COUNT - 1);
    endRow = clamp(endRow, 0, ROW_COUNT - 1);
    if (startRow > endRow) std::swap(startRow, endRow);
    
    startR = clamp(startR, 0, 255);
    startG = clamp(startG, 0, 255);
    startB = clamp(startB, 0, 255);
    endR = clamp(endR, 0, 255);
    endG = clamp(endG, 0, 255);
    endB = clamp(endB, 0, 255);
    
    // Configure gradient automation
    m_gradientAutomation.enabled = true;
    m_gradientAutomation.paletteIndex = paletteIndex;
    m_gradientAutomation.startRow = startRow;
    m_gradientAutomation.endRow = endRow;
    m_gradientAutomation.startR = static_cast<uint8_t>(startR);
    m_gradientAutomation.startG = static_cast<uint8_t>(startG);
    m_gradientAutomation.startB = static_cast<uint8_t>(startB);
    m_gradientAutomation.endR = static_cast<uint8_t>(endR);
    m_gradientAutomation.endG = static_cast<uint8_t>(endG);
    m_gradientAutomation.endB = static_cast<uint8_t>(endB);
    m_gradientAutomation.speed = speed;
    m_gradientAutomation.phase = 0.0f;
    
    // Apply immediately
    applyGradientAutomation();
}

void WResPaletteManager::enableBarsAutomation(int paletteIndex, int startRow, int endRow,
                                               int barHeight, const uint8_t colors[][3], int numColors,
                                               float speed) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clamp parameters
    paletteIndex = clamp(paletteIndex, 0, PER_ROW_COLORS - 1);
    startRow = clamp(startRow, 0, ROW_COUNT - 1);
    endRow = clamp(endRow, 0, ROW_COUNT - 1);
    if (startRow > endRow) std::swap(startRow, endRow);
    
    barHeight = clamp(barHeight, 1, ROW_COUNT);
    numColors = clamp(numColors, 1, 4);
    
    // Configure bars automation
    m_barsAutomation.enabled = true;
    m_barsAutomation.paletteIndex = paletteIndex;
    m_barsAutomation.startRow = startRow;
    m_barsAutomation.endRow = endRow;
    m_barsAutomation.barHeight = barHeight;
    m_barsAutomation.numColors = numColors;
    m_barsAutomation.speed = speed;
    m_barsAutomation.phase = 0.0f;
    
    // Copy colors
    for (int i = 0; i < numColors && i < 4; i++) {
        m_barsAutomation.colors[i][0] = colors[i][0];
        m_barsAutomation.colors[i][1] = colors[i][1];
        m_barsAutomation.colors[i][2] = colors[i][2];
    }
    
    // Apply immediately
    applyBarsAutomation();
}

void WResPaletteManager::disableAutomation() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_gradientAutomation.enabled = false;
    m_barsAutomation.enabled = false;
}

void WResPaletteManager::updateAutomation(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    bool needsUpdate = false;
    
    // Update gradient animation
    if (m_gradientAutomation.enabled && m_gradientAutomation.speed > 0.0f) {
        m_gradientAutomation.phase += deltaTime * m_gradientAutomation.speed;
        // Wrap phase to [0, 1]
        m_gradientAutomation.phase = std::fmod(m_gradientAutomation.phase, 1.0f);
        needsUpdate = true;
    }
    
    // Update bars animation
    if (m_barsAutomation.enabled && m_barsAutomation.speed > 0.0f) {
        m_barsAutomation.phase += deltaTime * m_barsAutomation.speed;
        // Phase can be unbounded for scrolling effect
        needsUpdate = true;
    }
    
    // Apply automation if anything changed
    if (needsUpdate) {
        if (m_gradientAutomation.enabled) {
            applyGradientAutomation();
        }
        if (m_barsAutomation.enabled) {
            applyBarsAutomation();
        }
    }
}

bool WResPaletteManager::isAutomationActive() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_gradientAutomation.enabled || m_barsAutomation.enabled;
}

void WResPaletteManager::applyGradientAutomation() {
    // Must be called with mutex held
    
    if (!m_gradientAutomation.enabled) {
        return;
    }
    
    int paletteIndex = m_gradientAutomation.paletteIndex;
    int startRow = m_gradientAutomation.startRow;
    int endRow = m_gradientAutomation.endRow;
    int rowCount = endRow - startRow + 1;
    
    // Animation: oscillate colors if speed > 0
    float phase = m_gradientAutomation.phase;
    float startR, startG, startB, endR, endG, endB;
    
    if (m_gradientAutomation.speed > 0.0f) {
        // Oscillate between start and end colors
        float t = (std::sin(phase * 2.0f * 3.14159f) + 1.0f) * 0.5f;  // [0, 1]
        startR = m_gradientAutomation.startR + t * (m_gradientAutomation.endR - m_gradientAutomation.startR);
        startG = m_gradientAutomation.startG + t * (m_gradientAutomation.endG - m_gradientAutomation.startG);
        startB = m_gradientAutomation.startB + t * (m_gradientAutomation.endB - m_gradientAutomation.startB);
        endR = m_gradientAutomation.endR + t * (m_gradientAutomation.startR - m_gradientAutomation.endR);
        endG = m_gradientAutomation.endG + t * (m_gradientAutomation.startG - m_gradientAutomation.endG);
        endB = m_gradientAutomation.endB + t * (m_gradientAutomation.startB - m_gradientAutomation.endB);
    } else {
        // Static gradient
        startR = m_gradientAutomation.startR;
        startG = m_gradientAutomation.startG;
        startB = m_gradientAutomation.startB;
        endR = m_gradientAutomation.endR;
        endG = m_gradientAutomation.endG;
        endB = m_gradientAutomation.endB;
    }
    
    // Apply gradient to rows
    for (int i = 0; i < rowCount; i++) {
        int row = startRow + i;
        float t = (rowCount > 1) ? (static_cast<float>(i) / (rowCount - 1)) : 0.0f;
        
        uint8_t r = static_cast<uint8_t>(startR + t * (endR - startR));
        uint8_t g = static_cast<uint8_t>(startG + t * (endG - startG));
        uint8_t b = static_cast<uint8_t>(startB + t * (endB - startB));
        
        m_perRowPalettes[row][paletteIndex][0] = r;
        m_perRowPalettes[row][paletteIndex][1] = g;
        m_perRowPalettes[row][paletteIndex][2] = b;
        m_perRowPalettes[row][paletteIndex][3] = 255;  // Full opacity
    }
    
    m_dirty = true;
    m_floatDataDirty = true;
}

void WResPaletteManager::applyBarsAutomation() {
    // Must be called with mutex held
    
    if (!m_barsAutomation.enabled) {
        return;
    }
    
    int paletteIndex = m_barsAutomation.paletteIndex;
    int startRow = m_barsAutomation.startRow;
    int endRow = m_barsAutomation.endRow;
    int barHeight = m_barsAutomation.barHeight;
    int numColors = m_barsAutomation.numColors;
    float phase = m_barsAutomation.phase;
    
    // Calculate scroll offset (wraps around)
    int scrollOffset = static_cast<int>(phase * barHeight) % (barHeight * numColors);
    
    // Apply bars to rows
    for (int row = startRow; row <= endRow; row++) {
        int relativeRow = row - startRow;
        int barPosition = (relativeRow + scrollOffset) / barHeight;
        int colorIndex = barPosition % numColors;
        
        uint8_t r = m_barsAutomation.colors[colorIndex][0];
        uint8_t g = m_barsAutomation.colors[colorIndex][1];
        uint8_t b = m_barsAutomation.colors[colorIndex][2];
        
        m_perRowPalettes[row][paletteIndex][0] = r;
        m_perRowPalettes[row][paletteIndex][1] = g;
        m_perRowPalettes[row][paletteIndex][2] = b;
        m_perRowPalettes[row][paletteIndex][3] = 255;  // Full opacity
    }
    
    m_dirty = true;
    m_floatDataDirty = true;
}

} // namespace SuperTerminal