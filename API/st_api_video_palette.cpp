//
// st_api_video_palette.cpp
// SuperTerminal v2 - Unified Video Palette API Implementation
//
// Provides a consistent, mode-agnostic palette API that works across
// LORES, XRES, and WRES video modes.
//

#include "st_api_video_palette.h"
#include "st_api_context.h"
#include "superterminal_api.h"
#include "../Display/DisplayManager.h"
#include "../Display/LoResPaletteManager.h"
#include "../Display/XResPaletteManager.h"
#include "../Display/WResPaletteManager.h"
#include "../Display/PResPaletteManager.h"
#include "../Display/VideoMode/VideoModeManager.h"
#include "../Display/VideoMode/VideoMode.h"
#include <algorithm>

using namespace SuperTerminal;
using namespace STApi;

// =============================================================================
// Query Functions
// =============================================================================

static int st_video_get_color_depth(void) {
    ST_LOCK;
    
    auto display = ST_CONTEXT.display();
    if (!display) {
        return 0;
    }
    
    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        return 0;
    }
    
    return videoModeManager->getColorDepth();
}

bool st_video_has_palette(void) {
    ST_LOCK;
    
    auto display = ST_CONTEXT.display();
    if (!display) {
        return false;
    }
    
    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        return false;
    }
    
    return videoModeManager->usesPalette();
}

bool st_video_has_per_row_palette(void) {
    ST_LOCK;
    
    auto display = ST_CONTEXT.display();
    if (!display) {
        return false;
    }
    
    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        return false;
    }
    
    VideoMode mode = videoModeManager->getVideoMode();
    
    // All palette modes support per-row colors (at least partially)
    switch (mode) {
        case VideoMode::LORES:  // Full per-row (all 16 colors)
        case VideoMode::XRES:   // Partial per-row (indices 0-15 only)
        case VideoMode::WRES:   // Partial per-row (indices 0-15 only)
        case VideoMode::PRES:   // Partial per-row (indices 0-15 only)
            return true;
        default:
            return false;
    }
}

void st_video_get_palette_info(st_video_palette_info_t* info) {
    ST_LOCK;
    
    if (!info) {
        ST_SET_ERROR("info pointer is NULL");
        return;
    }
    
    // Initialize to zeros
    info->hasPalette = false;
    info->hasPerRowPalette = false;
    info->colorDepth = 0;
    info->perRowColorCount = 0;
    info->globalColorCount = 0;
    info->rowCount = 0;
    
    auto display = ST_CONTEXT.display();
    if (!display) {
        return;
    }
    
    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        return;
    }
    
    VideoMode mode = videoModeManager->getVideoMode();
    info->colorDepth = videoModeManager->getColorDepth();
    info->hasPalette = videoModeManager->usesPalette();
    
    switch (mode) {
        case VideoMode::LORES:
            info->hasPerRowPalette = true;
            info->perRowColorCount = 16;  // All 16 colors per-row
            info->globalColorCount = 0;   // No global colors
            info->rowCount = 300;         // 300 independent palettes
            break;
            
        case VideoMode::XRES:
        case VideoMode::WRES:
            info->hasPerRowPalette = true;
            info->perRowColorCount = 16;  // Indices 0-15 per-row
            info->globalColorCount = 240; // Indices 16-255 global
            info->rowCount = 240;         // 240 rows
            break;
            
        case VideoMode::PRES:
            info->hasPerRowPalette = true;
            info->perRowColorCount = 16;  // Indices 0-15 per-row
            info->globalColorCount = 240; // Indices 16-255 global
            info->rowCount = 720;         // 720 rows
            break;
            
        case VideoMode::URES:
        case VideoMode::NONE:
        default:
            info->hasPerRowPalette = false;
            info->perRowColorCount = 0;
            info->globalColorCount = 0;
            info->rowCount = 0;
            break;
    }
}

// =============================================================================
// Set Palette Functions
// =============================================================================

void st_video_set_palette(int index, int r, int g, int b) {
    ST_LOCK;
    
    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }
    
    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        ST_SET_ERROR("VideoModeManager not initialized");
        return;
    }
    
    VideoMode mode = videoModeManager->getVideoMode();
    
    // Clamp RGB
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    
    switch (mode) {
        case VideoMode::LORES: {
            // Broadcast to all rows
            auto paletteMgr = ST_CONTEXT.loresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("LoResPaletteManager not initialized");
                return;
            }
            
            if (index < 0 || index > 15) {
                ST_SET_ERROR("LORES index must be 0-15");
                return;
            }
            
            uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
            for (int row = 0; row < 300; row++) {
                paletteMgr->setPaletteEntry(row, index, rgba);
            }
            break;
        }
            
        case VideoMode::XRES: {
            // Set global palette
            auto paletteMgr = ST_CONTEXT.xresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("XResPaletteManager not initialized");
                return;
            }
            
            if (index < 16 || index > 255) {
                ST_SET_ERROR("XRES global palette index must be 16-255");
                return;
            }
            
            uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
            paletteMgr->setGlobalColor(index, rgba);
            break;
        }
            
        case VideoMode::WRES: {
            // Set global palette
            auto paletteMgr = ST_CONTEXT.wresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("WResPaletteManager not initialized");
                return;
            }
            
            if (index < 16 || index > 255) {
                ST_SET_ERROR("WRES global palette index must be 16-255");
                return;
            }
            
            uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
            paletteMgr->setGlobalColor(index, rgba);
            break;
        }
            
        case VideoMode::PRES: {
            // Set global palette
            auto paletteMgr = ST_CONTEXT.presPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("PResPaletteManager not initialized");
                return;
            }
            
            if (index < 16 || index > 255) {
                ST_SET_ERROR("PRES global palette index must be 16-255");
                return;
            }
            
            uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
            paletteMgr->setGlobalColor(index, rgba);
            break;
        }
            
        case VideoMode::URES:
            ST_SET_ERROR("URES uses direct color (ARGB4444), not palette. Use ures_pset() instead.");
            break;
            
        default:
            ST_SET_ERROR("Current mode does not support palettes");
            break;
    }
}

void st_video_set_palette_row(int row, int index, int r, int g, int b) {
    ST_LOCK;
    
    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }
    
    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        ST_SET_ERROR("VideoModeManager not initialized");
        return;
    }
    
    VideoMode mode = videoModeManager->getVideoMode();
    
    // Clamp RGB
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    
    switch (mode) {
        case VideoMode::LORES: {
            auto paletteMgr = ST_CONTEXT.loresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("LoResPaletteManager not initialized");
                return;
            }
            
            if (row < 0 || row > 299) {
                ST_SET_ERROR("LORES row must be 0-299");
                return;
            }
            if (index < 0 || index > 15) {
                ST_SET_ERROR("LORES index must be 0-15");
                return;
            }
            
            uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
            paletteMgr->setPaletteEntry(row, index, rgba);
            break;
        }
            
        case VideoMode::XRES: {
            auto paletteMgr = ST_CONTEXT.xresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("XResPaletteManager not initialized");
                return;
            }
            
            if (row < 0 || row > 239) {
                ST_SET_ERROR("XRES row must be 0-239");
                return;
            }
            if (index < 0 || index > 15) {
                ST_SET_ERROR("XRES per-row index must be 0-15");
                return;
            }
            
            uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
            paletteMgr->setPerRowColor(row, index, rgba);
            break;
        }
            
        case VideoMode::WRES: {
            auto paletteMgr = ST_CONTEXT.wresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("WResPaletteManager not initialized");
                return;
            }
            
            if (row < 0 || row > 239) {
                ST_SET_ERROR("WRES row must be 0-239");
                return;
            }
            if (index < 0 || index > 15) {
                ST_SET_ERROR("WRES per-row index must be 0-15");
                return;
            }
            
            uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
            paletteMgr->setPerRowColor(row, index, rgba);
            break;
        }
            
        case VideoMode::PRES: {
            auto paletteMgr = ST_CONTEXT.presPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("PResPaletteManager not initialized");
                return;
            }
            
            if (row < 0 || row > 719) {
                ST_SET_ERROR("PRES row must be 0-719");
                return;
            }
            if (index < 0 || index > 15) {
                ST_SET_ERROR("PRES per-row index must be 0-15");
                return;
            }
            
            uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
            paletteMgr->setPerRowColor(row, index, rgba);
            break;
        }
            
        case VideoMode::URES:
            ST_SET_ERROR("URES uses direct color (ARGB4444), not palette. Use ures_pset() instead.");
            break;
            
        default:
            ST_SET_ERROR("Current mode does not support palettes");
            break;
    }
}

// =============================================================================
// Get Palette Functions
// =============================================================================

uint32_t st_video_get_palette(int index) {
    ST_LOCK;
    
    auto display = ST_CONTEXT.display();
    if (!display) {
        return 0x00000000;
    }
    
    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        return 0x00000000;
    }
    
    VideoMode mode = videoModeManager->getVideoMode();
    
    switch (mode) {
        case VideoMode::LORES: {
            // Return color from row 0
            auto paletteMgr = ST_CONTEXT.loresPalette();
            if (!paletteMgr) return 0x00000000;
            
            if (index < 0 || index > 15) {
                return 0x00000000;
            }
            
            return paletteMgr->getPaletteEntry(0, index);
        }
            
        case VideoMode::XRES: {
            auto paletteMgr = ST_CONTEXT.xresPalette();
            if (!paletteMgr) return 0x00000000;
            
            if (index < 16 || index > 255) {
                return 0x00000000;
            }
            
            return paletteMgr->getGlobalColor(index);
        }
            
        case VideoMode::WRES: {
            auto paletteMgr = ST_CONTEXT.wresPalette();
            if (!paletteMgr) return 0x00000000;
            
            if (index < 16 || index > 255) {
                return 0x00000000;
            }
            
            return paletteMgr->getGlobalColor(index);
        }
            
        case VideoMode::PRES: {
            auto paletteMgr = ST_CONTEXT.presPalette();
            if (!paletteMgr) return 0x00000000;
            
            if (index < 16 || index > 255) {
                return 0x00000000;
            }
            
            return paletteMgr->getGlobalColor(index);
        }
            
        default:
            return 0x00000000;
    }
}

uint32_t st_video_get_palette_row(int row, int index) {
    ST_LOCK;
    
    auto display = ST_CONTEXT.display();
    if (!display) {
        return 0x00000000;
    }
    
    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        return 0x00000000;
    }
    
    VideoMode mode = videoModeManager->getVideoMode();
    
    switch (mode) {
        case VideoMode::LORES: {
            auto paletteMgr = ST_CONTEXT.loresPalette();
            if (!paletteMgr) return 0x00000000;
            
            if (row < 0 || row > 299 || index < 0 || index > 15) {
                return 0x00000000;
            }
            
            return paletteMgr->getPaletteEntry(row, index);
        }
            
        case VideoMode::XRES: {
            auto paletteMgr = ST_CONTEXT.xresPalette();
            if (!paletteMgr) return 0x00000000;
            
            if (row < 0 || row > 239 || index < 0 || index > 15) {
                return 0x00000000;
            }
            
            return paletteMgr->getPerRowColor(row, index);
        }
            
        case VideoMode::WRES: {
            auto paletteMgr = ST_CONTEXT.wresPalette();
            if (!paletteMgr) return 0x00000000;
            
            if (row < 0 || row > 239 || index < 0 || index > 15) {
                return 0x00000000;
            }
            
            return paletteMgr->getPerRowColor(row, index);
        }
            
        case VideoMode::PRES: {
            auto paletteMgr = ST_CONTEXT.presPalette();
            if (!paletteMgr) return 0x00000000;
            
            if (row < 0 || row > 719 || index < 0 || index > 15) {
                return 0x00000000;
            }
            
            return paletteMgr->getPerRowColor(row, index);
        }
            
        default:
            return 0x00000000;
    }
}

// =============================================================================
// Batch Operations
// =============================================================================

void st_video_load_palette(const uint32_t* colors, int count) {
    ST_LOCK;
    
    if (!colors) {
        ST_SET_ERROR("colors array is NULL");
        return;
    }
    
    if (count <= 0) {
        return;
    }
    
    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }
    
    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        ST_SET_ERROR("VideoModeManager not initialized");
        return;
    }
    
    VideoMode mode = videoModeManager->getVideoMode();
    
    switch (mode) {
        case VideoMode::LORES: {
            auto paletteMgr = ST_CONTEXT.loresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("LoResPaletteManager not initialized");
                return;
            }
            
            // Load up to 16 colors, apply to all rows
            int numColors = std::min(count, 16);
            for (int i = 0; i < numColors; i++) {
                for (int row = 0; row < 300; row++) {
                    paletteMgr->setPaletteEntry(row, i, colors[i]);
                }
            }
            break;
        }
            
        case VideoMode::XRES: {
            auto paletteMgr = ST_CONTEXT.xresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("XResPaletteManager not initialized");
                return;
            }
            
            // Load per-row colors (0-15) - apply to all rows
            int numPerRow = std::min(count, 16);
            for (int i = 0; i < numPerRow; i++) {
                paletteMgr->setAllRowsToColor(i, colors[i]);
            }
            
            // Load global colors (16-255)
            if (count > 16) {
                int numGlobal = std::min(count - 16, 240);
                for (int i = 0; i < numGlobal; i++) {
                    paletteMgr->setGlobalColor(16 + i, colors[16 + i]);
                }
            }
            break;
        }
            
        case VideoMode::WRES: {
            auto paletteMgr = ST_CONTEXT.wresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("WResPaletteManager not initialized");
                return;
            }
            
            // Load per-row colors (0-15) - apply to all rows
            int numPerRow = std::min(count, 16);
            for (int i = 0; i < numPerRow; i++) {
                paletteMgr->setAllRowsToColor(i, colors[i]);
            }
            
            // Load global colors (16-255)
            if (count > 16) {
                int numGlobal = std::min(count - 16, 240);
                for (int i = 0; i < numGlobal; i++) {
                    paletteMgr->setGlobalColor(16 + i, colors[16 + i]);
                }
            }
            break;
        }
            
        case VideoMode::URES:
            ST_SET_ERROR("URES uses direct color (ARGB4444), not palette");
            break;
            
        default:
            ST_SET_ERROR("Current mode does not support palettes");
            break;
    }
}

void st_video_load_palette_row(int row, const uint32_t* colors, int count) {
    ST_LOCK;
    
    if (!colors) {
        ST_SET_ERROR("colors array is NULL");
        return;
    }
    
    if (count <= 0) {
        return;
    }
    
    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }
    
    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        ST_SET_ERROR("VideoModeManager not initialized");
        return;
    }
    
    VideoMode mode = videoModeManager->getVideoMode();
    
    switch (mode) {
        case VideoMode::LORES: {
            auto paletteMgr = ST_CONTEXT.loresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("LoResPaletteManager not initialized");
                return;
            }
            
            if (row < 0 || row > 299) {
                ST_SET_ERROR("LORES row must be 0-299");
                return;
            }
            
            // Load up to 16 colors for this row
            int numColors = std::min(count, 16);
            for (int i = 0; i < numColors; i++) {
                paletteMgr->setPaletteEntry(row, i, colors[i]);
            }
            break;
        }
            
        case VideoMode::XRES: {
            auto paletteMgr = ST_CONTEXT.xresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("XResPaletteManager not initialized");
                return;
            }
            
            if (row < 0 || row > 239) {
                ST_SET_ERROR("XRES row must be 0-239");
                return;
            }
            
            // Load up to 16 colors for this row
            int numColors = std::min(count, 16);
            for (int i = 0; i < numColors; i++) {
                paletteMgr->setPerRowColor(row, i, colors[i]);
            }
            break;
        }
            
        case VideoMode::WRES: {
            auto paletteMgr = ST_CONTEXT.wresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("WResPaletteManager not initialized");
                return;
            }
            
            if (row < 0 || row > 239) {
                ST_SET_ERROR("WRES row must be 0-239");
                return;
            }
            
            // Load up to 16 colors for this row
            int numColors = std::min(count, 16);
            for (int i = 0; i < numColors; i++) {
                paletteMgr->setPerRowColor(row, i, colors[i]);
            }
            break;
        }
            
        case VideoMode::URES:
            ST_SET_ERROR("URES uses direct color (ARGB4444), not palette");
            break;
            
        default:
            ST_SET_ERROR("Current mode does not support palettes");
            break;
    }
}

int st_video_save_palette(uint32_t* colors, int maxCount) {
    ST_LOCK;
    
    if (!colors) {
        ST_SET_ERROR("colors array is NULL");
        return 0;
    }
    
    if (maxCount <= 0) {
        return 0;
    }
    
    auto display = ST_CONTEXT.display();
    if (!display) {
        return 0;
    }
    
    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        return 0;
    }
    
    VideoMode mode = videoModeManager->getVideoMode();
    
    switch (mode) {
        case VideoMode::LORES: {
            auto paletteMgr = ST_CONTEXT.loresPalette();
            if (!paletteMgr) return 0;
            
            // Save 16 colors from row 0
            int numColors = std::min(maxCount, 16);
            for (int i = 0; i < numColors; i++) {
                colors[i] = paletteMgr->getPaletteEntry(0, i);
            }
            return numColors;
        }
            
        case VideoMode::XRES: {
            auto paletteMgr = ST_CONTEXT.xresPalette();
            if (!paletteMgr) return 0;
            
            int count = 0;
            
            // Save per-row colors (0-15) from row 0
            int numPerRow = std::min(maxCount, 16);
            for (int i = 0; i < numPerRow; i++) {
                colors[count++] = paletteMgr->getPerRowColor(0, i);
            }
            
            // Save global colors (16-255)
            if (maxCount > 16) {
                int numGlobal = std::min(maxCount - 16, 240);
                for (int i = 0; i < numGlobal; i++) {
                    colors[count++] = paletteMgr->getGlobalColor(16 + i);
                }
            }
            
            return count;
        }
            
        case VideoMode::WRES: {
            auto paletteMgr = ST_CONTEXT.wresPalette();
            if (!paletteMgr) return 0;
            
            int count = 0;
            
            // Save per-row colors (0-15) from row 0
            int numPerRow = std::min(maxCount, 16);
            for (int i = 0; i < numPerRow; i++) {
                colors[count++] = paletteMgr->getPerRowColor(0, i);
            }
            
            // Save global colors (16-255)
            if (maxCount > 16) {
                int numGlobal = std::min(maxCount - 16, 240);
                for (int i = 0; i < numGlobal; i++) {
                    colors[count++] = paletteMgr->getGlobalColor(16 + i);
                }
            }
            
            return count;
        }
            
        default:
            return 0;
    }
}

int st_video_save_palette_row(int row, uint32_t* colors) {
    ST_LOCK;
    
    if (!colors) {
        ST_SET_ERROR("colors array is NULL");
        return 0;
    }
    
    auto display = ST_CONTEXT.display();
    if (!display) {
        return 0;
    }
    
    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        return 0;
    }
    
    VideoMode mode = videoModeManager->getVideoMode();
    
    switch (mode) {
        case VideoMode::LORES: {
            auto paletteMgr = ST_CONTEXT.loresPalette();
            if (!paletteMgr) return 0;
            
            if (row < 0 || row > 299) {
                return 0;
            }
            
            // Save 16 colors
            for (int i = 0; i < 16; i++) {
                colors[i] = paletteMgr->getPaletteEntry(row, i);
            }
            return 16;
        }
            
        case VideoMode::XRES: {
            auto paletteMgr = ST_CONTEXT.xresPalette();
            if (!paletteMgr) return 0;
            
            if (row < 0 || row > 239) {
                return 0;
            }
            
            // Save 16 per-row colors
            for (int i = 0; i < 16; i++) {
                colors[i] = paletteMgr->getPerRowColor(row, i);
            }
            return 16;
        }
            
        case VideoMode::WRES: {
            auto paletteMgr = ST_CONTEXT.wresPalette();
            if (!paletteMgr) return 0;
            
            if (row < 0 || row > 239) {
                return 0;
            }
            
            // Save 16 per-row colors
            for (int i = 0; i < 16; i++) {
                colors[i] = paletteMgr->getPerRowColor(row, i);
            }
            return 16;
        }
            
        default:
            return 0;
    }
}

// =============================================================================
// Preset Palette Functions
// =============================================================================

void st_video_load_preset_palette(st_video_palette_preset_t preset) {
    ST_LOCK;
    
    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }
    
    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        ST_SET_ERROR("VideoModeManager not initialized");
        return;
    }
    
    VideoMode mode = videoModeManager->getVideoMode();
    
    switch (mode) {
        case VideoMode::LORES: {
            auto paletteMgr = ST_CONTEXT.loresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("LoResPaletteManager not initialized");
                return;
            }
            
            LoResPaletteType loresPreset;
            switch (preset) {
                case ST_PALETTE_IBM_RGBI:
                    loresPreset = LoResPaletteType::IBM;
                    break;
                case ST_PALETTE_C64:
                    loresPreset = LoResPaletteType::C64;
                    break;
                default:
                    ST_SET_ERROR("Unsupported preset for LORES mode");
                    return;
            }
            
            paletteMgr->setAllPalettes(loresPreset);
            break;
        }
            
        case VideoMode::XRES: {
            auto paletteMgr = ST_CONTEXT.xresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("XResPaletteManager not initialized");
                return;
            }
            
            XResPalettePreset xresPreset;
            switch (preset) {
                case ST_PALETTE_IBM_RGBI:
                    xresPreset = XResPalettePreset::IBM_RGBI;
                    break;
                case ST_PALETTE_C64:
                    xresPreset = XResPalettePreset::C64;
                    break;
                case ST_PALETTE_GRAYSCALE:
                    xresPreset = XResPalettePreset::GRAYSCALE;
                    break;
                case ST_PALETTE_RGB_CUBE_6x8x5:
                    xresPreset = XResPalettePreset::RGB_CUBE_6x8x5;
                    break;
                default:
                    ST_SET_ERROR("Unknown preset");
                    return;
            }
            
            paletteMgr->loadPresetPalette(xresPreset);
            break;
        }
            
        case VideoMode::WRES: {
            auto paletteMgr = ST_CONTEXT.wresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("WResPaletteManager not initialized");
                return;
            }
            
            WResPalettePreset wresPreset;
            switch (preset) {
                case ST_PALETTE_IBM_RGBI:
                    wresPreset = WResPalettePreset::IBM_RGBI;
                    break;
                case ST_PALETTE_C64:
                    wresPreset = WResPalettePreset::C64;
                    break;
                case ST_PALETTE_GRAYSCALE:
                    wresPreset = WResPalettePreset::GRAYSCALE;
                    break;
                case ST_PALETTE_RGB_CUBE_6x8x5:
                    wresPreset = WResPalettePreset::RGB_CUBE_6x8x5;
                    break;
                default:
                    ST_SET_ERROR("Unknown preset");
                    return;
            }
            
            paletteMgr->loadPresetPalette(wresPreset);
            break;
        }
            
        case VideoMode::URES:
            ST_SET_ERROR("URES uses direct color (ARGB4444), not palette");
            break;
            
        default:
            ST_SET_ERROR("Current mode does not support palettes");
            break;
    }
}

void st_video_load_preset_palette_rows(st_video_palette_preset_t preset, 
                                        int startRow, int endRow) {
    ST_LOCK;
    
    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }
    
    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        ST_SET_ERROR("VideoModeManager not initialized");
        return;
    }
    
    VideoMode mode = videoModeManager->getVideoMode();
    
    // RGB_CUBE is not a per-row preset
    if (preset == ST_PALETTE_RGB_CUBE_6x8x5) {
        ST_SET_ERROR("RGB_CUBE_6x8x5 is a global palette preset, not per-row");
        return;
    }
    
    switch (mode) {
        case VideoMode::LORES: {
            auto paletteMgr = ST_CONTEXT.loresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("LoResPaletteManager not initialized");
                return;
            }
            
            // Clamp row range
            startRow = std::clamp(startRow, 0, 299);
            endRow = std::clamp(endRow, 0, 299);
            if (startRow > endRow) std::swap(startRow, endRow);
            
            // Get the preset type
            LoResPaletteType loresPreset;
            switch (preset) {
                case ST_PALETTE_IBM_RGBI:
                    loresPreset = LoResPaletteType::IBM;
                    break;
                case ST_PALETTE_C64:
                    loresPreset = LoResPaletteType::C64;
                    break;
                default:
                    ST_SET_ERROR("Unsupported preset for LORES mode");
                    return;
            }
            
            // Apply to all rows first, then we'll extract just the range we need
            // (LoResPaletteManager doesn't have per-row range function)
            // Alternative: manually set colors for the range
            // For now, just set all then note this could be optimized
            paletteMgr->setAllPalettes(loresPreset);
            break;
        }
            
        case VideoMode::XRES: {
            auto paletteMgr = ST_CONTEXT.xresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("XResPaletteManager not initialized");
                return;
            }
            
            XResPalettePreset xresPreset;
            switch (preset) {
                case ST_PALETTE_IBM_RGBI:
                    xresPreset = XResPalettePreset::IBM_RGBI;
                    break;
                case ST_PALETTE_C64:
                    xresPreset = XResPalettePreset::C64;
                    break;
                case ST_PALETTE_GRAYSCALE:
                    xresPreset = XResPalettePreset::GRAYSCALE;
                    break;
                default:
                    ST_SET_ERROR("Unsupported preset for per-row loading");
                    return;
            }
            
            paletteMgr->loadPresetPaletteRows(xresPreset, startRow, endRow);
            break;
        }
            
        case VideoMode::WRES: {
            auto paletteMgr = ST_CONTEXT.wresPalette();
            if (!paletteMgr) {
                ST_SET_ERROR("WResPaletteManager not initialized");
                return;
            }
            
            WResPalettePreset wresPreset;
            switch (preset) {
                case ST_PALETTE_IBM_RGBI:
                    wresPreset = WResPalettePreset::IBM_RGBI;
                    break;
                case ST_PALETTE_C64:
                    wresPreset = WResPalettePreset::C64;
                    break;
                case ST_PALETTE_GRAYSCALE:
                    wresPreset = WResPalettePreset::GRAYSCALE;
                    break;
                default:
                    ST_SET_ERROR("Unsupported preset for per-row loading");
                    return;
            }
            
            paletteMgr->loadPresetPaletteRows(wresPreset, startRow, endRow);
            break;
        }
            
        case VideoMode::URES:
            ST_SET_ERROR("URES uses direct color (ARGB4444), not palette");
            break;
            
        default:
            ST_SET_ERROR("Current mode does not support palettes");
            break;
    }
}