//
// PaletteBank.cpp
// SuperTerminal Framework - Tilemap System
//
// Implementation of PaletteBank class
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "PaletteBank.h"
#include <cmath>
#include <cstring>
#include <algorithm>

#ifdef __OBJC__
#import <Metal/Metal.h>
#endif

namespace SuperTerminal {

// =============================================================================
// PRESET PALETTE DATA
// =============================================================================

// Preset palette definitions (16 colors each)
// Convention: [0]=transparent black, [1]=opaque black, [2-15]=colors

static const PaletteColor PRESET_DESERT[16] = {
    {0, 0, 0, 0},           // [0] Transparent black
    {0, 0, 0, 255},         // [1] Opaque black
    {240, 220, 130, 255},   // [2] Sandy yellow
    {210, 180, 100, 255},   // [3] Dark sand
    {180, 150, 80, 255},    // [4] Darker sand
    {255, 200, 100, 255},   // [5] Bright sand
    {160, 120, 60, 255},    // [6] Brown rock
    {200, 160, 100, 255},   // [7] Light rock
    {100, 80, 50, 255},     // [8] Dark rock
    {135, 206, 235, 255},   // [9] Sky blue
    {0, 100, 200, 255},     // [10] Deep sky
    {255, 255, 255, 255},   // [11] White (clouds)
    {50, 150, 50, 255},     // [12] Cactus green
    {200, 100, 50, 255},    // [13] Terracotta
    {150, 75, 25, 255},     // [14] Dark brown
    {255, 150, 50, 255}     // [15] Orange accent
};

static const PaletteColor PRESET_FOREST[16] = {
    {0, 0, 0, 0},           // [0] Transparent black
    {0, 0, 0, 255},         // [1] Opaque black
    {50, 180, 50, 255},     // [2] Grass green
    {40, 150, 40, 255},     // [3] Dark grass
    {60, 200, 60, 255},     // [4] Bright grass
    {100, 200, 100, 255},   // [5] Light grass
    {80, 120, 40, 255},     // [6] Olive/moss
    {120, 80, 40, 255},     // [7] Brown dirt
    {160, 120, 80, 255},    // [8] Light dirt
    {135, 206, 235, 255},   // [9] Sky blue
    {255, 255, 255, 255},   // [10] White
    {150, 100, 50, 255},    // [11] Tree bark
    {200, 150, 100, 255},   // [12] Light bark
    {255, 200, 50, 255},    // [13] Flower yellow
    {255, 100, 100, 255},   // [14] Flower red
    {200, 100, 200, 255}    // [15] Flower purple
};

static const PaletteColor PRESET_CAVE[16] = {
    {0, 0, 0, 0},           // [0] Transparent black
    {0, 0, 0, 255},         // [1] Opaque black
    {64, 64, 64, 255},      // [2] Dark gray stone
    {48, 48, 48, 255},      // [3] Darker stone
    {80, 80, 80, 255},      // [4] Light stone
    {96, 96, 96, 255},      // [5] Lighter stone
    {32, 40, 56, 255},      // [6] Blue-gray
    {56, 64, 80, 255},      // [7] Cool gray
    {80, 100, 120, 255},    // [8] Light cool gray
    {64, 128, 192, 255},    // [9] Crystal blue
    {96, 160, 224, 255},    // [10] Bright crystal
    {112, 64, 32, 255},     // [11] Brown mineral
    {144, 96, 48, 255},     // [12] Orange mineral
    {192, 128, 64, 255},    // [13] Gold ore
    {224, 160, 80, 255},    // [14] Bright gold
    {255, 255, 255, 255}    // [15] White crystal
};

static const PaletteColor PRESET_ICE[16] = {
    {0, 0, 0, 0},           // [0] Transparent black
    {0, 0, 0, 255},         // [1] Opaque black
    {200, 220, 255, 255},   // [2] Ice blue
    {180, 200, 240, 255},   // [3] Light ice
    {160, 180, 220, 255},   // [4] Mid ice
    {140, 160, 200, 255},   // [5] Dark ice
    {220, 240, 255, 255},   // [6] Bright ice
    {100, 150, 200, 255},   // [7] Deep blue
    {80, 120, 180, 255},    // [8] Darker blue
    {255, 255, 255, 255},   // [9] White snow
    {240, 240, 255, 255},   // [10] Light snow
    {200, 200, 220, 255},   // [11] Gray ice
    {150, 150, 180, 255},   // [12] Dark gray
    {120, 200, 240, 255},   // [13] Cyan ice
    {100, 180, 220, 255},   // [14] Aqua
    {80, 160, 200, 255}     // [15] Teal
};

static const PaletteColor PRESET_LAVA[16] = {
    {0, 0, 0, 0},           // [0] Transparent black
    {0, 0, 0, 255},         // [1] Opaque black
    {255, 100, 0, 255},     // [2] Bright orange
    {255, 80, 0, 255},      // [3] Orange-red
    {255, 60, 0, 255},      // [4] Red-orange
    {240, 40, 0, 255},      // [5] Dark red
    {200, 30, 0, 255},      // [6] Darker red
    {160, 20, 0, 255},      // [7] Very dark red
    {255, 255, 100, 255},   // [8] Yellow glow
    {255, 200, 0, 255},     // [9] Yellow-orange
    {255, 150, 0, 255},     // [10] Orange glow
    {80, 40, 20, 255},      // [11] Dark brown
    {60, 30, 15, 255},      // [12] Darker brown
    {40, 20, 10, 255},      // [13] Very dark
    {128, 64, 32, 255},     // [14] Brown rock
    {100, 50, 25, 255}      // [15] Dark rock
};

static const PaletteColor PRESET_NIGHT[16] = {
    {0, 0, 0, 0},           // [0] Transparent black
    {0, 0, 0, 255},         // [1] Opaque black
    {20, 20, 40, 255},      // [2] Dark blue
    {30, 30, 60, 255},      // [3] Night blue
    {40, 40, 80, 255},      // [4] Mid blue
    {50, 50, 100, 255},     // [5] Lighter blue
    {60, 40, 80, 255},      // [6] Purple
    {80, 60, 100, 255},     // [7] Light purple
    {100, 80, 120, 255},    // [8] Lavender
    {255, 255, 200, 255},   // [9] Moon yellow
    {200, 200, 255, 255},   // [10] Star white
    {150, 150, 200, 255},   // [11] Dim star
    {40, 60, 40, 255},      // [12] Dark green
    {60, 80, 60, 255},      // [13] Night green
    {80, 100, 80, 255},     // [14] Dim green
    {30, 30, 30, 255}       // [15] Very dark gray
};

static const PaletteColor PRESET_WATER[16] = {
    {0, 0, 0, 0},           // [0] Transparent black
    {0, 0, 0, 255},         // [1] Opaque black
    {0, 100, 200, 255},     // [2] Deep blue
    {0, 120, 220, 255},     // [3] Dark blue
    {0, 140, 240, 255},     // [4] Mid blue
    {20, 160, 255, 255},    // [5] Light blue
    {40, 180, 255, 255},    // [6] Bright blue
    {60, 200, 255, 255},    // [7] Cyan blue
    {80, 220, 255, 255},    // [8] Light cyan
    {100, 240, 255, 255},   // [9] Bright cyan
    {150, 250, 255, 255},   // [10] Very light cyan
    {200, 255, 255, 255},   // [11] White foam
    {0, 80, 160, 255},      // [12] Very deep
    {0, 60, 120, 255},      // [13] Abyss blue
    {255, 255, 255, 255},   // [14] White
    {180, 230, 255, 255}    // [15] Sky blue
};

static const PaletteColor PRESET_METAL[16] = {
    {0, 0, 0, 0},           // [0] Transparent black
    {0, 0, 0, 255},         // [1] Opaque black
    {128, 128, 128, 255},   // [2] Gray
    {160, 160, 160, 255},   // [3] Light gray
    {192, 192, 192, 255},   // [4] Silver
    {224, 224, 224, 255},   // [5] Light silver
    {255, 255, 255, 255},   // [6] White shine
    {96, 96, 96, 255},      // [7] Dark gray
    {64, 64, 64, 255},      // [8] Darker gray
    {32, 32, 32, 255},      // [9] Very dark
    {180, 180, 200, 255},   // [10] Blue tint
    {200, 180, 160, 255},   // [11] Warm tint
    {160, 140, 120, 255},   // [12] Bronze
    {140, 120, 100, 255},   // [13] Dark bronze
    {120, 100, 80, 255},    // [14] Rust
    {100, 80, 60, 255}      // [15] Dark rust
};

static const PaletteColor PRESET_CRYSTAL[16] = {
    {0, 0, 0, 0},           // [0] Transparent black
    {0, 0, 0, 255},         // [1] Opaque black
    {255, 100, 200, 255},   // [2] Pink
    {200, 100, 255, 255},   // [3] Purple
    {100, 200, 255, 255},   // [4] Cyan
    {255, 200, 100, 255},   // [5] Orange
    {100, 255, 200, 255},   // [6] Mint
    {255, 255, 100, 255},   // [7] Yellow
    {255, 100, 100, 255},   // [8] Red
    {100, 255, 100, 255},   // [9] Green
    {100, 100, 255, 255},   // [10] Blue
    {255, 255, 255, 255},   // [11] White
    {200, 150, 255, 255},   // [12] Light purple
    {150, 200, 255, 255},   // [13] Light cyan
    {255, 200, 150, 255},   // [14] Light orange
    {150, 255, 200, 255}    // [15] Light mint
};

// Preset lookup table
struct PresetEntry {
    const char* name;
    const PaletteColor* data;
};

static const PresetEntry PRESETS[] = {
    {"desert", PRESET_DESERT},
    {"forest", PRESET_FOREST},
    {"cave", PRESET_CAVE},
    {"ice", PRESET_ICE},
    {"lava", PRESET_LAVA},
    {"night", PRESET_NIGHT},
    {"water", PRESET_WATER},
    {"metal", PRESET_METAL},
    {"crystal", PRESET_CRYSTAL},
    {nullptr, nullptr}  // Sentinel
};

// =============================================================================
// Construction / Destruction
// =============================================================================

PaletteBank::PaletteBank(int32_t paletteCount, int32_t colorsPerPalette, MTLDevicePtr device)
    : m_paletteCount(paletteCount)
    , m_colorsPerPalette(colorsPerPalette)
    , m_name("PaletteBank")
    , m_device(device)
    , m_texture(nullptr)
    , m_dirty(true)
{
    // Allocate CPU storage
    m_data.resize(paletteCount * colorsPerPalette);
    
    // Initialize all palettes with convention
    for (int32_t i = 0; i < paletteCount; i++) {
        setDefaultColors(i);
    }
    
    // Initialize GPU if device provided
    if (m_device) {
        initialize(m_device);
    }
}

PaletteBank::~PaletteBank() {
    shutdown();
}

PaletteBank::PaletteBank(PaletteBank&& other) noexcept
    : m_paletteCount(other.m_paletteCount)
    , m_colorsPerPalette(other.m_colorsPerPalette)
    , m_name(std::move(other.m_name))
    , m_data(std::move(other.m_data))
    , m_device(other.m_device)
    , m_texture(other.m_texture)
    , m_dirty(other.m_dirty)
{
    other.m_device = nullptr;
    other.m_texture = nullptr;
}

PaletteBank& PaletteBank::operator=(PaletteBank&& other) noexcept {
    if (this != &other) {
        shutdown();
        
        m_paletteCount = other.m_paletteCount;
        m_colorsPerPalette = other.m_colorsPerPalette;
        m_name = std::move(other.m_name);
        m_data = std::move(other.m_data);
        m_device = other.m_device;
        m_texture = other.m_texture;
        m_dirty = other.m_dirty;
        
        other.m_device = nullptr;
        other.m_texture = nullptr;
    }
    return *this;
}

// =============================================================================
// Initialization
// =============================================================================

bool PaletteBank::initialize(MTLDevicePtr device) {
#ifdef __OBJC__
    if (!device) {
        return false;
    }
    
    m_device = device;
    
    // Create texture
    if (!createTexture()) {
        return false;
    }
    
    // Upload initial data
    uploadToGPU(-1);
    
    return true;
#else
    return false;
#endif
}

void PaletteBank::shutdown() {
#ifdef __OBJC__
    if (m_texture) {
        id<MTLTexture> texture = (__bridge_transfer id<MTLTexture>)m_texture;
        texture = nil;
        m_texture = nullptr;
    }
#endif
    m_device = nullptr;
}

bool PaletteBank::createTexture() {
#ifdef __OBJC__
    if (!m_device) {
        return false;
    }
    
    id<MTLDevice> device = (__bridge id<MTLDevice>)m_device;
    
    // Create texture descriptor
    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
        width:m_colorsPerPalette
        height:m_paletteCount
        mipmapped:NO];
    
    descriptor.usage = MTLTextureUsageShaderRead;
    descriptor.storageMode = MTLStorageModeShared;
    
    // Create texture
    id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
    if (!texture) {
        return false;
    }
    
    m_texture = (__bridge_retained void*)texture;
    return true;
#else
    return false;
#endif
}

// =============================================================================
// Palette Operations
// =============================================================================

bool PaletteBank::setPalette(int32_t paletteIndex, const PaletteColor* colors, int32_t colorCount) {
    if (!isValidPalette(paletteIndex)) {
        return false;
    }
    
    if (colorCount != m_colorsPerPalette) {
        return false;
    }
    
    // Copy colors
    int32_t startIndex = getIndex(paletteIndex, 0);
    std::memcpy(&m_data[startIndex], colors, colorCount * sizeof(PaletteColor));
    
    // Enforce convention
    setDefaultColors(paletteIndex);
    
    m_dirty = true;
    return true;
}

int32_t PaletteBank::getPalette(int32_t paletteIndex, PaletteColor* outColors, int32_t maxColors) const {
    if (!isValidPalette(paletteIndex)) {
        return 0;
    }
    
    int32_t colorsToCopy = std::min(maxColors, m_colorsPerPalette);
    int32_t startIndex = getIndex(paletteIndex, 0);
    
    std::memcpy(outColors, &m_data[startIndex], colorsToCopy * sizeof(PaletteColor));
    
    return colorsToCopy;
}

bool PaletteBank::setColor(int32_t paletteIndex, int32_t colorIndex, const PaletteColor& color) {
    if (!isValidPalette(paletteIndex) || !isValidColor(colorIndex)) {
        return false;
    }
    
    // Don't allow changing convention colors (0 and 1)
    if (colorIndex == PALETTE_INDEX_TRANSPARENT || colorIndex == PALETTE_INDEX_BLACK) {
        return false;
    }
    
    int32_t index = getIndex(paletteIndex, colorIndex);
    m_data[index] = color;
    
    m_dirty = true;
    return true;
}

PaletteColor PaletteBank::getColor(int32_t paletteIndex, int32_t colorIndex) const {
    if (!isValidPalette(paletteIndex) || !isValidColor(colorIndex)) {
        return PaletteColor(0, 0, 0, 0);  // Transparent black
    }
    
    int32_t index = getIndex(paletteIndex, colorIndex);
    return m_data[index];
}

bool PaletteBank::copyPalette(int32_t srcIndex, int32_t dstIndex) {
    if (!isValidPalette(srcIndex) || !isValidPalette(dstIndex)) {
        return false;
    }
    
    int32_t srcStart = getIndex(srcIndex, 0);
    int32_t dstStart = getIndex(dstIndex, 0);
    
    std::memcpy(&m_data[dstStart], &m_data[srcStart], m_colorsPerPalette * sizeof(PaletteColor));
    
    m_dirty = true;
    return true;
}

void PaletteBank::fillPalette(int32_t paletteIndex, const PaletteColor& color) {
    if (!isValidPalette(paletteIndex)) {
        return;
    }
    
    int32_t startIndex = getIndex(paletteIndex, 0);
    
    // Fill all colors except 0 and 1 (preserve convention)
    for (int32_t i = PALETTE_INDEX_FIRST_COLOR; i < m_colorsPerPalette; i++) {
        m_data[startIndex + i] = color;
    }
    
    m_dirty = true;
}

void PaletteBank::clearPalette(int32_t paletteIndex) {
    if (!isValidPalette(paletteIndex)) {
        return;
    }
    
    int32_t startIndex = getIndex(paletteIndex, 0);
    
    // Clear to transparent except convention colors
    for (int32_t i = 0; i < m_colorsPerPalette; i++) {
        if (i == PALETTE_INDEX_TRANSPARENT) {
            m_data[startIndex + i] = PaletteColor(0, 0, 0, 0);
        } else if (i == PALETTE_INDEX_BLACK) {
            m_data[startIndex + i] = PaletteColor(0, 0, 0, 255);
        } else {
            m_data[startIndex + i] = PaletteColor(0, 0, 0, 0);
        }
    }
    
    m_dirty = true;
}

void PaletteBank::clearAll() {
    for (int32_t i = 0; i < m_paletteCount; i++) {
        clearPalette(i);
    }
}

// =============================================================================
// Preset Palettes
// =============================================================================

bool PaletteBank::loadPreset(int32_t paletteIndex, const char* presetName) {
    if (!isValidPalette(paletteIndex) || !presetName) {
        return false;
    }
    
    // Find preset
    for (const PresetEntry* entry = PRESETS; entry->name != nullptr; entry++) {
        if (std::strcmp(entry->name, presetName) == 0) {
            return setPalette(paletteIndex, entry->data, 16);
        }
    }
    
    return false;
}

std::vector<std::string> PaletteBank::getPresetNames() {
    std::vector<std::string> names;
    
    for (const PresetEntry* entry = PRESETS; entry->name != nullptr; entry++) {
        names.push_back(entry->name);
    }
    
    return names;
}

bool PaletteBank::hasPreset(const char* presetName) {
    if (!presetName) {
        return false;
    }
    
    for (const PresetEntry* entry = PRESETS; entry->name != nullptr; entry++) {
        if (std::strcmp(entry->name, presetName) == 0) {
            return true;
        }
    }
    
    return false;
}

// =============================================================================
// Convention Enforcement
// =============================================================================

void PaletteBank::enforceConvention(int32_t paletteIndex) {
    if (paletteIndex < 0) {
        // Enforce for all palettes
        for (int32_t i = 0; i < m_paletteCount; i++) {
            setDefaultColors(i);
        }
    } else if (isValidPalette(paletteIndex)) {
        setDefaultColors(paletteIndex);
    }
    
    m_dirty = true;
}

bool PaletteBank::checkConvention(int32_t paletteIndex) const {
    if (!isValidPalette(paletteIndex)) {
        return false;
    }
    
    int32_t startIndex = getIndex(paletteIndex, 0);
    
    // Check index 0: transparent black
    const PaletteColor& c0 = m_data[startIndex + PALETTE_INDEX_TRANSPARENT];
    if (c0.r != 0 || c0.g != 0 || c0.b != 0 || c0.a != 0) {
        return false;
    }
    
    // Check index 1: opaque black
    const PaletteColor& c1 = m_data[startIndex + PALETTE_INDEX_BLACK];
    if (c1.r != 0 || c1.g != 0 || c1.b != 0 || c1.a != 255) {
        return false;
    }
    
    return true;
}

void PaletteBank::setDefaultColors(int32_t paletteIndex) {
    if (!isValidPalette(paletteIndex)) {
        return;
    }
    
    int32_t startIndex = getIndex(paletteIndex, 0);
    
    // Index 0: Transparent black
    m_data[startIndex + PALETTE_INDEX_TRANSPARENT] = PaletteColor(0, 0, 0, 0);
    
    // Index 1: Opaque black
    m_data[startIndex + PALETTE_INDEX_BLACK] = PaletteColor(0, 0, 0, 255);
}

// =============================================================================
// GPU Upload
// =============================================================================

void PaletteBank::uploadToGPU(int32_t paletteIndex) {
#ifdef __OBJC__
    if (!m_texture || !m_dirty) {
        return;
    }
    
    id<MTLTexture> texture = (__bridge id<MTLTexture>)m_texture;
    
    if (paletteIndex < 0) {
        // Upload all palettes
        MTLRegion region = MTLRegionMake2D(0, 0, m_colorsPerPalette, m_paletteCount);
        
        [texture replaceRegion:region
                   mipmapLevel:0
                     withBytes:m_data.data()
                   bytesPerRow:m_colorsPerPalette * sizeof(PaletteColor)];
    } else if (isValidPalette(paletteIndex)) {
        // Upload single palette
        MTLRegion region = MTLRegionMake2D(0, paletteIndex, m_colorsPerPalette, 1);
        
        int32_t startIndex = getIndex(paletteIndex, 0);
        
        [texture replaceRegion:region
                   mipmapLevel:0
                     withBytes:&m_data[startIndex]
                   bytesPerRow:m_colorsPerPalette * sizeof(PaletteColor)];
    }
    
    m_dirty = false;
#endif
}

// =============================================================================
// Palette Manipulation
// =============================================================================

void PaletteBank::lerpPalettes(int32_t paletteA, int32_t paletteB, float t, int32_t outPalette) {
    if (!isValidPalette(paletteA) || !isValidPalette(paletteB) || !isValidPalette(outPalette)) {
        return;
    }
    
    t = std::max(0.0f, std::min(1.0f, t));  // Clamp to [0, 1]
    
    int32_t startA = getIndex(paletteA, 0);
    int32_t startB = getIndex(paletteB, 0);
    int32_t startOut = getIndex(outPalette, 0);
    
    for (int32_t i = 0; i < m_colorsPerPalette; i++) {
        m_data[startOut + i] = lerpColor(m_data[startA + i], m_data[startB + i], t);
    }
    
    // Preserve convention
    setDefaultColors(outPalette);
    
    m_dirty = true;
}

void PaletteBank::rotatePalette(int32_t paletteIndex, int32_t startIndex, int32_t endIndex, int32_t amount) {
    if (!isValidPalette(paletteIndex)) {
        return;
    }
    
    // Clamp range
    startIndex = std::max(static_cast<int32_t>(PALETTE_INDEX_FIRST_COLOR), startIndex);  // Don't rotate convention colors
    endIndex = std::min(m_colorsPerPalette - 1, endIndex);
    
    if (startIndex >= endIndex || amount == 0) {
        return;
    }
    
    int32_t paletteStart = getIndex(paletteIndex, 0);
    int32_t rangeSize = endIndex - startIndex + 1;
    
    // Normalize amount to range
    amount = amount % rangeSize;
    if (amount < 0) amount += rangeSize;
    
    if (amount == 0) return;
    
    // Rotate using temporary buffer
    std::vector<PaletteColor> temp(rangeSize);
    
    for (int32_t i = 0; i < rangeSize; i++) {
        temp[i] = m_data[paletteStart + startIndex + i];
    }
    
    for (int32_t i = 0; i < rangeSize; i++) {
        int32_t srcIndex = (i + amount) % rangeSize;
        m_data[paletteStart + startIndex + i] = temp[srcIndex];
    }
    
    m_dirty = true;
}

void PaletteBank::adjustBrightness(int32_t paletteIndex, float brightness) {
    if (!isValidPalette(paletteIndex)) {
        return;
    }
    
    brightness = std::max(0.0f, brightness);  // Don't allow negative
    
    int32_t startIndex = getIndex(paletteIndex, 0);
    
    for (int32_t i = PALETTE_INDEX_FIRST_COLOR; i < m_colorsPerPalette; i++) {
        PaletteColor& color = m_data[startIndex + i];
        color.r = clamp8((int32_t)(color.r * brightness));
        color.g = clamp8((int32_t)(color.g * brightness));
        color.b = clamp8((int32_t)(color.b * brightness));
    }
    
    m_dirty = true;
}

void PaletteBank::adjustSaturation(int32_t paletteIndex, float saturation) {
    if (!isValidPalette(paletteIndex)) {
        return;
    }
    
    saturation = std::max(0.0f, saturation);
    
    int32_t startIndex = getIndex(paletteIndex, 0);
    
    for (int32_t i = PALETTE_INDEX_FIRST_COLOR; i < m_colorsPerPalette; i++) {
        PaletteColor& color = m_data[startIndex + i];
        
        // Convert to grayscale
        float gray = 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;
        
        // Lerp between grayscale and original color
        color.r = clamp8((int32_t)(gray + (color.r - gray) * saturation));
        color.g = clamp8((int32_t)(gray + (color.g - gray) * saturation));
        color.b = clamp8((int32_t)(gray + (color.b - gray) * saturation));
    }
    
    m_dirty = true;
}

// =============================================================================
// Internal Helpers
// =============================================================================

PaletteColor PaletteBank::lerpColor(const PaletteColor& a, const PaletteColor& b, float t) {
    return PaletteColor(
        clamp8((int32_t)(a.r + (b.r - a.r) * t)),
        clamp8((int32_t)(a.g + (b.g - a.g) * t)),
        clamp8((int32_t)(a.b + (b.b - a.b) * t)),
        clamp8((int32_t)(a.a + (b.a - a.a) * t))
    );
}

// =============================================================================
// File I/O (Stubs - to be implemented)
// =============================================================================

bool PaletteBank::loadFromFile(int32_t paletteIndex, const char* filePath) {
    // TODO: Implement file loading (PNG, ACT, GPL, PAL formats)
    return false;
}

bool PaletteBank::saveToFile(int32_t paletteIndex, const char* filePath) const {
    // TODO: Implement file saving
    return false;
}

bool PaletteBank::loadBankFromFile(const char* filePath) {
    // TODO: Implement bank loading
    return false;
}

bool PaletteBank::saveBankToFile(const char* filePath) const {
    // TODO: Implement bank saving
    return false;
}

} // namespace SuperTerminal