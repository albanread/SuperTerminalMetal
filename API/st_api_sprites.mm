//
// st_api_sprites.cpp
// SuperTerminal v2 - C API Sprite Implementation
//
// Sprite loading, display, and manipulation API functions
//

#include "superterminal_api.h"
#include "st_api_context.h"
#include "../Display/SpriteManager.h"
#import <CoreGraphics/CoreGraphics.h>
#import <Metal/Metal.h>

using namespace SuperTerminal;
using namespace STApi;

// =============================================================================
// Display API - Sprites
// =============================================================================

ST_API STSpriteID st_sprite_load(const char* path) {
    printf("st_sprite_load: Called with path=%s\n", path ? path : "NULL");

    if (!path) {
        ST_SET_ERROR("Sprite path is null");
        printf("st_sprite_load: Path is null, returning -1\n");
        return -1;
    }

    // Check if this is a .sprtz file
    const char* ext = strrchr(path, '.');
    if (ext && strcmp(ext, ".sprtz") == 0) {
        printf("st_sprite_load: Detected .sprtz extension, routing to loadSpriteFromSPRTZ\n");
        return st_sprite_load_sprtz(path);
    }

    printf("st_sprite_load: Acquiring ST_LOCK...\n");
    ST_LOCK;
    printf("st_sprite_load: ST_LOCK acquired\n");

    ST_CHECK_PTR_RET(ST_CONTEXT.sprites(), "SpriteManager", -1);
    printf("st_sprite_load: SpriteManager exists, calling loadSprite...\n");

    // Load sprite from file (PNG, JPG, etc.)
    uint16_t spriteId = ST_CONTEXT.sprites()->loadSprite(path);
    printf("st_sprite_load: loadSprite returned spriteId=%d\n", spriteId);

    if (spriteId == INVALID_SPRITE_ID) {
        ST_SET_ERROR("Failed to load sprite from file");
        printf("st_sprite_load: Invalid sprite ID, returning -1\n");
        return -1;
    }

    // Register the sprite and return a handle
    printf("st_sprite_load: Registering sprite...\n");
    int32_t handle = ST_CONTEXT.registerSprite(spriteId);
    printf("st_sprite_load: Success, returning handle=%d\n", handle);
    return handle;
}

ST_API STSpriteID st_sprite_load_builtin(const char* name) {
    if (!name) {
        ST_SET_ERROR("Sprite name is null");
        return -1;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.sprites(), "SpriteManager", -1);

    // For now, builtin sprites are not yet implemented
    // This would require an asset manager with embedded sprite data
    ST_SET_ERROR("Builtin sprites not yet implemented");
    return -1;
}

ST_API void st_sprite_show(STSpriteID sprite, int x, int y) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.sprites(), "SpriteManager");

    int spriteId = ST_CONTEXT.getSpriteId(sprite);
    if (spriteId < 0) {
        ST_SET_ERROR("Invalid sprite ID");
        return;
    }

    ST_CONTEXT.sprites()->showSprite(static_cast<uint16_t>(spriteId),
                                     static_cast<float>(x),
                                     static_cast<float>(y));
}

ST_API void st_sprite_hide(STSpriteID sprite) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.sprites(), "SpriteManager");

    int spriteId = ST_CONTEXT.getSpriteId(sprite);
    if (spriteId < 0) {
        ST_SET_ERROR("Invalid sprite ID");
        return;
    }

    ST_CONTEXT.sprites()->hideSprite(static_cast<uint16_t>(spriteId));
}

ST_API void st_sprite_transform(STSpriteID sprite, int x, int y,
                                float rotation, float scale_x, float scale_y) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.sprites(), "SpriteManager");

    int spriteId = ST_CONTEXT.getSpriteId(sprite);
    if (spriteId < 0) {
        ST_SET_ERROR("Invalid sprite ID");
        return;
    }

    uint16_t sid = static_cast<uint16_t>(spriteId);

    // Set position
    ST_CONTEXT.sprites()->moveSprite(sid, static_cast<float>(x), static_cast<float>(y));

    // Set scale
    ST_CONTEXT.sprites()->setScale(sid, scale_x, scale_y);

    // Set rotation
    ST_CONTEXT.sprites()->setRotation(sid, rotation);
}

ST_API void st_sprite_tint(STSpriteID sprite, STColor color) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.sprites(), "SpriteManager");

    int spriteId = ST_CONTEXT.getSpriteId(sprite);
    if (spriteId < 0) {
        ST_SET_ERROR("Invalid sprite ID");
        return;
    }

    // Extract RGBA components
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;

    ST_CONTEXT.sprites()->setTint(
        static_cast<uint16_t>(spriteId),
        r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f
    );
}

ST_API void st_sprite_unload(STSpriteID sprite) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.sprites(), "SpriteManager");

    int spriteId = ST_CONTEXT.getSpriteId(sprite);
    if (spriteId < 0) {
        ST_SET_ERROR("Invalid sprite ID");
        return;
    }

    // Unload the sprite from the sprite manager
    ST_CONTEXT.sprites()->unloadSprite(static_cast<uint16_t>(spriteId));

    // Unregister the handle
    ST_CONTEXT.unregisterSprite(sprite);
}

ST_API void st_sprite_unload_all() {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.sprites(), "SpriteManager");

    // Clear all sprites
    ST_CONTEXT.sprites()->clearAll();
}

// =============================================================================
// Sprite Drawing API - DrawIntoSprite / EndDrawIntoSprite
// =============================================================================

ST_API STSpriteID st_sprite_begin_draw(int width, int height) {
    NSLog(@"st_sprite_begin_draw: Called with width=%d, height=%d", width, height);

    if (width <= 0 || height <= 0) {
        ST_SET_ERROR("Invalid sprite dimensions");
        NSLog(@"st_sprite_begin_draw: Invalid dimensions, returning -1");
        return -1;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.sprites(), "SpriteManager", -1);

    // Check if already drawing into a sprite or file
    if (ST_CONTEXT.isDrawingIntoSprite()) {
        ST_SET_ERROR("Already drawing into a sprite - call EndDrawIntoSprite first");
        NSLog(@"st_sprite_begin_draw: Already drawing into sprite, returning -1");
        return -1;
    }

    if (ST_CONTEXT.isDrawingToFile()) {
        ST_SET_ERROR("Already drawing to file - call EndDrawToFile first");
        NSLog(@"st_sprite_begin_draw: Already drawing to file, returning -1");
        return -1;
    }

    // Get next available sprite ID
    uint16_t spriteId = ST_CONTEXT.sprites()->getNextAvailableId();
    if (spriteId == INVALID_SPRITE_ID) {
        ST_SET_ERROR("No available sprite IDs");
        NSLog(@"st_sprite_begin_draw: No sprite IDs available, returning -1");
        return -1;
    }

    NSLog(@"st_sprite_begin_draw: Allocated sprite ID %d", spriteId);

    // Allocate bitmap data (RGBA, 4 bytes per pixel)
    size_t bytesPerRow = width * 4;
    size_t dataSize = bytesPerRow * height;
    void* bitmapData = calloc(dataSize, 1);

    if (!bitmapData) {
        ST_SET_ERROR("Failed to allocate bitmap memory");
        NSLog(@"st_sprite_begin_draw: Memory allocation failed, returning -1");
        return -1;
    }

    NSLog(@"st_sprite_begin_draw: Allocated %zu bytes for %dx%d bitmap", dataSize, width, height);

    // Create bitmap context (RGBA format for simplicity)
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        bitmapData,
        width,
        height,
        8,
        bytesPerRow,
        colorSpace,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big  // RGBA format
    );
    CGColorSpaceRelease(colorSpace);

    if (!context) {
        free(bitmapData);
        ST_SET_ERROR("Failed to create CGContext");
        NSLog(@"st_sprite_begin_draw: CGContext creation failed, returning -1");
        return -1;
    }

    NSLog(@"st_sprite_begin_draw: CGContext created successfully");

    // Clear to transparent black
    CGContextClearRect(context, CGRectMake(0, 0, width, height));

    // Set default rendering properties
    CGContextSetShouldAntialias(context, true);
    CGContextSetAllowsAntialiasing(context, true);
    CGContextSetInterpolationQuality(context, kCGInterpolationHigh);

    // Flip coordinate system to match SuperTerminal coordinates (origin at top-left)
    CGContextTranslateCTM(context, 0, height);
    CGContextScaleCTM(context, 1.0, -1.0);

    NSLog(@"st_sprite_begin_draw: CGContext configured, coordinate system flipped");

    // Store the drawing context in the API context
    ST_CONTEXT.beginSpriteDrawing(spriteId, width, height, context, bitmapData);

    NSLog(@"st_sprite_begin_draw: Success, sprite context ready for drawing");

    // Register and return handle
    int32_t handle = ST_CONTEXT.registerSprite(spriteId);
    NSLog(@"st_sprite_begin_draw: Returning handle=%d", handle);
    return handle;
}

ST_API void st_sprite_end_draw() {
    NSLog(@"st_sprite_end_draw: Called");

    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.sprites(), "SpriteManager");

    // Check if we're currently drawing into a sprite
    if (!ST_CONTEXT.isDrawingIntoSprite()) {
        ST_SET_ERROR("Not currently drawing into a sprite");
        NSLog(@"st_sprite_end_draw: Not drawing into sprite, returning");
        return;
    }

    // Get the drawing context info
    CGContextRef context = ST_CONTEXT.getSpriteDrawContext();
    void* bitmapData = ST_CONTEXT.getSpriteDrawBitmapData();
    int width = ST_CONTEXT.getSpriteDrawWidth();
    int height = ST_CONTEXT.getSpriteDrawHeight();
    uint16_t spriteId = ST_CONTEXT.getSpriteDrawId();

    NSLog(@"st_sprite_end_draw: Finalizing sprite ID %d (%dx%d)", spriteId, width, height);

    // Flush the context to ensure all drawing is complete
    CGContextFlush(context);

    // Get the pixel data (RGBA format)
    const uint8_t* pixels = (const uint8_t*)bitmapData;

    // Sample first few pixels for debugging
    const uint32_t* pixelData = (const uint32_t*)pixels;
    NSLog(@"st_sprite_end_draw: First 4 pixels: 0x%08X 0x%08X 0x%08X 0x%08X",
           pixelData[0], pixelData[1], pixelData[2], pixelData[3]);

    // Set texture for the existing sprite ID (don't allocate new sprite ID)
    NSLog(@"st_sprite_end_draw: Setting texture for existing sprite ID %d", spriteId);
    bool success = ST_CONTEXT.sprites()->setSpriteTexture(spriteId, pixels, width, height);

    if (!success) {
        ST_SET_ERROR("Failed to set sprite texture");
        NSLog(@"st_sprite_end_draw: Failed to set sprite texture for ID %d", spriteId);
    } else {
        NSLog(@"st_sprite_end_draw: Sprite texture set successfully for ID %d", spriteId);
    }

    // Clean up context and bitmap data
    CGContextRelease(context);
    free(bitmapData);

    // Clear the drawing state
    ST_CONTEXT.endSpriteDrawing();

    NSLog(@"st_sprite_end_draw: Complete");
}

// =============================================================================
// Indexed Sprite API
// =============================================================================

ST_API STSpriteID st_sprite_load_indexed(const uint8_t* indices, int width, int height,
                                         const uint8_t* palette) {
    if (!indices || !palette) {
        ST_SET_ERROR("Indices or palette is null");
        return -1;
    }

    if (width <= 0 || height <= 0) {
        ST_SET_ERROR("Invalid sprite dimensions");
        return -1;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.sprites(), "SpriteManager", -1);

    // Load indexed sprite
    uint16_t spriteId = ST_CONTEXT.sprites()->loadSpriteIndexed(indices, width, height, palette);

    if (spriteId == INVALID_SPRITE_ID) {
        ST_SET_ERROR("Failed to load indexed sprite");
        return -1;
    }

    // Register the sprite and return a handle
    int32_t handle = ST_CONTEXT.registerSprite(spriteId);
    return handle;
}

ST_API STSpriteID st_sprite_load_indexed_from_rgba(const uint8_t* pixels, int width, int height,
                                                    uint8_t* out_palette) {
    if (!pixels) {
        ST_SET_ERROR("Pixel data is null");
        return -1;
    }

    if (width <= 0 || height <= 0) {
        ST_SET_ERROR("Invalid sprite dimensions");
        return -1;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.sprites(), "SpriteManager", -1);

    // Load indexed sprite with automatic quantization
    uint16_t spriteId = ST_CONTEXT.sprites()->loadSpriteIndexedFromRGBA(pixels, width, height, out_palette);

    if (spriteId == INVALID_SPRITE_ID) {
        ST_SET_ERROR("Failed to load indexed sprite from RGBA");
        return -1;
    }

    // Register the sprite and return a handle
    int32_t handle = ST_CONTEXT.registerSprite(spriteId);
    return handle;
}

ST_API STSpriteID st_sprite_load_sprtz(const char* path) {
    if (!path) {
        ST_SET_ERROR("SPRTZ path is null");
        return -1;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.sprites(), "SpriteManager", -1);

    // Load sprite from SPRTZ file
    uint16_t spriteId = ST_CONTEXT.sprites()->loadSpriteFromSPRTZ(path);

    if (spriteId == INVALID_SPRITE_ID) {
        ST_SET_ERROR("Failed to load sprite from SPRTZ file");
        return -1;
    }

    // Register the sprite and return a handle
    int32_t handle = ST_CONTEXT.registerSprite(spriteId);
    return handle;
}

ST_API bool st_sprite_is_indexed(STSpriteID sprite) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.sprites(), "SpriteManager", false);

    int spriteId = ST_CONTEXT.getSpriteId(sprite);
    if (spriteId < 0) {
        ST_SET_ERROR("Invalid sprite ID");
        return false;
    }

    return ST_CONTEXT.sprites()->isSpriteIndexed(static_cast<uint16_t>(spriteId));
}

ST_API bool st_sprite_set_palette(STSpriteID sprite, const uint8_t* palette) {
    if (!palette) {
        ST_SET_ERROR("Palette is null");
        return false;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.sprites(), "SpriteManager", false);

    int spriteId = ST_CONTEXT.getSpriteId(sprite);
    if (spriteId < 0) {
        ST_SET_ERROR("Invalid sprite ID");
        return false;
    }

    return ST_CONTEXT.sprites()->setSpritePalette(static_cast<uint16_t>(spriteId), palette);
}

ST_API bool st_sprite_get_palette(STSpriteID sprite, uint8_t* out_palette) {
    if (!out_palette) {
        ST_SET_ERROR("Output palette buffer is null");
        return false;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.sprites(), "SpriteManager", false);

    int spriteId = ST_CONTEXT.getSpriteId(sprite);
    if (spriteId < 0) {
        ST_SET_ERROR("Invalid sprite ID");
        return false;
    }

    return ST_CONTEXT.sprites()->getSpritePalette(static_cast<uint16_t>(spriteId), out_palette);
}

ST_API bool st_sprite_set_standard_palette(STSpriteID sprite, uint8_t standard_palette_id) {
    if (standard_palette_id >= 32) {
        ST_SET_ERROR("Invalid standard palette ID (must be 0-31)");
        return false;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.sprites(), "SpriteManager", false);

    int spriteId = ST_CONTEXT.getSpriteId(sprite);
    if (spriteId < 0) {
        ST_SET_ERROR("Invalid sprite ID");
        return false;
    }

    return ST_CONTEXT.sprites()->setSpriteStandardPalette(static_cast<uint16_t>(spriteId), standard_palette_id);
}

ST_API bool st_sprite_set_palette_color(STSpriteID sprite, int color_index,
                                        uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.sprites(), "SpriteManager", false);

    int spriteId = ST_CONTEXT.getSpriteId(sprite);
    if (spriteId < 0) {
        ST_SET_ERROR("Invalid sprite ID");
        return false;
    }

    return ST_CONTEXT.sprites()->setSpritePaletteColor(static_cast<uint16_t>(spriteId),
                                                       static_cast<uint8_t>(color_index),
                                                       r, g, b, a);
}

ST_API bool st_sprite_lerp_palette(STSpriteID sprite, const uint8_t* palette_a,
                                   const uint8_t* palette_b, float t) {
    if (!palette_a || !palette_b) {
        ST_SET_ERROR("Palette is null");
        return false;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.sprites(), "SpriteManager", false);

    int spriteId = ST_CONTEXT.getSpriteId(sprite);
    if (spriteId < 0) {
        ST_SET_ERROR("Invalid sprite ID");
        return false;
    }

    return ST_CONTEXT.sprites()->lerpSpritePalette(static_cast<uint16_t>(spriteId),
                                                   palette_a, palette_b, t);
}

ST_API bool st_sprite_rotate_palette(STSpriteID sprite, int start_index, int end_index, int amount) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.sprites(), "SpriteManager", false);

    int spriteId = ST_CONTEXT.getSpriteId(sprite);
    if (spriteId < 0) {
        ST_SET_ERROR("Invalid sprite ID");
        return false;
    }

    return ST_CONTEXT.sprites()->rotateSpritePalette(static_cast<uint16_t>(spriteId),
                                                     start_index, end_index, amount);
}

ST_API bool st_sprite_adjust_brightness(STSpriteID sprite, float brightness) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.sprites(), "SpriteManager", false);

    int spriteId = ST_CONTEXT.getSpriteId(sprite);
    if (spriteId < 0) {
        ST_SET_ERROR("Invalid sprite ID");
        return false;
    }

    return ST_CONTEXT.sprites()->adjustSpritePaletteBrightness(static_cast<uint16_t>(spriteId),
                                                                brightness);
}

ST_API bool st_sprite_copy_palette(STSpriteID src_sprite, STSpriteID dst_sprite) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.sprites(), "SpriteManager", false);

    int srcSpriteId = ST_CONTEXT.getSpriteId(src_sprite);
    int dstSpriteId = ST_CONTEXT.getSpriteId(dst_sprite);

    if (srcSpriteId < 0 || dstSpriteId < 0) {
        ST_SET_ERROR("Invalid sprite ID");
        return false;
    }

    return ST_CONTEXT.sprites()->copySpritePalette(static_cast<uint16_t>(srcSpriteId),
                                                   static_cast<uint16_t>(dstSpriteId));
}
