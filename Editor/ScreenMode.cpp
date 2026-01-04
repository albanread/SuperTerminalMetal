//
//  ScreenMode.cpp
//  SuperTerminal Framework - Screen Mode Manager Implementation
//
//  Configurable text grid resolutions (20x12, 40x25, 80x25, 80x50, 90x60)
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "ScreenMode.h"
#include "Display/TextGrid.h"
#include <algorithm>

namespace SuperTerminal {

// =============================================================================
// Predefined Screen Modes
// =============================================================================

const ScreenModeInfo ScreenModeManager::s_predefinedModes[] = {
    // MODE_20x12 - Tiny (early 8-bit systems, ZX Spectrum style)
    ScreenModeInfo(20, 12, 16, 16, "20×12", "Tiny (Early 8-bit)"),
    
    // MODE_40x25 - Classic (Commodore 64, Apple II, Atari 800)
    ScreenModeInfo(40, 25, 16, 16, "40×25", "Classic (C64/Apple II)"),
    
    // MODE_80x25 - Standard (DOS VGA text mode, most common)
    ScreenModeInfo(80, 25, 8, 16, "80×25", "Standard (VGA)"),
    
    // MODE_80x50 - High-res (VGA 50-line mode)
    ScreenModeInfo(80, 50, 8, 8, "80×50", "High Resolution"),
    
    // MODE_90x60 - Modern (large displays, modern terminals)
    ScreenModeInfo(90, 60, 10, 12, "90×60", "Modern (Large Display)")
};

const int ScreenModeManager::s_predefinedModeCount = 
    sizeof(s_predefinedModes) / sizeof(s_predefinedModes[0]);

// =============================================================================
// Global Instance
// =============================================================================

static ScreenModeManager* g_screenModeManager = nullptr;

ScreenModeManager* getScreenModeManager() {
    return g_screenModeManager;
}

void setScreenModeManager(ScreenModeManager* manager) {
    g_screenModeManager = manager;
}

// =============================================================================
// ScreenModeManager Implementation
// =============================================================================

ScreenModeManager::ScreenModeManager()
    : m_currentMode(ScreenMode::MODE_80x25)
    , m_fontScaling(FontScaling::FIXED_WINDOW)
    , m_defaultCellWidth(8)
    , m_defaultCellHeight(16)
{
    initializePredefinedModes();
}

ScreenModeManager::~ScreenModeManager() {
    // Nothing to clean up
}

// =============================================================================
// Predefined Modes
// =============================================================================

void ScreenModeManager::initializePredefinedModes() {
    // Modes are statically defined, nothing to do here
}

std::vector<ScreenModeInfo> ScreenModeManager::getAvailableModes() const {
    std::vector<ScreenModeInfo> modes;
    
    for (int i = 0; i < s_predefinedModeCount; ++i) {
        modes.push_back(s_predefinedModes[i]);
    }
    
    return modes;
}

ScreenModeInfo ScreenModeManager::getModeInfo(ScreenMode mode) const {
    if (mode == ScreenMode::CUSTOM) {
        return m_customModeInfo;
    }
    
    int index = static_cast<int>(mode);
    if (index >= 0 && index < s_predefinedModeCount) {
        return s_predefinedModes[index];
    }
    
    // Default to 80x25
    return s_predefinedModes[static_cast<int>(ScreenMode::MODE_80x25)];
}

ScreenMode ScreenModeManager::getModeByName(const std::string& name) const {
    for (int i = 0; i < s_predefinedModeCount; ++i) {
        if (s_predefinedModes[i].name == name) {
            return static_cast<ScreenMode>(i);
        }
    }
    
    return ScreenMode::MODE_80x25; // Default
}

ScreenModeInfo ScreenModeManager::getCurrentModeInfo() const {
    return getModeInfo(m_currentMode);
}

// =============================================================================
// Mode Switching
// =============================================================================

bool ScreenModeManager::setMode(ScreenMode mode) {
    if (mode == ScreenMode::CUSTOM) {
        // Custom mode requires explicit configuration
        return false;
    }
    
    ScreenModeInfo info = getModeInfo(mode);
    if (!validateMode(info)) {
        return false;
    }
    
    m_currentMode = mode;
    notifyModeChanged();
    
    return true;
}

bool ScreenModeManager::setCustomMode(int columns, int rows) {
    if (!isValidMode(columns, rows)) {
        return false;
    }
    
    m_customModeInfo.columns = columns;
    m_customModeInfo.rows = rows;
    m_customModeInfo.name = std::to_string(columns) + "×" + std::to_string(rows);
    m_customModeInfo.description = "Custom Mode";
    
    // Calculate recommended cell size
    getRecommendedCellSize(columns, rows, 
                          m_customModeInfo.cellWidth, 
                          m_customModeInfo.cellHeight);
    
    m_currentMode = ScreenMode::CUSTOM;
    notifyModeChanged();
    
    return true;
}

bool ScreenModeManager::setModeByName(const std::string& name) {
    ScreenMode mode = getModeByName(name);
    return setMode(mode);
}

// =============================================================================
// Font Scaling
// =============================================================================

void ScreenModeManager::calculateCellSize(int windowWidth, int windowHeight,
                                         int& outCellWidth, int& outCellHeight) const
{
    ScreenModeInfo info = getCurrentModeInfo();
    
    if (m_fontScaling == FontScaling::FIXED_FONT) {
        outCellWidth = m_defaultCellWidth;
        outCellHeight = m_defaultCellHeight;
    } else {
        // Calculate cell size to fit window
        outCellWidth = windowWidth / info.columns;
        outCellHeight = windowHeight / info.rows;
        
        // Ensure minimum cell size
        outCellWidth = std::max(4, outCellWidth);
        outCellHeight = std::max(8, outCellHeight);
    }
}

void ScreenModeManager::calculateWindowSize(ScreenMode mode,
                                           int& outWidth, int& outHeight) const
{
    ScreenModeInfo info = getModeInfo(mode);
    
    if (m_fontScaling == FontScaling::FIXED_FONT) {
        outWidth = info.columns * m_defaultCellWidth;
        outHeight = info.rows * m_defaultCellHeight;
    } else {
        outWidth = info.getWindowWidth();
        outHeight = info.getWindowHeight();
    }
}

// =============================================================================
// Integration with DisplayManager
// =============================================================================

bool ScreenModeManager::applyToTextGrid(TextGrid* textGrid) {
    if (!textGrid) {
        return false;
    }
    
    ScreenModeInfo info = getCurrentModeInfo();
    
    if (!validateMode(info)) {
        return false;
    }
    
    textGrid->resize(info.columns, info.rows);
    
    return true;
}

bool ScreenModeManager::applyModeAndResize(TextGrid* textGrid,
                                          int windowWidth, int windowHeight,
                                          int& outNewWidth, int& outNewHeight)
{
    if (!textGrid) {
        return false;
    }
    
    ScreenModeInfo info = getCurrentModeInfo();
    
    if (!validateMode(info)) {
        return false;
    }
    
    // Resize text grid
    textGrid->resize(info.columns, info.rows);
    
    // Calculate new window size based on scaling mode
    if (m_fontScaling == FontScaling::FIXED_FONT) {
        // Resize window to fit font
        outNewWidth = info.columns * m_defaultCellWidth;
        outNewHeight = info.rows * m_defaultCellHeight;
    } else {
        // Keep window size, scale font
        outNewWidth = windowWidth;
        outNewHeight = windowHeight;
    }
    
    return true;
}

// =============================================================================
// Persistence
// =============================================================================

void ScreenModeManager::saveToPreferences() {
    // TODO: Implement using NSUserDefaults or similar
    // For now, this is a placeholder
    
    // Example implementation (would need Objective-C++):
    // NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    // [defaults setInteger:static_cast<int>(m_currentMode) forKey:@"ScreenMode"];
    // [defaults synchronize];
}

bool ScreenModeManager::loadFromPreferences() {
    // TODO: Implement using NSUserDefaults or similar
    // For now, return false (use default mode)
    
    // Example implementation:
    // NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    // int modeInt = [defaults integerForKey:@"ScreenMode"];
    // if (modeInt >= 0 && modeInt < s_predefinedModeCount) {
    //     return setMode(static_cast<ScreenMode>(modeInt));
    // }
    
    return false;
}

// =============================================================================
// Utility
// =============================================================================

const char* ScreenModeManager::modeToString(ScreenMode mode) {
    switch (mode) {
        case ScreenMode::MODE_20x12: return "20x12";
        case ScreenMode::MODE_40x25: return "40x25";
        case ScreenMode::MODE_80x25: return "80x25";
        case ScreenMode::MODE_80x50: return "80x50";
        case ScreenMode::MODE_90x60: return "90x60";
        case ScreenMode::CUSTOM:     return "custom";
        default:                     return "unknown";
    }
}

ScreenMode ScreenModeManager::stringToMode(const char* str) {
    if (!str) return ScreenMode::MODE_80x25;
    
    std::string s = str;
    
    if (s == "20x12") return ScreenMode::MODE_20x12;
    if (s == "40x25") return ScreenMode::MODE_40x25;
    if (s == "80x25") return ScreenMode::MODE_80x25;
    if (s == "80x50") return ScreenMode::MODE_80x50;
    if (s == "90x60") return ScreenMode::MODE_90x60;
    if (s == "custom") return ScreenMode::CUSTOM;
    
    return ScreenMode::MODE_80x25; // Default
}

bool ScreenModeManager::isValidMode(int columns, int rows) {
    // Reasonable limits
    return columns >= 20 && columns <= 200 &&
           rows >= 10 && rows <= 100;
}

void ScreenModeManager::getRecommendedCellSize(int columns, int rows,
                                              int& outCellWidth, int& outCellHeight)
{
    // Heuristic for cell size based on grid dimensions
    if (columns <= 40) {
        outCellWidth = 16;
        outCellHeight = 16;
    } else if (columns <= 80 && rows <= 25) {
        outCellWidth = 8;
        outCellHeight = 16;
    } else if (columns <= 80 && rows <= 50) {
        outCellWidth = 8;
        outCellHeight = 8;
    } else {
        // Large grids
        outCellWidth = 10;
        outCellHeight = 12;
    }
}

// =============================================================================
// Internal Helpers
// =============================================================================

bool ScreenModeManager::validateMode(const ScreenModeInfo& info) const {
    return isValidMode(info.columns, info.rows) &&
           info.cellWidth > 0 && info.cellWidth <= 32 &&
           info.cellHeight > 0 && info.cellHeight <= 32;
}

void ScreenModeManager::notifyModeChanged() {
    // TODO: Implement event system for mode change notifications
    // For now, this is a no-op
    
    // Future: Notify listeners (DisplayManager, Editor, etc.)
}

} // namespace SuperTerminal