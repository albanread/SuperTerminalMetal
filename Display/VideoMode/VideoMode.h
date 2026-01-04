//
// VideoMode.h
// SuperTerminal Framework
//
// Video mode enumeration and utilities
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef SUPERTERMINAL_VIDEO_MODE_H
#define SUPERTERMINAL_VIDEO_MODE_H

#include <cstdint>
#include <string>

namespace SuperTerminal {

/// Video display modes
/// Each mode has specific resolution and color format characteristics
enum class VideoMode : uint8_t {
    NONE = 0,      ///< No video mode active (text-only rendering)
    LORES = 1,     ///< Low resolution chunky mode (80×60 to 640×300, 16-color palette with per-pixel alpha)
    XRES = 2,      ///< Extended resolution (320×240, 256-color hybrid palette)
    WRES = 3,      ///< Wide resolution (432×240, 256-color hybrid palette)
    URES = 4,      ///< Ultra resolution (1280×720, 16-bit ARGB4444 direct color)
    PRES = 5       ///< Premium resolution (1280×720, 256-color hybrid palette)
};

/// Get string name for video mode
/// @param mode Video mode
/// @return String representation (e.g., "XRES", "URES")
inline const char* videoModeToString(VideoMode mode) {
    switch (mode) {
        case VideoMode::NONE:  return "NONE";
        case VideoMode::LORES: return "LORES";
        case VideoMode::XRES:  return "XRES";
        case VideoMode::WRES:  return "WRES";
        case VideoMode::URES:  return "URES";
        case VideoMode::PRES:  return "PRES";
        default:               return "UNKNOWN";
    }
}

/// Get resolution for video mode
/// @param mode Video mode
/// @param width Output width in pixels
/// @param height Output height in pixels
/// @note LORES returns default 160×75, actual resolution is dynamic
inline void getVideoModeResolution(VideoMode mode, int& width, int& height) {
    switch (mode) {
        case VideoMode::NONE:
            width = 0;
            height = 0;
            break;
        case VideoMode::LORES:
            // LORES is dynamic, return common default
            width = 160;
            height = 75;
            break;
        case VideoMode::XRES:
            width = 320;
            height = 240;
            break;
        case VideoMode::WRES:
            width = 432;
            height = 240;
            break;
        case VideoMode::URES:
            width = 1280;
            height = 720;
            break;
        case VideoMode::PRES:
            width = 1280;
            height = 720;
            break;
        default:
            width = 0;
            height = 0;
            break;
    }
}

/// Get bits per pixel for video mode
/// @param mode Video mode
/// @return Bits per pixel
inline int getVideoModeBitsPerPixel(VideoMode mode) {
    switch (mode) {
        case VideoMode::LORES: return 8;   // 4-bit color + 4-bit alpha packed
        case VideoMode::XRES:  return 8;   // 8-bit palette index
        case VideoMode::WRES:  return 8;   // 8-bit palette index
        case VideoMode::URES:  return 16;  // 16-bit ARGB4444
        case VideoMode::PRES:  return 8;   // 8-bit palette index
        default:               return 0;
    }
}

/// Get color depth (number of colors) for video mode
/// @param mode Video mode
/// @return Number of colors available
inline int getVideoModeColorDepth(VideoMode mode) {
    switch (mode) {
        case VideoMode::LORES: return 16;      // 16 colors (4-bit palette)
        case VideoMode::XRES:  return 256;     // 256 colors (8-bit palette)
        case VideoMode::WRES:  return 256;     // 256 colors (8-bit palette)
        case VideoMode::URES:  return 4096;    // 4096 colors (4-bit per channel RGB)
        case VideoMode::PRES:  return 256;     // 256 colors (8-bit palette)
        default:               return 0;
    }
}

/// Check if video mode uses palette
/// @param mode Video mode
/// @return true if mode uses indexed color palette
inline bool videoModeUsesPalette(VideoMode mode) {
    switch (mode) {
        case VideoMode::LORES: return true;
        case VideoMode::XRES:  return true;
        case VideoMode::WRES:  return true;
        case VideoMode::URES:  return false;  // Direct color
        case VideoMode::PRES:  return true;
        default:               return false;
    }
}

/// Check if video mode supports alpha channel
/// @param mode Video mode
/// @return true if mode has alpha channel support
inline bool videoModeSupportsAlpha(VideoMode mode) {
    switch (mode) {
        case VideoMode::LORES: return true;   // Per-pixel 4-bit alpha
        case VideoMode::XRES:  return false;  // Palette-based (color 0 = transparent)
        case VideoMode::WRES:  return false;  // Palette-based (color 0 = transparent)
        case VideoMode::URES:  return true;   // 4-bit alpha channel
        case VideoMode::PRES:  return false;  // Palette-based (color 0 = transparent)
        default:               return false;
    }
}

} // namespace SuperTerminal

#endif // SUPERTERMINAL_VIDEO_MODE_H