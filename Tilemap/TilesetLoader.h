//
// TilesetLoader.h
// SuperTerminal Framework - Tilemap System
//
// Utility for loading tilesets from the asset system
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef TILESETLOADER_H
#define TILESETLOADER_H

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#ifdef __OBJC__
@protocol MTLDevice;
@protocol MTLTexture;
typedef id<MTLDevice> MTLDevicePtr;
typedef id<MTLTexture> MTLTexturePtr;
#else
typedef void* MTLDevicePtr;
typedef void* MTLTexturePtr;
#endif

namespace SuperTerminal {

// Forward declarations
class Tileset;
class AssetManager;

/// Tileset loading options
struct TilesetLoadOptions {
    int32_t tileWidth = 16;          // Tile width in pixels
    int32_t tileHeight = 16;         // Tile height in pixels
    int32_t margin = 0;              // Margin around atlas (pixels)
    int32_t spacing = 0;             // Spacing between tiles (pixels)
    bool generateMipmaps = true;     // Generate mipmaps for texture
    bool premultiplyAlpha = false;   // Premultiply alpha channel
    
    TilesetLoadOptions() = default;
};

/// TilesetLoader: Load tilesets from asset system or files
///
/// Supports:
/// - Loading from AssetManager (asset ID or name)
/// - Loading from filesystem (PNG, JPEG, etc.)
/// - Automatic texture creation for Metal
/// - Mipmap generation
/// - Format conversion
///
/// Usage:
///   TilesetLoader loader(device, assetManager);
///   auto tileset = loader.loadFromAsset("dungeon_tiles");
///   if (tileset) {
///       layer->setTileset(tileset);
///   }
///
class TilesetLoader {
public:
    // =================================================================
    // Construction
    // =================================================================
    
    /// Create loader with Metal device and asset manager
    /// @param device Metal device for texture creation
    /// @param assetManager Asset manager for loading (optional)
    TilesetLoader(MTLDevicePtr device, AssetManager* assetManager = nullptr);
    
    /// Destructor
    ~TilesetLoader();
    
    // Disable copy, allow move
    TilesetLoader(const TilesetLoader&) = delete;
    TilesetLoader& operator=(const TilesetLoader&) = delete;
    TilesetLoader(TilesetLoader&&) noexcept;
    TilesetLoader& operator=(TilesetLoader&&) noexcept;
    
    // =================================================================
    // Loading from Asset System
    // =================================================================
    
    /// Load tileset from asset by name
    /// @param assetName Asset name in database
    /// @param options Loading options (tile size, margin, etc.)
    /// @return Tileset on success, nullptr on failure
    std::shared_ptr<Tileset> loadFromAsset(const std::string& assetName,
                                           const TilesetLoadOptions& options = TilesetLoadOptions());
    
    /// Load tileset from asset by ID
    /// @param assetID Asset ID in database
    /// @param options Loading options
    /// @return Tileset on success, nullptr on failure
    std::shared_ptr<Tileset> loadFromAssetID(uint64_t assetID,
                                             const TilesetLoadOptions& options = TilesetLoadOptions());
    
    // =================================================================
    // Loading from Filesystem
    // =================================================================
    
    /// Load tileset from image file
    /// @param filePath Path to image file (PNG, JPEG, etc.)
    /// @param options Loading options
    /// @return Tileset on success, nullptr on failure
    std::shared_ptr<Tileset> loadFromFile(const std::string& filePath,
                                          const TilesetLoadOptions& options = TilesetLoadOptions());
    
    /// Load tileset from image data in memory
    /// @param imageData Raw image data (PNG, JPEG, etc.)
    /// @param dataSize Size of image data in bytes
    /// @param options Loading options
    /// @return Tileset on success, nullptr on failure
    std::shared_ptr<Tileset> loadFromMemory(const void* imageData, size_t dataSize,
                                            const TilesetLoadOptions& options = TilesetLoadOptions());
    
    // =================================================================
    // Creating Textures from Raw Data
    // =================================================================
    
    /// Create tileset from raw RGBA pixel data
    /// @param pixelData Raw RGBA pixel data (8 bits per channel)
    /// @param width Image width in pixels
    /// @param height Image height in pixels
    /// @param options Loading options
    /// @return Tileset on success, nullptr on failure
    std::shared_ptr<Tileset> createFromRGBA(const void* pixelData, 
                                            int32_t width, int32_t height,
                                            const TilesetLoadOptions& options = TilesetLoadOptions());
    
    // =================================================================
    // Configuration
    // =================================================================
    
    /// Set default loading options
    void setDefaultOptions(const TilesetLoadOptions& options);
    
    /// Get default loading options
    const TilesetLoadOptions& getDefaultOptions() const;
    
    /// Set asset manager
    void setAssetManager(AssetManager* assetManager);
    
    /// Get asset manager
    AssetManager* getAssetManager() const;
    
    // =================================================================
    // Error Handling
    // =================================================================
    
    /// Get last error message
    const std::string& getLastError() const;
    
    /// Clear last error
    void clearError();
    
    // =================================================================
    // Utilities
    // =================================================================
    
    /// Check if file extension is supported
    /// @param extension File extension (e.g., ".png", ".jpg")
    /// @return true if supported
    static bool isSupportedFormat(const std::string& extension);
    
    /// Get list of supported image formats
    /// @return Vector of supported extensions (e.g., {".png", ".jpg"})
    static std::vector<std::string> getSupportedFormats();
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    
    // Helper methods
    MTLTexturePtr createMetalTexture(const void* pixelData, 
                                     int32_t width, int32_t height,
                                     bool generateMipmaps);
    
    std::shared_ptr<Tileset> createTilesetFromTexture(MTLTexturePtr texture,
                                                      const TilesetLoadOptions& options,
                                                      const std::string& name);
    
    bool loadImageFromFile(const std::string& filePath,
                          void** outPixelData,
                          int32_t* outWidth,
                          int32_t* outHeight,
                          int32_t* outComponents);
    
    bool loadImageFromMemory(const void* data, size_t size,
                            void** outPixelData,
                            int32_t* outWidth,
                            int32_t* outHeight,
                            int32_t* outComponents);
    
    void freeImageData(void* pixelData);
    
    void setError(const std::string& error);
};

} // namespace SuperTerminal

#endif // TILESETLOADER_H