//
//  SpriteCompression.cpp
//  SPRED - Sprite Editor
//
//  SPRTZ compressed sprite format implementation
//

#include "SpriteCompression.h"
#include "../Data/PaletteLibrary.h"
#include <fstream>
#include <cstring>
#include <cstdio>
#include <zlib.h>

using namespace SuperTerminal;

namespace SPRED {

bool SpriteCompression::saveSPRTZ(const std::string& filename,
                                   int width, int height,
                                   const uint8_t* pixels,
                                   const uint8_t* palette) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Compress pixel data
    int pixelCount = width * height;
    std::vector<uint8_t> compressed;
    compressRLE(pixels, pixelCount, compressed);

    // Write header
    const char magic[4] = {'S', 'P', 'T', 'Z'};
    file.write(magic, 4);

    uint16_t version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));

    uint8_t w = static_cast<uint8_t>(width);
    uint8_t h = static_cast<uint8_t>(height);
    file.write(reinterpret_cast<const char*>(&w), 1);
    file.write(reinterpret_cast<const char*>(&h), 1);

    uint32_t uncompressedSize = static_cast<uint32_t>(pixelCount);
    uint32_t compressedSize = static_cast<uint32_t>(compressed.size());
    file.write(reinterpret_cast<const char*>(&uncompressedSize), sizeof(uncompressedSize));
    file.write(reinterpret_cast<const char*>(&compressedSize), sizeof(compressedSize));

    // Write palette (indices 2-15 only, RGB only)
    // Skip indices 0 and 1 (transparent and opaque black)
    for (int i = 2; i < 16; i++) {
        int offset = i * 4;
        uint8_t rgb[3] = {palette[offset], palette[offset + 1], palette[offset + 2]};
        file.write(reinterpret_cast<const char*>(rgb), 3);
    }

    // Write compressed pixel data
    file.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());

    return file.good();
}

bool SpriteCompression::loadSPRTZ(const std::string& filename,
                                   int& outWidth, int& outHeight,
                                   uint8_t* outPixels,
                                   uint8_t* outPalette) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Read and verify header
    char magic[4];
    file.read(magic, 4);
    if (magic[0] != 'S' || magic[1] != 'P' || magic[2] != 'T' || magic[3] != 'Z') {
        return false;
    }

    uint16_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != 1) {
        return false;
    }

    uint8_t w, h;
    file.read(reinterpret_cast<char*>(&w), 1);
    file.read(reinterpret_cast<char*>(&h), 1);
    outWidth = w;
    outHeight = h;

    uint32_t uncompressedSize, compressedSize;
    file.read(reinterpret_cast<char*>(&uncompressedSize), sizeof(uncompressedSize));
    file.read(reinterpret_cast<char*>(&compressedSize), sizeof(compressedSize));

    // Verify sizes
    int expectedPixels = outWidth * outHeight;
    if (uncompressedSize != static_cast<uint32_t>(expectedPixels)) {
        return false;
    }

    // Read palette (indices 2-15, RGB only)
    // Set up fixed colors 0 and 1
    outPalette[0] = 0;   // R
    outPalette[1] = 0;   // G
    outPalette[2] = 0;   // B
    outPalette[3] = 0;   // A (transparent)

    outPalette[4] = 0;   // R
    outPalette[5] = 0;   // G
    outPalette[6] = 0;   // B
    outPalette[7] = 255; // A (opaque)

    // Read colors 2-15
    for (int i = 2; i < 16; i++) {
        uint8_t rgb[3];
        file.read(reinterpret_cast<char*>(rgb), 3);
        int offset = i * 4;
        outPalette[offset + 0] = rgb[0];
        outPalette[offset + 1] = rgb[1];
        outPalette[offset + 2] = rgb[2];
        outPalette[offset + 3] = 255; // Always opaque for colors 2-15
    }

    // Read compressed pixel data
    std::vector<uint8_t> compressed(compressedSize);
    file.read(reinterpret_cast<char*>(compressed.data()), compressedSize);

    if (!file.good()) {
        return false;
    }

    // Decompress
    return decompressRLE(compressed.data(), compressedSize, outPixels, expectedPixels);
}

void SpriteCompression::compressRLE(const uint8_t* pixels, int pixelCount,
                                     std::vector<uint8_t>& compressed) {
    // Use zlib compression instead of custom RLE
    compressed.clear();
    
    // Estimate compressed size (zlib's compressBound gives upper limit)
    uLongf compressedSize = compressBound(pixelCount);
    compressed.resize(compressedSize);
    
    // Compress using zlib
    int result = compress2(compressed.data(), &compressedSize,
                          pixels, pixelCount,
                          Z_BEST_COMPRESSION);
    
    if (result != Z_OK) {
        printf("[SpriteCompression::compressRLE] ERROR: zlib compression failed with code %d\n", result);
        compressed.clear();
        return;
    }
    
    // Resize to actual compressed size
    compressed.resize(compressedSize);
    printf("[SpriteCompression::compressRLE] Compressed %d bytes to %zu bytes using zlib\n", 
           pixelCount, compressed.size());
}

bool SpriteCompression::decompressRLE(const uint8_t* compressed, size_t compressedSize,
                                       uint8_t* pixels, int pixelCount) {
    printf("[SpriteCompression::decompressRLE] Starting zlib decompression: compressedSize=%zu, pixelCount=%d\n", 
          compressedSize, pixelCount);
    
    // Use zlib decompression
    uLongf uncompressedSize = pixelCount;
    int result = uncompress(pixels, &uncompressedSize,
                           compressed, compressedSize);
    
    if (result != Z_OK) {
        printf("[SpriteCompression::decompressRLE] ERROR: zlib decompression failed with code %d\n", result);
        return false;
    }
    
    if (uncompressedSize != static_cast<uLongf>(pixelCount)) {
        printf("[SpriteCompression::decompressRLE] ERROR: Size mismatch (got %lu, expected %d)\n", 
              uncompressedSize, pixelCount);
        return false;
    }
    
    printf("[SpriteCompression::decompressRLE] Decompression SUCCESS: %lu bytes\n", uncompressedSize);
    return true;
}

size_t SpriteCompression::estimateCompressedSize(const uint8_t* pixels, int pixelCount) {
    // Use zlib's compressBound for accurate estimation
    return compressBound(pixelCount);
}

// =============================================================================
// SPRTZ v2 API (Standard Palette Support)
// =============================================================================

bool SpriteCompression::saveSPRTZv2Standard(const std::string& filename,
                                            int width, int height,
                                            const uint8_t* pixels,
                                            uint8_t standardPaletteID) {
    if (standardPaletteID >= 32) {
        return false; // Invalid palette ID
    }
    
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Compress pixel data
    int pixelCount = width * height;
    std::vector<uint8_t> compressed;
    compressRLE(pixels, pixelCount, compressed);

    // Write header
    const char magic[4] = {'S', 'P', 'T', 'Z'};
    file.write(magic, 4);

    uint16_t version = 2;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));

    uint8_t w = static_cast<uint8_t>(width);
    uint8_t h = static_cast<uint8_t>(height);
    file.write(reinterpret_cast<const char*>(&w), 1);
    file.write(reinterpret_cast<const char*>(&h), 1);

    uint32_t uncompressedSize = static_cast<uint32_t>(pixelCount);
    uint32_t compressedSize = static_cast<uint32_t>(compressed.size());
    file.write(reinterpret_cast<const char*>(&uncompressedSize), sizeof(uncompressedSize));
    file.write(reinterpret_cast<const char*>(&compressedSize), sizeof(compressedSize));

    // Write palette mode (standard palette ID)
    file.write(reinterpret_cast<const char*>(&standardPaletteID), 1);

    // Write compressed pixel data (no palette data needed!)
    file.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());

    return file.good();
}

bool SpriteCompression::saveSPRTZv2Custom(const std::string& filename,
                                          int width, int height,
                                          const uint8_t* pixels,
                                          const uint8_t* palette) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Compress pixel data
    int pixelCount = width * height;
    std::vector<uint8_t> compressed;
    compressRLE(pixels, pixelCount, compressed);

    // Write header
    const char magic[4] = {'S', 'P', 'T', 'Z'};
    file.write(magic, 4);

    uint16_t version = 2;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));

    uint8_t w = static_cast<uint8_t>(width);
    uint8_t h = static_cast<uint8_t>(height);
    file.write(reinterpret_cast<const char*>(&w), 1);
    file.write(reinterpret_cast<const char*>(&h), 1);

    uint32_t uncompressedSize = static_cast<uint32_t>(pixelCount);
    uint32_t compressedSize = static_cast<uint32_t>(compressed.size());
    file.write(reinterpret_cast<const char*>(&uncompressedSize), sizeof(uncompressedSize));
    file.write(reinterpret_cast<const char*>(&compressedSize), sizeof(compressedSize));

    // Write palette mode (0xFF = custom)
    uint8_t paletteMode = 0xFF;
    file.write(reinterpret_cast<const char*>(&paletteMode), 1);

    // Write custom palette (indices 2-15 only, RGB only)
    for (int i = 2; i < 16; i++) {
        int offset = i * 4;
        uint8_t rgb[3] = {palette[offset], palette[offset + 1], palette[offset + 2]};
        file.write(reinterpret_cast<const char*>(rgb), 3);
    }

    // Write compressed pixel data
    file.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());

    return file.good();
}

bool SpriteCompression::loadSPRTZv2(const std::string& filename,
                                    int& outWidth, int& outHeight,
                                    uint8_t* outPixels,
                                    uint8_t* outPalette,
                                    bool& outIsStandardPalette,
                                    uint8_t& outStandardPaletteID) {
    printf("[SpriteCompression::loadSPRTZv2] Opening file: %s\n", filename.c_str());
    
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        printf("[SpriteCompression::loadSPRTZv2] ERROR: Failed to open file\n");
        return false;
    }

    // Read and verify header
    char magic[4];
    file.read(magic, 4);
    printf("[SpriteCompression::loadSPRTZv2] Magic bytes: 0x%02X 0x%02X 0x%02X 0x%02X ('%c%c%c%c')\n",
          (unsigned char)magic[0], (unsigned char)magic[1], (unsigned char)magic[2], (unsigned char)magic[3],
          magic[0], magic[1], magic[2], magic[3]);
    
    if (magic[0] != 'S' || magic[1] != 'P' || magic[2] != 'T' || magic[3] != 'Z') {
        printf("[SpriteCompression::loadSPRTZv2] ERROR: Invalid magic bytes (expected 'SPTZ')\n");
        return false;
    }

    uint16_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    printf("[SpriteCompression::loadSPRTZv2] Version: %d\n", version);
    
    // Support both v1 and v2
    if (version == 1) {
        printf("[SpriteCompression::loadSPRTZv2] Detected v1 format, reading as custom palette\n");
        // v1 format: read as custom palette
        outIsStandardPalette = false;
        outStandardPaletteID = 0xFF;
        
        // Continue with v1 loading
        uint8_t w, h;
        file.read(reinterpret_cast<char*>(&w), 1);
        file.read(reinterpret_cast<char*>(&h), 1);
        outWidth = w;
        outHeight = h;

        uint32_t uncompressedSize, compressedSize;
        file.read(reinterpret_cast<char*>(&uncompressedSize), sizeof(uncompressedSize));
        file.read(reinterpret_cast<char*>(&compressedSize), sizeof(compressedSize));

        int expectedPixels = outWidth * outHeight;
        if (uncompressedSize != static_cast<uint32_t>(expectedPixels)) {
            return false;
        }

        // Set up fixed colors 0 and 1
        outPalette[0] = 0; outPalette[1] = 0; outPalette[2] = 0; outPalette[3] = 0;
        outPalette[4] = 0; outPalette[5] = 0; outPalette[6] = 0; outPalette[7] = 255;

        // Read palette
        for (int i = 2; i < 16; i++) {
            uint8_t rgb[3];
            file.read(reinterpret_cast<char*>(rgb), 3);
            int offset = i * 4;
            outPalette[offset + 0] = rgb[0];
            outPalette[offset + 1] = rgb[1];
            outPalette[offset + 2] = rgb[2];
            outPalette[offset + 3] = 255;
        }

        // Read and decompress pixels
        std::vector<uint8_t> compressed(compressedSize);
        file.read(reinterpret_cast<char*>(compressed.data()), compressedSize);
        if (!file.good()) {
            printf("[SpriteCompression::loadSPRTZv2] ERROR: Failed to read compressed data\n");
            return false;
        }
        
        printf("[SpriteCompression::loadSPRTZv2] Decompressing RLE data...\n");
        bool result = decompressRLE(compressed.data(), compressedSize, outPixels, expectedPixels);
        printf("[SpriteCompression::loadSPRTZv2] Decompression %s\n", result ? "SUCCESS" : "FAILED");
        return result;
    }
    else if (version == 2) {
        printf("[SpriteCompression::loadSPRTZv2] Processing v2 format\n");
        // v2 format: check palette mode
        uint8_t w, h;
        file.read(reinterpret_cast<char*>(&w), 1);
        file.read(reinterpret_cast<char*>(&h), 1);
        outWidth = w;
        outHeight = h;
        printf("[SpriteCompression::loadSPRTZv2] Dimensions: %dx%d\n", outWidth, outHeight);

        uint32_t uncompressedSize, compressedSize;
        file.read(reinterpret_cast<char*>(&uncompressedSize), sizeof(uncompressedSize));
        file.read(reinterpret_cast<char*>(&compressedSize), sizeof(compressedSize));
        printf("[SpriteCompression::loadSPRTZv2] Uncompressed size: %u, Compressed size: %u\n", 
              uncompressedSize, compressedSize);

        int expectedPixels = outWidth * outHeight;
        if (uncompressedSize != static_cast<uint32_t>(expectedPixels)) {
            printf("[SpriteCompression::loadSPRTZv2] ERROR: Size mismatch (uncompressed=%u, expected=%d)\n",
                  uncompressedSize, expectedPixels);
            return false;
        }

        // Read palette mode
        uint8_t paletteMode;
        file.read(reinterpret_cast<char*>(&paletteMode), 1);
        printf("[SpriteCompression::loadSPRTZv2] Palette mode: 0x%02X (%d)\n", paletteMode, paletteMode);

        // Set up fixed colors 0 and 1
        outPalette[0] = 0; outPalette[1] = 0; outPalette[2] = 0; outPalette[3] = 0;
        outPalette[4] = 0; outPalette[5] = 0; outPalette[6] = 0; outPalette[7] = 255;

        if (paletteMode == 0xFF) {
            printf("[SpriteCompression::loadSPRTZv2] Using custom palette\n");
            // Custom palette
            outIsStandardPalette = false;
            outStandardPaletteID = 0xFF;

            // Read custom palette
            for (int i = 2; i < 16; i++) {
                uint8_t rgb[3];
                file.read(reinterpret_cast<char*>(rgb), 3);
                int offset = i * 4;
                outPalette[offset + 0] = rgb[0];
                outPalette[offset + 1] = rgb[1];
                outPalette[offset + 2] = rgb[2];
                outPalette[offset + 3] = 255;
            }
            printf("[SpriteCompression::loadSPRTZv2] Custom palette loaded (14 colors)\n");
        } else if (paletteMode < 32) {
            printf("[SpriteCompression::loadSPRTZv2] Using standard palette ID %d\n", paletteMode);
            // Standard palette
            outIsStandardPalette = true;
            outStandardPaletteID = paletteMode;

            // Load palette from standard library
            if (!StandardPaletteLibrary::isInitialized()) {
                printf("[SpriteCompression::loadSPRTZv2] ERROR: StandardPaletteLibrary not initialized!\n");
                printf("[SpriteCompression::loadSPRTZv2] Last error: %s\n", 
                      StandardPaletteLibrary::getLastError().c_str());
                return false; // Library not loaded
            }

            printf("[SpriteCompression::loadSPRTZv2] StandardPaletteLibrary is initialized, loading palette...\n");
            if (!StandardPaletteLibrary::copyPaletteRGBA(paletteMode, outPalette)) {
                printf("[SpriteCompression::loadSPRTZv2] ERROR: Failed to copy standard palette %d\n", paletteMode);
                return false; // Failed to get palette
            }
            printf("[SpriteCompression::loadSPRTZv2] Standard palette %d loaded successfully\n", paletteMode);
        } else {
            printf("[SpriteCompression::loadSPRTZv2] ERROR: Invalid palette mode 0x%02X\n", paletteMode);
            return false; // Invalid palette mode
        }

        // Read and decompress pixels
        std::streampos posBeforeRead = file.tellg();
        printf("[SpriteCompression::loadSPRTZv2] File position before reading compressed data: %lld\n", (long long)posBeforeRead);
        
        std::vector<uint8_t> compressed(compressedSize);
        file.read(reinterpret_cast<char*>(compressed.data()), compressedSize);
        if (!file.good()) {
            printf("[SpriteCompression::loadSPRTZv2] ERROR: Failed to read compressed data\n");
            return false;
        }
        
        printf("[SpriteCompression::loadSPRTZv2] Actually read %zu bytes\n", compressed.size());
        printf("[SpriteCompression::loadSPRTZv2] First 5 compressed bytes: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
               compressed[0], compressed[1], compressed[2], compressed[3], compressed[4]);
        if (compressed.size() >= 5) {
            size_t lastIdx = compressed.size() - 1;
            printf("[SpriteCompression::loadSPRTZv2] Last 5 compressed bytes: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
                   compressed[lastIdx-4], compressed[lastIdx-3], compressed[lastIdx-2], compressed[lastIdx-1], compressed[lastIdx]);
        }
        
        printf("[SpriteCompression::loadSPRTZv2] Decompressing RLE data...\n");
        bool result = decompressRLE(compressed.data(), compressedSize, outPixels, expectedPixels);
        printf("[SpriteCompression::loadSPRTZv2] Decompression %s\n", result ? "SUCCESS" : "FAILED");
        return result;
    }
    
    printf("[SpriteCompression::loadSPRTZv2] ERROR: Unsupported version %d\n", version);
    return false; // Unsupported version
}

int SpriteCompression::getFileVersion(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return 0;
    }

    // Read magic
    char magic[4];
    file.read(magic, 4);
    if (magic[0] != 'S' || magic[1] != 'P' || magic[2] != 'T' || magic[3] != 'Z') {
        return 0;
    }

    // Read version
    uint16_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));

    return static_cast<int>(version);
}

} // namespace SPRED