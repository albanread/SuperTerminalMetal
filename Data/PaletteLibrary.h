//
// PaletteLibrary.h
// SuperTerminal Framework - Standard Palette Library
//
// Loader for 32 predefined standard palettes
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef PALETTELIBRARY_H
#define PALETTELIBRARY_H

#include <string>
#include <cstdint>
#include "../Tilemap/PaletteBank.h"

namespace SuperTerminal {

/// Standard Palette Library Constants
constexpr int32_t STANDARD_PALETTE_COUNT = 32;
constexpr int32_t STANDARD_PALETTE_COLORS = 16;

/// Standard Palette IDs - Retro Platforms (0-7)
constexpr uint8_t STANDARD_PALETTE_C64             = 0;
constexpr uint8_t STANDARD_PALETTE_CGA             = 1;
constexpr uint8_t STANDARD_PALETTE_CGA_ALT         = 2;
constexpr uint8_t STANDARD_PALETTE_ZX_SPECTRUM     = 3;
constexpr uint8_t STANDARD_PALETTE_NES             = 4;
constexpr uint8_t STANDARD_PALETTE_GAMEBOY         = 5;
constexpr uint8_t STANDARD_PALETTE_GAMEBOY_COLOR   = 6;
constexpr uint8_t STANDARD_PALETTE_APPLE_II        = 7;

/// Standard Palette IDs - Natural Biomes (8-15)
constexpr uint8_t STANDARD_PALETTE_FOREST          = 8;
constexpr uint8_t STANDARD_PALETTE_DESERT          = 9;
constexpr uint8_t STANDARD_PALETTE_ICE             = 10;
constexpr uint8_t STANDARD_PALETTE_OCEAN           = 11;
constexpr uint8_t STANDARD_PALETTE_LAVA            = 12;
constexpr uint8_t STANDARD_PALETTE_SWAMP           = 13;
constexpr uint8_t STANDARD_PALETTE_CAVE            = 14;
constexpr uint8_t STANDARD_PALETTE_MOUNTAIN        = 15;

/// Standard Palette IDs - Themed (16-23)
constexpr uint8_t STANDARD_PALETTE_DUNGEON         = 16;
constexpr uint8_t STANDARD_PALETTE_NEON            = 17;
constexpr uint8_t STANDARD_PALETTE_PASTEL          = 18;
constexpr uint8_t STANDARD_PALETTE_EARTH           = 19;
constexpr uint8_t STANDARD_PALETTE_METAL           = 20;
constexpr uint8_t STANDARD_PALETTE_CRYSTAL         = 21;
constexpr uint8_t STANDARD_PALETTE_TOXIC           = 22;
constexpr uint8_t STANDARD_PALETTE_BLOOD           = 23;

/// Standard Palette IDs - Utility (24-31)
constexpr uint8_t STANDARD_PALETTE_GRAYSCALE       = 24;
constexpr uint8_t STANDARD_PALETTE_SEPIA           = 25;
constexpr uint8_t STANDARD_PALETTE_BLUE_TINT       = 26;
constexpr uint8_t STANDARD_PALETTE_GREEN_TINT      = 27;
constexpr uint8_t STANDARD_PALETTE_RED_TINT        = 28;
constexpr uint8_t STANDARD_PALETTE_HIGH_CONTRAST   = 29;
constexpr uint8_t STANDARD_PALETTE_COLORBLIND_SAFE = 30;
constexpr uint8_t STANDARD_PALETTE_NIGHT_MODE      = 31;

/// Special value for custom palette (SPRTZ v2)
constexpr uint8_t PALETTE_MODE_CUSTOM = 0xFF;

/// Standard Palette Metadata
struct StandardPaletteInfo {
    uint8_t id;
    const char* name;
    const char* description;
    const char* category;
};

/// StandardPaletteLibrary: Global library of predefined palettes
///
/// Loads 32 standard palettes from JSON or binary format.
/// Used by both SPRED and SuperTerminal framework for consistent palette references.
///
/// Usage:
///   StandardPaletteLibrary::initialize("standard_palettes.json");
///   const PaletteColor* forest = StandardPaletteLibrary::getPalette(STANDARD_PALETTE_FOREST);
///
class StandardPaletteLibrary {
public:
    // =================================================================
    // Initialization
    // =================================================================
    
    /// Initialize library from JSON file
    /// @param jsonPath Path to standard_palettes.json
    /// @return true on success
    static bool initializeFromJSON(const std::string& jsonPath);
    
    /// Initialize library from binary file
    /// @param palPath Path to standard_palettes.pal
    /// @return true on success
    static bool initializeFromBinary(const std::string& palPath);
    
    /// Auto-detect format and load
    /// @param path Path to palette file (.json or .pal)
    /// @return true on success
    static bool initialize(const std::string& path);
    
    /// Check if library is initialized
    /// @return true if palettes are loaded
    static bool isInitialized();
    
    /// Shutdown and free memory
    static void shutdown();
    
    // =================================================================
    // Palette Access
    // =================================================================
    
    /// Get palette by ID
    /// @param paletteID Palette ID (0-31)
    /// @return Pointer to 16 colors, or nullptr if invalid
    static const PaletteColor* getPalette(uint8_t paletteID);
    
    /// Get palette name
    /// @param paletteID Palette ID (0-31)
    /// @return Palette name, or nullptr if invalid
    static const char* getPaletteName(uint8_t paletteID);
    
    /// Get palette description
    /// @param paletteID Palette ID (0-31)
    /// @return Palette description, or nullptr if invalid
    static const char* getPaletteDescription(uint8_t paletteID);
    
    /// Get palette category
    /// @param paletteID Palette ID (0-31)
    /// @return Category ("retro", "biome", "themed", "utility"), or nullptr
    static const char* getPaletteCategory(uint8_t paletteID);
    
    /// Get all palette info
    /// @param paletteID Palette ID (0-31)
    /// @return Pointer to palette info, or nullptr if invalid
    static const StandardPaletteInfo* getPaletteInfo(uint8_t paletteID);
    
    // =================================================================
    // Validation
    // =================================================================
    
    /// Check if palette ID is valid standard palette
    /// @param paletteID ID to check
    /// @return true if 0-31, false otherwise
    static bool isValidPaletteID(uint8_t paletteID);
    
    /// Check if palette mode is standard (not custom)
    /// @param paletteMode Palette mode byte from SPRTZ v2
    /// @return true if standard (0-31), false if custom (0xFF)
    static bool isStandardPaletteMode(uint8_t paletteMode);
    
    // =================================================================
    // Palette Operations
    // =================================================================
    
    /// Copy palette to buffer
    /// @param paletteID Palette ID (0-31)
    /// @param outColors Output buffer (must hold 16 colors)
    /// @return true on success
    static bool copyPalette(uint8_t paletteID, PaletteColor* outColors);
    
    /// Copy palette to RGBA buffer (for SPRED compatibility)
    /// @param paletteID Palette ID (0-31)
    /// @param outRGBA Output buffer (must hold 64 bytes: 16 colors × RGBA)
    /// @return true on success
    static bool copyPaletteRGBA(uint8_t paletteID, uint8_t* outRGBA);
    
    /// Find closest matching standard palette
    /// @param customPalette Custom palette (16 colors)
    /// @param outDistance Optional output for color distance
    /// @return Best matching palette ID, or 0xFF if no good match
    static uint8_t findClosestPalette(const PaletteColor* customPalette, 
                                     int32_t* outDistance = nullptr);
    
    // =================================================================
    // Enumeration
    // =================================================================
    
    /// Get total palette count
    /// @return 32
    static constexpr int32_t getPaletteCount() { return STANDARD_PALETTE_COUNT; }
    
    /// Enumerate all palettes
    /// @param callback Function called for each palette (id, info)
    static void enumeratePalettes(void (*callback)(uint8_t id, const StandardPaletteInfo* info));
    
    /// Get palettes by category
    /// @param category Category name ("retro", "biome", "themed", "utility")
    /// @param outIDs Output array (must hold at least 32 IDs)
    /// @return Number of palettes in category
    static int32_t getPalettesByCategory(const char* category, uint8_t* outIDs);
    
    // =================================================================
    // Error Handling
    // =================================================================
    
    /// Get last error message
    /// @return Error string, or empty if no error
    static const std::string& getLastError();
    
    /// Clear last error
    static void clearError();
    
private:
    // =================================================================
    // Internal Storage
    // =================================================================
    
    struct LibraryData;
    static LibraryData* s_data;
    
    // JSON parsing helpers
    static bool parseJSON(const std::string& jsonContent);
    
    // Binary format helpers
    static bool parseBinary(const uint8_t* binaryData, size_t size);
    
    // Error reporting
    static void setError(const std::string& error);
    
    // Color distance calculation
    static int32_t colorDistance(const PaletteColor& c1, const PaletteColor& c2);
};

} // namespace SuperTerminal

#endif // PALETTELIBRARY_H