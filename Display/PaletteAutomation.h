//
// PaletteAutomation.h
// SuperTerminal Framework
//
// Shared palette automation structures for Copper-style scanline effects
// Used by XResPaletteManager, WResPaletteManager, and PResPaletteManager
//
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef SUPERTERMINAL_PALETTE_AUTOMATION_H
#define SUPERTERMINAL_PALETTE_AUTOMATION_H

#include <cstdint>

namespace SuperTerminal {

/// Palette automation types (Copper-style effects)
enum class PaletteAutomationType {
    NONE = 0,         // No automation active
    GRADIENT,         // Linear gradient between two colors
    BARS,             // Moving color bars/blocks
    CYCLE             // Palette cycling
};

/// Palette automation state for gradient effect
/// Applies a smooth color gradient across a range of scanlines
struct PaletteGradientAutomation {
    bool enabled = false;
    int paletteIndex = 0;        // Which palette index to modify (0-15)
    int startRow = 0;            // First scanline
    int endRow = 239;            // Last scanline (inclusive)
    uint8_t startR = 0, startG = 0, startB = 0;  // Start color RGB
    uint8_t endR = 255, endG = 255, endB = 255;  // End color RGB
    float speed = 0.0f;          // Animation speed (0 = static)
    float phase = 0.0f;          // Current animation phase (for oscillation)
};

/// Palette automation state for bars effect
/// Creates moving or static color bars (like Amiga copper bars)
struct PaletteBarsAutomation {
    bool enabled = false;
    int paletteIndex = 0;        // Which palette index to modify (0-15)
    int startRow = 0;            // First scanline
    int endRow = 239;            // Last scanline (inclusive)
    int barHeight = 8;           // Height of each bar in scanlines
    uint8_t colors[4][3];        // Up to 4 bar colors (RGB)
    int numColors = 2;           // Number of colors to cycle through (1-4)
    float speed = 0.0f;          // Animation speed (0 = static)
    float phase = 0.0f;          // Current animation phase (for scrolling)
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_PALETTE_AUTOMATION_H