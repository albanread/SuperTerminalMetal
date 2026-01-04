//
// LoResPaletteManager.h
// SuperTerminal v2
//
// Manages up to 300 independent 16-color RGBA palettes for chunky graphics.
// Supports LORES (75 rows), MIDRES (150 rows), and HIRES (300 rows).
//
// THREAD SAFETY:
// - All public methods are thread-safe
// - Internal state is protected by mutex
//

#ifndef SUPERTERMINAL_LORES_PALETTE_MANAGER_H
#define SUPERTERMINAL_LORES_PALETTE_MANAGER_H

#include <cstdint>
#include <mutex>

namespace SuperTerminal {

/// Preset palette types
enum class LoResPaletteType {
    IBM,    // RGBI 16-color palette (CGA/EGA)
    C64     // Commodore 64 16-color palette
};

/// LoResPaletteManager: Manages up to 300 × 16 RGBA palettes for chunky graphics
///
/// Responsibilities:
/// - Store 300 independent 16-color palettes (19,200 bytes total)
/// - Provide preset palettes (IBM RGBI, Commodore 64)
/// - Allow per-entry palette customization
/// - Track dirty state for efficient GPU uploads
/// - Thread-safe access for rendering
///
/// Memory Layout:
/// - 300 palettes × 16 colors × 4 components (RGBA)
/// - Total: 19,200 bytes
/// - Each color: uint8_t[4] = {R, G, B, A}
///
/// Row Mapping:
/// - Pixel Row 0  → Palette 0
/// - Pixel Row 1  → Palette 1
/// - ...
/// - Pixel Row 299 → Palette 299
/// - One palette per pixel row (supports up to 640×300 resolution)
class LoResPaletteManager {
public:
    /// Create a new LoResPaletteManager
    /// Initializes with IBM RGBI palette by default for all 300 rows
    LoResPaletteManager();
    
    /// Destructor
    ~LoResPaletteManager();
    
    // Disable copy and move (mutex can't be moved)
    LoResPaletteManager(const LoResPaletteManager&) = delete;
    LoResPaletteManager& operator=(const LoResPaletteManager&) = delete;
    LoResPaletteManager(LoResPaletteManager&&) = delete;
    LoResPaletteManager& operator=(LoResPaletteManager&&) = delete;
    
    /// Set all 300 palettes to a preset configuration
    /// @param type Palette preset (IBM or C64)
    /// @note Thread Safety: Safe to call from any thread
    void setAllPalettes(LoResPaletteType type);
    
    /// Set a specific palette entry
    /// @param row Palette row (0-299)
    /// @param index Color index within palette (0-15)
    /// @param rgba Color in ARGB format (0xAARRGGBB)
    /// @note Thread Safety: Safe to call from any thread
    /// @note Invalid parameters are clamped to valid ranges
    void setPaletteEntry(int row, int index, uint32_t rgba);
    
    /// Get a specific palette entry
    /// @param row Palette row (0-299)
    /// @param index Color index within palette (0-15)
    /// @return Color in ARGB format (0xAARRGGBB)
    /// @note Thread Safety: Safe to call from any thread
    /// @note Returns 0x00000000 for invalid parameters
    uint32_t getPaletteEntry(int row, int index) const;
    
    /// Get raw palette data for GPU upload
    /// @return Pointer to palette data (19,200 bytes)
    /// @note Thread Safety: Safe to call from render thread with lock held
    /// @note Data format: [row][color][component] where component is RGBA
    /// @note Size: 19,200 bytes (300 rows × 16 colors × 4 components)
    const uint8_t* getPaletteData() const { return &m_palettes[0][0][0]; }
    
    /// Get palette data size in bytes
    /// @return Size of palette data (19,200 bytes)
    size_t getPaletteDataSize() const { return 300 * 16 * 4; }
    
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
    /// Palette storage: 300 palettes × 16 colors × 4 components (RGBA)
    uint8_t m_palettes[300][16][4];
    
    /// Dirty flag for tracking changes
    bool m_dirty;
    
    /// Mutex for thread-safe access
    mutable std::mutex m_mutex;
    
    /// Initialize all palettes with IBM RGBI colors
    void initIBMPalette();
    
    /// Initialize all palettes with Commodore 64 colors
    void initC64Palette();
    
    /// Set all palettes to a single palette definition
    /// @param palette Array of 16 ARGB colors (0xAARRGGBB)
    void setAllPalettesTo(const uint32_t palette[16]);
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_LORES_PALETTE_MANAGER_H