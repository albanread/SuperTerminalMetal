//
// st_api_tilemap.cpp
// SuperTerminal v2 - C API Tilemap Implementation
//
// Tilemap system API functions
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "superterminal_api.h"
#include "st_api_context.h"
#include "../Tilemap/TilemapManager.h"
#include "../Tilemap/Tilemap.h"
#include "../Tilemap/Tileset.h"
#include "../Tilemap/TilemapLayer.h"
#include "../Tilemap/Camera.h"
#include "../Assets/AssetManager.h"
#include "../Display/DisplayManager.h"
#include "../Metal/MetalRenderer.h"

#ifdef __OBJC__
#import <Metal/Metal.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#endif

using namespace STApi;
using namespace SuperTerminal;

// =============================================================================
// Helper Functions
// =============================================================================

#ifdef __OBJC__
// Load texture from image file
static id<MTLTexture> loadTextureFromFile(const char* filePath, id<MTLDevice> device) {
    if (!filePath || !device) {
        return nil;
    }
    
    @autoreleasepool {
        NSString* path = [NSString stringWithUTF8String:filePath];
        NSURL* url = [NSURL fileURLWithPath:path];
        
        CGImageSourceRef source = CGImageSourceCreateWithURL((__bridge CFURLRef)url, NULL);
        if (!source) {
            NSLog(@"[Tileset] Failed to create image source from: %@", path);
            return nil;
        }
        
        CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, NULL);
        CFRelease(source);
        
        if (!image) {
            NSLog(@"[Tileset] Failed to create image from source");
            return nil;
        }
        
        size_t width = CGImageGetWidth(image);
        size_t height = CGImageGetHeight(image);
        
        // Create texture descriptor
        MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                              width:width
                                                                                             height:height
                                                                                          mipmapped:NO];
        descriptor.usage = MTLTextureUsageShaderRead;
        
        id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
        if (!texture) {
            CGImageRelease(image);
            NSLog(@"[Tileset] Failed to create Metal texture");
            return nil;
        }
        
        // Create bitmap context with BGRA format to match Metal texture
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(NULL, width, height, 8, width * 4, colorSpace,
                                                     kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
        CGColorSpaceRelease(colorSpace);
        
        if (!context) {
            CGImageRelease(image);
            NSLog(@"[Tileset] Failed to create bitmap context");
            return nil;
        }
        
        // Draw image into context
        CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
        CGImageRelease(image);
        
        // Copy pixel data to texture
        void* pixelData = CGBitmapContextGetData(context);
        [texture replaceRegion:MTLRegionMake2D(0, 0, width, height)
                   mipmapLevel:0
                     withBytes:pixelData
                   bytesPerRow:width * 4];
        
        CGContextRelease(context);
        
        NSLog(@"[Tileset] Loaded texture from file: %@ (%zux%zu)", path, width, height);
        return texture;
    }
}

// Load texture from asset data
static id<MTLTexture> loadTextureFromAsset(const char* assetName, id<MTLDevice> device) {
    if (!assetName || !device) {
        return nil;
    }
    
    AssetManager* assetMgr = ST_CONTEXT.assets();
    if (!assetMgr) {
        NSLog(@"[Tileset] AssetManager not available");
        return nil;
    }
    
    // Load asset
    AssetHandle handle = assetMgr->loadAsset(assetName);
    if (handle == INVALID_ASSET_HANDLE) {
        NSLog(@"[Tileset] Failed to load asset: %s", assetName);
        return nil;
    }
    
    const AssetMetadata* metadata = assetMgr->getAssetMetadata(handle);
    if (!metadata || metadata->data.empty()) {
        assetMgr->unloadAsset(handle);
        NSLog(@"[Tileset] Asset has no data: %s", assetName);
        return nil;
    }
    
    @autoreleasepool {
        // Create data object from asset data
        NSData* data = [NSData dataWithBytes:metadata->data.data() length:metadata->data.size()];
        
        CGImageSourceRef source = CGImageSourceCreateWithData((__bridge CFDataRef)data, NULL);
        if (!source) {
            assetMgr->unloadAsset(handle);
            NSLog(@"[Tileset] Failed to create image source from asset: %s", assetName);
            return nil;
        }
        
        CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, NULL);
        CFRelease(source);
        
        if (!image) {
            assetMgr->unloadAsset(handle);
            NSLog(@"[Tileset] Failed to create image from asset");
            return nil;
        }
        
        size_t width = CGImageGetWidth(image);
        size_t height = CGImageGetHeight(image);
        
        // Create texture descriptor
        MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                              width:width
                                                                                             height:height
                                                                                          mipmapped:NO];
        descriptor.usage = MTLTextureUsageShaderRead;
        
        id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
        if (!texture) {
            CGImageRelease(image);
            assetMgr->unloadAsset(handle);
            NSLog(@"[Tileset] Failed to create Metal texture");
            return nil;
        }
        
        // Create bitmap context with BGRA format to match Metal texture
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(NULL, width, height, 8, width * 4, colorSpace,
                                                     kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
        CGColorSpaceRelease(colorSpace);
        
        if (!context) {
            CGImageRelease(image);
            assetMgr->unloadAsset(handle);
            NSLog(@"[Tileset] Failed to create bitmap context");
            return nil;
        }
        
        // Draw image into context
        CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
        CGImageRelease(image);
        
        // Copy pixel data to texture
        void* pixelData = CGBitmapContextGetData(context);
        [texture replaceRegion:MTLRegionMake2D(0, 0, width, height)
                   mipmapLevel:0
                     withBytes:pixelData
                   bytesPerRow:width * 4];
        
        CGContextRelease(context);
        assetMgr->unloadAsset(handle);
        
        NSLog(@"[Tileset] Loaded texture from asset: %s (%zux%zu)", assetName, width, height);
        return texture;
    }
}
#endif

// =============================================================================
// Tileset Management
// =============================================================================

ST_API STTilesetID st_tileset_load(const char* imagePath, 
                                    int32_t tileWidth, int32_t tileHeight,
                                    int32_t margin, int32_t spacing)
{
#ifdef __OBJC__
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR_RET(mgr, "TilemapManager", -1);
    
    if (!mgr->isInitialized()) {
        ST_SET_ERROR("TilemapManager not initialized");
        return -1;
    }
    
    if (!imagePath) {
        ST_SET_ERROR("Invalid image path");
        return -1;
    }
    
    if (tileWidth <= 0 || tileHeight <= 0) {
        ST_SET_ERROR("Invalid tile dimensions");
        return -1;
    }
    
    // Get Metal device from renderer
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
        return -1;
    }
    
    // Load texture from file
    id<MTLTexture> texture = loadTextureFromFile(imagePath, device);
    if (!texture) {
        ST_SET_ERROR("Failed to load texture from file");
        return -1;
    }
    
    // Create tileset
    int32_t tilesetID = mgr->createTileset(imagePath);
    if (tilesetID < 0) {
        ST_SET_ERROR("Failed to create tileset");
        return -1;
    }
    
    auto tileset = mgr->getTileset(tilesetID);
    if (!tileset) {
        ST_SET_ERROR("Failed to get tileset");
        return -1;
    }
    
    // Initialize tileset with texture
    tileset->initialize(texture, tileWidth, tileHeight, imagePath);
    tileset->setMargin(margin);
    tileset->setSpacing(spacing);
    tileset->recalculateLayout();
    
    ST_CLEAR_ERROR();
    return tilesetID;
#else
    (void)imagePath;
    (void)tileWidth;
    (void)tileHeight;
    (void)margin;
    (void)spacing;
    ST_SET_ERROR("Tileset loading requires Objective-C++");
    return -1;
#endif
}

ST_API STTilesetID st_tileset_load_asset(const char* assetName,
                                          int32_t tileWidth, int32_t tileHeight,
                                          int32_t margin, int32_t spacing)
{
#ifdef __OBJC__
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR_RET(mgr, "TilemapManager", -1);
    
    if (!mgr->isInitialized()) {
        ST_SET_ERROR("TilemapManager not initialized");
        return -1;
    }
    
    if (!assetName) {
        ST_SET_ERROR("Invalid asset name");
        return -1;
    }
    
    if (tileWidth <= 0 || tileHeight <= 0) {
        ST_SET_ERROR("Invalid tile dimensions");
        return -1;
    }
    
    // Get Metal device from renderer
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
        return -1;
    }
    
    // Load texture from asset
    id<MTLTexture> texture = loadTextureFromAsset(assetName, device);
    if (!texture) {
        ST_SET_ERROR("Failed to load texture from asset");
        return -1;
    }
    
    // Create tileset
    int32_t tilesetID = mgr->createTileset(assetName);
    if (tilesetID < 0) {
        ST_SET_ERROR("Failed to create tileset");
        return -1;
    }
    
    auto tileset = mgr->getTileset(tilesetID);
    if (!tileset) {
        ST_SET_ERROR("Failed to get tileset");
        return -1;
    }
    
    // Initialize tileset with texture
    tileset->initialize(texture, tileWidth, tileHeight, assetName);
    tileset->setMargin(margin);
    tileset->setSpacing(spacing);
    tileset->recalculateLayout();
    
    ST_CLEAR_ERROR();
    return tilesetID;
#else
    (void)assetName;
    (void)tileWidth;
    (void)tileHeight;
    (void)margin;
    (void)spacing;
    ST_SET_ERROR("Tileset loading requires Objective-C++");
    return -1;
#endif
}

ST_API void st_tileset_destroy(STTilesetID tileset)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    mgr->removeTileset(tileset);
    
    ST_CLEAR_ERROR();
}

ST_API int32_t st_tileset_get_tile_count(STTilesetID tileset)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR_RET(mgr, "TilemapManager", 0);
    
    auto ts = mgr->getTileset(tileset);
    if (!ts) {
        ST_SET_ERROR("Invalid tileset ID");
        return 0;
    }
    
    ST_CLEAR_ERROR();
    return ts->getTileCount();
}

ST_API void st_tileset_get_dimensions(STTilesetID tileset, int32_t* columns, int32_t* rows)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    auto ts = mgr->getTileset(tileset);
    if (!ts) {
        ST_SET_ERROR("Invalid tileset ID");
        if (columns) *columns = 0;
        if (rows) *rows = 0;
        return;
    }
    
    if (columns) *columns = ts->getColumns();
    if (rows) *rows = ts->getRows();
    
    ST_CLEAR_ERROR();
}

// =============================================================================
// Tilemap Management
// =============================================================================

ST_API STTilemapID st_tilemap_create(int32_t width, int32_t height, 
                                      int32_t tileWidth, int32_t tileHeight)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR_RET(mgr, "TilemapManager", -1);
    
    if (!mgr->isInitialized()) {
        ST_SET_ERROR("TilemapManager not initialized");
        return -1;
    }
    
    int32_t id = mgr->createTilemap(width, height, tileWidth, tileHeight);
    
    ST_CLEAR_ERROR();
    return id;
}

ST_API STTilemapID st_tilemap_load_file(const char* filePath, STLayerID* outLayerIDs, 
                                         int32_t maxLayers, int32_t* outLayerCount)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR_RET(mgr, "TilemapManager", -1);
    
    if (!mgr->isInitialized()) {
        ST_SET_ERROR("TilemapManager not initialized");
        return -1;
    }
    
    if (!filePath) {
        ST_SET_ERROR("Invalid file path");
        return -1;
    }
    
    std::vector<int32_t> layerIDs;
    int32_t tilemapID = mgr->loadTilemapFromFile(filePath, layerIDs);
    
    if (tilemapID < 0) {
        ST_SET_ERROR(mgr->getLastError().c_str());
        return -1;
    }
    
    // Copy layer IDs to output array
    if (outLayerIDs && maxLayers > 0) {
        int32_t count = std::min(static_cast<int32_t>(layerIDs.size()), maxLayers);
        for (int32_t i = 0; i < count; ++i) {
            outLayerIDs[i] = layerIDs[i];
        }
    }
    
    if (outLayerCount) {
        *outLayerCount = static_cast<int32_t>(layerIDs.size());
    }
    
    ST_CLEAR_ERROR();
    return tilemapID;
}

ST_API bool st_tilemap_save_file(STTilemapID tilemap, const char* filePath,
                                  const STLayerID* layerIDs, int32_t layerCount,
                                  bool saveCamera)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR_RET(mgr, "TilemapManager", false);
    
    if (!filePath) {
        ST_SET_ERROR("Invalid file path");
        return false;
    }
    
    std::vector<int32_t> layerIDsVec;
    if (layerIDs && layerCount > 0) {
        layerIDsVec.assign(layerIDs, layerIDs + layerCount);
    }
    
    bool success = mgr->saveTilemapToFile(tilemap, filePath, 
                                          layerIDs ? &layerIDsVec : nullptr,
                                          saveCamera);
    
    if (!success) {
        ST_SET_ERROR(mgr->getLastError().c_str());
        return false;
    }
    
    ST_CLEAR_ERROR();
    return true;
}

ST_API STTilemapID st_tilemap_load_asset(const char* assetName)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR_RET(mgr, "TilemapManager", -1);
    
    if (!mgr->isInitialized()) {
        ST_SET_ERROR("TilemapManager not initialized");
        return -1;
    }
    
    if (!assetName) {
        ST_SET_ERROR("Invalid asset name");
        return -1;
    }
    
    int32_t tilemapID = mgr->loadTilemapFromAsset(assetName);
    
    if (tilemapID < 0) {
        ST_SET_ERROR(mgr->getLastError().c_str());
        return -1;
    }
    
    ST_CLEAR_ERROR();
    return tilemapID;
}

ST_API bool st_tilemap_save_asset(STTilemapID tilemap, const char* assetName,
                                   const STLayerID* layerIDs, int32_t layerCount,
                                   bool saveCamera)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR_RET(mgr, "TilemapManager", false);
    
    if (!assetName) {
        ST_SET_ERROR("Invalid asset name");
        return false;
    }
    
    std::vector<int32_t> layerIDsVec;
    if (layerIDs && layerCount > 0) {
        layerIDsVec.assign(layerIDs, layerIDs + layerCount);
    }
    
    bool success = mgr->saveTilemapToAsset(tilemap, assetName,
                                           layerIDs ? &layerIDsVec : nullptr,
                                           saveCamera);
    
    if (!success) {
        ST_SET_ERROR(mgr->getLastError().c_str());
        return false;
    }
    
    ST_CLEAR_ERROR();
    return true;
}

ST_API void st_tilemap_destroy(STTilemapID tilemap)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    mgr->removeTilemap(tilemap);
    
    ST_CLEAR_ERROR();
}

ST_API void st_tilemap_get_size(STTilemapID tilemap, int32_t* width, int32_t* height)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    auto tm = mgr->getTilemap(tilemap);
    if (tm) {
        if (width) *width = tm->getWidth();
        if (height) *height = tm->getHeight();
        ST_CLEAR_ERROR();
    } else {
        ST_SET_ERROR("Invalid tilemap ID");
        if (width) *width = 0;
        if (height) *height = 0;
    }
}

// =============================================================================
// Layer Management
// =============================================================================

ST_API STLayerID st_tilemap_create_layer(const char* name)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR_RET(mgr, "TilemapManager", -1);
    
    if (!mgr->isInitialized()) {
        ST_SET_ERROR("TilemapManager not initialized");
        return -1;
    }
    
    std::string layerName = name ? name : "";
    int32_t id = mgr->createLayer(layerName);
    
    ST_CLEAR_ERROR();
    return id;
}

ST_API void st_tilemap_destroy_layer(STLayerID layer)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    mgr->removeLayer(layer);
    
    ST_CLEAR_ERROR();
}

ST_API void st_tilemap_layer_set_tilemap(STLayerID layer, STTilemapID tilemap)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    mgr->setLayerTilemap(layer, tilemap);
    
    ST_CLEAR_ERROR();
}

ST_API void st_tilemap_layer_set_tileset(STLayerID layer, STTilesetID tileset)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    mgr->setLayerTileset(layer, tileset);
    
    ST_CLEAR_ERROR();
}

ST_API void st_tilemap_layer_set_parallax(STLayerID layer, float parallaxX, float parallaxY)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    auto lyr = mgr->getLayer(layer);
    if (lyr) {
        lyr->setParallax(parallaxX, parallaxY);
        ST_CLEAR_ERROR();
    } else {
        ST_SET_ERROR("Invalid layer ID");
    }
}

ST_API void st_tilemap_layer_set_opacity(STLayerID layer, float opacity)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    auto lyr = mgr->getLayer(layer);
    if (lyr) {
        lyr->setOpacity(opacity);
        ST_CLEAR_ERROR();
    } else {
        ST_SET_ERROR("Invalid layer ID");
    }
}

ST_API void st_tilemap_layer_set_visible(STLayerID layer, bool visible)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    auto lyr = mgr->getLayer(layer);
    if (lyr) {
        lyr->setVisible(visible);
        ST_CLEAR_ERROR();
    } else {
        ST_SET_ERROR("Invalid layer ID");
    }
}

ST_API void st_tilemap_layer_set_z_order(STLayerID layer, int32_t zOrder)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    auto lyr = mgr->getLayer(layer);
    if (lyr) {
        lyr->setZOrder(zOrder);
        mgr->sortLayers();
        ST_CLEAR_ERROR();
    } else {
        ST_SET_ERROR("Invalid layer ID");
    }
}

ST_API void st_tilemap_layer_set_auto_scroll(STLayerID layer, float scrollX, float scrollY)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    auto lyr = mgr->getLayer(layer);
    if (lyr) {
        lyr->setAutoScroll(scrollX, scrollY);
        ST_CLEAR_ERROR();
    } else {
        ST_SET_ERROR("Invalid layer ID");
    }
}

// =============================================================================
// Tile Manipulation
// =============================================================================

ST_API void st_tilemap_set_tile(STLayerID layer, int32_t x, int32_t y, uint16_t tileID)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    auto lyr = mgr->getLayer(layer);
    if (!lyr) {
        ST_SET_ERROR("Invalid layer ID");
        return;
    }
    
    auto tilemap = lyr->getTilemap();
    if (!tilemap) {
        ST_SET_ERROR("Layer has no tilemap");
        return;
    }
    
    TileData tile(tileID);
    tilemap->setTile(x, y, tile);
    
    ST_CLEAR_ERROR();
}

ST_API uint16_t st_tilemap_get_tile(STLayerID layer, int32_t x, int32_t y)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR_RET(mgr, "TilemapManager", 0);
    
    auto lyr = mgr->getLayer(layer);
    if (!lyr) {
        ST_SET_ERROR("Invalid layer ID");
        return 0;
    }
    
    auto tilemap = lyr->getTilemap();
    if (!tilemap) {
        ST_SET_ERROR("Layer has no tilemap");
        return 0;
    }
    
    TileData tile = tilemap->getTile(x, y);
    
    ST_CLEAR_ERROR();
    return tile.getTileID();
}

ST_API void st_tilemap_fill_rect(STLayerID layer, int32_t x, int32_t y,
                                   int32_t width, int32_t height, uint16_t tileID)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    auto lyr = mgr->getLayer(layer);
    if (!lyr) {
        ST_SET_ERROR("Invalid layer ID");
        return;
    }
    
    auto tilemap = lyr->getTilemap();
    if (!tilemap) {
        ST_SET_ERROR("Layer has no tilemap");
        return;
    }
    
    TileData tile(tileID);
    tilemap->fillRect(x, y, width, height, tile);
    
    ST_CLEAR_ERROR();
}

ST_API void st_tilemap_clear(STLayerID layer)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    auto lyr = mgr->getLayer(layer);
    if (!lyr) {
        ST_SET_ERROR("Invalid layer ID");
        return;
    }
    
    auto tilemap = lyr->getTilemap();
    if (!tilemap) {
        ST_SET_ERROR("Layer has no tilemap");
        return;
    }
    
    tilemap->clear();
    
    ST_CLEAR_ERROR();
}

// =============================================================================
// Camera Control
// =============================================================================

ST_API void st_tilemap_set_camera(float x, float y)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    mgr->setCameraPosition(x, y);
    
    ST_CLEAR_ERROR();
}

ST_API void st_tilemap_move_camera(float dx, float dy)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    mgr->moveCamera(dx, dy);
    
    ST_CLEAR_ERROR();
}

ST_API void st_tilemap_get_camera(float* x, float* y)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    auto camera = mgr->getCamera();
    if (camera) {
        if (x) *x = camera->getX();
        if (y) *y = camera->getY();
        ST_CLEAR_ERROR();
    } else {
        ST_SET_ERROR("No camera available");
        if (x) *x = 0.0f;
        if (y) *y = 0.0f;
    }
}

ST_API void st_tilemap_set_zoom(float zoom)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    mgr->setCameraZoom(zoom);
    
    ST_CLEAR_ERROR();
}

ST_API void st_tilemap_camera_follow(float targetX, float targetY, float smoothness)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    auto camera = mgr->getCamera();
    if (camera) {
        camera->setFollowSmoothing(smoothness);
        mgr->cameraFollow(targetX, targetY);
        ST_CLEAR_ERROR();
    } else {
        ST_SET_ERROR("No camera available");
    }
}

ST_API void st_tilemap_set_camera_bounds(float x, float y, float width, float height)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    mgr->setCameraBounds(x, y, width, height);
    
    ST_CLEAR_ERROR();
}

ST_API void st_tilemap_camera_shake(float magnitude, float duration)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    mgr->cameraShake(magnitude, duration);
    
    ST_CLEAR_ERROR();
}

// =============================================================================
// Update
// =============================================================================

ST_API void st_tilemap_update(float dt)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    mgr->update(dt);
    
    ST_CLEAR_ERROR();
}

// =============================================================================
// Coordinate Conversion
// =============================================================================

ST_API void st_tilemap_world_to_tile(STLayerID layer, float worldX, float worldY,
                                      int32_t* tileX, int32_t* tileY)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    auto lyr = mgr->getLayer(layer);
    if (!lyr) {
        ST_SET_ERROR("Invalid layer ID");
        if (tileX) *tileX = 0;
        if (tileY) *tileY = 0;
        return;
    }
    
    auto tilemap = lyr->getTilemap();
    if (!tilemap) {
        ST_SET_ERROR("Layer has no tilemap");
        if (tileX) *tileX = 0;
        if (tileY) *tileY = 0;
        return;
    }
    
    int32_t tx, ty;
    tilemap->worldToTile(worldX, worldY, tx, ty);
    
    if (tileX) *tileX = tx;
    if (tileY) *tileY = ty;
    
    ST_CLEAR_ERROR();
}

ST_API void st_tilemap_tile_to_world(STLayerID layer, int32_t tileX, int32_t tileY,
                                      float* worldX, float* worldY)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR(mgr, "TilemapManager");
    
    auto lyr = mgr->getLayer(layer);
    if (!lyr) {
        ST_SET_ERROR("Invalid layer ID");
        if (worldX) *worldX = 0.0f;
        if (worldY) *worldY = 0.0f;
        return;
    }
    
    auto tilemap = lyr->getTilemap();
    if (!tilemap) {
        ST_SET_ERROR("Layer has no tilemap");
        if (worldX) *worldX = 0.0f;
        if (worldY) *worldY = 0.0f;
        return;
    }
    
    float wx, wy;
    tilemap->tileToWorld(tileX, tileY, wx, wy);
    
    if (worldX) *worldX = wx;
    if (worldY) *worldY = wy;
    
    ST_CLEAR_ERROR();
}

// =============================================================================
// Initialization (called by framework)
// =============================================================================

ST_API bool st_tilemap_init(float viewportWidth, float viewportHeight)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    ST_CHECK_PTR_RET(mgr, "TilemapManager", false);
    
    if (mgr->isInitialized()) {
        ST_SET_ERROR("TilemapManager already initialized");
        return false;
    }
    
    mgr->initialize(viewportWidth, viewportHeight);
    
    ST_CLEAR_ERROR();
    return true;
}

ST_API void st_tilemap_shutdown(void)
{
    ST_LOCK;
    
    TilemapManager* mgr = ST_CONTEXT.tilemap();
    if (!mgr) return;
    
    mgr->shutdown();
    
    ST_CLEAR_ERROR();
}