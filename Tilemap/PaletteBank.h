//
// PaletteBank.h
// SuperTerminal Framework - Tilemap System
//
// Palette bank management for indexed color tiles
// Manages multiple 16-color palettes with GPU texture storage
//
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef PALETTEBANK_H
#define PALETTEBANK_H

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

#ifdef __OBJC__
@protocol MTLDevice;
@protocol MTLTexture;
typedef id<MTLDevice> MTLDevicePtr;
typedef id<MTLTexture> MTLTexturePtr;
#else
typedef void* MTLDevicePtr;
typedef void* MTLTexturePtr;
#endif

namespace SuperTerminal {

/// RGBA color (8-bit per channel)
struct PaletteColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
    
    PaletteColor() = default;
    PaletteColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) 
        : r(r), g(g), b(b), a(a) {}
    
    bool operator==(const PaletteColor& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
    
    bool operator!=(const PaletteColor& other) const {
        return !(*this == other);
    }
    
    /// Check if color is transparent
    bool isTransparent() const { return a == 0; }
    
    /// Check if color is opaque
    bool isOpaque() const { return a == 255; }
    
    /// Convert to 32-bit RGBA integer
    uint32_t toRGBA32() const {
        return (r << 24) | (g << 16) | (b << 8) | a;
    }
    
    /// Convert from 32-bit RGBA integer
    static PaletteColor fromRGBA32(uint32_t rgba) {
        return PaletteColor(
            (rgba >> 24) & 0xFF,
            (rgba >> 16) & 0xFF,
            (rgba >> 8) & 0xFF,
            rgba & 0xFF
        );
    }
};

/// Palette constants
constexpr int32_t PALETTE_DEFAULT_COUNT = 32;      // 32 palettes
constexpr int32_t PALETTE_DEFAULT_COLORS = 16;     // 16 colors each
constexpr int32_t PALETTE_MAX_COLORS = 256;        // Maximum colors per palette

/// Palette index constants (enforced convention for 16-color palettes)
constexpr uint8_t PALETTE_INDEX_TRANSPARENT = 0;   // Always transparent black
constexpr uint8_t PALETTE_INDEX_BLACK = 1;         // Always opaque black
constexpr uint8_t PALETTE_INDEX_FIRST_COLOR = 2;   // First user-defined color
constexpr uint8_t PALETTE_USABLE_COLORS = 14;      // 14 usable colors (2-15)

/// PaletteBank: Manages multiple color palettes with GPU storage
///
/// Features:
/// - Multiple palettes (default 32)
/// - 16 colors per palette (4-bit indexed)
/// - GPU texture storage (uploaded on demand)
/// - Preset palette library
/// - File I/O (PNG, ACT, GPL formats)
/// - Convention enforcement (index 0=transparent, 1=black)
///
/// Memory Layout:
/// - CPU: paletteCount × colorsPerPalette × 4 bytes (RGBA)
/// - GPU: Same as CPU, stored as Metal texture
///
/// Thread Safety: Not thread-safe. Use from render thread only.
///
class PaletteBank {
public:
    // =================================================================
    // Construction
    // =================================================================
    
    /// Create palette bank
    /// @param paletteCount Number of palettes (default 32)
    /// @param colorsPerPalette Colors per palette (default 16)
    /// @param device Metal device (nullptr = use default)
    PaletteBank(int32_t paletteCount = PALETTE_DEFAULT_COUNT,
                int32_t colorsPerPalette = PALETTE_DEFAULT_COLORS,
                MTLDevicePtr device = nullptr);
    
    /// Destructor
    ~PaletteBank();
    
    // Disable copy, allow move
    PaletteBank(const PaletteBank&) = delete;
    PaletteBank& operator=(const PaletteBank&) = delete;
    PaletteBank(PaletteBank&&) noexcept;
    PaletteBank& operator=(PaletteBank&&) noexcept;
    
    // =================================================================
    // Initialization
    // =================================================================
    
    /// Initialize GPU resources
    /// @param device Metal device
    /// @return true on success
    bool initialize(MTLDevicePtr device);
    
    /// Check if initialized
    bool isInitialized() const { return m_texture != nullptr; }
    
    /// Shutdown and release GPU resources
    void shutdown();
    
    // =================================================================
    // Palette Operations
    // =================================================================
    
    /// Set entire palette
    /// @param paletteIndex Palette index (0-31)
    /// @param colors Array of colors
    /// @param colorCount Number of colors (must match colorsPerPalette)
    /// @return true on success
    bool setPalette(int32_t paletteIndex, const PaletteColor* colors, int32_t colorCount);
    
    /// Get entire palette
    /// @param paletteIndex Palette index (0-31)
    /// @param outColors Output color array (must have space for colorsPerPalette)
    /// @param maxColors Maximum colors to read
    /// @return Number of colors read, or 0 on error
    int32_t getPalette(int32_t paletteIndex, PaletteColor* outColors, int32_t maxColors) const;
    
    /// Set single color in palette
    /// @param paletteIndex Palette index (0-31)
    /// @param colorIndex Color index (0-15)
    /// @param color Color to set
    /// @return true on success
    bool setColor(int32_t paletteIndex, int32_t colorIndex, const PaletteColor& color);
    
    /// Get single color from palette
    /// @param paletteIndex Palette index (0-31)
    /// @param colorIndex Color index (0-15)
    /// @return Color at index, or transparent black on error
    PaletteColor getColor(int32_t paletteIndex, int32_t colorIndex) const;
    
    /// Copy palette from one index to another
    /// @param srcIndex Source palette index
    /// @param dstIndex Destination palette index
    /// @return true on success
    bool copyPalette(int32_t srcIndex, int32_t dstIndex);
    
    /// Fill palette with single color
    /// @param paletteIndex Palette index
    /// @param color Color to fill with
    void fillPalette(int32_t paletteIndex, const PaletteColor& color);
    
    /// Clear palette (set all to transparent black)
    void clearPalette(int32_t paletteIndex);
    
    /// Clear all palettes
    void clearAll();
    
    // =================================================================
    // Preset Palettes
    // =================================================================
    
    /// Load preset palette by name
    /// @param paletteIndex Palette index to load into
    /// @param presetName Preset name (e.g., "desert", "forest", "cave")
    /// @return true on success
    bool loadPreset(int32_t paletteIndex, const char* presetName);
    
    /// Get list of available preset names
    static std::vector<std::string> getPresetNames();
    
    /// Check if preset exists
    static bool hasPreset(const char* presetName);
    
    // =================================================================
    // File I/O
    // =================================================================
    
    /// Load palette from file (PNG, ACT, GPL, PAL formats)
    /// @param paletteIndex Palette index to load into
    /// @param filePath Path to palette file
    /// @return true on success
    bool loadFromFile(int32_t paletteIndex, const char* filePath);
    
    /// Save palette to file
    /// @param paletteIndex Palette index to save
    /// @param filePath Path to save to
    /// @return true on success
    bool saveToFile(int32_t paletteIndex, const char* filePath) const;
    
    /// Load all palettes from bank file
    /// @param filePath Path to bank file
    /// @return true on success
    bool loadBankFromFile(const char* filePath);
    
    /// Save all palettes to bank file
    /// @param filePath Path to save to
    /// @return true on success
    bool saveBankToFile(const char* filePath) const;
    
    // =================================================================
    // Convention Enforcement
    // =================================================================
    
    /// Enforce palette convention (index 0=transparent, 1=black)
    /// @param paletteIndex Palette index to enforce (-1 = all palettes)
    void enforceConvention(int32_t paletteIndex = -1);
    
    /// Check if palette follows convention
    /// @param paletteIndex Palette index to check
    /// @return true if follows convention
    bool checkConvention(int32_t paletteIndex) const;
    
    /// Set default colors (index 0 and 1) for palette
    void setDefaultColors(int32_t paletteIndex);
    
    // =================================================================
    // Properties
    // =================================================================
    
    /// Get number of palettes
    inline int32_t getPaletteCount() const { return m_paletteCount; }
    
    /// Get colors per palette
    inline int32_t getColorsPerPalette() const { return m_colorsPerPalette; }
    
    /// Get total color count
    inline int32_t getTotalColorCount() const { 
        return m_paletteCount * m_colorsPerPalette; 
    }
    
    /// Get palette name
    const std::string& getName() const { return m_name; }
    
    /// Set palette name
    void setName(const std::string& name) { m_name = name; }
    
    // =================================================================
    // Validation
    // =================================================================
    
    /// Check if palette index is valid
    inline bool isValidPalette(int32_t paletteIndex) const {
        return paletteIndex >= 0 && paletteIndex < m_paletteCount;
    }
    
    /// Check if color index is valid
    inline bool isValidColor(int32_t colorIndex) const {
        return colorIndex >= 0 && colorIndex < m_colorsPerPalette;
    }
    
    // =================================================================
    // GPU Access
    // =================================================================
    
    /// Get GPU texture (Metal texture containing all palettes)
    /// @return Metal texture (RGBA8, width=colorsPerPalette, height=paletteCount)
    MTLTexturePtr getTexture() const { return m_texture; }
    
    /// Upload palette data to GPU
    /// @param paletteIndex Palette to upload (-1 = all palettes)
    void uploadToGPU(int32_t paletteIndex = -1);
    
    /// Check if palette data needs GPU upload
    bool isDirty() const { return m_dirty; }
    
    /// Mark palette as dirty (needs GPU upload)
    void markDirty() { m_dirty = true; }
    
    /// Clear dirty flag
    void clearDirty() { m_dirty = false; }
    
    // =================================================================
    // Direct Data Access
    // =================================================================
    
    /// Get raw color data pointer (read-only)
    const PaletteColor* getData() const { return m_data.data(); }
    
    /// Get raw color data pointer (mutable - marks dirty)
    PaletteColor* getDataMutable() { 
        m_dirty = true;
        return m_data.data(); 
    }
    
    /// Get memory size in bytes
    size_t getMemorySize() const {
        return m_data.size() * sizeof(PaletteColor);
    }
    
    // =================================================================
    // Palette Manipulation
    // =================================================================
    
    /// Lerp between two palettes
    /// @param paletteA First palette index
    /// @param paletteB Second palette index
    /// @param t Interpolation factor (0.0 = A, 1.0 = B)
    /// @param outPalette Output palette index
    void lerpPalettes(int32_t paletteA, int32_t paletteB, float t, int32_t outPalette);
    
    /// Rotate palette colors (for animation effects)
    /// @param paletteIndex Palette index
    /// @param startIndex Start color index
    /// @param endIndex End color index
    /// @param amount Rotation amount (positive = right, negative = left)
    void rotatePalette(int32_t paletteIndex, int32_t startIndex, int32_t endIndex, int32_t amount);
    
    /// Adjust palette brightness
    /// @param paletteIndex Palette index
    /// @param brightness Brightness multiplier (1.0 = normal, 0.5 = darker, 2.0 = brighter)
    void adjustBrightness(int32_t paletteIndex, float brightness);
    
    /// Adjust palette saturation
    /// @param paletteIndex Palette index
    /// @param saturation Saturation multiplier (0.0 = grayscale, 1.0 = normal, 2.0 = vibrant)
    void adjustSaturation(int32_t paletteIndex, float saturation);
    
private:
    // =================================================================
    // Internal State
    // =================================================================
    
    // Configuration
    int32_t m_paletteCount;
    int32_t m_colorsPerPalette;
    std::string m_name;
    
    // CPU data storage
    // Layout: [palette0_color0, palette0_color1, ..., palette1_color0, ...]
    std::vector<PaletteColor> m_data;
    
    // GPU resources
    MTLDevicePtr m_device;
    MTLTexturePtr m_texture;
    
    // Dirty tracking
    bool m_dirty;
    
    // =================================================================
    // Internal Helpers
    // =================================================================
    
    /// Calculate linear index for palette/color
    inline int32_t getIndex(int32_t paletteIndex, int32_t colorIndex) const {
        return paletteIndex * m_colorsPerPalette + colorIndex;
    }
    
    /// Create GPU texture
    bool createTexture();
    
    /// Load preset palette data
    static const PaletteColor* getPresetData(const char* presetName, int32_t& outColorCount);
    
    /// Clamp value to range
    static inline uint8_t clamp8(int32_t value) {
        return value < 0 ? 0 : (value > 255 ? 255 : (uint8_t)value);
    }
    
    /// Lerp between two colors
    static PaletteColor lerpColor(const PaletteColor& a, const PaletteColor& b, float t);
};

} // namespace SuperTerminal

#endif // PALETTEBANK_H