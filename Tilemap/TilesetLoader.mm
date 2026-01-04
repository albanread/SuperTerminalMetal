//
// TilesetLoader.mm
// SuperTerminal Framework - Tilemap System
//
// Implementation of TilesetLoader with AssetManager integration
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#import "TilesetLoader.h"
#import "Tileset.h"
#import "../Assets/AssetManager.h"
#import "../Assets/AssetMetadata.h"
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <vector>
#import <string>

namespace SuperTerminal {

// =============================================================================
// Implementation Structure
// =============================================================================

struct TilesetLoader::Impl {
    id<MTLDevice> device = nil;
    AssetManager* assetManager = nullptr;
    TilesetLoadOptions defaultOptions;
    std::string lastError;

    Impl() = default;
};

// =============================================================================
// Construction
// =============================================================================

TilesetLoader::TilesetLoader(MTLDevicePtr device, AssetManager* assetManager)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->device = device;
    m_impl->assetManager = assetManager;
}

TilesetLoader::~TilesetLoader() = default;

TilesetLoader::TilesetLoader(TilesetLoader&&) noexcept = default;
TilesetLoader& TilesetLoader::operator=(TilesetLoader&&) noexcept = default;

// =============================================================================
// Loading from Asset System
// =============================================================================

std::shared_ptr<Tileset> TilesetLoader::loadFromAsset(const std::string& assetName,
                                                      const TilesetLoadOptions& options) {
    if (!m_impl->assetManager) {
        setError("No AssetManager configured");
        return nullptr;
    }

    // Load asset metadata
    const AssetMetadata* metadata = m_impl->assetManager->getAssetMetadataByName(assetName);
    if (!metadata) {
        setError("Asset not found: " + assetName);
        return nullptr;
    }

    // Check if asset is an image
    if (metadata->kind != AssetKind::IMAGE &&
        metadata->kind != AssetKind::SPRITE &&
        metadata->kind != AssetKind::TILE) {
        setError("Asset is not an image: " + assetName);
        return nullptr;
    }

    // Get asset data
    if (metadata->data.empty()) {
        setError("Asset has no data: " + assetName);
        return nullptr;
    }

    // Load from memory (asset data is already in memory via AssetManager cache)
    return loadFromMemory(metadata->data.data(), metadata->data.size(), options);
}

std::shared_ptr<Tileset> TilesetLoader::loadFromAssetID(uint64_t assetID,
                                                        const TilesetLoadOptions& options) {
    if (!m_impl->assetManager) {
        setError("No AssetManager configured");
        return nullptr;
    }

    // For now, we need to get by name. In a full implementation, we'd add a getByID method
    setError("loadFromAssetID not yet implemented - use loadFromAsset(name) instead");
    return nullptr;
}

// =============================================================================
// Loading from Filesystem
// =============================================================================

std::shared_ptr<Tileset> TilesetLoader::loadFromFile(const std::string& filePath,
                                                     const TilesetLoadOptions& options) {
    void* pixelData = nullptr;
    int32_t width = 0, height = 0, components = 0;

    if (!loadImageFromFile(filePath, &pixelData, &width, &height, &components)) {
        return nullptr;
    }

    // Create tileset from pixel data
    auto tileset = createFromRGBA(pixelData, width, height, options);

    // Free image data
    freeImageData(pixelData);

    if (tileset) {
        // Extract filename for tileset name
        NSString* path = [NSString stringWithUTF8String:filePath.c_str()];
        NSString* filename = [[path lastPathComponent] stringByDeletingPathExtension];
        tileset->setName([filename UTF8String]);
    }

    return tileset;
}

std::shared_ptr<Tileset> TilesetLoader::loadFromMemory(const void* imageData, size_t dataSize,
                                                       const TilesetLoadOptions& options) {
    void* pixelData = nullptr;
    int32_t width = 0, height = 0, components = 0;

    if (!loadImageFromMemory(imageData, dataSize, &pixelData, &width, &height, &components)) {
        return nullptr;
    }

    // Create tileset from pixel data
    auto tileset = createFromRGBA(pixelData, width, height, options);

    // Free image data
    freeImageData(pixelData);

    return tileset;
}

// =============================================================================
// Creating Textures from Raw Data
// =============================================================================

std::shared_ptr<Tileset> TilesetLoader::createFromRGBA(const void* pixelData,
                                                       int32_t width, int32_t height,
                                                       const TilesetLoadOptions& options) {
    if (!m_impl->device) {
        setError("No Metal device available");
        return nullptr;
    }

    if (!pixelData || width <= 0 || height <= 0) {
        setError("Invalid pixel data or dimensions");
        return nullptr;
    }

    // Create Metal texture
    MTLTexturePtr texture = createMetalTexture(pixelData, width, height, options.generateMipmaps);
    if (!texture) {
        setError("Failed to create Metal texture");
        return nullptr;
    }

    // Create tileset
    return createTilesetFromTexture(texture, options, "");
}

// =============================================================================
// Configuration
// =============================================================================

void TilesetLoader::setDefaultOptions(const TilesetLoadOptions& options) {
    m_impl->defaultOptions = options;
}

const TilesetLoadOptions& TilesetLoader::getDefaultOptions() const {
    return m_impl->defaultOptions;
}

void TilesetLoader::setAssetManager(AssetManager* assetManager) {
    m_impl->assetManager = assetManager;
}

AssetManager* TilesetLoader::getAssetManager() const {
    return m_impl->assetManager;
}

// =============================================================================
// Error Handling
// =============================================================================

const std::string& TilesetLoader::getLastError() const {
    return m_impl->lastError;
}

void TilesetLoader::clearError() {
    m_impl->lastError.clear();
}

// =============================================================================
// Utilities
// =============================================================================

bool TilesetLoader::isSupportedFormat(const std::string& extension) {
    NSString* ext = [NSString stringWithUTF8String:extension.c_str()];
    ext = [ext lowercaseString];

    return [ext isEqualToString:@".png"] ||
           [ext isEqualToString:@".jpg"] ||
           [ext isEqualToString:@".jpeg"] ||
           [ext isEqualToString:@".bmp"] ||
           [ext isEqualToString:@".gif"] ||
           [ext isEqualToString:@".tiff"] ||
           [ext isEqualToString:@".tif"];
}

std::vector<std::string> TilesetLoader::getSupportedFormats() {
    return {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tiff", ".tif"};
}

// =============================================================================
// Helper Methods
// =============================================================================

MTLTexturePtr TilesetLoader::createMetalTexture(const void* pixelData,
                                                int32_t width, int32_t height,
                                                bool generateMipmaps) {
    if (!m_impl->device || !pixelData) {
        return nil;
    }

    // Create texture descriptor
    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                          width:width
                                                                                         height:height
                                                                                      mipmapped:generateMipmaps];
    descriptor.usage = MTLTextureUsageShaderRead;
    descriptor.storageMode = MTLStorageModePrivate; // GPU-only for best performance

    // Calculate mip levels
    if (generateMipmaps) {
        int32_t maxDim = std::max(width, height);
        descriptor.mipmapLevelCount = (int32_t)std::floor(std::log2(maxDim)) + 1;
    }

    // Create texture
    id<MTLTexture> texture = [m_impl->device newTextureWithDescriptor:descriptor];
    if (!texture) {
        return nil;
    }

    // Upload base level data
    MTLRegion region = MTLRegionMake2D(0, 0, width, height);
    [texture replaceRegion:region
               mipmapLevel:0
                 withBytes:pixelData
               bytesPerRow:width * 4]; // RGBA = 4 bytes per pixel

    // Generate mipmaps if requested
    if (generateMipmaps) {
        id<MTLCommandQueue> commandQueue = [m_impl->device newCommandQueue];
        id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
        id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
        [blitEncoder generateMipmapsForTexture:texture];
        [blitEncoder endEncoding];
        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];
    }

    return texture;
}

std::shared_ptr<Tileset> TilesetLoader::createTilesetFromTexture(MTLTexturePtr texture,
                                                                 const TilesetLoadOptions& options,
                                                                 const std::string& name) {
    if (!texture) {
        return nullptr;
    }

    // Create tileset
    auto tileset = std::make_shared<Tileset>();

    // Initialize with texture
    tileset->initialize(texture, options.tileWidth, options.tileHeight, name);

    // Set margin and spacing
    tileset->setMargin(options.margin);
    tileset->setSpacing(options.spacing);

    // Recalculate layout
    tileset->recalculateLayout();

    return tileset;
}

bool TilesetLoader::loadImageFromFile(const std::string& filePath,
                                      void** outPixelData,
                                      int32_t* outWidth,
                                      int32_t* outHeight,
                                      int32_t* outComponents) {
    // Use NSImage to load the image
    NSString* path = [NSString stringWithUTF8String:filePath.c_str()];
    NSImage* image = [[NSImage alloc] initWithContentsOfFile:path];

    if (!image) {
        setError("Failed to load image from file: " + filePath);
        return false;
    }

    // Get image dimensions
    NSSize size = [image size];
    int32_t width = (int32_t)size.width;
    int32_t height = (int32_t)size.height;

    if (width <= 0 || height <= 0) {
        setError("Invalid image dimensions");
        return false;
    }

    // Create bitmap representation
    NSBitmapImageRep* bitmap = [[NSBitmapImageRep alloc]
                                initWithBitmapDataPlanes:NULL
                                              pixelsWide:width
                                              pixelsHigh:height
                                           bitsPerSample:8
                                         samplesPerPixel:4
                                                hasAlpha:YES
                                                isPlanar:NO
                                          colorSpaceName:NSDeviceRGBColorSpace
                                             bytesPerRow:width * 4
                                            bitsPerPixel:32];

    if (!bitmap) {
        setError("Failed to create bitmap representation");
        return false;
    }

    // Draw image into bitmap
    [NSGraphicsContext saveGraphicsState];
    NSGraphicsContext* context = [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];
    [NSGraphicsContext setCurrentContext:context];
    [image drawInRect:NSMakeRect(0, 0, width, height)];
    [NSGraphicsContext restoreGraphicsState];

    // Allocate pixel data buffer
    size_t dataSize = width * height * 4;
    uint8_t* pixelData = (uint8_t*)malloc(dataSize);
    if (!pixelData) {
        setError("Failed to allocate pixel data buffer");
        return false;
    }

    // Copy pixel data
    memcpy(pixelData, [bitmap bitmapData], dataSize);

    // Set output parameters
    *outPixelData = pixelData;
    *outWidth = width;
    *outHeight = height;
    *outComponents = 4;

    return true;
}

bool TilesetLoader::loadImageFromMemory(const void* data, size_t size,
                                       void** outPixelData,
                                       int32_t* outWidth,
                                       int32_t* outHeight,
                                       int32_t* outComponents) {
    // Create NSData from raw bytes
    NSData* imageData = [NSData dataWithBytes:data length:size];
    if (!imageData) {
        setError("Failed to create NSData from memory");
        return false;
    }

    // Load image from data
    NSImage* image = [[NSImage alloc] initWithData:imageData];
    if (!image) {
        setError("Failed to load image from memory");
        return false;
    }

    // Get image dimensions
    NSSize imageSize = [image size];
    int32_t width = (int32_t)imageSize.width;
    int32_t height = (int32_t)imageSize.height;

    if (width <= 0 || height <= 0) {
        setError("Invalid image dimensions");
        return false;
    }

    // Create bitmap representation
    NSBitmapImageRep* bitmap = [[NSBitmapImageRep alloc]
                                initWithBitmapDataPlanes:NULL
                                              pixelsWide:width
                                              pixelsHigh:height
                                           bitsPerSample:8
                                         samplesPerPixel:4
                                                hasAlpha:YES
                                                isPlanar:NO
                                          colorSpaceName:NSDeviceRGBColorSpace
                                             bytesPerRow:width * 4
                                            bitsPerPixel:32];

    if (!bitmap) {
        setError("Failed to create bitmap representation");
        return false;
    }

    // Draw image into bitmap
    [NSGraphicsContext saveGraphicsState];
    NSGraphicsContext* context = [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];
    [NSGraphicsContext setCurrentContext:context];
    [image drawInRect:NSMakeRect(0, 0, width, height)];
    [NSGraphicsContext restoreGraphicsState];

    // Allocate pixel data buffer
    size_t dataSize = width * height * 4;
    uint8_t* pixelData = (uint8_t*)malloc(dataSize);
    if (!pixelData) {
        setError("Failed to allocate pixel data buffer");
        return false;
    }

    // Copy pixel data
    memcpy(pixelData, [bitmap bitmapData], dataSize);

    // Set output parameters
    *outPixelData = pixelData;
    *outWidth = width;
    *outHeight = height;
    *outComponents = 4;

    return true;
}

void TilesetLoader::freeImageData(void* pixelData) {
    if (pixelData) {
        free(pixelData);
    }
}

void TilesetLoader::setError(const std::string& error) {
    m_impl->lastError = error;
    NSLog(@"[TilesetLoader] ERROR: %s", error.c_str());
}

} // namespace SuperTerminal
