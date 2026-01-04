//
//  CartManager.h
//  SuperTerminal Framework - Cart Management System
//
//  High-level cart management
//  Handles cart lifecycle, asset operations, and menu/command integration
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef CART_MANAGER_H
#define CART_MANAGER_H

#include "CartLoader.h"
#include "CartAssetProvider.h"
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <chrono>

namespace SuperTerminal {

// Forward declarations
class AssetManager;

/// Cart mode enumeration
enum class CartMode {
    None,           // No cart active
    Development,    // Cart opened for editing (read-write)
    Play            // Cart opened for playing (read-only)
};

/// Cart operation result
struct CartOperationResult {
    bool success;
    std::string message;
    std::string programSource;  // Loaded program (for useCart/runCart)
    std::vector<std::string> warnings;

    CartOperationResult() : success(false) {}

    static CartOperationResult Success(const std::string& msg = "") {
        CartOperationResult result;
        result.success = true;
        result.message = msg;
        return result;
    }

    static CartOperationResult Failure(const std::string& msg) {
        CartOperationResult result;
        result.success = false;
        result.message = msg;
        return result;
    }

    void addWarning(const std::string& warning) {
        warnings.push_back(warning);
    }
};

/// Cart manager configuration
struct CartManagerConfig {
    bool autoSave = true;                           // Auto-save in development mode
    int autoSaveIntervalSeconds = 30;               // Auto-save interval
    bool confirmClose = true;                       // Confirm before closing unsaved cart
    bool validateOnLoad = true;                     // Validate cart on load

    CartManagerConfig() = default;
};

/// Cart manager statistics
struct CartManagerStats {
    int loadCount = 0;                              // Number of carts loaded
    int saveCount = 0;                              // Number of saves performed
    int autoSaveCount = 0;                          // Number of auto-saves
    std::chrono::steady_clock::time_point lastSave; // Last save time
    bool hasUnsavedChanges = false;                 // Unsaved changes flag

    CartManagerStats() : lastSave(std::chrono::steady_clock::now()) {}
};

/// Cart info display structure
struct CartInfo {
    CartMetadata metadata;
    int spriteCount = 0;
    int tilesetCount = 0;
    int soundCount = 0;
    int musicCount = 0;
    int dataFileCount = 0;
    int64_t totalSize = 0;
    int64_t assetSize = 0;
    std::string mode;  // "Development" or "Play"

    CartInfo() = default;
};

/// Callback types
using CartLoadedCallback = std::function<void(const CartInfo&)>;
using CartErrorCallback = std::function<void(const std::string&)>;
using CartMessageCallback = std::function<void(const std::string&)>;
using CartSplashCallback = std::function<void(const std::vector<uint8_t>& imageData, int width, int height)>;
using CartIntroMusicCallback = std::function<void(const std::vector<uint8_t>& musicData, const std::string& format)>;

/// CartManager: High-level cart lifecycle and command management
///
/// Responsibilities:
/// - Cart creation, loading, saving, and closing
/// - Development vs Play mode management
/// - Auto-save functionality
/// - Asset import/export to carts
/// - Integration with AssetManager via CartAssetProvider
/// - Command-line interface helpers
///
/// Thread Safety:
/// - Should be used from main thread only
/// - Coordinates with CartLoader and AssetManager
///
class CartManager {
public:
    /// Constructor
    CartManager();

    /// Destructor
    ~CartManager();

    // Disable copy, allow move
    CartManager(const CartManager&) = delete;
    CartManager& operator=(const CartManager&) = delete;
    CartManager(CartManager&&) noexcept = default;
    CartManager& operator=(CartManager&&) noexcept = default;

    // === INITIALIZATION ===

    /// Initialize cart manager
    /// @param assetManager Pointer to asset manager (for integration)
    /// @param config Configuration options
    void initialize(AssetManager* assetManager, const CartManagerConfig& config = CartManagerConfig());

    /// Set configuration
    /// @param config New configuration
    void setConfig(const CartManagerConfig& config) { m_config = config; }

    /// Get configuration
    /// @return Current configuration
    const CartManagerConfig& getConfig() const { return m_config; }

    // === CART LIFECYCLE ===

    /// Create a new cart file
    /// @param path Path to new .crt file
    /// @param title Cart title
    /// @param author Cart author
    /// @param version Cart version (default: "1.0.0")
    /// @param description Cart description
    /// @return Operation result
    CartOperationResult createCart(const std::string& path,
                                   const std::string& title,
                                   const std::string& author,
                                   const std::string& version = "1.0.0",
                                   const std::string& description = "");

    /// Use cart (open for development/editing)
    /// @param path Cart file path
    /// @return Operation result with program source
    CartOperationResult useCart(const std::string& path);

    /// Run cart (open for playing, read-only)
    /// @param path Cart file path
    /// @return Operation result with program source
    CartOperationResult runCart(const std::string& path);

    /// Close the current cart
    /// @param saveChanges Save changes before closing (default: ask user)
    /// @return Operation result
    CartOperationResult closeCart(bool saveChanges = true);

    /// Save current cart
    /// @return Operation result
    CartOperationResult saveCart();

    /// Check if a cart is currently active
    /// @return true if cart is loaded
    bool isCartActive() const { return m_loader && m_loader->isLoaded(); }

    /// Get current cart mode
    /// @return Current cart mode
    CartMode getCartMode() const { return m_currentMode; }

    /// Check if cart has unsaved changes
    /// @return true if cart is dirty
    bool isDirty() const { return m_isDirty; }

    /// Mark cart as dirty (modified)
    void markDirty() { m_isDirty = true; }

    /// Mark cart as clean (saved)
    void markClean() { m_isDirty = false; }

    // === CART ACCESS ===

    /// Get cart info
    /// @return Cart information structure
    CartInfo getCartInfo() const;

    /// Get program source from cart
    /// @return Program source code
    std::string getProgramSource() const;

    /// Update program source in cart
    /// @param source New program source
    /// @return Operation result
    CartOperationResult updateProgramSource(const std::string& source);

    /// Get cart loader (direct access)
    /// @return CartLoader pointer (may be null)
    CartLoader* getLoader() { return m_loader.get(); }
    const CartLoader* getLoader() const { return m_loader.get(); }

    /// Get asset provider
    /// @return CartAssetProvider pointer (may be null)
    CartAssetProvider* getAssetProvider() { return m_assetProvider.get(); }
    const CartAssetProvider* getAssetProvider() const { return m_assetProvider.get(); }

    // === METADATA OPERATIONS ===

    /// Set metadata value (SET INFO command)
    /// @param key Metadata key (name, author, version, description, etc.)
    /// @param value Metadata value
    /// @return Operation result
    CartOperationResult setMetadata(const std::string& key, const std::string& value);

    /// Get metadata value
    /// @param key Metadata key
    /// @return Metadata value (empty if not found)
    std::string getMetadata(const std::string& key) const;

    // === ASSET OPERATIONS ===

    /// Add sprite to cart from file
    /// @param filePath Source file path
    /// @param name Asset name (without extension)
    /// @return Operation result
    CartOperationResult addSpriteFromFile(const std::string& filePath,
                                          const std::string& name);

    /// Add tileset to cart from file
    /// @param filePath Source file path
    /// @param name Asset name (without extension)
    /// @param tileWidth Tile width in pixels
    /// @param tileHeight Tile height in pixels
    /// @return Operation result
    CartOperationResult addTilesetFromFile(const std::string& filePath,
                                           const std::string& name,
                                           int tileWidth,
                                           int tileHeight);

    /// Add sound to cart from file
    /// @param filePath Source file path
    /// @param name Asset name (without extension)
    /// @return Operation result
    CartOperationResult addSoundFromFile(const std::string& filePath,
                                         const std::string& name);

    /// Add music to cart from file
    /// @param filePath Source file path
    /// @param name Asset name (without extension)
    /// @return Operation result
    CartOperationResult addMusicFromFile(const std::string& filePath,
                                         const std::string& name);

    /// Add SID music to cart from file
    /// @param filePath Source file path (.sid)
    /// @param name Asset name (without extension)
    /// @return Operation result
    CartOperationResult addSidFromFile(const std::string& filePath,
                                       const std::string& name);

    /// Add ABC music to cart from file
    /// @param filePath Source file path (.abc)
    /// @param name Asset name (without extension)
    /// @return Operation result
    CartOperationResult addAbcFromFile(const std::string& filePath,
                                       const std::string& name);

    /// Add MIDI music to cart from file
    /// @param filePath Source file path (.mid/.midi)
    /// @param name Asset name (without extension)
    /// @return Operation result
    CartOperationResult addMidiFromFile(const std::string& filePath,
                                        const std::string& name);

    /// Add data file to cart
    /// @param filePath Source file path
    /// @param cartPath Path within cart (e.g., "data/levels.txt")
    /// @return Operation result
    CartOperationResult addDataFile(const std::string& filePath,
                                    const std::string& cartPath);

    /// Delete sprite from cart
    /// @param name Sprite name
    /// @return Operation result
    CartOperationResult deleteSprite(const std::string& name);

    /// Delete tileset from cart
    /// @param name Tileset name
    /// @return Operation result
    CartOperationResult deleteTileset(const std::string& name);

    /// Delete sound from cart
    /// @param name Sound name
    /// @return Operation result
    CartOperationResult deleteSound(const std::string& name);

    /// Delete music from cart
    /// @param name Music name
    /// @return Operation result
    CartOperationResult deleteMusic(const std::string& name);

    /// Delete data file from cart
    /// @param path Data file path within cart
    /// @return Operation result
    CartOperationResult deleteDataFile(const std::string& path);

    /// Delete asset by name (searches all tables)
    /// @param name Asset name to search for
    /// @param outDeletedFrom Output string describing what was deleted
    /// @return Operation result with success/failure and description
    CartOperationResult deleteAssetByName(const std::string& name, std::string& outDeletedFrom);

    /// List assets in cart
    /// @param assetType Asset type filter ("sprites", "tilesets", "sounds", "music", "data", or "" for all)
    /// @return Vector of asset names/paths
    std::vector<std::string> listAssets(const std::string& assetType = "") const;

    // === ASSET MANAGER INTEGRATION ===

    /// Register cart assets with AssetManager
    /// @param assetManager AssetManager instance
    /// @return true if successful
    bool registerWithAssetManager(AssetManager* assetManager);

    /// Unregister cart assets from AssetManager
    /// @param assetManager AssetManager instance
    void unregisterFromAssetManager(AssetManager* assetManager);

    // === AUTO-SAVE ===

    /// Update auto-save timer (call from main loop)
    /// @param deltaTime Time since last update in seconds
    void updateAutoSave(double deltaTime);

    /// Force auto-save if due
    void checkAutoSave();

    // === VALIDATION ===

    /// Validate cart file without loading
    /// @param path Cart file path
    /// @return Operation result
    static CartOperationResult validateCartFile(const std::string& path);

    // === UTILITY ===

    /// Check if path is a cart file
    /// @param path File path to check
    /// @return true if appears to be a cart file
    static bool isCartFile(const std::string& path);

    /// Get cart file extension for carts
    /// @return ".crt"
    static std::string getCartExtension() { return ".crt"; }

    // === STATISTICS ===

    /// Get manager statistics
    /// @return Stats structure
    const CartManagerStats& getStats() const { return m_stats; }

    /// Reset statistics
    void resetStats();

    // === ERROR HANDLING ===

    /// Get last error message
    /// @return Error message or empty string
    std::string getLastError() const { return m_lastError; }

    /// Clear last error
    void clearLastError() { m_lastError.clear(); }

    // === CALLBACKS ===

    /// Set callback for when cart is loaded
    /// @param callback Function called after cart loads
    void setOnCartLoaded(std::function<void(const std::string&)> callback) {
        m_onCartLoaded = callback;
    }

    /// Set callback for when cart is closed
    /// @param callback Function called after cart closes
    void setOnCartClosed(std::function<void()> callback) {
        m_onCartClosed = callback;
    }

    /// Set callback for when cart is saved
    /// @param callback Function called after save (bool = auto-save)
    void setOnCartSaved(std::function<void(bool)> callback) {
        m_onCartSaved = callback;
    }

    /// Set callback for unsaved changes prompt
    /// @param callback Function that prompts user, returns true if should save
    void setOnPromptSave(std::function<bool()> callback) {
        m_onPromptSave = callback;
    }

    /// Set callback for splash screen display
    /// @param callback Function called when Splash! image is found (imageData, width, height)
    void setOnSplashScreen(CartSplashCallback callback) {
        m_onSplashScreen = callback;
    }

    /// Set callback for intro music playback
    /// @param callback Function called when Intro! music is found (musicData, format)
    void setOnIntroMusic(CartIntroMusicCallback callback) {
        m_onIntroMusic = callback;
    }

    // === UTILITY ===

    /// Get cart loader (for advanced operations)
    /// @return Pointer to cart loader or nullptr
    CartLoader* getCartLoader() { return m_loader.get(); }

    /// Get asset provider (for integration)
    /// @return Pointer to asset provider or nullptr
    CartAssetProvider* getCartAssetProvider() { return m_assetProvider.get(); }

private:
    // Configuration
    CartManagerConfig m_config;

    // Cart loader
    std::unique_ptr<CartLoader> m_loader;

    // Asset provider for AssetManager integration
    std::unique_ptr<CartAssetProvider> m_assetProvider;

    // Current cart mode
    CartMode m_currentMode;

    // Dirty flag (unsaved changes)
    bool m_isDirty;

    // Error tracking
    std::string m_lastError;

    // Auto-save configuration
    bool m_autoSaveEnabled;
    int m_autoSaveInterval;  // seconds
    double m_timeSinceLastSave;  // seconds

    // Callbacks
    std::function<void(const std::string&)> m_onCartLoaded;
    std::function<void()> m_onCartClosed;
    std::function<void(bool)> m_onCartSaved;
    std::function<bool()> m_onPromptSave;
    CartSplashCallback m_onSplashScreen;
    CartIntroMusicCallback m_onIntroMusic;

    // Statistics
    CartManagerStats m_stats;

    // Asset manager reference (not owned)
    AssetManager* m_assetManager;

    // Current cart path
    std::string m_cartPath;

    // === INTERNAL HELPERS ===

    /// Set error message
    void setError(const std::string& message);

    /// Load cart file (common implementation)
    bool loadCartInternal(const std::string& path, bool readOnly);

    /// Register cart assets with AssetManager
    bool registerCartAssets();

    /// Unregister cart assets from AssetManager
    void unregisterCartAssets();

    /// Load and display splash screen if available (checks for "Splash!" sprite)
    void loadSplashScreen();

    /// Load and play intro music if available (checks for "Intro!" music)
    void loadIntroMusic();

    /// Read asset file into memory
    bool readAssetFile(const std::string& path, std::vector<uint8_t>& outData);

    /// Determine asset type from file extension
    std::string getAssetTypeFromExtension(const std::string& path);

    /// Mark cart as modified
    void markModified();

    /// Mark cart as saved
    void markSaved();
};

} // namespace SuperTerminal

#endif // CART_MANAGER_H
