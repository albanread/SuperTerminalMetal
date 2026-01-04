//
// TilemapFormat.h
// SuperTerminal Framework - Tilemap System
//
// Tilemap file format for saving/loading tilemaps to disk
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef TILEMAPFORMAT_H
#define TILEMAPFORMAT_H

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <ctime>

namespace SuperTerminal {

// Forward declarations
class Tilemap;
class TilemapLayer;
class Tileset;
class Camera;

/// Tilemap file format version
constexpr uint32_t TILEMAP_FORMAT_VERSION = 1;
constexpr uint32_t TILEMAP_MAGIC = 0x544D4150; // "TMAP" in hex

/// Compression types
enum class CompressionType : uint8_t {
    None = 0,
    RLE,        // Run-length encoding
    ZSTD,       // Zstandard compression
    LZ4         // LZ4 compression
};

/// Layer data encoding
enum class LayerEncoding : uint8_t {
    Raw = 0,        // Uncompressed raw tile data
    CSV,            // Comma-separated values (text)
    Base64,         // Base64 encoded binary
    Base64GZip,     // Base64 + GZip
    Base64Zstd      // Base64 + Zstandard
};

/// Tilemap metadata
struct TilemapMetadata {
    uint32_t version = TILEMAP_FORMAT_VERSION;
    uint32_t width = 0;             // Map width in tiles
    uint32_t height = 0;            // Map height in tiles
    uint32_t tileWidth = 16;        // Tile width in pixels
    uint32_t tileHeight = 16;       // Tile height in pixels
    
    std::string name;               // Map name
    std::string description;        // Map description
    std::string author;             // Map author
    std::string tilesetName;        // Primary tileset name
    std::string tilesetPath;        // Tileset file path
    
    uint32_t layerCount = 0;        // Number of layers
    
    CompressionType compression = CompressionType::None;
    LayerEncoding encoding = LayerEncoding::Raw;
    
    // Timestamps
    std::time_t createdAt = 0;
    std::time_t modifiedAt = 0;
    
    // Custom properties (key-value pairs)
    std::vector<std::pair<std::string, std::string>> properties;
    
    TilemapMetadata() = default;
};

/// Layer metadata
struct LayerMetadata {
    std::string name;               // Layer name
    int32_t id = -1;                // Layer ID
    int32_t zOrder = 0;             // Z-order for rendering
    
    float parallaxX = 1.0f;         // Horizontal parallax factor
    float parallaxY = 1.0f;         // Vertical parallax factor
    float opacity = 1.0f;           // Layer opacity (0.0-1.0)
    
    float offsetX = 0.0f;           // Layer offset X
    float offsetY = 0.0f;           // Layer offset Y
    
    float autoScrollX = 0.0f;       // Auto-scroll X (pixels/sec)
    float autoScrollY = 0.0f;       // Auto-scroll Y (pixels/sec)
    
    bool visible = true;            // Layer visibility
    bool locked = false;            // Layer locked (editor hint)
    
    uint32_t dataSize = 0;          // Size of layer data in bytes
    uint32_t dataOffset = 0;        // Offset to layer data in file
    
    // Custom properties
    std::vector<std::pair<std::string, std::string>> properties;
    
    LayerMetadata() = default;
};

/// Camera state (optional)
struct CameraState {
    float x = 0.0f;                 // Camera X position
    float y = 0.0f;                 // Camera Y position
    float zoom = 1.0f;              // Camera zoom
    
    float minX = 0.0f;              // Camera bounds
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    
    CameraState() = default;
};

/// Tileset reference (for multi-tileset maps)
struct TilesetReference {
    std::string name;               // Tileset name
    std::string path;               // File path or asset name
    uint32_t firstGID = 1;          // First global tile ID
    uint32_t tileCount = 0;         // Number of tiles in tileset
    
    int32_t tileWidth = 16;         // Tile dimensions
    int32_t tileHeight = 16;
    int32_t margin = 0;             // Tileset margin
    int32_t spacing = 0;            // Tile spacing
    
    TilesetReference() = default;
};

/// Complete tilemap file structure
struct TilemapFile {
    TilemapMetadata metadata;
    CameraState camera;
    std::vector<LayerMetadata> layers;
    std::vector<TilesetReference> tilesets;
    std::vector<std::vector<uint16_t>> layerData; // Tile data per layer
    
    TilemapFile() = default;
};

/// Save/load options
struct TilemapSaveOptions {
    CompressionType compression = CompressionType::None;
    LayerEncoding encoding = LayerEncoding::Raw;
    bool saveCamera = true;
    bool embedTilesets = false;     // Future: embed tileset data
    bool prettyPrint = false;       // For text formats (JSON)
    
    TilemapSaveOptions() = default;
};

/// TilemapFormat: Serialization and deserialization of tilemaps
///
/// Supports multiple formats:
/// - Binary (.stmap) - Compact binary format
/// - JSON (.json) - Human-readable format
/// - Tiled TMX (.tmx) - Tiled map editor format (import/export)
/// - CSV (.csv) - Simple CSV format
///
/// Features:
/// - Multiple compression options
/// - Layer data encoding options
/// - Metadata and custom properties
/// - Multi-tileset support
/// - Camera state preservation
///
class TilemapFormat {
public:
    // =================================================================
    // Binary Format (.stmap)
    // =================================================================
    
    /// Save tilemap to binary format
    /// @param tilemap Tilemap to save
    /// @param filePath Output file path
    /// @param layers Layers to save (optional)
    /// @param camera Camera state to save (optional)
    /// @param options Save options
    /// @return true on success
    static bool saveBinary(const Tilemap& tilemap,
                          const std::string& filePath,
                          const std::vector<TilemapLayer*>* layers = nullptr,
                          const Camera* camera = nullptr,
                          const TilemapSaveOptions& options = TilemapSaveOptions());
    
    /// Load tilemap from binary format
    /// @param filePath Input file path
    /// @param outTilemap Output tilemap (created)
    /// @param outLayers Output layers (created)
    /// @param outCamera Output camera state (optional)
    /// @return true on success
    static bool loadBinary(const std::string& filePath,
                          std::shared_ptr<Tilemap>& outTilemap,
                          std::vector<std::shared_ptr<TilemapLayer>>& outLayers,
                          CameraState* outCamera = nullptr);
    
    // =================================================================
    // JSON Format (.json)
    // =================================================================
    
    /// Save tilemap to JSON format
    /// @param tilemap Tilemap to save
    /// @param filePath Output file path
    /// @param layers Layers to save
    /// @param camera Camera state to save (optional)
    /// @param options Save options
    /// @return true on success
    static bool saveJSON(const Tilemap& tilemap,
                        const std::string& filePath,
                        const std::vector<TilemapLayer*>* layers = nullptr,
                        const Camera* camera = nullptr,
                        const TilemapSaveOptions& options = TilemapSaveOptions());
    
    /// Load tilemap from JSON format
    /// @param filePath Input file path
    /// @param outTilemap Output tilemap
    /// @param outLayers Output layers
    /// @param outCamera Output camera state (optional)
    /// @return true on success
    static bool loadJSON(const std::string& filePath,
                        std::shared_ptr<Tilemap>& outTilemap,
                        std::vector<std::shared_ptr<TilemapLayer>>& outLayers,
                        CameraState* outCamera = nullptr);
    
    // =================================================================
    // Tiled TMX Format (.tmx) - Import/Export
    // =================================================================
    
    /// Import from Tiled TMX format
    /// @param filePath TMX file path
    /// @param outTilemap Output tilemap
    /// @param outLayers Output layers
    /// @param outTilesets Output tileset references
    /// @return true on success
    static bool importTiledTMX(const std::string& filePath,
                              std::shared_ptr<Tilemap>& outTilemap,
                              std::vector<std::shared_ptr<TilemapLayer>>& outLayers,
                              std::vector<TilesetReference>& outTilesets);
    
    /// Export to Tiled TMX format
    /// @param tilemap Tilemap to export
    /// @param filePath Output TMX file path
    /// @param layers Layers to export
    /// @param tilesets Tileset references
    /// @return true on success
    static bool exportTiledTMX(const Tilemap& tilemap,
                              const std::string& filePath,
                              const std::vector<TilemapLayer*>& layers,
                              const std::vector<TilesetReference>& tilesets);
    
    // =================================================================
    // CSV Format (.csv) - Simple export
    // =================================================================
    
    /// Export layer to CSV
    /// @param tilemap Tilemap to export
    /// @param filePath Output CSV file path
    /// @param layerIndex Layer index to export (default: 0)
    /// @return true on success
    static bool exportCSV(const Tilemap& tilemap,
                         const std::string& filePath,
                         int32_t layerIndex = 0);
    
    /// Import layer from CSV
    /// @param filePath Input CSV file path
    /// @param outTilemap Output tilemap
    /// @return true on success
    static bool importCSV(const std::string& filePath,
                         std::shared_ptr<Tilemap>& outTilemap);
    
    // =================================================================
    // Utility Functions
    // =================================================================
    
    /// Build TilemapFile structure from components
    /// @param tilemap Source tilemap
    /// @param layers Source layers
    /// @param camera Source camera (optional)
    /// @param options Save options
    /// @return TilemapFile structure
    static TilemapFile buildFile(const Tilemap& tilemap,
                                 const std::vector<TilemapLayer*>& layers,
                                 const Camera* camera = nullptr,
                                 const TilemapSaveOptions& options = TilemapSaveOptions());
    
    /// Create tilemap from file structure
    /// @param file Source file structure
    /// @param outTilemap Output tilemap
    /// @param outLayers Output layers
    /// @param outCamera Output camera (optional)
    /// @return true on success
    static bool createFromFile(const TilemapFile& file,
                              std::shared_ptr<Tilemap>& outTilemap,
                              std::vector<std::shared_ptr<TilemapLayer>>& outLayers,
                              CameraState* outCamera = nullptr);
    
    /// Compress tile data
    /// @param data Input tile data
    /// @param type Compression type
    /// @param output Output compressed data
    /// @return true on success
    static bool compressData(const std::vector<uint16_t>& data,
                            CompressionType type,
                            std::vector<uint8_t>& output);
    
    /// Decompress tile data
    /// @param input Input compressed data
    /// @param type Compression type
    /// @param output Output tile data
    /// @param expectedSize Expected output size (tiles)
    /// @return true on success
    static bool decompressData(const std::vector<uint8_t>& input,
                              CompressionType type,
                              std::vector<uint16_t>& output,
                              size_t expectedSize);
    
    /// Detect file format from extension
    /// @param filePath File path
    /// @return Format name ("binary", "json", "tmx", "csv", "unknown")
    static std::string detectFormat(const std::string& filePath);
    
    /// Validate tilemap file
    /// @param filePath File path
    /// @param outError Error message (if validation fails)
    /// @return true if valid
    static bool validate(const std::string& filePath, std::string& outError);
    
    /// Get last error message
    /// @return Error message
    static const std::string& getLastError();
    
    /// Clear last error
    static void clearError();
    
private:
    static std::string s_lastError;
    
    // Helper methods
    static void setError(const std::string& error);
    static bool writeU32(std::ofstream& file, uint32_t value);
    static bool readU32(std::ifstream& file, uint32_t& value);
    static bool writeU16(std::ofstream& file, uint16_t value);
    static bool readU16(std::ifstream& file, uint16_t& value);
    static bool writeString(std::ofstream& file, const std::string& str);
    static bool readString(std::ifstream& file, std::string& str);
    static bool writeFloat(std::ofstream& file, float value);
    static bool readFloat(std::ifstream& file, float& value);
};

} // namespace SuperTerminal

#endif // TILEMAPFORMAT_H