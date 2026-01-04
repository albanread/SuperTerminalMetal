//
//  SpriteCompression.h
//  SPRED - Sprite Editor
//
//  SPRTZ compressed sprite format (v1 and v2)
//

#ifndef SPRED_SPRITE_COMPRESSION_H
#define SPRED_SPRITE_COMPRESSION_H

#include <cstdint>
#include <string>
#include <vector>

namespace SPRED {

/// SPRTZ Format Specification
/// ===========================
///
/// Version 1 (Original):
/// ---------------------
/// Header (16 bytes):
/// Offset | Size | Type    | Description
/// -------|------|---------|----------------------------------
/// 0x00   | 4    | char[4] | Magic: "SPTZ"
/// 0x04   | 2    | uint16  | Version (1)
/// 0x06   | 1    | uint8   | Width (8, 16, or 40)
/// 0x07   | 1    | uint8   | Height (8, 16, or 40)
/// 0x08   | 4    | uint32  | Uncompressed pixel data size
/// 0x0C   | 4    | uint32  | Compressed pixel data size
///
/// Palette (42 bytes):
/// Index 0 and 1 are fixed (transparent/opaque black), not stored
/// Indices 2-15 stored as RGB (14 colors × 3 bytes = 42 bytes)
///
/// Offset | Size | Description
/// -------|------|----------------------------------
/// 0x10   | 3    | Color 2: RGB
/// 0x13   | 3    | Color 3: RGB
/// ...    | 3    | ...
/// 0x37   | 3    | Color 15: RGB
///
/// Compressed Pixel Data (variable):
/// Offset | Size     | Description
/// -------|----------|----------------------------------
/// 0x3A   | variable | RLE compressed pixel data
///
/// Version 2 (Standard Palette Support):
/// --------------------------------------
/// Header (16 bytes) - Same as v1
///
/// Palette Descriptor (1 or 43 bytes):
/// Offset | Size | Description
/// -------|------|----------------------------------
/// 0x10   | 1    | Palette Mode
///                |   0x00-0x1F (0-31): Standard palette ID
///                |   0xFF (255):       Custom palette
///
/// If Palette Mode = 0x00-0x1F (Standard):
///   Compressed pixel data starts at 0x11 (1 byte palette)
///
/// If Palette Mode = 0xFF (Custom):
///   0x11   | 42   | RGB×14  | Custom palette colors 2-15
///   0x3B   | n    |         | Compressed pixel data
///
/// RLE Compression (same for v1 and v2):
/// --------------------------------------
/// Simple run-length encoding for 4-bit indices:
/// - If count <= 15: [count:4bits][value:4bits] (1 byte)
/// - If count > 15:  [0xF0][count:8bits][value:4bits padding:4bits] (3 bytes)
///
/// Example:
/// Raw: 0 0 0 0 0 1 1 2 2 2
/// RLE: [5:4][0:4] [2:4][1:4] [3:4][2:4]
///      = 0x50 0x21 0x32
///
/// For runs > 15:
/// Raw: 20 zeros
/// RLE: [0xF0][20:8][0:4][0:4]
///      = 0xF0 0x14 0x00

class SpriteCompression {
public:
    // =================================================================
    // Version 1 API (Backward Compatible)
    // =================================================================
    
    /// Save sprite in SPRTZ v1 format (custom palette)
    /// @param filename Output file path
    /// @param width Sprite width (8, 16, or 40)
    /// @param height Sprite height (8, 16, or 40)
    /// @param pixels Raw pixel data (width × height indices)
    /// @param palette Full 64-byte palette (RGBA)
    /// @return true if successful
    static bool saveSPRTZ(const std::string& filename,
                          int width, int height,
                          const uint8_t* pixels,
                          const uint8_t* palette);

    /// Load sprite from SPRTZ format (auto-detects v1 or v2)
    /// @param filename Input file path
    /// @param outWidth Output sprite width
    /// @param outHeight Output sprite height
    /// @param outPixels Output pixel buffer (must be at least 40×40 = 1600 bytes)
    /// @param outPalette Output palette buffer (must be 64 bytes)
    /// @return true if successful
    static bool loadSPRTZ(const std::string& filename,
                          int& outWidth, int& outHeight,
                          uint8_t* outPixels,
                          uint8_t* outPalette);
    
    // =================================================================
    // Version 2 API (Standard Palette Support)
    // =================================================================
    
    /// Save sprite in SPRTZ v2 format with standard palette reference
    /// @param filename Output file path
    /// @param width Sprite width
    /// @param height Sprite height
    /// @param pixels Raw pixel data (width × height indices)
    /// @param standardPaletteID Standard palette ID (0-31)
    /// @return true if successful
    static bool saveSPRTZv2Standard(const std::string& filename,
                                    int width, int height,
                                    const uint8_t* pixels,
                                    uint8_t standardPaletteID);
    
    /// Save sprite in SPRTZ v2 format with custom palette
    /// @param filename Output file path
    /// @param width Sprite width
    /// @param height Sprite height
    /// @param pixels Raw pixel data (width × height indices)
    /// @param palette Full 64-byte palette (RGBA)
    /// @return true if successful
    static bool saveSPRTZv2Custom(const std::string& filename,
                                  int width, int height,
                                  const uint8_t* pixels,
                                  const uint8_t* palette);
    
    /// Load sprite from SPRTZ v2 format (with palette mode info)
    /// @param filename Input file path
    /// @param outWidth Output sprite width
    /// @param outHeight Output sprite height
    /// @param outPixels Output pixel buffer (must be at least 40×40 = 1600 bytes)
    /// @param outPalette Output palette buffer (must be 64 bytes)
    /// @param outIsStandardPalette Output: true if uses standard palette
    /// @param outStandardPaletteID Output: standard palette ID (if applicable)
    /// @return true if successful
    static bool loadSPRTZv2(const std::string& filename,
                            int& outWidth, int& outHeight,
                            uint8_t* outPixels,
                            uint8_t* outPalette,
                            bool& outIsStandardPalette,
                            uint8_t& outStandardPaletteID);
    
    // =================================================================
    // Utility Functions
    // =================================================================

    /// Get compression statistics
    /// @param pixels Raw pixel data
    /// @param pixelCount Number of pixels
    /// @return Estimated compressed size in bytes
    static size_t estimateCompressedSize(const uint8_t* pixels, int pixelCount);
    
    /// Get SPRTZ file version
    /// @param filename SPRTZ file path
    /// @return Version number (1 or 2), or 0 on error
    static int getFileVersion(const std::string& filename);

private:
    /// RLE compress pixel data
    /// @param pixels Input pixel data
    /// @param pixelCount Number of pixels
    /// @param compressed Output compressed data
    static void compressRLE(const uint8_t* pixels, int pixelCount,
                           std::vector<uint8_t>& compressed);

    /// RLE decompress pixel data
    /// @param compressed Input compressed data
    /// @param compressedSize Size of compressed data
    /// @param pixels Output pixel buffer
    /// @param pixelCount Expected number of pixels
    /// @return true if successful
    static bool decompressRLE(const uint8_t* compressed, size_t compressedSize,
                             uint8_t* pixels, int pixelCount);
};

/// Get file format description for documentation
inline std::string getSPRTZFormatDescription() {
    return R"(
SPRTZ Format Specification
===========================

SPRTZ is a compressed sprite format for indexed 4-bit sprites.
It stores sprite dimensions, a 14-color RGB palette (indices 2-15),
and RLE-compressed pixel data.

File Structure:
---------------
1. Header (16 bytes)
2. Palette (42 bytes) - Colors 2-15 only (RGB)
3. Compressed pixel data (variable)

Total file size: 58 bytes + compressed data

Header Layout:
--------------
Offset | Size | Description
-------|------|-------------------------------------
0x00   | 4    | Magic: "SPTZ"
0x04   | 2    | Version (1)
0x06   | 1    | Width (8, 16, or 40)
0x07   | 1    | Height (8, 16, or 40)
0x08   | 4    | Uncompressed size (W×H bytes)
0x0C   | 4    | Compressed size (bytes)

Palette Layout (42 bytes):
--------------------------
Colors 0 and 1 are implicit:
  - Index 0: Transparent black (0,0,0,0)
  - Index 1: Opaque black (0,0,0,255)

Stored colors (indices 2-15):
  Offset 0x10-0x3A: 14 colors × 3 bytes RGB

Compression Algorithm:
----------------------
Run-Length Encoding (RLE) for 4-bit values:

Short runs (count 1-15):
  [count:4bits][value:4bits]
  1 byte per run

Long runs (count 16-255):
  [0xF0][count:8bits][value:4bits][padding:4bits]
  3 bytes per run

Example:
--------
Sprite: 16×16 pixels with simple patterns
Uncompressed: 256 bytes
Compressed: ~50-150 bytes (depending on content)

Compression Ratio:
------------------
- Solid color: ~95% reduction
- Simple patterns: ~50-70% reduction
- Complex/noisy: ~10-30% reduction
- Worst case: ~110% (slightly larger)

File Extension: .sprtz
MIME Type: application/x-sprtz
)";
}

} // namespace SPRED

#endif // SPRED_SPRITE_COMPRESSION_H