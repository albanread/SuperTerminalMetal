// =============================================================================
// st_api_tileset_drawing.mm
// SuperTerminal API - Tileset Drawing Implementation
//
// Implements DrawToTileset/DrawTile/EndDrawToTileset for procedural tileset generation
// =============================================================================

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Metal/Metal.h>
#include "superterminal_api.h"
#include "st_api_context.h"
#include "../Tilemap/TilemapManager.h"
#include "../Tilemap/Tileset.h"
#include "../Display/DisplayManager.h"
#include "../Metal/MetalRenderer.h"

using namespace STApi;
using namespace SuperTerminal;

// =============================================================================
// Tileset Drawing API Implementation
// =============================================================================

ST_API STTilesetID st_tileset_begin_draw(int tileWidth, int tileHeight, int columns, int rows) {
    NSLog(@"st_tileset_begin_draw: Called with tileWidth=%d, tileHeight=%d, columns=%d, rows=%d",
          tileWidth, tileHeight, columns, rows);

    if (tileWidth <= 0 || tileHeight <= 0) {
        ST_SET_ERROR("Invalid tile dimensions");
        NSLog(@"st_tileset_begin_draw: Invalid tile dimensions");
        return -1;
    }

    if (columns <= 0 || rows <= 0) {
        ST_SET_ERROR("Invalid tileset grid dimensions");
        NSLog(@"st_tileset_begin_draw: Invalid grid dimensions");
        return -1;
    }

    ST_LOCK;

    // Check if already drawing
    if (ST_CONTEXT.isDrawingIntoSprite()) {
        ST_SET_ERROR("Already drawing into a sprite - call EndDrawIntoSprite first");
        NSLog(@"st_tileset_begin_draw: Already drawing into sprite");
        return -1;
    }

    if (ST_CONTEXT.isDrawingToFile()) {
        ST_SET_ERROR("Already drawing to file - call EndDrawToFile first");
        NSLog(@"st_tileset_begin_draw: Already drawing to file");
        return -1;
    }

    if (ST_CONTEXT.isDrawingToTileset()) {
        ST_SET_ERROR("Already drawing to tileset - call EndDrawToTileset first");
        NSLog(@"st_tileset_begin_draw: Already drawing to tileset");
        return -1;
    }

    // Get TilemapManager
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR_RET(mgr, "TilemapManager", -1);

    if (!mgr->isInitialized()) {
        ST_SET_ERROR("TilemapManager not initialized");
        NSLog(@"st_tileset_begin_draw: TilemapManager not initialized");
        return -1;
    }

    // Create tileset ID
    int32_t tilesetID = mgr->createTileset("procedural_tileset");
    if (tilesetID < 0) {
        ST_SET_ERROR("Failed to create tileset");
        NSLog(@"st_tileset_begin_draw: Failed to create tileset");
        return -1;
    }

    NSLog(@"st_tileset_begin_draw: Created tileset ID %d", tilesetID);

    // Calculate atlas dimensions
    int atlasWidth = tileWidth * columns;
    int atlasHeight = tileHeight * rows;

    NSLog(@"st_tileset_begin_draw: Atlas dimensions: %dx%d (%d tiles)",
          atlasWidth, atlasHeight, columns * rows);

    // Allocate bitmap data (RGBA, 4 bytes per pixel)
    size_t bytesPerRow = atlasWidth * 4;
    size_t dataSize = bytesPerRow * atlasHeight;
    void* bitmapData = calloc(dataSize, 1);

    if (!bitmapData) {
        ST_SET_ERROR("Failed to allocate bitmap memory");
        NSLog(@"st_tileset_begin_draw: Memory allocation failed");
        return -1;
    }

    NSLog(@"st_tileset_begin_draw: Allocated %zu bytes for %dx%d atlas", dataSize, atlasWidth, atlasHeight);

    // Create CGColorSpace
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

    // Create CGContext for drawing
    CGContextRef context = CGBitmapContextCreate(
        bitmapData,
        atlasWidth,
        atlasHeight,
        8,  // bits per component
        bytesPerRow,
        colorSpace,
        kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast  // RGBA
    );

    CGColorSpaceRelease(colorSpace);

    if (!context) {
        free(bitmapData);
        ST_SET_ERROR("Failed to create CGContext");
        NSLog(@"st_tileset_begin_draw: CGContext creation failed");
        return -1;
    }

    NSLog(@"st_tileset_begin_draw: CGContext created successfully");

    // Clear to transparent black
    CGContextClearRect(context, CGRectMake(0, 0, atlasWidth, atlasHeight));

    // Set initial drawing state
    CGContextSetShouldAntialias(context, true);
    CGContextSetAllowsAntialiasing(context, true);
    CGContextSetInterpolationQuality(context, kCGInterpolationHigh);

    // Flip coordinate system to match SuperTerminal coordinates (origin top-left)
    CGContextTranslateCTM(context, 0, atlasHeight);
    CGContextScaleCTM(context, 1.0, -1.0);

    NSLog(@"st_tileset_begin_draw: CGContext configured, coordinate system flipped");

    // Store state in context
    ST_CONTEXT.beginTilesetDrawing(tilesetID, tileWidth, tileHeight, columns, rows,
                                    atlasWidth, atlasHeight, context, bitmapData);

    NSLog(@"st_tileset_begin_draw: Success, tileset context ready for drawing");
    return tilesetID;
}

ST_API bool st_tileset_draw_tile(int tileIndex) {
    NSLog(@"st_tileset_draw_tile: Called with tileIndex=%d", tileIndex);

    ST_LOCK;

    if (!ST_CONTEXT.isDrawingToTileset()) {
        ST_SET_ERROR("Not currently drawing to tileset");
        NSLog(@"st_tileset_draw_tile: Not drawing to tileset");
        return false;
    }

    int columns = ST_CONTEXT.getTilesetDrawColumns();
    int rows = ST_CONTEXT.getTilesetDrawRows();
    int totalTiles = columns * rows;

    if (tileIndex < 0 || tileIndex >= totalTiles) {
        ST_SET_ERROR("Tile index out of range");
        NSLog(@"st_tileset_draw_tile: Tile index %d out of range (0-%d)", tileIndex, totalTiles - 1);
        return false;
    }

    // Calculate tile position in atlas
    int tileCol = tileIndex % columns;
    int tileRow = tileIndex / columns;
    int tileWidth = ST_CONTEXT.getTilesetDrawTileWidth();
    int tileHeight = ST_CONTEXT.getTilesetDrawTileHeight();
    int tileX = tileCol * tileWidth;
    int tileY = tileRow * tileHeight;

    NSLog(@"st_tileset_draw_tile: Tile %d at grid position (%d, %d), pixel position (%d, %d)",
          tileIndex, tileCol, tileRow, tileX, tileY);

    // Get context
    CGContextRef context = ST_CONTEXT.getTilesetDrawContext();
    if (!context) {
        ST_SET_ERROR("Tileset context is null");
        NSLog(@"st_tileset_draw_tile: Context is null");
        return false;
    }

    // Restore any previous tile transform
    CGContextRestoreGState(context);

    // Save state before applying new transform
    CGContextSaveGState(context);

    // Set clipping region to this tile (prevents drawing outside tile bounds)
    CGRect tileRect = CGRectMake(tileX, tileY, tileWidth, tileHeight);
    CGContextClipToRect(context, tileRect);

    // Translate origin to top-left of this tile
    // This makes (0,0) the top-left corner of the current tile
    CGContextTranslateCTM(context, tileX, tileY);

    NSLog(@"st_tileset_draw_tile: Transform applied - drawing coordinates (0,0) to (%d,%d) map to tile %d",
          tileWidth, tileHeight, tileIndex);

    // Store current tile index
    ST_CONTEXT.setTilesetDrawCurrentTile(tileIndex);

    return true;
}

ST_API bool st_tileset_end_draw() {
    NSLog(@"st_tileset_end_draw: Called");

    ST_LOCK;

    if (!ST_CONTEXT.isDrawingToTileset()) {
        ST_SET_ERROR("Not currently drawing to tileset");
        NSLog(@"st_tileset_end_draw: Not drawing to tileset");
        return false;
    }

    // Get drawing state
    CGContextRef context = ST_CONTEXT.getTilesetDrawContext();
    void* pixels = ST_CONTEXT.getTilesetDrawBitmapData();
    int atlasWidth = ST_CONTEXT.getTilesetDrawWidth();
    int atlasHeight = ST_CONTEXT.getTilesetDrawHeight();
    int tileWidth = ST_CONTEXT.getTilesetDrawTileWidth();
    int tileHeight = ST_CONTEXT.getTilesetDrawTileHeight();
    int tilesetId = ST_CONTEXT.getTilesetDrawId();

    NSLog(@"st_tileset_end_draw: Finalizing tileset ID %d (atlas %dx%d, tile %dx%d)",
          tilesetId, atlasWidth, atlasHeight, tileWidth, tileHeight);

    // Restore any tile transform
    CGContextRestoreGState(context);

    // Flush the context to ensure all drawing is complete
    CGContextFlush(context);

    // Sample first few pixels for debugging
    const uint32_t* pixelData = (const uint32_t*)pixels;
    NSLog(@"st_tileset_end_draw: First 4 pixels: 0x%08X 0x%08X 0x%08X 0x%08X",
           pixelData[0], pixelData[1], pixelData[2], pixelData[3]);

    // Get Metal device
    id<MTLDevice> device = nil;
    DisplayManager* displayMgr = ST_CONTEXT.display();
    if (displayMgr) {
        auto renderer = displayMgr->getRenderer();
        if (renderer) {
            device = renderer->getDevice();
        }
    }

    if (!device) {
        ST_SET_ERROR("Metal device not available");
        NSLog(@"st_tileset_end_draw: Metal device not available");
        CGContextRelease(context);
        free(pixels);
        ST_CONTEXT.endTilesetDrawing();
        return false;
    }

    // Create Metal texture from bitmap
    MTLTextureDescriptor* desc = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                     width:atlasWidth
                                    height:atlasHeight
                                 mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;

    id<MTLTexture> texture = [device newTextureWithDescriptor:desc];
    if (!texture) {
        ST_SET_ERROR("Failed to create Metal texture");
        NSLog(@"st_tileset_end_draw: Failed to create Metal texture");
        CGContextRelease(context);
        free(pixels);
        ST_CONTEXT.endTilesetDrawing();
        return false;
    }

    // Upload pixel data to texture
    MTLRegion region = MTLRegionMake2D(0, 0, atlasWidth, atlasHeight);
    NSUInteger bytesPerRow = atlasWidth * 4;
    [texture replaceRegion:region mipmapLevel:0 withBytes:pixels bytesPerRow:bytesPerRow];

    NSLog(@"st_tileset_end_draw: Texture created and uploaded successfully");

    // Get tileset and initialize it
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    if (!mgr) {
        ST_SET_ERROR("TilemapManager not available");
        NSLog(@"st_tileset_end_draw: TilemapManager not available");
        CGContextRelease(context);
        free(pixels);
        ST_CONTEXT.endTilesetDrawing();
        return false;
    }

    auto tileset = mgr->getTileset(tilesetId);
    if (!tileset) {
        ST_SET_ERROR("Failed to get tileset");
        NSLog(@"st_tileset_end_draw: Failed to get tileset");
        CGContextRelease(context);
        free(pixels);
        ST_CONTEXT.endTilesetDrawing();
        return false;
    }

    // Initialize tileset with texture
    tileset->initialize(texture, tileWidth, tileHeight, "procedural_tileset");
    tileset->setMargin(0);
    tileset->setSpacing(0);
    tileset->recalculateLayout();

    NSLog(@"st_tileset_end_draw: Tileset initialized with %d tiles", tileset->getTileCount());

    // Clean up context and bitmap
    CGContextRelease(context);
    free(pixels);

    // Clear state
    ST_CONTEXT.endTilesetDrawing();

    NSLog(@"st_tileset_end_draw: Complete (success)");
    return true;
}
