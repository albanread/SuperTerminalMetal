//
//  ScreenMode.h
//  SuperTerminal Framework - Screen Mode Manager
//
//  Configurable text grid resolutions (20x12, 40x25, 80x25, 80x50, 90x60)
//  Copyright © 2024 SuperTerminal. All rights reserved.
//  really only 80x25 tested.

#ifndef SCREEN_MODE_H
#define SCREEN_MODE_H

#include <string>
#include <vector>
#include <cstdint>

namespace SuperTerminal {

// Forward declarations
class TextGrid;
class DisplayManager;

// =============================================================================
// ScreenModeInfo - Text mode configuration
// =============================================================================

struct ScreenModeInfo {
    int columns;            // Grid width in characters
    int rows;               // Grid height in characters
    int cellWidth;          // Character cell width in pixels
    int cellHeight;         // Character cell height in pixels
    std::string name;       // Display name (e.g., "80×25")
    std::string description;// Description (e.g., "Standard VGA")

    ScreenModeInfo()
        : columns(80), rows(25), cellWidth(8), cellHeight(16)
        , name("80×25"), description("Standard")
    {}

    ScreenModeInfo(int cols, int rows_, int cw, int ch, const char* n, const char* desc)
        : columns(cols), rows(rows_), cellWidth(cw), cellHeight(ch)
        , name(n), description(desc)
    {}

    // Get total window size in pixels
    int getWindowWidth() const { return columns * cellWidth; }
    int getWindowHeight() const { return rows * cellHeight; }

    // Get total character cells
    int getTotalCells() const { return columns * rows; }

    // Check if same mode
    bool operator==(const ScreenModeInfo& other) const {
        return columns == other.columns && rows == other.rows;
    }

    bool operator!=(const ScreenModeInfo& other) const {
        return !(*this == other);
    }
};

// =============================================================================
// ScreenMode - Predefined screen modes (classic retro resolutions)
// =============================================================================

enum class ScreenMode {
    MODE_20x12,     // Tiny (early 8-bit systems)
    MODE_40x25,     // Classic (Commodore 64, Apple II, Atari)
    MODE_80x25,     // Standard (DOS, VGA text mode)
    MODE_80x50,     // High-res (VGA 50-line mode)
    MODE_90x60,     // Modern (large displays)
    CUSTOM          // User-defined mode
};

// =============================================================================
// ScreenModeManager - Manage screen mode selection and switching
// =============================================================================

class ScreenModeManager {
public:
    // Constructor
    ScreenModeManager();

    // Destructor
    ~ScreenModeManager();

    // =========================================================================
    // Predefined Modes
    // =========================================================================

    /// Get all available screen modes
    std::vector<ScreenModeInfo> getAvailableModes() const;

    /// Get info for specific mode
    ScreenModeInfo getModeInfo(ScreenMode mode) const;

    /// Get mode by name
    ScreenMode getModeByName(const std::string& name) const;

    /// Get current mode
    ScreenMode getCurrentMode() const { return m_currentMode; }

    /// Get current mode info
    ScreenModeInfo getCurrentModeInfo() const;

    // =========================================================================
    // Mode Switching
    // =========================================================================

    /// Set screen mode
    /// @param mode Screen mode to set
    /// @return true if successful
    bool setMode(ScreenMode mode);

    /// Set custom mode
    /// @param columns Number of columns
    /// @param rows Number of rows
    /// @return true if successful
    bool setCustomMode(int columns, int rows);

    /// Set mode by name (e.g., "80×25")
    /// @param name Mode name
    /// @return true if successful
    bool setModeByName(const std::string& name);

    // =========================================================================
    // Font Scaling
    // =========================================================================

    /// Get font scaling mode
    enum class FontScaling {
        FIXED_WINDOW,   // Scale font to fit fixed window size
        FIXED_FONT,     // Resize window to fit fixed font size
        AUTO            // Automatic (prefer fixed window)
    };

    /// Set font scaling mode
    void setFontScaling(FontScaling scaling) { m_fontScaling = scaling; }

    /// Get font scaling mode
    FontScaling getFontScaling() const { return m_fontScaling; }

    /// Calculate optimal cell size for window
    /// @param windowWidth Window width in pixels
    /// @param windowHeight Window height in pixels
    /// @param outCellWidth Output: cell width
    /// @param outCellHeight Output: cell height
    void calculateCellSize(int windowWidth, int windowHeight,
                          int& outCellWidth, int& outCellHeight) const;

    /// Calculate optimal window size for mode
    /// @param mode Screen mode
    /// @param outWidth Output: window width
    /// @param outHeight Output: window height
    void calculateWindowSize(ScreenMode mode,
                            int& outWidth, int& outHeight) const;

    // =========================================================================
    // Integration with DisplayManager
    // =========================================================================

    /// Apply current mode to TextGrid
    /// @param textGrid TextGrid to configure
    /// @return true if successful
    bool applyToTextGrid(TextGrid* textGrid);

    /// Apply mode and resize window
    /// @param textGrid TextGrid to configure
    /// @param windowWidth Current window width
    /// @param windowHeight Current window height
    /// @param outNewWidth Output: new window width
    /// @param outNewHeight Output: new window height
    /// @return true if successful
    bool applyModeAndResize(TextGrid* textGrid,
                           int windowWidth, int windowHeight,
                           int& outNewWidth, int& outNewHeight);

    // =========================================================================
    // Persistence
    // =========================================================================

    /// Save current mode to preferences
    void saveToPreferences();

    /// Load mode from preferences
    /// @return true if loaded successfully
    bool loadFromPreferences();

    /// Get preference key for screen mode
    static const char* getPreferenceKey() { return "ScreenMode"; }

    /// Get preference key for custom columns
    static const char* getCustomColumnsKey() { return "ScreenModeCustomColumns"; }

    /// Get preference key for custom rows
    static const char* getCustomRowsKey() { return "ScreenModeCustomRows"; }

    // =========================================================================
    // Utility
    // =========================================================================

    /// Convert ScreenMode to string
    static const char* modeToString(ScreenMode mode);

    /// Convert string to ScreenMode
    static ScreenMode stringToMode(const char* str);

    /// Check if mode is valid
    static bool isValidMode(int columns, int rows);

    /// Get recommended cell size for mode
    static void getRecommendedCellSize(int columns, int rows,
                                      int& outCellWidth, int& outCellHeight);

private:
    // =========================================================================
    // Member Variables
    // =========================================================================

    ScreenMode m_currentMode;           // Current screen mode
    ScreenModeInfo m_customModeInfo;    // Custom mode configuration
    FontScaling m_fontScaling;          // Font scaling strategy

    // Default cell sizes (for FIXED_FONT mode)
    int m_defaultCellWidth;
    int m_defaultCellHeight;

    // Predefined modes database
    static const ScreenModeInfo s_predefinedModes[];
    static const int s_predefinedModeCount;

    // =========================================================================
    // Internal Helpers
    // =========================================================================

    /// Initialize predefined modes
    void initializePredefinedModes();

    /// Validate mode configuration
    bool validateMode(const ScreenModeInfo& info) const;

    /// Notify mode change listeners (future: event system)
    void notifyModeChanged();
};

// =============================================================================
// Global Access (Optional)
// =============================================================================

/// Get global screen mode manager instance
ScreenModeManager* getScreenModeManager();

/// Set global screen mode manager instance
void setScreenModeManager(ScreenModeManager* manager);

} // namespace SuperTerminal

#endif // SCREEN_MODE_H
