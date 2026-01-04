//
//  CartLoader.h
//  SuperTerminal Framework - Cart Loading System
//
//  Loads and manages FasterBASIC cart files (.crt)
//  Copyright © 2024-2026 Alban Read, SuperTerminal. All rights reserved.
//

#ifndef CART_LOADER_H
#define CART_LOADER_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>

// Forward declare SQLite types to avoid exposing sqlite3.h in header
struct sqlite3;

namespace SuperTerminal {

/// Cart metadata structure
struct CartMetadata {
    std::string title;
    std::string author;
    std::string version;
    std::string description;
    std::string dateCreated;
    std::string engineVersion;
    std::string category;
    std::string icon;
    std::string screenshot;
    std::string website;
    std::string license;
    std::string rating;
    std::string players;
    std::string controls;

    CartMetadata() = default;
};

/// Cart program structure
struct CartProgram {
    std::string source;
    std::string format;      // "basic" or "compiled"
    std::string entryPoint;
    std::string notes;

    CartProgram() : format("basic") {}
};

/// Cart sprite asset
struct CartSprite {
    std::string name;
    std::vector<uint8_t> data;
    int32_t width;
    int32_t height;
    std::string format;      // "png", "rgba", "indexed"
    std::string notes;

    CartSprite() : width(0), height(0), format("png") {}
};

/// Cart tileset asset
struct CartTileset {
    std::string name;
    std::vector<uint8_t> data;
    int32_t tileWidth;
    int32_t tileHeight;
    int32_t tilesAcross;
    int32_t tilesDown;
    std::string format;      // "png", "rgba", "indexed"
    int32_t margin;
    int32_t spacing;
    std::string notes;

    CartTileset() : tileWidth(0), tileHeight(0), tilesAcross(0), tilesDown(0),
                    format("png"), margin(0), spacing(0) {}
};

/// Cart sound asset
struct CartSound {
    std::string name;
    std::vector<uint8_t> data;
    std::string format;      // "wav", "raw", "aiff"
    int32_t sampleRate;
    int32_t channels;
    int32_t bitsPerSample;
    std::string notes;

    CartSound() : format("wav"), sampleRate(44100), channels(1), bitsPerSample(16) {}
};

/// Cart music asset
struct CartMusic {
    std::string name;
    std::vector<uint8_t> data;
    std::string format;      // "sid", "mod", "xm", "s3m", "it"
    double durationSeconds;
    std::string notes;

    CartMusic() : format("sid"), durationSeconds(0.0) {}
};

/// Cart data file
struct CartDataFile {
    std::string path;
    std::vector<uint8_t> data;
    std::string mimeType;
    std::string notes;

    CartDataFile() = default;
};

/// Cart validation result
struct CartValidationResult {
    bool valid;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    CartValidationResult() : valid(true) {}

    void addError(const std::string& error) {
        errors.push_back(error);
        valid = false;
    }

    void addWarning(const std::string& warning) {
        warnings.push_back(warning);
    }
};

/// CartLoader: Loads and manages FasterBASIC cart files
///
/// Responsibilities:
/// - Open/close cart database files
/// - Validate cart structure and contents
/// - Load program source code
/// - Provide access to cart assets (sprites, sounds, etc.)
/// - Query cart metadata
/// - Manage cart lifecycle
///
/// Thread Safety:
/// - Not thread-safe, use from main thread only
/// - Asset data is copied, so returned data can be used safely
///
class CartLoader {
public:
    /// Constructor
    CartLoader();

    /// Destructor
    ~CartLoader();

    // Disable copy
    CartLoader(const CartLoader&) = delete;
    CartLoader& operator=(const CartLoader&) = delete;

    // Allow move
    CartLoader(CartLoader&& other) noexcept;
    CartLoader& operator=(CartLoader&& other) noexcept;

    // === CART LOADING ===

    /// Load a cart from file
    /// @param cartPath Path to .crt file
    /// @param readOnly Open in read-only mode (default: false)
    /// @return true if loaded successfully
    bool loadCart(const std::string& cartPath, bool readOnly = false);

    /// Unload the current cart
    void unloadCart();

    /// Check if cart is opened in read-only mode
    /// @return true if read-only
    bool isReadOnly() const { return m_readOnly; }

    /// Check if a cart is currently loaded
    /// @return true if cart is loaded
    bool isLoaded() const { return m_db != nullptr; }

    /// Get the path to the currently loaded cart
    /// @return Cart file path or empty string if none loaded
    std::string getCartPath() const { return m_cartPath; }

    // === CART CREATION ===

    /// Create a new empty cart file
    /// @param cartPath Path to new .crt file (will be created)
    /// @param metadata Initial metadata for the cart
    /// @return true if created successfully
    static bool createCart(const std::string& cartPath, const CartMetadata& metadata);

    // === VALIDATION ===

    /// Validate a cart file without loading it
    /// @param cartPath Path to .crt file
    /// @return Validation result with errors/warnings
    static CartValidationResult validateCart(const std::string& cartPath);

    /// Validate the currently loaded cart
    /// @return Validation result
    CartValidationResult validate() const;

    // === METADATA ACCESS ===

    /// Get cart metadata
    /// @return Cart metadata structure
    CartMetadata getMetadata() const;

    /// Get specific metadata value
    /// @param key Metadata key
    /// @return Metadata value or empty string if not found
    std::string getMetadataValue(const std::string& key) const;

    /// Get schema version
    /// @return Schema version number
    int getSchemaVersion() const;

    // === PROGRAM ACCESS ===

    /// Get the program source code
    /// @return Program structure with source code
    CartProgram getProgram() const;

    /// Get just the program source (convenience method)
    /// @return Source code string
    std::string getProgramSource() const;

    // === ASSET QUERIES ===

    /// List all sprite names
    /// @return Vector of sprite asset names
    std::vector<std::string> listSprites() const;

    /// List all tileset names
    /// @return Vector of tileset asset names
    std::vector<std::string> listTilesets() const;

    /// List all sound names
    /// @return Vector of sound asset names
    std::vector<std::string> listSounds() const;

    /// List all music names
    /// @return Vector of music asset names
    std::vector<std::string> listMusic() const;

    /// List all data file paths
    /// @return Vector of data file paths
    std::vector<std::string> listDataFiles() const;

    /// Check if sprite exists
    /// @param name Sprite name
    /// @return true if exists
    bool hasSprite(const std::string& name) const;

    /// Check if tileset exists
    /// @param name Tileset name
    /// @return true if exists
    bool hasTileset(const std::string& name) const;

    /// Check if sound exists
    /// @param name Sound name
    /// @return true if exists
    bool hasSound(const std::string& name) const;

    /// Check if music exists
    /// @param name Music name
    /// @return true if exists
    bool hasMusic(const std::string& name) const;

    /// Check if data file exists
    /// @param path Data file path
    /// @return true if exists
    bool hasDataFile(const std::string& path) const;

    // === ASSET LOADING ===

    /// Load sprite asset
    /// @param name Sprite name (without extension)
    /// @param outSprite Output sprite structure
    /// @return true if loaded successfully
    bool loadSprite(const std::string& name, CartSprite& outSprite) const;

    /// Load tileset asset
    /// @param name Tileset name (without extension)
    /// @param outTileset Output tileset structure
    /// @return true if loaded successfully
    bool loadTileset(const std::string& name, CartTileset& outTileset) const;

    /// Load sound asset
    /// @param name Sound name (without extension)
    /// @param outSound Output sound structure
    /// @return true if loaded successfully
    bool loadSound(const std::string& name, CartSound& outSound) const;

    /// Load music asset
    /// @param name Music name (without extension)
    /// @param outMusic Output music structure
    /// @return true if loaded successfully
    bool loadMusic(const std::string& name, CartMusic& outMusic) const;

    /// Load data file
    /// @param path Data file path
    /// @param outDataFile Output data file structure
    /// @return true if loaded successfully
    bool loadDataFile(const std::string& path, CartDataFile& outDataFile) const;

    // === CART MODIFICATION (Read-Write Mode Only) ===

    /// Update program source code
    /// @param source New program source
    /// @return true if updated successfully
    bool updateProgram(const std::string& source);

    /// Add or update sprite asset
    /// @param sprite Sprite to add/update
    /// @return true if successful
    bool addSprite(const CartSprite& sprite);

    /// Add or update tileset asset
    /// @param tileset Tileset to add/update
    /// @return true if successful
    bool addTileset(const CartTileset& tileset);

    /// Add or update sound asset
    /// @param sound Sound to add/update
    /// @return true if successful
    bool addSound(const CartSound& sound);

    /// Add or update music asset
    /// @param music Music to add/update
    /// @return true if successful
    bool addMusic(const CartMusic& music);

    /// Add or update data file
    /// @param dataFile Data file to add/update
    /// @return true if successful
    bool addDataFile(const CartDataFile& dataFile);

    /// Delete sprite asset
    /// @param name Sprite name
    /// @return true if successful
    bool deleteSprite(const std::string& name);

    /// Delete tileset asset
    /// @param name Tileset name
    /// @return true if successful
    bool deleteTileset(const std::string& name);

    /// Delete sound asset
    /// @param name Sound name
    /// @return true if successful
    bool deleteSound(const std::string& name);

    /// Delete music asset
    /// @param name Music name
    /// @return true if successful
    bool deleteMusic(const std::string& name);

    /// Delete data file
    /// @param path Data file path
    /// @return true if successful
    bool deleteDataFile(const std::string& path);

    /// Delete asset by name (searches all tables)
    /// @param name Asset name to search for
    /// @param outDeletedFrom Output string describing what was deleted
    /// @return true if found and deleted, false if not found
    bool deleteAssetByName(const std::string& name, std::string& outDeletedFrom);

    /// Update cart metadata
    /// @param key Metadata key
    /// @param value Metadata value
    /// @return true if successful
    bool updateMetadata(const std::string& key, const std::string& value);

    /// Commit all changes (flush to disk)
    /// @return true if successful
    bool commit();

    // === STATISTICS ===

    /// Get total number of sprites
    /// @return Sprite count
    int getSpriteCount() const;

    /// Get total number of tilesets
    /// @return Tileset count
    int getTilesetCount() const;

    /// Get total number of sounds
    /// @return Sound count
    int getSoundCount() const;

    /// Get total number of music tracks
    /// @return Music count
    int getMusicCount() const;

    /// Get total number of data files
    /// @return Data file count
    int getDataFileCount() const;

    /// Get total cart size in bytes
    /// @return Cart file size
    int64_t getCartSize() const;

    /// Get total size of all assets in bytes
    /// @return Sum of all asset data sizes
    int64_t getTotalAssetSize() const;

    // === ERROR HANDLING ===

    /// Get last error message
    /// @return Error message or empty string
    std::string getLastError() const { return m_lastError; }

    /// Clear last error
    void clearLastError() { m_lastError.clear(); }

    // === UTILITY ===

    /// Check if file is a valid cart file
    /// @param filePath Path to check
    /// @return true if appears to be a valid cart
    static bool isCartFile(const std::string& filePath);

    /// Get cart format version this loader supports
    /// @return Maximum supported schema version
    static int getSupportedSchemaVersion() { return 1; }

private:
    // SQLite database handle
    sqlite3* m_db;

    // Cart file path
    std::string m_cartPath;

    // Read-only flag
    bool m_readOnly;

    // Cached metadata (loaded once)
    mutable bool m_metadataCached;
    mutable CartMetadata m_metadata;

    // Error tracking
    mutable std::string m_lastError;

    // === INTERNAL HELPERS ===

    /// Set error message
    void setError(const std::string& message) const;

    /// Open database
    bool openDatabase(const std::string& path, bool readOnly);

    /// Close database
    void closeDatabase();

    /// Check if required tables exist
    bool checkSchema() const;

    /// Load metadata from database
    bool loadMetadata() const;

    /// Strip file extension from name
    static std::string stripExtension(const std::string& name);

    /// Execute SQL query and check result
    bool executeSQL(const std::string& sql) const;

    /// Get single string value from query
    bool queryString(const std::string& sql, std::string& outValue) const;

    /// Get single integer value from query
    bool queryInt(const std::string& sql, int& outValue) const;

    /// Get single int64 value from query
    bool queryInt64(const std::string& sql, int64_t& outValue) const;

    /// Check if table exists
    bool tableExists(const std::string& tableName) const;

    /// Check if asset exists in table
    bool assetExists(const std::string& table, const std::string& name) const;

    /// Get asset count in table
    int getAssetCount(const std::string& table) const;

    /// Load BLOB data
    bool loadBlob(const std::string& table, const std::string& nameColumn,
                  const std::string& name, std::vector<uint8_t>& outData) const;

    /// Check if cart is writable
    bool checkWritable() const;

    /// Begin transaction
    bool beginTransaction();

    /// Commit transaction
    bool commitTransaction();

    /// Rollback transaction
    bool rollbackTransaction();
};

} // namespace SuperTerminal

#endif // CART_LOADER_H
