//
//  CartManager.cpp
//  SuperTerminal Framework - Cart Management System
//
//  High-level cart management for FBRunner3
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "CartManager.h"
#include "../Assets/AssetManager.h"
#include "../Debug/Logger.h"
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

namespace SuperTerminal {

// =============================================================================
// Helper Functions
// =============================================================================

/// Create directory with intermediate directories (like mkdir -p)
static bool mkdirp(const char* path, mode_t mode) {
    char tmp[1024];
    char* p = NULL;
    size_t len;
    
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            struct stat st;
            if (stat(tmp, &st) != 0) {
                if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
                    return false;
                }
            }
            *p = '/';
        }
    }
    
    struct stat st;
    if (stat(tmp, &st) != 0) {
        if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
            return false;
        }
    }
    return true;
}

/// Get the standard carts directory path
/// Returns: ~/Library/Application Support/SuperTerminal/carts
static std::string getCartsDirectoryPath() {
    const char* home = getenv("HOME");
    if (!home) {
        return "./carts";
    }
    
#ifdef __APPLE__
    return std::string(home) + "/Library/Application Support/SuperTerminal/carts";
#else
    return std::string(home) + "/.local/share/SuperTerminal/carts";
#endif
}

/// Ensure the carts directory exists, creating it if needed
/// Returns: true if directory exists or was created successfully
static bool ensureCartsDirectoryExists() {
    std::string cartsDir = getCartsDirectoryPath();
    
    struct stat st;
    if (stat(cartsDir.c_str(), &st) == 0) {
        // Directory already exists
        return true;
    }
    
    // Try to create directory with intermediate directories
    if (!mkdirp(cartsDir.c_str(), 0755)) {
        LOG_WARNINGF("Could not create carts directory: %s (error: %s)", 
                     cartsDir.c_str(), strerror(errno));
        return false;
    }
    
    return true;
}

/// Resolve cart path - if relative, prepend carts directory
static std::string resolveCartPath(const std::string& path) {
    // If path is absolute, use as-is
    if (!path.empty() && path[0] == '/') {
        return path;
    }
    
    // If path starts with ~, expand home directory
    if (!path.empty() && path[0] == '~') {
        const char* home = getenv("HOME");
        if (home) {
            return std::string(home) + path.substr(1);
        }
        return path;
    }
    
    // Otherwise, treat as relative to carts directory
    std::string cartsDir = getCartsDirectoryPath();
    return cartsDir + "/" + path;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

CartManager::CartManager()
    : m_loader(nullptr)
    , m_assetProvider(nullptr)
    , m_currentMode(CartMode::None)
    , m_isDirty(false)
    , m_autoSaveEnabled(true)
    , m_autoSaveInterval(30)
    , m_timeSinceLastSave(0.0)
    , m_assetManager(nullptr)
{
}

CartManager::~CartManager() {
    if (isCartActive()) {
        closeCart(false);  // Don't save on destruction
    }
}

// =============================================================================
// Initialization
// =============================================================================

void CartManager::initialize(AssetManager* assetManager, const CartManagerConfig& config) {
    m_assetManager = assetManager;
    m_config = config;
    m_autoSaveEnabled = config.autoSave;
    m_autoSaveInterval = config.autoSaveIntervalSeconds;
}

// =============================================================================
// Cart Lifecycle
// =============================================================================

CartOperationResult CartManager::createCart(const std::string& path,
                                           const std::string& title,
                                           const std::string& author,
                                           const std::string& version,
                                           const std::string& description) {
    // Validate path is not empty
    if (path.empty()) {
        return CartOperationResult::Failure("Cart path cannot be empty");
    }
    
    // Ensure carts directory exists
    if (!ensureCartsDirectoryExists()) {
        LOG_WARNING("Could not ensure carts directory exists");
    }
    
    // Resolve path (handle relative paths)
    std::string resolvedPath = resolveCartPath(path);
    
    // Check if file already exists
    struct stat buffer;
    if (stat(resolvedPath.c_str(), &buffer) == 0) {
        return CartOperationResult::Failure("File already exists: " + resolvedPath);
    }
    
    // Close any existing cart (safe check with m_loader)
    if (m_loader && m_loader->isLoaded()) {
        auto closeResult = closeCart();
        if (!closeResult.success) {
            return closeResult;
        }
    }
    
    // Build metadata
    CartMetadata metadata;
    metadata.title = title.empty() ? "Untitled Cart" : title;
    metadata.author = author;
    metadata.version = version;
    metadata.description = description;
    metadata.engineVersion = "FBRunner3 1.0";
    
    // Get current date/time
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    char dateStr[100];
    std::strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M:%S", std::localtime(&now_c));
    metadata.dateCreated = dateStr;
    
    // Create cart file directly using CartLoader
    if (!CartLoader::createCart(resolvedPath, metadata)) {
        return CartOperationResult::Failure("Failed to create cart file");
    }
    
    // Open it in development mode
    auto result = useCart(resolvedPath);
    if (result.success) {
        result.message = "Cart created successfully: " + resolvedPath;
    }
    
    return result;
}

CartOperationResult CartManager::useCart(const std::string& path) {
    // Resolve path (handle relative paths)
    std::string resolvedPath = resolveCartPath(path);
    
    LOG_DEBUGF("useCart() ENTERED, path=%s (resolved: %s)", path.c_str(), resolvedPath.c_str());
    
    // Close any existing cart
    LOG_DEBUG("Checking isCartActive()...");
    if (isCartActive()) {
        LOG_DEBUG("Cart is active, calling closeCart()...");
        auto closeResult = closeCart();
        LOG_DEBUGF("closeCart() returned, success=%d", closeResult.success);
        if (!closeResult.success) {
            return closeResult;
        }
    } else {
        LOG_DEBUG("No cart active, skipping close");
    }
    
    LOG_DEBUG("Past isCartActive check");
    
    // Create loader if needed
    if (!m_loader) {
        m_loader = std::make_unique<CartLoader>();
    }
    
    // Validate if configured
    if (m_config.validateOnLoad) {
        auto validation = CartLoader::validateCart(resolvedPath);
        if (!validation.valid) {
            std::string errorMsg = "Cart validation failed:\n";
            for (const auto& error : validation.errors) {
                errorMsg += "  - " + error + "\n";
            }
            return CartOperationResult::Failure(errorMsg);
        }
        
        // Collect warnings
        CartOperationResult result;
        for (const auto& warning : validation.warnings) {
            result.addWarning(warning);
        }
    }
    
    // Load cart in read-write mode
    if (!m_loader->loadCart(resolvedPath, false)) {
        return CartOperationResult::Failure("Failed to load cart: " + m_loader->getLastError());
    }
    
    // Update state
    m_cartPath = path;
    m_currentMode = CartMode::Development;
    m_isDirty = false;
    m_timeSinceLastSave = 0.0;
    
    // Create asset provider
    m_assetProvider = std::make_unique<CartAssetProvider>(m_loader.get());
    
    // Register with asset manager
    if (m_assetManager) {
        registerCartAssets();
    }
    
    // Get program source
    std::string programSource = m_loader->getProgramSource();
    
    // Update statistics
    m_stats.loadCount++;
    
    // Invoke callback
    if (m_onCartLoaded) {
        m_onCartLoaded(path);
    }
    
    // Return result with program
    CartOperationResult result = CartOperationResult::Success("Cart opened for editing");
    result.programSource = programSource;
    return result;
}

CartOperationResult CartManager::runCart(const std::string& path) {
    // Resolve path (handle relative paths)
    std::string resolvedPath = resolveCartPath(path);
    // Close any existing cart
    if (isCartActive()) {
        auto closeResult = closeCart();
        if (!closeResult.success) {
            return closeResult;
        }
    }
    
    // Create loader if needed
    if (!m_loader) {
        m_loader = std::make_unique<CartLoader>();
    }
    
    // Validate if configured
    if (m_config.validateOnLoad) {
        auto validation = CartLoader::validateCart(resolvedPath);
        if (!validation.valid) {
            std::string errorMsg = "Cart validation failed:\n";
            for (const auto& error : validation.errors) {
                errorMsg += "  - " + error + "\n";
            }
            return CartOperationResult::Failure(errorMsg);
        }
    }
    
    // Load cart in read-only mode
    if (!m_loader->loadCart(resolvedPath, true)) {
        return CartOperationResult::Failure("Failed to load cart: " + m_loader->getLastError());
    }
    
    // Update state
    m_cartPath = resolvedPath;
    m_currentMode = CartMode::Play;
    m_isDirty = false;
    m_timeSinceLastSave = 0.0;
    
    // Create asset provider
    m_assetProvider = std::make_unique<CartAssetProvider>(m_loader.get());
    
    // Register with asset manager
    if (m_assetManager) {
        registerCartAssets();
    }
    
    // Load splash screen and intro music if available
    loadSplashScreen();
    loadIntroMusic();
    
    // Get program source
    std::string programSource = m_loader->getProgramSource();
    
    // Update statistics
    m_stats.loadCount++;
    
    // Invoke callback
    if (m_onCartLoaded) {
        m_onCartLoaded(path);
    }
    
    // Return result with program
    CartOperationResult result = CartOperationResult::Success("Cart opened for playing");
    result.programSource = programSource;
    return result;
}

CartOperationResult CartManager::closeCart(bool saveChanges) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    // Check for unsaved changes
    if (m_isDirty && m_currentMode == CartMode::Development) {
        if (saveChanges) {
            // Prompt user if configured and callback exists
            if (m_config.confirmClose && m_onPromptSave) {
                bool shouldSave = m_onPromptSave();
                if (shouldSave) {
                    auto saveResult = saveCart();
                    if (!saveResult.success) {
                        return CartOperationResult::Failure("Failed to save before closing: " + saveResult.message);
                    }
                }
            } else {
                // Auto-save
                auto saveResult = saveCart();
                if (!saveResult.success) {
                    return CartOperationResult::Failure("Failed to save before closing: " + saveResult.message);
                }
            }
        }
    }
    
    // Unregister from asset manager
    if (m_assetManager) {
        unregisterCartAssets();
    }
    
    // Clean up
    m_assetProvider.reset();
    m_loader->unloadCart();
    m_loader.reset();
    
    // Reset state
    m_cartPath.clear();
    m_currentMode = CartMode::None;
    m_isDirty = false;
    m_timeSinceLastSave = 0.0;
    
    // Invoke callback
    if (m_onCartClosed) {
        m_onCartClosed();
    }
    
    return CartOperationResult::Success("Cart closed");
}

CartOperationResult CartManager::saveCart() {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    // Commit changes to database
    if (!m_loader->commit()) {
        return CartOperationResult::Failure("Failed to commit changes: " + m_loader->getLastError());
    }
    
    // Update state
    m_isDirty = false;
    m_timeSinceLastSave = 0.0;
    m_stats.saveCount++;
    m_stats.lastSave = std::chrono::steady_clock::now();
    m_stats.hasUnsavedChanges = false;
    
    // Invoke callback
    if (m_onCartSaved) {
        m_onCartSaved(false);  // false = manual save
    }
    
    return CartOperationResult::Success("Cart saved successfully");
}

// =============================================================================
// Cart Access
// =============================================================================

CartInfo CartManager::getCartInfo() const {
    CartInfo info;
    
    if (!isCartActive() || !m_loader) {
        return info;
    }
    
    info.metadata = m_loader->getMetadata();
    info.spriteCount = m_loader->getSpriteCount();
    info.tilesetCount = m_loader->getTilesetCount();
    info.soundCount = m_loader->getSoundCount();
    info.musicCount = m_loader->getMusicCount();
    info.dataFileCount = m_loader->getDataFileCount();
    info.totalSize = m_loader->getCartSize();
    info.assetSize = m_loader->getTotalAssetSize();
    
    switch (m_currentMode) {
        case CartMode::Development:
            info.mode = "Development";
            break;
        case CartMode::Play:
            info.mode = "Play";
            break;
        default:
            info.mode = "None";
            break;
    }
    
    return info;
}

std::string CartManager::getProgramSource() const {
    if (!isCartActive() || !m_loader) {
        return "";
    }
    
    return m_loader->getProgramSource();
}

CartOperationResult CartManager::updateProgramSource(const std::string& source) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    if (!m_loader->updateProgram(source)) {
        return CartOperationResult::Failure("Failed to update program: " + m_loader->getLastError());
    }
    
    markModified();
    
    return CartOperationResult::Success("Program updated");
}

// =============================================================================
// Metadata Operations
// =============================================================================

CartOperationResult CartManager::setMetadata(const std::string& key, const std::string& value) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    // Normalize key (lowercase)
    std::string normalizedKey = key;
    std::transform(normalizedKey.begin(), normalizedKey.end(), normalizedKey.begin(), ::tolower);
    
    if (!m_loader->updateMetadata(normalizedKey, value)) {
        return CartOperationResult::Failure("Failed to update metadata: " + m_loader->getLastError());
    }
    
    markModified();
    
    return CartOperationResult::Success("Metadata updated: " + key);
}

std::string CartManager::getMetadata(const std::string& key) const {
    if (!isCartActive() || !m_loader) {
        return "";
    }
    
    // Normalize key (lowercase)
    std::string normalizedKey = key;
    std::transform(normalizedKey.begin(), normalizedKey.end(), normalizedKey.begin(), ::tolower);
    
    return m_loader->getMetadataValue(normalizedKey);
}

// =============================================================================
// Asset Operations
// =============================================================================

CartOperationResult CartManager::addSpriteFromFile(const std::string& filePath, const std::string& name) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    // Read file data
    std::vector<uint8_t> data;
    if (!readAssetFile(filePath, data)) {
        return CartOperationResult::Failure("Failed to read file: " + filePath);
    }
    
    // Create sprite structure
    CartSprite sprite;
    sprite.name = name;
    sprite.data = data;
    sprite.format = "png";  // Assume PNG for now
    
    if (!m_loader->addSprite(sprite)) {
        return CartOperationResult::Failure("Failed to add sprite: " + m_loader->getLastError());
    }
    
    markModified();
    
    return CartOperationResult::Success("Sprite added: " + name);
}

CartOperationResult CartManager::addTilesetFromFile(const std::string& filePath,
                                                   const std::string& name,
                                                   int tileWidth,
                                                   int tileHeight) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    // Read file data
    std::vector<uint8_t> data;
    if (!readAssetFile(filePath, data)) {
        return CartOperationResult::Failure("Failed to read file: " + filePath);
    }
    
    // Create tileset structure
    CartTileset tileset;
    tileset.name = name;
    tileset.data = data;
    tileset.tileWidth = tileWidth;
    tileset.tileHeight = tileHeight;
    tileset.format = "png";
    
    if (!m_loader->addTileset(tileset)) {
        return CartOperationResult::Failure("Failed to add tileset: " + m_loader->getLastError());
    }
    
    markModified();
    
    return CartOperationResult::Success("Tileset added: " + name);
}

CartOperationResult CartManager::addSoundFromFile(const std::string& filePath, const std::string& name) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    // Read file data
    std::vector<uint8_t> data;
    if (!readAssetFile(filePath, data)) {
        return CartOperationResult::Failure("Failed to read file: " + filePath);
    }
    
    // Create sound structure
    CartSound sound;
    sound.name = name;
    sound.data = data;
    sound.format = "wav";  // Assume WAV for now
    
    if (!m_loader->addSound(sound)) {
        return CartOperationResult::Failure("Failed to add sound: " + m_loader->getLastError());
    }
    
    markModified();
    
    return CartOperationResult::Success("Sound added: " + name);
}

CartOperationResult CartManager::addMusicFromFile(const std::string& filePath, const std::string& name) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    // Read file data
    std::vector<uint8_t> data;
    if (!readAssetFile(filePath, data)) {
        return CartOperationResult::Failure("Failed to read file: " + filePath);
    }
    
    // Validate file is not empty
    if (data.empty()) {
        return CartOperationResult::Failure("Music file is empty: " + filePath);
    }
    
    // Create music structure
    CartMusic music;
    music.name = name;
    music.data = data;
    
    // Determine format from extension
    std::string ext = getAssetTypeFromExtension(filePath);
    if (ext == "sid") {
        music.format = "sid";
    } else if (ext == "mod") {
        music.format = "mod";
    } else {
        music.format = "unknown";
    }
    
    if (!m_loader->addMusic(music)) {
        return CartOperationResult::Failure("Failed to add music: " + m_loader->getLastError());
    }
    
    markModified();
    
    return CartOperationResult::Success("Music added: " + name);
}

CartOperationResult CartManager::addSidFromFile(const std::string& filePath, const std::string& name) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    // Read file data
    std::vector<uint8_t> data;
    if (!readAssetFile(filePath, data)) {
        return CartOperationResult::Failure("Failed to read SID file: " + filePath);
    }
    
    // Validate file is not empty
    if (data.empty()) {
        return CartOperationResult::Failure("SID file is empty: " + filePath);
    }
    
    // Create music structure with SID format
    CartMusic music;
    music.name = name;
    music.data = data;
    music.format = "sid";
    
    if (!m_loader->addMusic(music)) {
        return CartOperationResult::Failure("Failed to add SID music: " + m_loader->getLastError());
    }
    
    markModified();
    
    return CartOperationResult::Success("SID music added: " + name);
}

CartOperationResult CartManager::addAbcFromFile(const std::string& filePath, const std::string& name) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    // Read file data
    std::vector<uint8_t> data;
    if (!readAssetFile(filePath, data)) {
        return CartOperationResult::Failure("Failed to read ABC file: " + filePath);
    }
    
    // Validate file is not empty
    if (data.empty()) {
        return CartOperationResult::Failure("ABC file is empty: " + filePath);
    }
    
    // Create music structure with ABC format
    CartMusic music;
    music.name = name;
    music.data = data;
    music.format = "abc";
    
    if (!m_loader->addMusic(music)) {
        return CartOperationResult::Failure("Failed to add ABC music: " + m_loader->getLastError());
    }
    
    markModified();
    
    return CartOperationResult::Success("ABC music added: " + name);
}

CartOperationResult CartManager::addMidiFromFile(const std::string& filePath, const std::string& name) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    // Read file data
    std::vector<uint8_t> data;
    if (!readAssetFile(filePath, data)) {
        return CartOperationResult::Failure("Failed to read MIDI file: " + filePath);
    }
    
    // Validate file is not empty
    if (data.empty()) {
        return CartOperationResult::Failure("MIDI file is empty: " + filePath);
    }
    
    // Create music structure with MIDI format
    CartMusic music;
    music.name = name;
    music.data = data;
    music.format = "midi";
    
    if (!m_loader->addMusic(music)) {
        return CartOperationResult::Failure("Failed to add MIDI music: " + m_loader->getLastError());
    }
    
    markModified();
    
    return CartOperationResult::Success("MIDI music added: " + name);
}

CartOperationResult CartManager::addDataFile(const std::string& filePath, const std::string& cartPath) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    // Read file data
    std::vector<uint8_t> data;
    if (!readAssetFile(filePath, data)) {
        return CartOperationResult::Failure("Failed to read file: " + filePath);
    }
    
    // Create data file structure
    CartDataFile dataFile;
    dataFile.path = cartPath;
    dataFile.data = data;
    dataFile.mimeType = "application/octet-stream";
    
    if (!m_loader->addDataFile(dataFile)) {
        return CartOperationResult::Failure("Failed to add data file: " + m_loader->getLastError());
    }
    
    markModified();
    
    return CartOperationResult::Success("Data file added: " + cartPath);
}

CartOperationResult CartManager::deleteSprite(const std::string& name) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    if (!m_loader->deleteSprite(name)) {
        return CartOperationResult::Failure("Failed to delete sprite: " + m_loader->getLastError());
    }
    
    markModified();
    
    return CartOperationResult::Success("Sprite deleted: " + name);
}

CartOperationResult CartManager::deleteTileset(const std::string& name) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    if (!m_loader->deleteTileset(name)) {
        return CartOperationResult::Failure("Failed to delete tileset: " + m_loader->getLastError());
    }
    
    markModified();
    
    return CartOperationResult::Success("Tileset deleted: " + name);
}

CartOperationResult CartManager::deleteSound(const std::string& name) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    if (!m_loader->deleteSound(name)) {
        return CartOperationResult::Failure("Failed to delete sound: " + m_loader->getLastError());
    }
    
    markModified();
    
    return CartOperationResult::Success("Sound deleted: " + name);
}

CartOperationResult CartManager::deleteMusic(const std::string& name) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    if (!m_loader->deleteMusic(name)) {
        return CartOperationResult::Failure("Failed to delete music: " + m_loader->getLastError());
    }
    
    markModified();
    
    return CartOperationResult::Success("Music deleted: " + name);
}

CartOperationResult CartManager::deleteDataFile(const std::string& path) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    if (!m_loader->deleteDataFile(path)) {
        return CartOperationResult::Failure("Failed to delete data file: " + m_loader->getLastError());
    }
    
    markModified();
    
    return CartOperationResult::Success("Data file deleted: " + path);
}

CartOperationResult CartManager::deleteAssetByName(const std::string& name, std::string& outDeletedFrom) {
    if (!isCartActive()) {
        return CartOperationResult::Failure("No cart is currently active");
    }
    
    if (m_currentMode != CartMode::Development) {
        return CartOperationResult::Failure("Cart is in read-only mode");
    }
    
    if (!m_loader) {
        return CartOperationResult::Failure("Cart loader not initialized");
    }
    
    if (!m_loader->deleteAssetByName(name, outDeletedFrom)) {
        return CartOperationResult::Failure("Asset not found: " + name);
    }
    
    markModified();
    
    return CartOperationResult::Success("Deleted: " + name + " from " + outDeletedFrom);
}

std::vector<std::string> CartManager::listAssets(const std::string& assetType) const {
    std::vector<std::string> result;
    
    if (!isCartActive() || !m_loader) {
        return result;
    }
    
    if (assetType.empty() || assetType == "sprites") {
        auto sprites = m_loader->listSprites();
        result.insert(result.end(), sprites.begin(), sprites.end());
    }
    
    if (assetType.empty() || assetType == "tilesets") {
        auto tilesets = m_loader->listTilesets();
        result.insert(result.end(), tilesets.begin(), tilesets.end());
    }
    
    if (assetType.empty() || assetType == "sounds") {
        auto sounds = m_loader->listSounds();
        result.insert(result.end(), sounds.begin(), sounds.end());
    }
    
    if (assetType.empty() || assetType == "music") {
        auto music = m_loader->listMusic();
        result.insert(result.end(), music.begin(), music.end());
    }
    
    if (assetType.empty() || assetType == "data") {
        auto data = m_loader->listDataFiles();
        result.insert(result.end(), data.begin(), data.end());
    }
    
    return result;
}

// =============================================================================
// Asset Manager Integration
// =============================================================================

bool CartManager::registerWithAssetManager(AssetManager* assetManager) {
    m_assetManager = assetManager;
    if (isCartActive() && m_assetProvider) {
        return registerCartAssets();
    }
    return true;
}

void CartManager::unregisterFromAssetManager(AssetManager* assetManager) {
    if (m_assetManager == assetManager) {
        unregisterCartAssets();
        m_assetManager = nullptr;
    }
}

// =============================================================================
// Auto-Save
// =============================================================================

void CartManager::updateAutoSave(double deltaTime) {
    if (!m_autoSaveEnabled || !isCartActive() || m_currentMode != CartMode::Development) {
        return;
    }
    
    if (!m_isDirty) {
        return;
    }
    
    m_timeSinceLastSave += deltaTime;
    
    if (m_timeSinceLastSave >= m_autoSaveInterval) {
        checkAutoSave();
    }
}

void CartManager::checkAutoSave() {
    if (!m_autoSaveEnabled || !isCartActive() || m_currentMode != CartMode::Development) {
        return;
    }
    
    if (!m_isDirty) {
        return;
    }
    
    // Perform auto-save
    if (m_loader && m_loader->commit()) {
        m_isDirty = false;
        m_timeSinceLastSave = 0.0;
        m_stats.autoSaveCount++;
        m_stats.lastSave = std::chrono::steady_clock::now();
        m_stats.hasUnsavedChanges = false;
        
        // Invoke callback
        if (m_onCartSaved) {
            m_onCartSaved(true);  // true = auto-save
        }
    }
}

// =============================================================================
// Validation
// =============================================================================

CartOperationResult CartManager::validateCartFile(const std::string& path) {
    auto validation = CartLoader::validateCart(path);
    
    if (!validation.valid) {
        std::string errorMsg = "Cart validation failed:\n";
        for (const auto& error : validation.errors) {
            errorMsg += "  - " + error + "\n";
        }
        return CartOperationResult::Failure(errorMsg);
    }
    
    CartOperationResult result = CartOperationResult::Success("Cart is valid");
    for (const auto& warning : validation.warnings) {
        result.addWarning(warning);
    }
    
    return result;
}

// =============================================================================
// Utility
// =============================================================================

bool CartManager::isCartFile(const std::string& path) {
    return CartLoader::isCartFile(path);
}

void CartManager::resetStats() {
    m_stats = CartManagerStats();
}

// =============================================================================
// Internal Helpers
// =============================================================================

void CartManager::setError(const std::string& message) {
    m_lastError = message;
}

bool CartManager::loadCartInternal(const std::string& path, bool readOnly) {
    if (!m_loader) {
        m_loader = std::make_unique<CartLoader>();
    }
    
    return m_loader->loadCart(path, readOnly);
}

bool CartManager::registerCartAssets() {
    if (!m_assetManager || !m_assetProvider) {
        return false;
    }
    
    // Register cart asset provider with AssetManager
    // This allows the AssetManager to load assets from the active cart
    m_assetManager->setCartProvider(m_assetProvider.get());
    return true;
}

void CartManager::unregisterCartAssets() {
    if (!m_assetManager) {
        return;
    }
    
    // Unregister cart asset provider from AssetManager
    // This restores default behavior of loading only from database/filesystem
    m_assetManager->setCartProvider(nullptr);
}

void CartManager::loadSplashScreen() {
    if (!m_loader || !m_loader->isLoaded()) {
        return;
    }
    
    // Check for "Splash!" sprite
    if (!m_loader->hasSprite("Splash!")) {
        return;
    }
    
    // Load the splash sprite
    CartSprite splash;
    if (!m_loader->loadSprite("Splash!", splash)) {
        return;
    }
    
    // Invoke callback if set
    if (m_onSplashScreen) {
        m_onSplashScreen(splash.data, splash.width, splash.height);
    }
}

void CartManager::loadIntroMusic() {
    if (!m_loader || !m_loader->isLoaded()) {
        return;
    }
    
    // Check for "Intro!" music
    if (!m_loader->hasMusic("Intro!")) {
        return;
    }
    
    // Load the intro music
    CartMusic intro;
    if (!m_loader->loadMusic("Intro!", intro)) {
        return;
    }
    
    // Invoke callback if set
    if (m_onIntroMusic) {
        m_onIntroMusic(intro.data, intro.format);
    }
}

// createCartFile removed - now using CartLoader::createCart directly (no Python dependency)

bool CartManager::readAssetFile(const std::string& path, std::vector<uint8_t>& outData) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        setError("Failed to open file: " + path);
        return false;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    outData.resize(size);
    if (!file.read(reinterpret_cast<char*>(outData.data()), size)) {
        setError("Failed to read file: " + path);
        return false;
    }
    
    return true;
}

std::string CartManager::getAssetTypeFromExtension(const std::string& path) {
    size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos) {
        return "";
    }
    
    std::string ext = path.substr(dotPos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    return ext;
}

void CartManager::markModified() {
    m_isDirty = true;
    m_stats.hasUnsavedChanges = true;
}

void CartManager::markSaved() {
    m_isDirty = false;
    m_stats.hasUnsavedChanges = false;
    m_stats.lastSave = std::chrono::steady_clock::now();
}

} // namespace SuperTerminal