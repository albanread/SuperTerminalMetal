//
//  CartLoader.cpp
//  SuperTerminal Framework - Cart Loading System
//
//  Implementation of CartLoader for managing .crt files
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "CartLoader.h"
#include <sqlite3.h>
#include <sys/stat.h>
#include <algorithm>
#include <cctype>

namespace SuperTerminal {

// =============================================================================
// Constructor / Destructor
// =============================================================================

CartLoader::CartLoader()
    : m_db(nullptr)
    , m_readOnly(false)
    , m_metadataCached(false)
{
}

CartLoader::~CartLoader() {
    unloadCart();
}

CartLoader::CartLoader(CartLoader&& other) noexcept
    : m_db(other.m_db)
    , m_cartPath(std::move(other.m_cartPath))
    , m_readOnly(other.m_readOnly)
    , m_metadataCached(other.m_metadataCached)
    , m_metadata(std::move(other.m_metadata))
    , m_lastError(std::move(other.m_lastError))
{
    other.m_db = nullptr;
    other.m_readOnly = false;
    other.m_metadataCached = false;
}

CartLoader& CartLoader::operator=(CartLoader&& other) noexcept {
    if (this != &other) {
        unloadCart();
        
        m_db = other.m_db;
        m_cartPath = std::move(other.m_cartPath);
        m_readOnly = other.m_readOnly;
        m_metadataCached = other.m_metadataCached;
        m_metadata = std::move(other.m_metadata);
        m_lastError = std::move(other.m_lastError);
        
        other.m_db = nullptr;
        other.m_readOnly = false;
        other.m_metadataCached = false;
    }
    return *this;
}

// =============================================================================
// Cart Loading
// =============================================================================

bool CartLoader::loadCart(const std::string& cartPath, bool readOnly) {
    // Unload any existing cart
    if (m_db != nullptr) {
        unloadCart();
    }
    
    m_readOnly = readOnly;
    
    // Open the database
    if (!openDatabase(cartPath, readOnly)) {
        return false;
    }
    
    // Validate schema
    if (!checkSchema()) {
        setError("Cart has invalid or missing schema");
        closeDatabase();
        return false;
    }
    
    // Cache metadata
    if (!loadMetadata()) {
        setError("Failed to load cart metadata");
        closeDatabase();
        return false;
    }
    
    m_cartPath = cartPath;
    return true;
}

void CartLoader::unloadCart() {
    if (m_db != nullptr) {
        closeDatabase();
    }
    
    m_cartPath.clear();
    m_readOnly = false;
    m_metadataCached = false;
    m_metadata = CartMetadata();
    clearLastError();
}

// =============================================================================
// Cart Creation
// =============================================================================

bool CartLoader::createCart(const std::string& cartPath, const CartMetadata& metadata) {
    // Check if file already exists
    struct stat buffer;
    if (stat(cartPath.c_str(), &buffer) == 0) {
        fprintf(stderr, "[CartLoader] ERROR: File already exists: %s\n", cartPath.c_str());
        return false; // File exists
    }
    
    // Open/create new database
    sqlite3* db = nullptr;
    int rc = sqlite3_open(cartPath.c_str(), &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[CartLoader] ERROR: Failed to create database: %s (error: %s)\n", 
                cartPath.c_str(), sqlite3_errmsg(db ? db : nullptr));
        if (db) sqlite3_close(db);
        return false;
    }
    fprintf(stderr, "[CartLoader] Database created: %s\n", cartPath.c_str());
    
    // Begin transaction
    char* errMsg = nullptr;
    rc = sqlite3_exec(db, "BEGIN TRANSACTION", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[CartLoader] ERROR: Failed to begin transaction: %s\n", errMsg ? errMsg : "unknown");
        if (errMsg) sqlite3_free(errMsg);
        sqlite3_close(db);
        return false;
    }
    fprintf(stderr, "[CartLoader] Transaction started\n");
    
    // Create schema_version table
    const char* createSchemaVersion = 
        "CREATE TABLE schema_version ("
        "    version INTEGER PRIMARY KEY,"
        "    created_at TEXT NOT NULL,"
        "    description TEXT"
        ")";
    
    rc = sqlite3_exec(db, createSchemaVersion, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[CartLoader] ERROR: Failed to create schema_version table: %s\n", errMsg ? errMsg : "unknown");
        if (errMsg) sqlite3_free(errMsg);
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }
    
    // Insert schema version
    const char* insertVersion = 
        "INSERT INTO schema_version (version, created_at, description) "
        "VALUES (1, datetime('now'), 'FasterBASIC Cart Format v1.0')";
    
    rc = sqlite3_exec(db, insertVersion, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) sqlite3_free(errMsg);
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }
    
    // Create metadata table
    const char* createMetadata = 
        "CREATE TABLE metadata ("
        "    key TEXT PRIMARY KEY NOT NULL,"
        "    value TEXT NOT NULL"
        ")";
    
    rc = sqlite3_exec(db, createMetadata, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) sqlite3_free(errMsg);
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }
    
    // Create program table
    const char* createProgram = 
        "CREATE TABLE program ("
        "    id INTEGER PRIMARY KEY CHECK (id = 1),"
        "    source TEXT NOT NULL,"
        "    format TEXT DEFAULT 'basic' CHECK (format IN ('basic', 'compiled')),"
        "    entry_point TEXT,"
        "    notes TEXT"
        ")";
    
    rc = sqlite3_exec(db, createProgram, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) sqlite3_free(errMsg);
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }
    
    // Insert empty program
    const char* insertProgram = 
        "INSERT INTO program (id, source, format) VALUES (1, '', 'basic')";
    
    rc = sqlite3_exec(db, insertProgram, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) sqlite3_free(errMsg);
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }
    
    // Create sprites table
    const char* createSprites = 
        "CREATE TABLE sprites ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name TEXT UNIQUE NOT NULL,"
        "    data BLOB NOT NULL,"
        "    width INTEGER NOT NULL,"
        "    height INTEGER NOT NULL,"
        "    format TEXT DEFAULT 'png' CHECK (format IN ('png', 'rgba', 'indexed')),"
        "    notes TEXT,"
        "    created_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ")";
    
    rc = sqlite3_exec(db, createSprites, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) sqlite3_free(errMsg);
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }
    
    // Create tilesets table
    const char* createTilesets = 
        "CREATE TABLE tilesets ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name TEXT UNIQUE NOT NULL,"
        "    data BLOB NOT NULL,"
        "    tile_width INTEGER NOT NULL,"
        "    tile_height INTEGER NOT NULL,"
        "    tiles_across INTEGER NOT NULL,"
        "    tiles_down INTEGER NOT NULL,"
        "    format TEXT DEFAULT 'png' CHECK (format IN ('png', 'rgba', 'indexed')),"
        "    margin INTEGER DEFAULT 0,"
        "    spacing INTEGER DEFAULT 0,"
        "    notes TEXT,"
        "    created_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ")";
    
    rc = sqlite3_exec(db, createTilesets, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) sqlite3_free(errMsg);
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }
    
    // Create sounds table
    const char* createSounds = 
        "CREATE TABLE sounds ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name TEXT UNIQUE NOT NULL,"
        "    data BLOB NOT NULL,"
        "    format TEXT DEFAULT 'wav' CHECK (format IN ('wav', 'raw', 'aiff')),"
        "    sample_rate INTEGER DEFAULT 44100,"
        "    channels INTEGER DEFAULT 1,"
        "    bits_per_sample INTEGER DEFAULT 16,"
        "    notes TEXT,"
        "    created_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ")";
    
    rc = sqlite3_exec(db, createSounds, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) sqlite3_free(errMsg);
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }
    
    // Create music table
    const char* createMusic = 
        "CREATE TABLE music ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name TEXT UNIQUE NOT NULL,"
        "    data BLOB NOT NULL,"
        "    format TEXT DEFAULT 'sid' CHECK (format IN ('sid', 'mod', 'xm', 's3m', 'it', 'abc', 'midi')),"
        "    duration_seconds REAL,"
        "    notes TEXT,"
        "    created_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ")";
    
    rc = sqlite3_exec(db, createMusic, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) sqlite3_free(errMsg);
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }
    
    // Create data_files table
    const char* createDataFiles = 
        "CREATE TABLE data_files ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    path TEXT UNIQUE NOT NULL,"
        "    data BLOB NOT NULL,"
        "    mime_type TEXT,"
        "    notes TEXT,"
        "    created_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ")";
    
    rc = sqlite3_exec(db, createDataFiles, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) sqlite3_free(errMsg);
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }
    
    // Insert metadata
    sqlite3_stmt* stmt = nullptr;
    const char* insertMetadata = "INSERT INTO metadata (key, value) VALUES (?, ?)";
    
    rc = sqlite3_prepare_v2(db, insertMetadata, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }
    
    // Helper lambda to insert metadata
    auto insertMeta = [&](const char* key, const std::string& value) -> bool {
        if (value.empty()) return true; // Skip empty values
        
        sqlite3_reset(stmt);
        sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
        
        int result = sqlite3_step(stmt);
        return (result == SQLITE_DONE);
    };
    
    // Insert all metadata fields
    bool success = true;
    success = success && insertMeta("title", metadata.title.empty() ? "Untitled Cart" : metadata.title);
    success = success && insertMeta("author", metadata.author);
    success = success && insertMeta("version", metadata.version);
    success = success && insertMeta("description", metadata.description);
    success = success && insertMeta("date_created", metadata.dateCreated);
    success = success && insertMeta("engine_version", metadata.engineVersion.empty() ? "FBRunner3 1.0" : metadata.engineVersion);
    success = success && insertMeta("category", metadata.category);
    success = success && insertMeta("icon", metadata.icon);
    success = success && insertMeta("screenshot", metadata.screenshot);
    success = success && insertMeta("website", metadata.website);
    success = success && insertMeta("license", metadata.license);
    success = success && insertMeta("rating", metadata.rating);
    success = success && insertMeta("players", metadata.players);
    success = success && insertMeta("controls", metadata.controls);
    
    sqlite3_finalize(stmt);
    
    if (!success) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }
    
    // Commit transaction
    rc = sqlite3_exec(db, "COMMIT", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) sqlite3_free(errMsg);
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }
    
    // Close database
    sqlite3_close(db);
    fprintf(stderr, "[CartLoader] Cart created successfully: %s\n", cartPath.c_str());
    return true;
}

// =============================================================================
// Validation
// =============================================================================

CartValidationResult CartLoader::validateCart(const std::string& cartPath) {
    CartValidationResult result;
    
    // Check file exists
    struct stat buffer;
    if (stat(cartPath.c_str(), &buffer) != 0) {
        result.addError("Cart file does not exist: " + cartPath);
        return result;
    }
    
    // Try to open as SQLite database
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(cartPath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
    
    if (rc != SQLITE_OK) {
        result.addError("Not a valid SQLite database");
        if (db) sqlite3_close(db);
        return result;
    }
    
    // Check for required tables
    const char* requiredTables[] = {
        "schema_version",
        "metadata",
        "program"
    };
    
    for (const char* table : requiredTables) {
        std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
        sqlite3_stmt* stmt = nullptr;
        
        rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, table, -1, SQLITE_STATIC);
            rc = sqlite3_step(stmt);
            
            if (rc != SQLITE_ROW) {
                result.addError(std::string("Missing required table: ") + table);
            }
            sqlite3_finalize(stmt);
        }
    }
    
    // Check schema version
    std::string versionSQL = "SELECT version FROM schema_version LIMIT 1";
    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, versionSQL.c_str(), -1, &stmt, nullptr);
    
    if (rc == SQLITE_OK) {
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            int version = sqlite3_column_int(stmt, 0);
            if (version > getSupportedSchemaVersion()) {
                result.addError("Cart schema version " + std::to_string(version) + 
                              " is not supported (max: " + std::to_string(getSupportedSchemaVersion()) + ")");
            }
        } else {
            result.addError("No schema version found");
        }
        sqlite3_finalize(stmt);
    }
    
    // Check metadata (warnings only - metadata can be set later)
    const char* recommendedMetadata[] = {
        "title", "author", "version", "description", "date_created", "engine_version"
    };
    
    for (const char* key : recommendedMetadata) {
        std::string sql = "SELECT value FROM metadata WHERE key=?";
        sqlite3_stmt* stmt = nullptr;
        
        rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
            rc = sqlite3_step(stmt);
            
            if (rc != SQLITE_ROW) {
                result.addWarning(std::string("Missing metadata: ") + key);
            } else {
                const char* value = (const char*)sqlite3_column_text(stmt, 0);
                if (!value || strlen(value) == 0) {
                    result.addWarning(std::string("Empty metadata value for: ") + key);
                }
            }
            sqlite3_finalize(stmt);
        }
    }
    
    // Check program exists (warning only - program can be saved later)
    std::string programSQL = "SELECT COUNT(*) FROM program";
    stmt = nullptr;
    rc = sqlite3_prepare_v2(db, programSQL.c_str(), -1, &stmt, nullptr);
    
    if (rc == SQLITE_OK) {
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            int count = sqlite3_column_int(stmt, 0);
            if (count == 0) {
                result.addWarning("No program found in cart (can be saved later)");
            } else if (count > 1) {
                result.addWarning("Multiple programs found (only first will be used)");
            }
        }
        sqlite3_finalize(stmt);
    }
    
    sqlite3_close(db);
    return result;
}

CartValidationResult CartLoader::validate() const {
    if (!isLoaded()) {
        CartValidationResult result;
        result.addError("No cart is currently loaded");
        return result;
    }
    
    return validateCart(m_cartPath);
}

// =============================================================================
// Metadata Access
// =============================================================================

CartMetadata CartLoader::getMetadata() const {
    if (!isLoaded()) {
        return CartMetadata();
    }
    
    if (!m_metadataCached) {
        loadMetadata();
    }
    
    return m_metadata;
}

std::string CartLoader::getMetadataValue(const std::string& key) const {
    if (!isLoaded()) {
        return "";
    }
    
    std::string sql = "SELECT value FROM metadata WHERE key=?";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return "";
    }
    
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    
    std::string value;
    if (rc == SQLITE_ROW) {
        const char* text = (const char*)sqlite3_column_text(stmt, 0);
        if (text) {
            value = text;
        }
    }
    
    sqlite3_finalize(stmt);
    return value;
}

int CartLoader::getSchemaVersion() const {
    if (!isLoaded()) {
        return 0;
    }
    
    int version = 0;
    queryInt("SELECT version FROM schema_version LIMIT 1", version);
    return version;
}

// =============================================================================
// Program Access
// =============================================================================

CartProgram CartLoader::getProgram() const {
    CartProgram program;
    
    if (!isLoaded()) {
        return program;
    }
    
    std::string sql = "SELECT source, format, entry_point, notes FROM program LIMIT 1";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError("Failed to query program");
        return program;
    }
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char* source = (const char*)sqlite3_column_text(stmt, 0);
        const char* format = (const char*)sqlite3_column_text(stmt, 1);
        const char* entryPoint = (const char*)sqlite3_column_text(stmt, 2);
        const char* notes = (const char*)sqlite3_column_text(stmt, 3);
        
        if (source) program.source = source;
        if (format) program.format = format;
        if (entryPoint) program.entryPoint = entryPoint;
        if (notes) program.notes = notes;
    } else {
        setError("No program found in cart");
    }
    
    sqlite3_finalize(stmt);
    return program;
}

std::string CartLoader::getProgramSource() const {
    return getProgram().source;
}

// =============================================================================
// Asset Queries
// =============================================================================

std::vector<std::string> CartLoader::listSprites() const {
    std::vector<std::string> names;
    if (!isLoaded()) return names;
    
    std::string sql = "SELECT name FROM sprites ORDER BY name";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return names;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(stmt, 0);
        if (name) names.push_back(name);
    }
    
    sqlite3_finalize(stmt);
    return names;
}

std::vector<std::string> CartLoader::listTilesets() const {
    std::vector<std::string> names;
    if (!isLoaded()) return names;
    
    std::string sql = "SELECT name FROM tilesets ORDER BY name";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return names;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(stmt, 0);
        if (name) names.push_back(name);
    }
    
    sqlite3_finalize(stmt);
    return names;
}

std::vector<std::string> CartLoader::listSounds() const {
    std::vector<std::string> names;
    if (!isLoaded()) return names;
    
    std::string sql = "SELECT name FROM sounds ORDER BY name";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return names;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(stmt, 0);
        if (name) names.push_back(name);
    }
    
    sqlite3_finalize(stmt);
    return names;
}

std::vector<std::string> CartLoader::listMusic() const {
    std::vector<std::string> names;
    if (!isLoaded()) return names;
    
    std::string sql = "SELECT name FROM music ORDER BY name";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return names;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(stmt, 0);
        if (name) names.push_back(name);
    }
    
    sqlite3_finalize(stmt);
    return names;
}

std::vector<std::string> CartLoader::listDataFiles() const {
    std::vector<std::string> paths;
    if (!isLoaded()) return paths;
    
    std::string sql = "SELECT path FROM data_files ORDER BY path";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return paths;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* path = (const char*)sqlite3_column_text(stmt, 0);
        if (path) paths.push_back(path);
    }
    
    sqlite3_finalize(stmt);
    return paths;
}

bool CartLoader::hasSprite(const std::string& name) const {
    return assetExists("sprites", stripExtension(name));
}

bool CartLoader::hasTileset(const std::string& name) const {
    return assetExists("tilesets", stripExtension(name));
}

bool CartLoader::hasSound(const std::string& name) const {
    return assetExists("sounds", stripExtension(name));
}

bool CartLoader::hasMusic(const std::string& name) const {
    return assetExists("music", stripExtension(name));
}

bool CartLoader::hasDataFile(const std::string& path) const {
    return assetExists("data_files", path);
}

// =============================================================================
// Asset Loading
// =============================================================================

bool CartLoader::loadSprite(const std::string& name, CartSprite& outSprite) const {
    if (!isLoaded()) {
        setError("No cart loaded");
        return false;
    }
    
    std::string cleanName = stripExtension(name);
    std::string sql = "SELECT name, data, width, height, format, notes FROM sprites WHERE name=?";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError("Failed to prepare sprite query");
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, cleanName.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        setError("Sprite not found: " + cleanName);
        return false;
    }
    
    const char* assetName = (const char*)sqlite3_column_text(stmt, 0);
    const void* blob = sqlite3_column_blob(stmt, 1);
    int blobSize = sqlite3_column_bytes(stmt, 1);
    int width = sqlite3_column_int(stmt, 2);
    int height = sqlite3_column_int(stmt, 3);
    const char* format = (const char*)sqlite3_column_text(stmt, 4);
    const char* notes = (const char*)sqlite3_column_text(stmt, 5);
    
    if (assetName) outSprite.name = assetName;
    if (blob && blobSize > 0) {
        outSprite.data.resize(blobSize);
        memcpy(outSprite.data.data(), blob, blobSize);
    }
    outSprite.width = width;
    outSprite.height = height;
    if (format) outSprite.format = format;
    if (notes) outSprite.notes = notes;
    
    sqlite3_finalize(stmt);
    return true;
}

bool CartLoader::loadTileset(const std::string& name, CartTileset& outTileset) const {
    if (!isLoaded()) {
        setError("No cart loaded");
        return false;
    }
    
    std::string cleanName = stripExtension(name);
    std::string sql = "SELECT name, data, tile_width, tile_height, tiles_across, tiles_down, "
                     "format, margin, spacing, notes FROM tilesets WHERE name=?";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError("Failed to prepare tileset query");
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, cleanName.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        setError("Tileset not found: " + cleanName);
        return false;
    }
    
    const char* assetName = (const char*)sqlite3_column_text(stmt, 0);
    const void* blob = sqlite3_column_blob(stmt, 1);
    int blobSize = sqlite3_column_bytes(stmt, 1);
    
    if (assetName) outTileset.name = assetName;
    if (blob && blobSize > 0) {
        outTileset.data.resize(blobSize);
        memcpy(outTileset.data.data(), blob, blobSize);
    }
    outTileset.tileWidth = sqlite3_column_int(stmt, 2);
    outTileset.tileHeight = sqlite3_column_int(stmt, 3);
    outTileset.tilesAcross = sqlite3_column_int(stmt, 4);
    outTileset.tilesDown = sqlite3_column_int(stmt, 5);
    
    const char* format = (const char*)sqlite3_column_text(stmt, 6);
    if (format) outTileset.format = format;
    
    outTileset.margin = sqlite3_column_int(stmt, 7);
    outTileset.spacing = sqlite3_column_int(stmt, 8);
    
    const char* notes = (const char*)sqlite3_column_text(stmt, 9);
    if (notes) outTileset.notes = notes;
    
    sqlite3_finalize(stmt);
    return true;
}

bool CartLoader::loadSound(const std::string& name, CartSound& outSound) const {
    if (!isLoaded()) {
        setError("No cart loaded");
        return false;
    }
    
    std::string cleanName = stripExtension(name);
    std::string sql = "SELECT name, data, format, sample_rate, channels, bits_per_sample, notes "
                     "FROM sounds WHERE name=?";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError("Failed to prepare sound query");
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, cleanName.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        setError("Sound not found: " + cleanName);
        return false;
    }
    
    const char* assetName = (const char*)sqlite3_column_text(stmt, 0);
    const void* blob = sqlite3_column_blob(stmt, 1);
    int blobSize = sqlite3_column_bytes(stmt, 1);
    
    if (assetName) outSound.name = assetName;
    if (blob && blobSize > 0) {
        outSound.data.resize(blobSize);
        memcpy(outSound.data.data(), blob, blobSize);
    }
    
    const char* format = (const char*)sqlite3_column_text(stmt, 2);
    if (format) outSound.format = format;
    
    outSound.sampleRate = sqlite3_column_int(stmt, 3);
    outSound.channels = sqlite3_column_int(stmt, 4);
    outSound.bitsPerSample = sqlite3_column_int(stmt, 5);
    
    const char* notes = (const char*)sqlite3_column_text(stmt, 6);
    if (notes) outSound.notes = notes;
    
    sqlite3_finalize(stmt);
    return true;
}

bool CartLoader::loadMusic(const std::string& name, CartMusic& outMusic) const {
    if (!isLoaded()) {
        setError("No cart loaded");
        return false;
    }
    
    std::string cleanName = stripExtension(name);
    std::string sql = "SELECT name, data, format, duration_seconds, notes FROM music WHERE name=?";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError("Failed to prepare music query");
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, cleanName.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        setError("Music not found: " + cleanName);
        return false;
    }
    
    const char* assetName = (const char*)sqlite3_column_text(stmt, 0);
    const void* blob = sqlite3_column_blob(stmt, 1);
    int blobSize = sqlite3_column_bytes(stmt, 1);
    
    if (assetName) outMusic.name = assetName;
    if (blob && blobSize > 0) {
        outMusic.data.resize(blobSize);
        memcpy(outMusic.data.data(), blob, blobSize);
    }
    
    const char* format = (const char*)sqlite3_column_text(stmt, 2);
    if (format) outMusic.format = format;
    
    outMusic.durationSeconds = sqlite3_column_double(stmt, 3);
    
    const char* notes = (const char*)sqlite3_column_text(stmt, 4);
    if (notes) outMusic.notes = notes;
    
    sqlite3_finalize(stmt);
    return true;
}

bool CartLoader::loadDataFile(const std::string& path, CartDataFile& outDataFile) const {
    if (!isLoaded()) {
        setError("No cart loaded");
        return false;
    }
    
    std::string sql = "SELECT path, data, mime_type, notes FROM data_files WHERE path=?";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError("Failed to prepare data file query");
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        setError("Data file not found: " + path);
        return false;
    }
    
    const char* filePath = (const char*)sqlite3_column_text(stmt, 0);
    const void* blob = sqlite3_column_blob(stmt, 1);
    int blobSize = sqlite3_column_bytes(stmt, 1);
    const char* mimeType = (const char*)sqlite3_column_text(stmt, 2);
    const char* notes = (const char*)sqlite3_column_text(stmt, 3);
    
    if (filePath) outDataFile.path = filePath;
    if (blob && blobSize > 0) {
        outDataFile.data.resize(blobSize);
        memcpy(outDataFile.data.data(), blob, blobSize);
    }
    if (mimeType) outDataFile.mimeType = mimeType;
    if (notes) outDataFile.notes = notes;
    
    sqlite3_finalize(stmt);
    return true;
}

// =============================================================================
// Statistics
// =============================================================================

int CartLoader::getSpriteCount() const {
    return getAssetCount("sprites");
}

int CartLoader::getTilesetCount() const {
    return getAssetCount("tilesets");
}

int CartLoader::getSoundCount() const {
    return getAssetCount("sounds");
}

int CartLoader::getMusicCount() const {
    return getAssetCount("music");
}

int CartLoader::getDataFileCount() const {
    return getAssetCount("data_files");
}

int64_t CartLoader::getCartSize() const {
    if (!isLoaded()) {
        return 0;
    }
    
    struct stat st;
    if (stat(m_cartPath.c_str(), &st) == 0) {
        return st.st_size;
    }
    
    return 0;
}

int64_t CartLoader::getTotalAssetSize() const {
    if (!isLoaded()) {
        return 0;
    }
    
    int64_t total = 0;
    const char* tables[] = {"sprites", "tilesets", "sounds", "music", "data_files"};
    
    for (const char* table : tables) {
        std::string sql = std::string("SELECT SUM(LENGTH(data)) FROM ") + table;
        int64_t size = 0;
        if (queryInt64(sql, size)) {
            total += size;
        }
    }
    
    return total;
}

// =============================================================================
// Utility
// =============================================================================

bool CartLoader::isCartFile(const std::string& filePath) {
    // Check file extension
    if (filePath.length() < 4) {
        return false;
    }
    
    std::string ext = filePath.substr(filePath.length() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext != ".crt") {
        return false;
    }
    
    // Try to open as SQLite database
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(filePath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
    
    if (rc != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }
    
    // Check for schema_version table
    const char* sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='schema_version'";
    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    
    bool valid = false;
    if (rc == SQLITE_OK) {
        rc = sqlite3_step(stmt);
        valid = (rc == SQLITE_ROW);
        sqlite3_finalize(stmt);
    }
    
    sqlite3_close(db);
    return valid;
}

// =============================================================================
// Internal Helpers
// =============================================================================

void CartLoader::setError(const std::string& message) const {
    m_lastError = message;
}

bool CartLoader::openDatabase(const std::string& path, bool readOnly) {
    int flags = readOnly ? SQLITE_OPEN_READONLY : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    int rc = sqlite3_open_v2(path.c_str(), &m_db, flags, nullptr);
    
    if (rc != SQLITE_OK) {
        setError("Failed to open cart database: " + std::string(sqlite3_errmsg(m_db)));
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        return false;
    }
    
    return true;
}

void CartLoader::closeDatabase() {
    if (m_db != nullptr) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool CartLoader::checkSchema() const {
    if (!m_db) return false;
    
    // Check for required tables
    const char* requiredTables[] = {
        "schema_version",
        "metadata", 
        "program"
    };
    
    for (const char* table : requiredTables) {
        if (!tableExists(table)) {
            return false;
        }
    }
    
    return true;
}

bool CartLoader::loadMetadata() const {
    if (!m_db) return false;
    
    m_metadata = CartMetadata();
    
    // Load all metadata into structure
    m_metadata.title = getMetadataValue("title");
    m_metadata.author = getMetadataValue("author");
    m_metadata.version = getMetadataValue("version");
    m_metadata.description = getMetadataValue("description");
    m_metadata.dateCreated = getMetadataValue("date_created");
    m_metadata.engineVersion = getMetadataValue("engine_version");
    m_metadata.category = getMetadataValue("category");
    m_metadata.icon = getMetadataValue("icon");
    m_metadata.screenshot = getMetadataValue("screenshot");
    m_metadata.website = getMetadataValue("website");
    m_metadata.license = getMetadataValue("license");
    m_metadata.rating = getMetadataValue("rating");
    m_metadata.players = getMetadataValue("players");
    m_metadata.controls = getMetadataValue("controls");
    
    m_metadataCached = true;
    return true;
}

std::string CartLoader::stripExtension(const std::string& name) {
    size_t dotPos = name.find_last_of('.');
    if (dotPos != std::string::npos) {
        return name.substr(0, dotPos);
    }
    return name;
}

bool CartLoader::executeSQL(const std::string& sql) const {
    if (!m_db) return false;
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        if (errMsg) {
            setError(errMsg);
            sqlite3_free(errMsg);
        }
        return false;
    }
    
    return true;
}

bool CartLoader::queryString(const std::string& sql, std::string& outValue) const {
    if (!m_db) return false;
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        return false;
    }
    
    rc = sqlite3_step(stmt);
    bool success = false;
    
    if (rc == SQLITE_ROW) {
        const char* text = (const char*)sqlite3_column_text(stmt, 0);
        if (text) {
            outValue = text;
            success = true;
        }
    }
    
    sqlite3_finalize(stmt);
    return success;
}

bool CartLoader::queryInt(const std::string& sql, int& outValue) const {
    if (!m_db) return false;
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        return false;
    }
    
    rc = sqlite3_step(stmt);
    bool success = false;
    
    if (rc == SQLITE_ROW) {
        outValue = sqlite3_column_int(stmt, 0);
        success = true;
    }
    
    sqlite3_finalize(stmt);
    return success;
}

bool CartLoader::queryInt64(const std::string& sql, int64_t& outValue) const {
    if (!m_db) return false;
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        return false;
    }
    
    rc = sqlite3_step(stmt);
    bool success = false;
    
    if (rc == SQLITE_ROW) {
        outValue = sqlite3_column_int64(stmt, 0);
        success = true;
    }
    
    sqlite3_finalize(stmt);
    return success;
}

bool CartLoader::tableExists(const std::string& tableName) const {
    if (!m_db) return false;
    
    std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    
    bool exists = (rc == SQLITE_ROW);
    sqlite3_finalize(stmt);
    
    return exists;
}

bool CartLoader::assetExists(const std::string& table, const std::string& name) const {
    if (!m_db) return false;
    
    std::string nameCol = (table == "data_files") ? "path" : "name";
    std::string sql = "SELECT 1 FROM " + table + " WHERE " + nameCol + "=? LIMIT 1";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    
    bool exists = (rc == SQLITE_ROW);
    sqlite3_finalize(stmt);
    
    return exists;
}

int CartLoader::getAssetCount(const std::string& table) const {
    if (!m_db) return 0;
    
    std::string sql = "SELECT COUNT(*) FROM " + table;
    int count = 0;
    queryInt(sql, count);
    return count;
}

bool CartLoader::loadBlob(const std::string& table, const std::string& nameColumn,
                          const std::string& name, std::vector<uint8_t>& outData) const {
    if (!m_db) return false;
    
    std::string sql = "SELECT data FROM " + table + " WHERE " + nameColumn + "=?";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    
    bool success = false;
    if (rc == SQLITE_ROW) {
        const void* blob = sqlite3_column_blob(stmt, 0);
        int blobSize = sqlite3_column_bytes(stmt, 0);
        
        if (blob && blobSize > 0) {
            outData.resize(blobSize);
            memcpy(outData.data(), blob, blobSize);
            success = true;
        }
    }
    
    sqlite3_finalize(stmt);
    return success;
}

// =============================================================================
// Cart Modification (Read-Write Mode)
// =============================================================================

bool CartLoader::updateProgram(const std::string& source) {
    if (!checkWritable()) return false;
    
    std::string sql = "UPDATE program SET source=? WHERE id=1";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError("Failed to prepare update statement");
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, source.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        setError("Failed to update program");
        return false;
    }
    
    return true;
}

bool CartLoader::addSprite(const CartSprite& sprite) {
    if (!checkWritable()) return false;
    
    std::string sql = "INSERT OR REPLACE INTO sprites (name, data, width, height, format, notes) "
                     "VALUES (?, ?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError(std::string("Failed to prepare sprite insert: ") + sqlite3_errmsg(m_db));
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, sprite.name.c_str(), -1, SQLITE_TRANSIENT);
    // Handle empty blob safely
    if (sprite.data.empty()) {
        sqlite3_finalize(stmt);
        setError("Sprite data cannot be empty for '" + sprite.name + "'");
        return false;
    }
    sqlite3_bind_blob(stmt, 2, sprite.data.data(), static_cast<int>(sprite.data.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, sprite.width);
    sqlite3_bind_int(stmt, 4, sprite.height);
    sqlite3_bind_text(stmt, 5, sprite.format.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, sprite.notes.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::string error = std::string("Failed to add sprite '") + sprite.name + "': " + sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        setError(error);
        return false;
    }
    sqlite3_finalize(stmt);
    
    return true;
}

bool CartLoader::addTileset(const CartTileset& tileset) {
    if (!checkWritable()) return false;
    
    std::string sql = "INSERT OR REPLACE INTO tilesets (name, data, tile_width, tile_height, "
                     "tiles_across, tiles_down, format, margin, spacing, notes) "
                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError(std::string("Failed to prepare tileset insert: ") + sqlite3_errmsg(m_db));
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, tileset.name.c_str(), -1, SQLITE_TRANSIENT);
    // Handle empty blob safely
    if (tileset.data.empty()) {
        sqlite3_finalize(stmt);
        setError("Tileset data cannot be empty for '" + tileset.name + "'");
        return false;
    }
    sqlite3_bind_blob(stmt, 2, tileset.data.data(), static_cast<int>(tileset.data.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, tileset.tileWidth);
    sqlite3_bind_int(stmt, 4, tileset.tileHeight);
    sqlite3_bind_int(stmt, 5, tileset.tilesAcross);
    sqlite3_bind_int(stmt, 6, tileset.tilesDown);
    sqlite3_bind_text(stmt, 7, tileset.format.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, tileset.margin);
    sqlite3_bind_int(stmt, 9, tileset.spacing);
    sqlite3_bind_text(stmt, 10, tileset.notes.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::string error = std::string("Failed to add tileset '") + tileset.name + "': " + sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        setError(error);
        return false;
    }
    sqlite3_finalize(stmt);
    
    return true;
}

bool CartLoader::addSound(const CartSound& sound) {
    if (!checkWritable()) return false;
    
    std::string sql = "INSERT OR REPLACE INTO sounds (name, data, format, sample_rate, "
                     "channels, bits_per_sample, notes) VALUES (?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError(std::string("Failed to prepare sound insert: ") + sqlite3_errmsg(m_db));
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, sound.name.c_str(), -1, SQLITE_TRANSIENT);
    // Handle empty blob safely
    if (sound.data.empty()) {
        sqlite3_finalize(stmt);
        setError("Sound data cannot be empty for '" + sound.name + "'");
        return false;
    }
    sqlite3_bind_blob(stmt, 2, sound.data.data(), static_cast<int>(sound.data.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, sound.format.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, sound.sampleRate);
    sqlite3_bind_int(stmt, 5, sound.channels);
    sqlite3_bind_int(stmt, 6, sound.bitsPerSample);
    sqlite3_bind_text(stmt, 7, sound.notes.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::string error = std::string("Failed to add sound '") + sound.name + "': " + sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        setError(error);
        return false;
    }
    sqlite3_finalize(stmt);
    
    return true;
}

bool CartLoader::addMusic(const CartMusic& music) {
    if (!checkWritable()) return false;
    
    std::string sql = "INSERT OR REPLACE INTO music (name, data, format, duration_seconds, notes) "
                     "VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError(std::string("Failed to prepare music insert: ") + sqlite3_errmsg(m_db));
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, music.name.c_str(), -1, SQLITE_TRANSIENT);
    // Handle empty blob safely - SQLite requires special handling for empty blobs with NOT NULL constraint
    if (music.data.empty()) {
        sqlite3_finalize(stmt);
        setError("Music data cannot be empty for '" + music.name + "'");
        return false;
    }
    sqlite3_bind_blob(stmt, 2, music.data.data(), static_cast<int>(music.data.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, music.format.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, music.durationSeconds);
    sqlite3_bind_text(stmt, 5, music.notes.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::string error = std::string("Failed to add music '") + music.name + "': " + sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        setError(error);
        return false;
    }
    sqlite3_finalize(stmt);
    
    return true;
}

bool CartLoader::addDataFile(const CartDataFile& dataFile) {
    if (!checkWritable()) return false;
    
    std::string sql = "INSERT OR REPLACE INTO data_files (path, data, mime_type, notes) "
                     "VALUES (?, ?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError("Failed to prepare data file insert");
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, dataFile.path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 2, dataFile.data.data(), dataFile.data.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, dataFile.mimeType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, dataFile.notes.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        setError("Failed to add data file");
        return false;
    }
    
    return true;
}

bool CartLoader::deleteSprite(const std::string& name) {
    if (!checkWritable()) return false;
    
    std::string sql = "DELETE FROM sprites WHERE name=?";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError("Failed to prepare sprite delete");
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        setError("Failed to delete sprite");
        return false;
    }
    
    return true;
}

bool CartLoader::deleteTileset(const std::string& name) {
    if (!checkWritable()) return false;
    
    std::string sql = "DELETE FROM tilesets WHERE name=?";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError("Failed to prepare tileset delete");
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        setError("Failed to delete tileset");
        return false;
    }
    
    return true;
}

bool CartLoader::deleteSound(const std::string& name) {
    if (!checkWritable()) return false;
    
    std::string sql = "DELETE FROM sounds WHERE name=?";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError("Failed to prepare sound delete");
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        setError("Failed to delete sound");
        return false;
    }
    
    return true;
}

bool CartLoader::deleteMusic(const std::string& name) {
    if (!checkWritable()) return false;
    
    std::string sql = "DELETE FROM music WHERE name=?";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError("Failed to prepare music delete");
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        setError("Failed to delete music");
        return false;
    }
    
    return true;
}

bool CartLoader::deleteDataFile(const std::string& path) {
    if (!checkWritable()) return false;
    
    std::string sql = "DELETE FROM data_files WHERE path=?";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError("Failed to prepare data file delete");
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        setError("Failed to delete data file");
        return false;
    }
    
    return true;
}

bool CartLoader::deleteAssetByName(const std::string& name, std::string& outDeletedFrom) {
    if (!checkWritable()) return false;
    
    // Try sprites table
    {
        std::string sql = "DELETE FROM sprites WHERE name=?";
        sqlite3_stmt* stmt = nullptr;
        
        int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            
            if (rc == SQLITE_DONE && sqlite3_changes(m_db) > 0) {
                outDeletedFrom = "sprites";
                return true;
            }
        }
    }
    
    // Try tilesets table
    {
        std::string sql = "DELETE FROM tilesets WHERE name=?";
        sqlite3_stmt* stmt = nullptr;
        
        int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            
            if (rc == SQLITE_DONE && sqlite3_changes(m_db) > 0) {
                outDeletedFrom = "tilesets";
                return true;
            }
        }
    }
    
    // Try sounds table
    {
        std::string sql = "DELETE FROM sounds WHERE name=?";
        sqlite3_stmt* stmt = nullptr;
        
        int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            
            if (rc == SQLITE_DONE && sqlite3_changes(m_db) > 0) {
                outDeletedFrom = "sounds";
                return true;
            }
        }
    }
    
    // Try music table
    {
        std::string sql = "DELETE FROM music WHERE name=?";
        sqlite3_stmt* stmt = nullptr;
        
        int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            
            if (rc == SQLITE_DONE && sqlite3_changes(m_db) > 0) {
                outDeletedFrom = "music";
                return true;
            }
        }
    }
    
    // Try data_files table with various path patterns
    std::vector<std::string> pathsToTry = {
        name,                      // Direct name
        "scripts/" + name,         // Script path
        "music/" + name,           // Music path
        "sounds/" + name,          // Sounds path
        "data/" + name             // Data path
    };
    
    for (const auto& path : pathsToTry) {
        std::string sql = "DELETE FROM data_files WHERE path=?";
        sqlite3_stmt* stmt = nullptr;
        
        int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            
            if (rc == SQLITE_DONE && sqlite3_changes(m_db) > 0) {
                outDeletedFrom = "data_files (" + path + ")";
                return true;
            }
        }
    }
    
    // Not found in any table
    return false;
}

bool CartLoader::updateMetadata(const std::string& key, const std::string& value) {
    if (!checkWritable()) return false;
    
    std::string sql = "INSERT OR REPLACE INTO metadata (key, value) VALUES (?, ?)";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError("Failed to prepare metadata update");
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        setError("Failed to update metadata");
        return false;
    }
    
    // Invalidate cached metadata
    m_metadataCached = false;
    
    return true;
}

bool CartLoader::commit() {
    if (!m_db) return false;
    
    // SQLite auto-commits, but we can explicitly checkpoint WAL
    sqlite3_wal_checkpoint(m_db, nullptr);
    return true;
}

// =============================================================================
// Internal Helpers (Write Support)
// =============================================================================

bool CartLoader::checkWritable() const {
    if (!m_db) {
        setError("No cart loaded");
        return false;
    }
    
    if (m_readOnly) {
        setError("Cart is opened in read-only mode");
        return false;
    }
    
    return true;
}

bool CartLoader::beginTransaction() {
    if (!m_db) return false;
    return executeSQL("BEGIN TRANSACTION");
}

bool CartLoader::commitTransaction() {
    if (!m_db) return false;
    return executeSQL("COMMIT");
}

bool CartLoader::rollbackTransaction() {
    if (!m_db) return false;
    return executeSQL("ROLLBACK");
}

} // namespace SuperTerminal