//
// WResPaletteManager.h
// SuperTerminal v2
//
// Manages hybrid 256-color palette for WRES mode (432×240 graphics).
// Structure: 16 per-row colors (0-15) + 240 global colors (16-255).
//
// THREAD SAFETY:
// - All public methods are thread-safe
// - Internal state is protected by mutex
//

#ifndef SUPERTERMINAL_WRES_PALETTE_MANAGER_H
#define SUPERTERMINAL_WRES_PALETTE_MANAGER_H

#include <cstdint>
#include <mutex>
#include <memory>
#include "PaletteAutomation.h"

namespace SuperTerminal {

/// Preset palette types for WRES
enum class WResPalettePreset {
    IBM_RGBI,         // IBM CGA/EGA 16-color RGBI (for indices 0-15)
    C64,              // Commodore 64 16-color (for indices 0-15)
    GRAYSCALE,        // 16-level grayscale (for indices 0-15)
    RGB_CUBE_6x8x5    // 6×8×5 RGB cube 240 colors (for indices 16-255)
};

/// WResPaletteManager: Manages hybrid palette for WRES mode (432×240)
///
/// Responsibilities:
/// - Store hybrid palette: per-row (0-15) + global (16-255)
/// - Provide per-row palette customization for indices 0-15
/// - Provide global palette for indices 16-255
/// - Track dirty state for efficient GPU uploads
/// - Thread-safe access for rendering
///
/// Palette Structure:
/// - **Indices 0-15:** Per-row palette (240 rows × 16 colors = 3,840 entries)
/// - **Indices 16-255:** Global palette (240 colors, shared across all rows)
/// - **Total unique colors:** 4,080 palette entries
///
/// Memory Layout (CPU):
/// - Per-row: 240 rows × 16 colors × 4 bytes (RGBA) = 15,360 bytes
/// - Global: 240 colors × 4 bytes (RGBA) = 960 bytes
/// - Total CPU storage: 16,320 bytes
///
/// GPU Layout (float4):
/// - Per-row: 240 rows × 16 colors × 16 bytes (float4) = 61,440 bytes
/// - Global: 240 colors × 16 bytes (float4) = 3,840 bytes
/// - Total GPU buffer: 65,280 bytes
///
/// Usage:
/// - Indices 0-15: Can vary per row (raster effects, per-line gradients)
/// - Indices 16-255: Shared across all rows (sprites, UI, detailed artwork)
class WResPaletteManager {
public:
    /// Constants
    static constexpr int PER_ROW_COLORS = 16;     // Indices 0-15
    static constexpr int GLOBAL_COLORS = 240;     // Indices 16-255
    static constexpr int ROW_COUNT = 240;         // 240 scanlines
    static constexpr int TOTAL_INDICES = 256;     // 0-255
    
    /// Create a new WResPaletteManager
    /// Initializes with default palette:
    /// - Indices 0-15: IBM RGBI colors (all rows)
    /// - Indices 16-255: 6×8×5 RGB cube
    WResPaletteManager();
    
    /// Destructor
    ~WResPaletteManager();
    
    // Disable copy and move (mutex can't be moved)
    WResPaletteManager(const WResPaletteManager&) = delete;
    WResPaletteManager& operator=(const WResPaletteManager&) = delete;
    WResPaletteManager(WResPaletteManager&&) = delete;
    WResPaletteManager& operator=(WResPaletteManager&&) = delete;
    
    /// Set a per-row palette color (indices 0-15 only)
    /// @param row Row/scanline number (0-239)
    /// @param index Color index within per-row palette (0-15)
    /// @param rgba Color in ARGB format (0xAARRGGBB)
    /// @note Thread Safety: Safe to call from any thread
    /// @note Parameters are clamped to valid ranges
    void setPerRowColor(int row, int index, uint32_t rgba);
    
    /// Get a per-row palette color
    /// @param row Row/scanline number (0-239)
    /// @param index Color index within per-row palette (0-15)
    /// @return Color in ARGB format (0xAARRGGBB), or 0 if out of bounds
    /// @note Thread Safety: Safe to call from any thread
    uint32_t getPerRowColor(int row, int index) const;
    
    /// Set a global palette color (indices 16-255 only)
    /// @param index Color index in global palette (16-255)
    /// @param rgba Color in ARGB format (0xAARRGGBB)
    /// @note Thread Safety: Safe to call from any thread
    /// @note Parameters are clamped to valid ranges
    void setGlobalColor(int index, uint32_t rgba);
    
    /// Get a global palette color
    /// @param index Color index in global palette (16-255)
    /// @return Color in ARGB format (0xAARRGGBB), or 0 if out of bounds
    /// @note Thread Safety: Safe to call from any thread
    uint32_t getGlobalColor(int index) const;
    
    /// Set all rows to have the same per-row color (broadcast)
    /// @param index Color index within per-row palette (0-15)
    /// @param rgba Color in ARGB format (0xAARRGGBB)
    /// @note Thread Safety: Safe to call from any thread
    /// @note Useful for quickly setting a "global" color in the per-row range
    void setAllRowsToColor(int index, uint32_t rgba);
    
    /// Load a preset palette
    /// @param preset Preset type (IBM_RGBI, C64, etc.)
    /// @note Thread Safety: Safe to call from any thread
    /// @note 16-color presets apply to indices 0-15 (all rows)
    /// @note RGB_CUBE_6x8x5 applies to indices 16-255
    void loadPresetPalette(WResPalettePreset preset);
    
    /// Load preset palette to specific row range
    /// @param preset Preset type (only 16-color presets valid)
    /// @param startRow First row to modify (0-239)
    /// @param endRow Last row to modify (0-239, inclusive)
    /// @note Thread Safety: Safe to call from any thread
    /// @note Only affects indices 0-15 for specified rows
    void loadPresetPaletteRows(WResPalettePreset preset, int startRow, int endRow);
    
    // -------------------------------------------------------------------------
    // Palette Automation (Copper-style effects)
    // -------------------------------------------------------------------------
    
    /// Enable automatic gradient effect on a palette index
    /// @param paletteIndex Which palette index to modify (0-15)
    /// @param startRow First scanline (0-239)
    /// @param endRow Last scanline (0-239, inclusive)
    /// @param startR Start color red (0-255)
    /// @param startG Start color green (0-255)
    /// @param startB Start color blue (0-255)
    /// @param endR End color red (0-255)
    /// @param endG End color green (0-255)
    /// @param endB End color blue (0-255)
    /// @param speed Animation speed (0.0 = static, > 0 = animated)
    /// @note Thread Safety: Safe to call from any thread
    void enableGradientAutomation(int paletteIndex, int startRow, int endRow,
                                   int startR, int startG, int startB,
                                   int endR, int endG, int endB,
                                   float speed = 0.0f);
    
    /// Enable automatic color bars effect on a palette index
    /// @param paletteIndex Which palette index to modify (0-15)
    /// @param startRow First scanline (0-239)
    /// @param endRow Last scanline (0-239, inclusive)
    /// @param barHeight Height of each bar in scanlines
    /// @param colors Array of RGB colors (up to 4 colors, each 3 bytes)
    /// @param numColors Number of colors in array (1-4)
    /// @param speed Animation speed (0.0 = static, > 0 = animated)
    /// @note Thread Safety: Safe to call from any thread
    void enableBarsAutomation(int paletteIndex, int startRow, int endRow,
                              int barHeight, const uint8_t colors[][3], int numColors,
                              float speed = 0.0f);
    
    /// Disable all palette automation
    /// @note Thread Safety: Safe to call from any thread
    void disableAutomation();
    
    /// Update palette automation (call once per frame)
    /// @param deltaTime Time elapsed since last update (in seconds)
    /// @note Thread Safety: Safe to call from any thread
    /// @note This applies the automation effects to the palette
    void updateAutomation(float deltaTime);
    
    /// Check if any automation is active
    /// @return true if any automation effect is enabled
    bool isAutomationActive() const;
    
    /// Get raw palette data for GPU upload (converted to float4)
    /// @return Pointer to float palette buffer (65,280 bytes)
    /// @note Thread Safety: Caller must hold lock via getMutex()
    /// @note Data format: float4[240][16] (per-row) + float4[240] (global)
    /// @note This is expensive - caches converted data, only regenerates when dirty
    const float* getPaletteDataFloat() const;
    
    /// Get palette data size in bytes (float format)
    /// @return Size of GPU palette buffer (65,280 bytes)
    size_t getPaletteDataSize() const {
        return (ROW_COUNT * PER_ROW_COLORS + GLOBAL_COLORS) * 4 * sizeof(float);
    }
    
    /// Check if palette data has changed since last clearDirty()
    /// @return true if palette has been modified
    /// @note Thread Safety: Safe to call from any thread
    bool isDirty() const;
    
    /// Clear the dirty flag (call after GPU upload)
    /// @note Thread Safety: Safe to call from any thread
    void clearDirty();
    
    /// Get mutex for external synchronization (e.g., during GPU upload)
    /// @return Reference to internal mutex
    std::mutex& getMutex() const { return m_mutex; }

private:
    /// Per-row palette storage: 240 rows × 16 colors × 4 components (RGBA)
    /// Total: 15,360 bytes
    uint8_t m_perRowPalettes[240][16][4];
    
    /// Global palette storage: 240 colors × 4 components (RGBA)
    /// Total: 960 bytes
    uint8_t m_globalPalette[240][4];
    
    /// Cached float4 data for GPU upload
    /// Total: 65,280 bytes (240×16×16 + 240×16)
    mutable std::unique_ptr<float[]> m_floatData;
    
    /// Flag indicating float data needs regeneration
    mutable bool m_floatDataDirty;
    
    /// Dirty flag for tracking palette changes
    bool m_dirty;
    
    /// Mutex for thread-safe access
    mutable std::mutex m_mutex;
    
    /// Palette automation state
    PaletteGradientAutomation m_gradientAutomation;
    PaletteBarsAutomation m_barsAutomation;
    
    /// Initialize default palette (IBM RGBI 0-15, RGB cube 16-255)
    void initDefaultPalette();
    
    /// Apply gradient automation to palette (internal)
    void applyGradientAutomation();
    
    /// Apply bars automation to palette (internal)
    void applyBarsAutomation();
    
    /// Convert uint8_t RGBA palette to float4 format for GPU
    /// @note Must be called with mutex held
    void convertToFloat() const;
    
    /// Helper: Unpack 0xAARRGGBB to RGBA components
    static void unpackRGBA(uint32_t rgba, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a);
    
    /// Helper: Pack RGBA components to 0xAARRGGBB
    static uint32_t packRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_WRES_PALETTE_MANAGER_H