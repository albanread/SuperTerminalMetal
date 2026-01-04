//
// st_api_video_image.mm
// SuperTerminal v2.0
//
// Video mode image loading and saving API
// Supports loading PNG files into XRES/WRES/PRES buffers and saving buffers to PNG
//

#include "superterminal_api.h"
#include "st_api_context.h"
#include "../Display/DisplayManager.h"
#include "../Display/VideoMode/VideoModeManager.h"
#include "../Display/LoResBuffer.h"
#include "../Display/UResBuffer.h"
#include "../Display/XResBuffer.h"
#include "../Display/WResBuffer.h"
#include "../Display/PResBuffer.h"
#include "../Display/LoResPaletteManager.h"
#include "../Display/XResPaletteManager.h"
#include "../Display/WResPaletteManager.h"
#include "../Display/PResPaletteManager.h"

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>

namespace {

// Helper to load PNG and get dimensions and pixel data
// Optionally extract embedded palette if extractPalette is true
bool loadPNGFile(const char* filePath, std::vector<uint8_t>& pixels,
                 int& width, int& height, int& bitsPerPixel,
                 std::vector<uint8_t>* paletteRGB = nullptr) {
    if (!filePath) {
        return false;
    }

    NSString* path = [NSString stringWithUTF8String:filePath];
    NSURL* url = [NSURL fileURLWithPath:path];

    CGImageSourceRef source = CGImageSourceCreateWithURL((__bridge CFURLRef)url, NULL);
    if (!source) {
        NSLog(@"[load_image] Failed to create image source from: %@", path);
        return false;
    }

    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, NULL);
    CFRelease(source);

    if (!image) {
        NSLog(@"[load_image] Failed to load image from: %@", path);
        return false;
    }

    width = (int)CGImageGetWidth(image);
    height = (int)CGImageGetHeight(image);
    bitsPerPixel = (int)CGImageGetBitsPerPixel(image);

    // Check if image is indexed color (8-bit)
    CGColorSpaceRef colorSpace = CGImageGetColorSpace(image);
    CGColorSpaceModel colorModel = CGColorSpaceGetModel(colorSpace);
    bool isIndexed = (colorModel == kCGColorSpaceModelIndexed);

    NSLog(@"[load_image] Image: %dx%d, %d bpp, indexed=%d", width, height, bitsPerPixel, isIndexed);

    // Extract embedded palette if requested and image is indexed
    if (paletteRGB && isIndexed) {
        CGColorSpaceRef baseSpace = CGColorSpaceGetBaseColorSpace(colorSpace);
        size_t colorCount = CGColorSpaceGetColorTableCount(colorSpace);

        if (colorCount > 0) {
            paletteRGB->resize(768);  // Always 256 colors × 3 bytes

            // Allocate temporary buffer for color table
            std::vector<uint8_t> colorTable(colorCount * 3);
            CGColorSpaceGetColorTable(colorSpace, colorTable.data());

            // Copy available palette entries
            size_t copyCount = (colorCount < 256) ? colorCount : 256;
            memcpy(paletteRGB->data(), colorTable.data(), copyCount * 3);

            // Fill remaining entries with black if fewer than 256 colors
            if (copyCount < 256) {
                memset(paletteRGB->data() + (copyCount * 3), 0, (256 - copyCount) * 3);
            }

            NSLog(@"[load_image] Extracted %zu palette colors", colorCount);
        }
    }

    // Allocate pixel buffer
    size_t bytesPerPixel = isIndexed ? 1 : 4;
    pixels.resize(width * height * bytesPerPixel);

    if (isIndexed) {
        // Load as indexed color (8-bit)
        CGContextRef context = CGBitmapContextCreate(
            pixels.data(),
            width,
            height,
            8,
            width,
            colorSpace,
            (CGBitmapInfo)kCGImageAlphaNone
        );

        if (!context) {
            NSLog(@"[load_image] Failed to create indexed bitmap context");
            CGImageRelease(image);
            return false;
        }

        CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
        CGContextRelease(context);
        bitsPerPixel = 8;
    } else {
        // Load as RGBA
        CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(
            pixels.data(),
            width,
            height,
            8,
            width * 4,
            rgbColorSpace,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
        );
        CGColorSpaceRelease(rgbColorSpace);

        if (!context) {
            NSLog(@"[load_image] Failed to create RGBA bitmap context");
            CGImageRelease(image);
            return false;
        }

        CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
        CGContextRelease(context);
        bitsPerPixel = 32;
    }

    CGImageRelease(image);
    return true;
}

// Helper to save PNG file with optional palette data
bool savePNGFile(const char* filePath, const uint8_t* pixels,
                 int width, int height, bool isIndexed, const uint8_t* paletteRGB = nullptr) {
    if (!filePath || !pixels) {
        return false;
    }

    NSString* path = [NSString stringWithUTF8String:filePath];
    NSURL* url = [NSURL fileURLWithPath:path];

    CGImageRef image = nullptr;

    if (isIndexed && paletteRGB) {
        // Create indexed color space with embedded palette (256 colors × 3 bytes RGB)
        CGColorSpaceRef baseSpace = CGColorSpaceCreateDeviceRGB();
        CGColorSpaceRef colorSpace = CGColorSpaceCreateIndexed(
            baseSpace,
            255,  // lastIndex (0-255 = 256 colors)
            paletteRGB
        );
        CGColorSpaceRelease(baseSpace);

        CGDataProviderRef provider = CGDataProviderCreateWithData(
            NULL,
            pixels,
            width * height,
            NULL
        );

        image = CGImageCreate(
            width,
            height,
            8,      // bitsPerComponent
            8,      // bitsPerPixel
            width,  // bytesPerRow
            colorSpace,
            (CGBitmapInfo)kCGImageAlphaNone,
            provider,
            NULL,   // decode array
            false,  // shouldInterpolate
            kCGRenderingIntentDefault
        );

        CGDataProviderRelease(provider);
        CGColorSpaceRelease(colorSpace);
    } else {
        // Create RGBA image
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGDataProviderRef provider = CGDataProviderCreateWithData(
            NULL,
            pixels,
            width * height * 4,
            NULL
        );

        image = CGImageCreate(
            width,
            height,
            8,
            32,
            width * 4,
            colorSpace,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big,
            provider,
            NULL,
            false,
            kCGRenderingIntentDefault
        );

        CGDataProviderRelease(provider);
        CGColorSpaceRelease(colorSpace);
    }

    if (!image) {
        NSLog(@"[save_image] Failed to create CGImage");
        return false;
    }

    CGImageDestinationRef destination = CGImageDestinationCreateWithURL(
        (__bridge CFURLRef)url,
        kUTTypePNG,
        1,
        NULL
    );

    if (!destination) {
        NSLog(@"[save_image] Failed to create image destination");
        CGImageRelease(image);
        return false;
    }

    CGImageDestinationAddImage(destination, image, NULL);

    bool success = CGImageDestinationFinalize(destination);
    CFRelease(destination);
    CGImageRelease(image);

    if (success) {
        NSLog(@"[save_image] PNG saved: %@", path);
    } else {
        NSLog(@"[save_image] Failed to write PNG: %@", path);
    }

    return success;
}

// Helper to save palette to .pal file
// Format Version 4:
//   Header: "STPAL" (5 bytes) + version=4 (1 byte) + videoMode (1 byte) + paletteModel (1 byte)
//          + rowCount (2 bytes) + colorsPerRow (2 bytes) + globalColorCount (2 bytes)
//          + compression (1 byte) + uncompressedSize (4 bytes) + reserved (1 byte)
//   Data: Global palette (globalColorCount × 4 RGBA) + Per-row palettes (rowCount × colorsPerRow × 4 RGBA)
//   Data is compressed with zlib
bool savePaletteFile(const char* filePath, SuperTerminal::VideoMode mode,
                     STApi::Context& context) {
    if (!filePath) {
        return false;
    }

    // Determine video mode byte, palette model, and dimensions
    uint8_t videoModeByte = 0;
    uint8_t paletteModel = 0;
    uint16_t rowCount = 0;
    uint16_t colorsPerRow = 0;
    uint16_t globalColorCount = 0;

    switch (mode) {
        case SuperTerminal::VideoMode::LORES:
            videoModeByte = 1;
            paletteModel = 1;  // Per-row only
            rowCount = 300;
            colorsPerRow = 16;
            globalColorCount = 0;
            break;

        case SuperTerminal::VideoMode::XRES:
            videoModeByte = 2;
            paletteModel = 3;  // Hybrid
            rowCount = 240;
            colorsPerRow = 16;
            globalColorCount = 240;
            break;

        case SuperTerminal::VideoMode::WRES:
            videoModeByte = 3;
            paletteModel = 3;  // Hybrid
            rowCount = 240;
            colorsPerRow = 16;
            globalColorCount = 240;
            break;

        case SuperTerminal::VideoMode::URES:
            videoModeByte = 4;
            paletteModel = 0;  // None (direct color)
            rowCount = 0;
            colorsPerRow = 0;
            globalColorCount = 0;
            break;

        case SuperTerminal::VideoMode::PRES:
            videoModeByte = 5;
            paletteModel = 3;  // Hybrid
            rowCount = 720;
            colorsPerRow = 16;
            globalColorCount = 240;
            break;

        default:
            NSLog(@"[save_palette] Unsupported video mode");
            return false;
    }

    // Build uncompressed palette data
    std::vector<uint8_t> paletteData;

    // Save global palette data (if any)
    for (int i = 0; i < globalColorCount; i++) {
        uint32_t rgba = 0;
        int colorIndex = i + 16;  // Global colors start at index 16

        switch (mode) {
            case SuperTerminal::VideoMode::XRES: {
                auto paletteMgr = context.xresPalette();
                if (paletteMgr) {
                    rgba = paletteMgr->getGlobalColor(colorIndex);
                }
                break;
            }
            case SuperTerminal::VideoMode::WRES: {
                auto paletteMgr = context.wresPalette();
                if (paletteMgr) {
                    rgba = paletteMgr->getGlobalColor(colorIndex);
                }
                break;
            }
            case SuperTerminal::VideoMode::PRES: {
                auto paletteMgr = context.presPalette();
                if (paletteMgr) {
                    rgba = paletteMgr->getGlobalColor(colorIndex);
                }
                break;
            }
            default:
                break;
        }

        // Save RGBA (4 bytes per color)
        uint8_t r = (rgba >> 24) & 0xFF;
        uint8_t g = (rgba >> 16) & 0xFF;
        uint8_t b = (rgba >> 8) & 0xFF;
        uint8_t a = rgba & 0xFF;
        paletteData.push_back(r);
        paletteData.push_back(g);
        paletteData.push_back(b);
        paletteData.push_back(a);
    }

    // Save per-row palette data (if any)
    for (int row = 0; row < rowCount; row++) {
        for (int i = 0; i < colorsPerRow; i++) {
            uint32_t rgba = 0;

            switch (mode) {
                case SuperTerminal::VideoMode::LORES: {
                    auto paletteMgr = context.loresPalette();
                    if (paletteMgr) {
                        rgba = paletteMgr->getPaletteEntry(row, i);
                    }
                    break;
                }
                case SuperTerminal::VideoMode::XRES: {
                    auto paletteMgr = context.xresPalette();
                    if (paletteMgr) {
                        rgba = paletteMgr->getPerRowColor(row, i);
                    }
                    break;
                }
                case SuperTerminal::VideoMode::WRES: {
                    auto paletteMgr = context.wresPalette();
                    if (paletteMgr) {
                        rgba = paletteMgr->getPerRowColor(row, i);
                    }
                    break;
                }
                case SuperTerminal::VideoMode::PRES: {
                    auto paletteMgr = context.presPalette();
                    if (paletteMgr) {
                        rgba = paletteMgr->getPerRowColor(row, i);
                    }
                    break;
                }
                default:
                    break;
            }

            // Save RGBA (4 bytes per color)
            uint8_t r = (rgba >> 24) & 0xFF;
            uint8_t g = (rgba >> 16) & 0xFF;
            uint8_t b = (rgba >> 8) & 0xFF;
            uint8_t a = rgba & 0xFF;
            paletteData.push_back(r);
            paletteData.push_back(g);
            paletteData.push_back(b);
            paletteData.push_back(a);
        }
    }

    // Create NSData from palette data
    NSData* uncompressedData = [NSData dataWithBytes:paletteData.data()
                                               length:paletteData.size()];
    uint32_t uncompressedSize = (uint32_t)paletteData.size();

    // Compress using zlib
    NSError* error = nil;
    NSData* compressedData = nil;

    if (@available(macOS 10.11, *)) {
        compressedData = [uncompressedData compressedDataUsingAlgorithm:NSDataCompressionAlgorithmZlib
                                                                   error:&error];
    }

    if (!compressedData || error) {
        NSLog(@"[save_palette] Compression failed, saving uncompressed");
        compressedData = uncompressedData;
    }

    // Build final file with header (20 bytes)
    std::vector<uint8_t> fileData;

    // Header: "STPAL" (5 bytes)
    fileData.push_back('S');
    fileData.push_back('T');
    fileData.push_back('P');
    fileData.push_back('A');
    fileData.push_back('L');

    // Version: 4 (1 byte)
    fileData.push_back(4);

    // Video mode: 1=LORES, 2=XRES, 3=WRES, 4=URES, 5=PRES (1 byte)
    fileData.push_back(videoModeByte);

    // Palette model: 0=none, 1=per-row, 2=global, 3=hybrid (1 byte)
    fileData.push_back(paletteModel);

    // Row count: 16-bit big-endian (2 bytes)
    fileData.push_back((rowCount >> 8) & 0xFF);
    fileData.push_back(rowCount & 0xFF);

    // Colors per row: 16-bit big-endian (2 bytes)
    fileData.push_back((colorsPerRow >> 8) & 0xFF);
    fileData.push_back(colorsPerRow & 0xFF);

    // Global color count: 16-bit big-endian (2 bytes)
    fileData.push_back((globalColorCount >> 8) & 0xFF);
    fileData.push_back(globalColorCount & 0xFF);

    // Compression flag: 1=zlib (1 byte)
    uint8_t compressionFlag = (compressedData == uncompressedData) ? 0 : 1;
    fileData.push_back(compressionFlag);

    // Uncompressed size: 32-bit big-endian (4 bytes)
    fileData.push_back((uncompressedSize >> 24) & 0xFF);
    fileData.push_back((uncompressedSize >> 16) & 0xFF);
    fileData.push_back((uncompressedSize >> 8) & 0xFF);
    fileData.push_back(uncompressedSize & 0xFF);

    // Reserved: (1 byte)
    fileData.push_back(0);

    // Append compressed data
    const uint8_t* compressedBytes = (const uint8_t*)[compressedData bytes];
    fileData.insert(fileData.end(), compressedBytes, compressedBytes + [compressedData length]);

    // Write to file
    NSString* path = [NSString stringWithUTF8String:filePath];
    NSData* nsData = [NSData dataWithBytes:fileData.data() length:fileData.size()];
    BOOL success = [nsData writeToFile:path atomically:YES];

    if (success) {
        const char* modeName = "Unknown";
        switch (videoModeByte) {
            case 1: modeName = "LORES"; break;
            case 2: modeName = "XRES"; break;
            case 3: modeName = "WRES"; break;
            case 4: modeName = "URES"; break;
            case 5: modeName = "PRES"; break;
        }
        NSLog(@"[save_palette] Palette saved (v4 %s, model %d, %d rows, %s, %zu bytes): %@",
              modeName, paletteModel, rowCount,
              compressionFlag ? "compressed" : "uncompressed",
              fileData.size(), path);
    } else {
        NSLog(@"[save_palette] Failed to write palette: %@", path);
    }

    return success;
}

// Helper to load palette from .pal file (supports version 1, 2, 3, and 4)
bool loadPaletteFile(const char* filePath, SuperTerminal::VideoMode mode,
                     STApi::Context& context) {
    if (!filePath) {
        return false;
    }

    NSString* path = [NSString stringWithUTF8String:filePath];
    NSData* nsData = [NSData dataWithContentsOfFile:path];

    if (!nsData) {
        NSLog(@"[load_palette] Failed to read file: %@", path);
        return false;
    }

    const uint8_t* data = (const uint8_t*)[nsData bytes];
    size_t length = [nsData length];

    // Verify header
    if (length < 6) {
        NSLog(@"[load_palette] File too small");
        return false;
    }

    if (data[0] != 'S' || data[1] != 'T' || data[2] != 'P' ||
        data[3] != 'A' || data[4] != 'L') {
        NSLog(@"[load_palette] Invalid file header");
        return false;
    }

    uint8_t version = data[5];

    // Get current mode name for logging
    const char* currentModeName = "Unknown";
    uint8_t currentModeCode = 0;
    switch (mode) {
        case SuperTerminal::VideoMode::LORES:
            currentModeName = "LORES";
            currentModeCode = 1;
            break;
        case SuperTerminal::VideoMode::XRES:
            currentModeName = "XRES";
            currentModeCode = 2;
            break;
        case SuperTerminal::VideoMode::WRES:
            currentModeName = "WRES";
            currentModeCode = 3;
            break;
        case SuperTerminal::VideoMode::URES:
            currentModeName = "URES";
            currentModeCode = 4;
            break;
        case SuperTerminal::VideoMode::PRES:
            currentModeName = "PRES";
            currentModeCode = 5;
            break;
        default:
            break;
    }

    if (version == 1) {
        // Version 1: Global palette only, RGB (726 bytes total)
        if (length != 726) {
            NSLog(@"[load_palette] Invalid file size for v1: %zu (expected 726)", length);
            return false;
        }

        // Load palette data (240 colors, indices 16-255, RGB format)
        const uint8_t* palette = data + 6;
        for (int i = 16; i < 256; i++) {
            int offset = (i - 16) * 3;
            uint8_t r = palette[offset];
            uint8_t g = palette[offset + 1];
            uint8_t b = palette[offset + 2];
            uint32_t rgba = (r << 24) | (g << 16) | (b << 8) | 0xFF;

            switch (mode) {
                case SuperTerminal::VideoMode::XRES: {
                    auto paletteMgr = context.xresPalette();
                    if (paletteMgr) {
                        paletteMgr->setGlobalColor(i, rgba);
                    }
                    break;
                }
                case SuperTerminal::VideoMode::WRES: {
                    auto paletteMgr = context.wresPalette();
                    if (paletteMgr) {
                        paletteMgr->setGlobalColor(i, rgba);
                    }
                    break;
                }
                case SuperTerminal::VideoMode::PRES: {
                    auto paletteMgr = context.presPalette();
                    if (paletteMgr) {
                        paletteMgr->setGlobalColor(i, rgba);
                    }
                    break;
                }
                default:
                    break;
            }
        }

        NSLog(@"[load_palette] Palette loaded (v1 RGB, global only): %@", path);
        return true;

    } else if (version == 2) {
        // Version 2: Global + per-row palettes, RGB format
        if (length < 8) {
            NSLog(@"[load_palette] File too small for v2 header");
            return false;
        }

        // Read row count
        int rowCount = (data[6] << 8) | data[7];

        // Expected size: 8 (header) + 720 (global RGB) + rowCount * 16 * 3 (per-row RGB)
        size_t expectedSize = 8 + 240 * 3 + rowCount * 16 * 3;
        if (length != expectedSize) {
            NSLog(@"[load_palette] Invalid file size for v2: %zu (expected %zu)", length, expectedSize);
            return false;
        }

        const uint8_t* palette = data + 8;

        // Load global palette data (240 colors, indices 16-255, RGB format)
        for (int i = 16; i < 256; i++) {
            int offset = (i - 16) * 3;
            uint8_t r = palette[offset];
            uint8_t g = palette[offset + 1];
            uint8_t b = palette[offset + 2];
            uint32_t rgba = (r << 24) | (g << 16) | (b << 8) | 0xFF;

            switch (mode) {
                case SuperTerminal::VideoMode::XRES: {
                    auto paletteMgr = context.xresPalette();
                    if (paletteMgr) {
                        paletteMgr->setGlobalColor(i, rgba);
                    }
                    break;
                }
                case SuperTerminal::VideoMode::WRES: {
                    auto paletteMgr = context.wresPalette();
                    if (paletteMgr) {
                        paletteMgr->setGlobalColor(i, rgba);
                    }
                    break;
                }
                case SuperTerminal::VideoMode::PRES: {
                    auto paletteMgr = context.presPalette();
                    if (paletteMgr) {
                        paletteMgr->setGlobalColor(i, rgba);
                    }
                    break;
                }
                default:
                    break;
            }
        }

        // Load per-row palette data (rowCount × 16 colors, indices 0-15, RGB format)
        const uint8_t* rowPalette = palette + 240 * 3;
        for (int row = 0; row < rowCount; row++) {
            for (int i = 0; i < 16; i++) {
                int offset = (row * 16 + i) * 3;
                uint8_t r = rowPalette[offset];
                uint8_t g = rowPalette[offset + 1];
                uint8_t b = rowPalette[offset + 2];
                uint32_t rgba = (r << 24) | (g << 16) | (b << 8) | 0xFF;

                switch (mode) {
                    case SuperTerminal::VideoMode::XRES: {
                        auto paletteMgr = context.xresPalette();
                        if (paletteMgr) {
                            paletteMgr->setPerRowColor(row, i, rgba);
                        }
                        break;
                    }
                    case SuperTerminal::VideoMode::WRES: {
                        auto paletteMgr = context.wresPalette();
                        if (paletteMgr) {
                            paletteMgr->setPerRowColor(row, i, rgba);
                        }
                        break;
                    }
                    case SuperTerminal::VideoMode::PRES: {
                        auto paletteMgr = context.presPalette();
                        if (paletteMgr) {
                            paletteMgr->setPerRowColor(row, i, rgba);
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }

        NSLog(@"[load_palette] Palette loaded (v2 RGB, global + %d rows): %@", rowCount, path);
        return true;

    } else if (version == 3) {
        // Version 3: Legacy format with partial V4 features
        if (length < 14) {
            NSLog(@"[load_palette] File too small for v3 header");
            return false;
        }

        // Read video mode byte (V3 used different codes)
        uint8_t videoModeByte = data[6];
        const char* fileModeName = "Unknown";
        SuperTerminal::VideoMode fileMode = SuperTerminal::VideoMode::NONE;

        // V3 video mode codes: 1=XRES, 2=WRES, 3=PRES
        switch (videoModeByte) {
            case 1:
                fileModeName = "XRES";
                fileMode = SuperTerminal::VideoMode::XRES;
                break;
            case 2:
                fileModeName = "WRES";
                fileMode = SuperTerminal::VideoMode::WRES;
                break;
            case 3:
                fileModeName = "PRES";
                fileMode = SuperTerminal::VideoMode::PRES;
                break;
            default:
                NSLog(@"[load_palette] Unknown V3 video mode byte: %d", videoModeByte);
                return false;
        }

        // Validate video mode matches
        if (fileMode != mode) {
            NSLog(@"[load_palette] Warning: Palette is for %s but current mode is %s",
                  fileModeName, currentModeName);
        }

        // Read row count
        int rowCount = (data[7] << 8) | data[8];

        // Read compression flag
        uint8_t compressionFlag = data[9];

        // Read uncompressed size
        uint32_t uncompressedSize = (data[10] << 24) | (data[11] << 16) |
                                    (data[12] << 8) | data[13];

        // Expected uncompressed size: 960 (global RGBA) + rowCount * 16 * 4 (per-row RGBA)
        size_t expectedSize = 240 * 4 + rowCount * 16 * 4;
        if (uncompressedSize != expectedSize) {
            NSLog(@"[load_palette] Invalid uncompressed size: %u (expected %zu)",
                  uncompressedSize, expectedSize);
            return false;
        }

        // Get compressed data
        NSData* compressedData = [NSData dataWithBytes:data + 14 length:length - 14];
        NSData* paletteData = nil;

        if (compressionFlag == 1) {
            // Decompress
            NSError* error = nil;
            if (@available(macOS 10.11, *)) {
                paletteData = [compressedData decompressedDataUsingAlgorithm:NSDataCompressionAlgorithmZlib
                                                                        error:&error];
            }

            if (!paletteData || error) {
                NSLog(@"[load_palette] Decompression failed: %@", error);
                return false;
            }

            if ([paletteData length] != uncompressedSize) {
                NSLog(@"[load_palette] Decompressed size mismatch: %zu != %u",
                      [paletteData length], uncompressedSize);
                return false;
            }
        } else {
            // Not compressed
            paletteData = compressedData;
        }

        const uint8_t* palette = (const uint8_t*)[paletteData bytes];

        // Load global palette data (240 colors, indices 16-255, RGBA format)
        for (int i = 16; i < 256; i++) {
            int offset = (i - 16) * 4;
            uint8_t r = palette[offset];
            uint8_t g = palette[offset + 1];
            uint8_t b = palette[offset + 2];
            uint8_t a = palette[offset + 3];
            uint32_t rgba = (r << 24) | (g << 16) | (b << 8) | a;

            switch (mode) {
                case SuperTerminal::VideoMode::XRES: {
                    auto paletteMgr = context.xresPalette();
                    if (paletteMgr) {
                        paletteMgr->setGlobalColor(i, rgba);
                    }
                    break;
                }
                case SuperTerminal::VideoMode::WRES: {
                    auto paletteMgr = context.wresPalette();
                    if (paletteMgr) {
                        paletteMgr->setGlobalColor(i, rgba);
                    }
                    break;
                }
                case SuperTerminal::VideoMode::PRES: {
                    auto paletteMgr = context.presPalette();
                    if (paletteMgr) {
                        paletteMgr->setGlobalColor(i, rgba);
                    }
                    break;
                }
                default:
                    break;
            }
        }

        // Load per-row palette data (rowCount × 16 colors, indices 0-15, RGBA format)
        const uint8_t* rowPalette = palette + 240 * 4;
        for (int row = 0; row < rowCount; row++) {
            for (int i = 0; i < 16; i++) {
                int offset = (row * 16 + i) * 4;
                uint8_t r = rowPalette[offset];
                uint8_t g = rowPalette[offset + 1];
                uint8_t b = rowPalette[offset + 2];
                uint8_t a = rowPalette[offset + 3];
                uint32_t rgba = (r << 24) | (g << 16) | (b << 8) | a;

                switch (mode) {
                    case SuperTerminal::VideoMode::XRES: {
                        auto paletteMgr = context.xresPalette();
                        if (paletteMgr) {
                            paletteMgr->setPerRowColor(row, i, rgba);
                        }
                        break;
                    }
                    case SuperTerminal::VideoMode::WRES: {
                        auto paletteMgr = context.wresPalette();
                        if (paletteMgr) {
                            paletteMgr->setPerRowColor(row, i, rgba);
                        }
                        break;
                    }
                    case SuperTerminal::VideoMode::PRES: {
                        auto paletteMgr = context.presPalette();
                        if (paletteMgr) {
                            paletteMgr->setPerRowColor(row, i, rgba);
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }

        NSLog(@"[load_palette] Palette loaded (v3 RGBA %s, %s, global + %d rows): %@",
              fileModeName, compressionFlag ? "compressed" : "uncompressed", rowCount, path);
        return true;

    } else if (version == 4) {
        // Version 4: Complete universal format for all video modes
        if (length < 20) {
            NSLog(@"[load_palette] File too small for v4 header");
            return false;
        }

        // Read header fields
        uint8_t videoModeByte = data[6];
        uint8_t paletteModel = data[7];
        uint16_t rowCount = (data[8] << 8) | data[9];
        uint16_t colorsPerRow = (data[10] << 8) | data[11];
        uint16_t globalColorCount = (data[12] << 8) | data[13];
        uint8_t compressionFlag = data[14];
        uint32_t uncompressedSize = (data[15] << 24) | (data[16] << 16) |
                                     (data[17] << 8) | data[18];

        // Decode video mode
        const char* fileModeName = "Unknown";
        SuperTerminal::VideoMode fileMode = SuperTerminal::VideoMode::NONE;
        switch (videoModeByte) {
            case 1: fileModeName = "LORES"; fileMode = SuperTerminal::VideoMode::LORES; break;
            case 2: fileModeName = "XRES"; fileMode = SuperTerminal::VideoMode::XRES; break;
            case 3: fileModeName = "WRES"; fileMode = SuperTerminal::VideoMode::WRES; break;
            case 4: fileModeName = "URES"; fileMode = SuperTerminal::VideoMode::URES; break;
            case 5: fileModeName = "PRES"; fileMode = SuperTerminal::VideoMode::PRES; break;
            default:
                NSLog(@"[load_palette] Unknown video mode byte: %d", videoModeByte);
                return false;
        }

        // Validate video mode matches
        if (videoModeByte != currentModeCode && currentModeCode != 0) {
            NSLog(@"[load_palette] Warning: Palette is for %s but current mode is %s",
                  fileModeName, currentModeName);
        }

        // Validate palette model
        if (paletteModel > 3) {
            NSLog(@"[load_palette] Invalid palette model: %d", paletteModel);
            return false;
        }

        // Validate expected size
        size_t expectedSize = (globalColorCount * 4) + (rowCount * colorsPerRow * 4);
        if (uncompressedSize != expectedSize) {
            NSLog(@"[load_palette] Invalid uncompressed size: %u (expected %zu)",
                  uncompressedSize, expectedSize);
            return false;
        }

        // Get palette data
        NSData* compressedData = [NSData dataWithBytes:data + 20 length:length - 20];
        NSData* paletteData = nil;

        if (compressionFlag == 1) {
            // Decompress
            NSError* error = nil;
            if (@available(macOS 10.11, *)) {
                paletteData = [compressedData decompressedDataUsingAlgorithm:NSDataCompressionAlgorithmZlib
                                                                        error:&error];
            }

            if (!paletteData || error) {
                NSLog(@"[load_palette] Decompression failed: %@", error);
                return false;
            }

            if ([paletteData length] != uncompressedSize) {
                NSLog(@"[load_palette] Decompressed size mismatch: %zu != %u",
                      [paletteData length], uncompressedSize);
                return false;
            }
        } else {
            // Not compressed
            paletteData = compressedData;
        }

        const uint8_t* palette = (const uint8_t*)[paletteData bytes];
        size_t offset = 0;

        // Load global palette data (if any)
        for (int i = 0; i < globalColorCount; i++) {
            uint8_t r = palette[offset++];
            uint8_t g = palette[offset++];
            uint8_t b = palette[offset++];
            uint8_t a = palette[offset++];
            uint32_t rgba = (r << 24) | (g << 16) | (b << 8) | a;
            int colorIndex = i + 16;  // Global colors start at index 16

            switch (mode) {
                case SuperTerminal::VideoMode::XRES: {
                    auto paletteMgr = context.xresPalette();
                    if (paletteMgr) {
                        paletteMgr->setGlobalColor(colorIndex, rgba);
                    }
                    break;
                }
                case SuperTerminal::VideoMode::WRES: {
                    auto paletteMgr = context.wresPalette();
                    if (paletteMgr) {
                        paletteMgr->setGlobalColor(colorIndex, rgba);
                    }
                    break;
                }
                case SuperTerminal::VideoMode::PRES: {
                    auto paletteMgr = context.presPalette();
                    if (paletteMgr) {
                        paletteMgr->setGlobalColor(colorIndex, rgba);
                    }
                    break;
                }
                default:
                    break;
            }
        }

        // Load per-row palette data (if any)
        for (int row = 0; row < rowCount; row++) {
            for (int i = 0; i < colorsPerRow; i++) {
                uint8_t r = palette[offset++];
                uint8_t g = palette[offset++];
                uint8_t b = palette[offset++];
                uint8_t a = palette[offset++];
                uint32_t rgba = (r << 24) | (g << 16) | (b << 8) | a;

                switch (mode) {
                    case SuperTerminal::VideoMode::LORES: {
                        auto paletteMgr = context.loresPalette();
                        if (paletteMgr && row < 300 && i < 16) {
                            paletteMgr->setPaletteEntry(row, i, rgba);
                        }
                        break;
                    }
                    case SuperTerminal::VideoMode::XRES: {
                        auto paletteMgr = context.xresPalette();
                        if (paletteMgr && row < 240 && i < 16) {
                            paletteMgr->setPerRowColor(row, i, rgba);
                        }
                        break;
                    }
                    case SuperTerminal::VideoMode::WRES: {
                        auto paletteMgr = context.wresPalette();
                        if (paletteMgr && row < 240 && i < 16) {
                            paletteMgr->setPerRowColor(row, i, rgba);
                        }
                        break;
                    }
                    case SuperTerminal::VideoMode::PRES: {
                        auto paletteMgr = context.presPalette();
                        if (paletteMgr && row < 720 && i < 16) {
                            paletteMgr->setPerRowColor(row, i, rgba);
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }

        NSLog(@"[load_palette] Palette loaded (v4 %s, model %d, %d rows, %s): %@",
              fileModeName, paletteModel, rowCount,
              compressionFlag ? "compressed" : "uncompressed", path);
        return true;

    } else {
        NSLog(@"[load_palette] Unsupported version: %d", version);
        return false;
    }
}

} // anonymous namespace

// =============================================================================
// C API Implementation
// =============================================================================

ST_API bool st_video_load_image(const char* filePath, int bufferID,
                                 int destX, int destY, int maxWidth, int maxHeight) {
    ST_LOCK;

    auto display = STApi::Context::instance().display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return false;
    }

    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        ST_SET_ERROR("VideoModeManager not initialized");
        return false;
    }

    SuperTerminal::VideoMode currentMode = videoModeManager->getVideoMode();

    // Load image file with optional palette extraction
    std::vector<uint8_t> pixels;
    std::vector<uint8_t> extractedPalette;
    int imgWidth = 0, imgHeight = 0, bitsPerPixel = 0;

    if (!loadPNGFile(filePath, pixels, imgWidth, imgHeight, bitsPerPixel, nullptr)) {
        ST_SET_ERROR("Failed to load image file");
        return false;
    }

    // Determine actual copy dimensions
    int copyWidth = (maxWidth > 0) ? std::min(maxWidth, imgWidth) : imgWidth;
    int copyHeight = (maxHeight > 0) ? std::min(maxHeight, imgHeight) : imgHeight;

    // Load based on current video mode
    switch (currentMode) {
        case SuperTerminal::VideoMode::LORES: {
            if (bitsPerPixel != 8) {
                ST_SET_ERROR("LORES mode requires 8-bit indexed PNG");
                return false;
            }

            if (bufferID < 0 || bufferID >= 8) {
                ST_SET_ERROR("Invalid LORES buffer ID (must be 0-7)");
                return false;
            }

            auto buffer = display->getLoResBuffer(bufferID);
            if (!buffer) {
                ST_SET_ERROR("LORES buffer not available");
                return false;
            }

            int loResWidth, loResHeight;
            buffer->getSize(loResWidth, loResHeight);

            // Copy pixels to LORES buffer
            for (int y = 0; y < copyHeight; y++) {
                for (int x = 0; x < copyWidth; x++) {
                    if (destX + x >= 0 && destX + x < loResWidth &&
                        destY + y >= 0 && destY + y < loResHeight) {
                        uint8_t colorIndex = pixels[y * imgWidth + x];
                        // LORES stores 4-bit color + 4-bit alpha, use full alpha for now
                        buffer->setPixelAlpha(destX + x, destY + y, colorIndex, 0xF);
                    }
                }
            }

            return true;
        }

        case SuperTerminal::VideoMode::URES: {
            if (bitsPerPixel != 32 && bitsPerPixel != 16) {
                ST_SET_ERROR("URES mode requires 16-bit or 32-bit PNG");
                return false;
            }

            if (bufferID < 0 || bufferID >= 8) {
                ST_SET_ERROR("Invalid URES buffer ID (must be 0-7)");
                return false;
            }

            auto buffer = display->getUResBuffer(bufferID);
            if (!buffer) {
                ST_SET_ERROR("URES buffer not available");
                return false;
            }

            // Copy pixels to URES buffer
            for (int y = 0; y < copyHeight; y++) {
                for (int x = 0; x < copyWidth; x++) {
                    if (destX + x >= 0 && destX + x < 1280 &&
                        destY + y >= 0 && destY + y < 720) {
                        if (bitsPerPixel == 32) {
                            // RGBA image, convert to ARGB4444
                            size_t offset = (y * imgWidth + x) * 4;
                            uint8_t r = pixels[offset + 0];
                            uint8_t g = pixels[offset + 1];
                            uint8_t b = pixels[offset + 2];
                            uint8_t a = pixels[offset + 3];
                            // Convert 8-bit per channel to 4-bit per channel
                            uint16_t argb4444 = ((a >> 4) << 12) | ((r >> 4) << 8) |
                                               ((g >> 4) << 4) | (b >> 4);
                            buffer->setPixel(destX + x, destY + y, argb4444);
                        } else {
                            // Already 16-bit, assume ARGB4444
                            size_t offset = (y * imgWidth + x) * 2;
                            uint16_t color = (pixels[offset + 0] << 8) | pixels[offset + 1];
                            buffer->setPixel(destX + x, destY + y, color);
                        }
                    }
                }
            }

            return true;
        }

        case SuperTerminal::VideoMode::XRES: {
            if (bitsPerPixel != 8) {
                ST_SET_ERROR("XRES mode requires 8-bit indexed PNG");
                return false;
            }

            if (bufferID < 0 || bufferID >= 8) {
                ST_SET_ERROR("Invalid XRES buffer ID (must be 0-7)");
                return false;
            }

            auto buffer = display->getXResBuffer(bufferID);
            if (!buffer) {
                ST_SET_ERROR("XRES buffer not available");
                return false;
            }

            // Copy pixels to XRES buffer
            for (int y = 0; y < copyHeight; y++) {
                for (int x = 0; x < copyWidth; x++) {
                    if (destX + x >= 0 && destX + x < 320 &&
                        destY + y >= 0 && destY + y < 240) {
                        uint8_t colorIndex = pixels[y * imgWidth + x];
                        buffer->setPixel(destX + x, destY + y, colorIndex);
                    }
                }
            }

            return true;
        }

        case SuperTerminal::VideoMode::WRES: {
            if (bitsPerPixel != 8) {
                ST_SET_ERROR("WRES mode requires 8-bit indexed PNG");
                return false;
            }

            if (bufferID < 0 || bufferID >= 8) {
                ST_SET_ERROR("Invalid WRES buffer ID (must be 0-7)");
                return false;
            }

            auto buffer = display->getWResBuffer(bufferID);
            if (!buffer) {
                ST_SET_ERROR("WRES buffer not available");
                return false;
            }

            // Copy pixels to WRES buffer
            for (int y = 0; y < copyHeight; y++) {
                for (int x = 0; x < copyWidth; x++) {
                    if (destX + x >= 0 && destX + x < 432 &&
                        destY + y >= 0 && destY + y < 240) {
                        uint8_t colorIndex = pixels[y * imgWidth + x];
                        buffer->setPixel(destX + x, destY + y, colorIndex);
                    }
                }
            }

            return true;
        }

        case SuperTerminal::VideoMode::PRES: {
            if (bitsPerPixel != 8) {
                ST_SET_ERROR("PRES mode requires 8-bit indexed PNG");
                return false;
            }

            if (bufferID < 0 || bufferID >= 8) {
                ST_SET_ERROR("Invalid PRES buffer ID (must be 0-7)");
                return false;
            }

            auto buffer = display->getPResBuffer(bufferID);
            if (!buffer) {
                ST_SET_ERROR("PRES buffer not available");
                return false;
            }

            // Copy pixels to PRES buffer
            for (int y = 0; y < copyHeight; y++) {
                for (int x = 0; x < copyWidth; x++) {
                    if (destX + x >= 0 && destX + x < 1280 &&
                        destY + y >= 0 && destY + y < 720) {
                        uint8_t colorIndex = pixels[y * imgWidth + x];
                        buffer->setPixel(destX + x, destY + y, colorIndex);
                    }
                }
            }

            return true;
        }

        default:
            ST_SET_ERROR("Current video mode does not support load_image (use XRES, WRES, or PRES)");
            return false;
    }
}

ST_API bool st_video_save_palette_file(const char* filePath) {
    ST_LOCK;

    auto display = STApi::Context::instance().display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return false;
    }

    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        ST_SET_ERROR("VideoModeManager not initialized");
        return false;
    }

    SuperTerminal::VideoMode currentMode = videoModeManager->getVideoMode();

    // Check if current mode supports palettes
    if (currentMode != SuperTerminal::VideoMode::XRES &&
        currentMode != SuperTerminal::VideoMode::WRES &&
        currentMode != SuperTerminal::VideoMode::PRES) {
        ST_SET_ERROR("Current video mode does not support palettes (use XRES, WRES, or PRES)");
        return false;
    }

    return savePaletteFile(filePath, currentMode, STApi::Context::instance());
}

ST_API bool st_video_load_palette_file(const char* filePath) {
    ST_LOCK;

    auto display = STApi::Context::instance().display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return false;
    }

    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        ST_SET_ERROR("VideoModeManager not initialized");
        return false;
    }

    SuperTerminal::VideoMode currentMode = videoModeManager->getVideoMode();

    // Check if current mode supports palettes
    if (currentMode != SuperTerminal::VideoMode::XRES &&
        currentMode != SuperTerminal::VideoMode::WRES &&
        currentMode != SuperTerminal::VideoMode::PRES) {
        ST_SET_ERROR("Current video mode does not support palettes (use XRES, WRES, or PRES)");
        return false;
    }

    return loadPaletteFile(filePath, currentMode, STApi::Context::instance());
}

ST_API bool st_video_load_image(const char* filePath, int bufferID) {
    return st_video_load_image(filePath, bufferID, 0, 0, -1, -1);
}

ST_API bool st_video_save_image(const char* filePath, int bufferID) {
    ST_LOCK;

    auto display = STApi::Context::instance().display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return false;
    }

    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        ST_SET_ERROR("VideoModeManager not initialized");
        return false;
    }

    SuperTerminal::VideoMode currentMode = videoModeManager->getVideoMode();

    // Save based on current video mode
    switch (currentMode) {
        case SuperTerminal::VideoMode::LORES: {
            if (bufferID < 0 || bufferID >= 8) {
                ST_SET_ERROR("Invalid LORES buffer ID (must be 0-7)");
                return false;
            }

            auto buffer = display->getLoResBuffer(bufferID);
            if (!buffer) {
                ST_SET_ERROR("LORES buffer not available");
                return false;
            }

            // Get resolution
            int loResWidth, loResHeight;
            buffer->getSize(loResWidth, loResHeight);

            // Get pixel data (8-bit indexed, but LORES stores 4-bit color + 4-bit alpha)
            std::vector<uint8_t> pixels(loResWidth * loResHeight);
            for (int y = 0; y < loResHeight; y++) {
                for (int x = 0; x < loResWidth; x++) {
                    uint8_t colorIndex = buffer->getPixel(x, y);
                    uint8_t alpha = buffer->getPixelAlpha(x, y);
                    pixels[y * loResWidth + x] = colorIndex;
                    // Note: Alpha is lost in PNG, should be saved separately
                }
            }

            // Build palette data (16 colors × 3 bytes RGB from row 0)
            std::vector<uint8_t> paletteRGB(768);
            auto paletteMgr = STApi::Context::instance().loresPalette();
            if (paletteMgr) {
                // LORES has per-row palette, use row 0
                for (int i = 0; i < 16; i++) {
                    uint32_t rgba = paletteMgr->getPaletteEntry(0, i);
                    paletteRGB[i * 3 + 0] = (rgba >> 24) & 0xFF;  // R
                    paletteRGB[i * 3 + 1] = (rgba >> 16) & 0xFF;  // G
                    paletteRGB[i * 3 + 2] = (rgba >> 8) & 0xFF;   // B
                }
            }

            return savePNGFile(filePath, pixels.data(), loResWidth, loResHeight, true, paletteRGB.data());
        }

        case SuperTerminal::VideoMode::XRES: {
            if (bufferID < 0 || bufferID >= 8) {
                ST_SET_ERROR("Invalid XRES buffer ID (must be 0-7)");
                return false;
            }

            auto buffer = display->getXResBuffer(bufferID);
            if (!buffer) {
                ST_SET_ERROR("XRES buffer not available");
                return false;
            }

            // Get pixel data (320x240, 8-bit indexed)
            const uint8_t* pixels = buffer->getPixelData();

            // Build palette data (256 colors × 3 bytes RGB)
            std::vector<uint8_t> paletteRGB(768);
            auto paletteMgr = STApi::Context::instance().xresPalette();
            if (paletteMgr) {
                for (int i = 0; i < 256; i++) {
                    uint32_t rgba = paletteMgr->getGlobalColor(i);
                    paletteRGB[i * 3 + 0] = (rgba >> 24) & 0xFF;  // R
                    paletteRGB[i * 3 + 1] = (rgba >> 16) & 0xFF;  // G
                    paletteRGB[i * 3 + 2] = (rgba >> 8) & 0xFF;   // B
                }
            }

            return savePNGFile(filePath, pixels, 320, 240, true, paletteRGB.data());
        }

        case SuperTerminal::VideoMode::WRES: {
            if (bufferID < 0 || bufferID >= 8) {
                ST_SET_ERROR("Invalid WRES buffer ID (must be 0-7)");
                return false;
            }

            auto buffer = display->getWResBuffer(bufferID);
            if (!buffer) {
                ST_SET_ERROR("WRES buffer not available");
                return false;
            }

            // Get pixel data (640x240, 8-bit indexed)
            const uint8_t* pixels = buffer->getPixelData();

            // Build palette data (256 colors × 3 bytes RGB)
            std::vector<uint8_t> paletteRGB(768);
            auto paletteMgr = STApi::Context::instance().wresPalette();
            if (paletteMgr) {
                for (int i = 0; i < 256; i++) {
                    uint32_t rgba = paletteMgr->getGlobalColor(i);
                    paletteRGB[i * 3 + 0] = (rgba >> 24) & 0xFF;  // R
                    paletteRGB[i * 3 + 1] = (rgba >> 16) & 0xFF;  // G
                    paletteRGB[i * 3 + 2] = (rgba >> 8) & 0xFF;   // B
                }
            }

            return savePNGFile(filePath, pixels, 640, 240, true, paletteRGB.data());
        }

        case SuperTerminal::VideoMode::PRES: {
            if (bufferID < 0 || bufferID >= 8) {
                ST_SET_ERROR("Invalid PRES buffer ID (must be 0-7)");
                return false;
            }

            auto buffer = display->getPResBuffer(bufferID);
            if (!buffer) {
                ST_SET_ERROR("PRES buffer not available");
                return false;
            }

            // Get pixel data (640x480, 8-bit indexed)
            const uint8_t* pixels = buffer->getPixelData();

            // Build palette data (256 colors × 3 bytes RGB)
            // PRES uses 16 per-row colors (0-15) + 240 global colors (16-255)
            // For simplicity, we'll export using the global palette for all indices
            std::vector<uint8_t> paletteRGB(768);
            auto paletteMgr = STApi::Context::instance().presPalette();
            if (paletteMgr) {
                // Indices 0-15: use row 0's palette (or could average/use most common)
                for (int i = 0; i < 16; i++) {
                    uint32_t rgba = paletteMgr->getPerRowColor(0, i);
                    paletteRGB[i * 3 + 0] = (rgba >> 24) & 0xFF;  // R
                    paletteRGB[i * 3 + 1] = (rgba >> 16) & 0xFF;  // G
                    paletteRGB[i * 3 + 2] = (rgba >> 8) & 0xFF;   // B
                }
                // Indices 16-255: global palette
                for (int i = 16; i < 256; i++) {
                    uint32_t rgba = paletteMgr->getGlobalColor(i);
                    paletteRGB[i * 3 + 0] = (rgba >> 24) & 0xFF;  // R
                    paletteRGB[i * 3 + 1] = (rgba >> 16) & 0xFF;  // G
                    paletteRGB[i * 3 + 2] = (rgba >> 8) & 0xFF;   // B
                }
            }

            return savePNGFile(filePath, pixels, 640, 480, true, paletteRGB.data());
        }

        default:
            ST_SET_ERROR("Current video mode does not support save_image (use XRES, WRES, or PRES)");
            return false;
    }
}
// Need these includes - adding at end
