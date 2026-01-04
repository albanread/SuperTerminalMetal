//
// st_api_assets.cpp
// SuperTerminal v2 - C API Asset Implementation
//
// Asset loading and management API functions
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "superterminal_api.h"
#include "st_api_context.h"
#include "../Assets/AssetManager.h"
#include "../Assets/AssetMetadata.h"
#include <string>
#include <vector>
#include <cstring>

using namespace STApi;
using namespace SuperTerminal;

// =============================================================================
// Helper Functions
// =============================================================================

namespace {
    // Thread-local storage for string returns (to avoid memory management issues)
    thread_local std::string g_lastReturnedString;
    thread_local std::vector<const char*> g_lastReturnedNames;
    
    // Convert STAssetType to AssetKind
    AssetKind assetTypeToKind(STAssetType type) {
        switch (type) {
            case ST_ASSET_IMAGE:  return AssetKind::IMAGE;
            case ST_ASSET_SOUND:  return AssetKind::SOUND;
            case ST_ASSET_MUSIC:  return AssetKind::MUSIC;
            case ST_ASSET_FONT:   return AssetKind::FONT;
            case ST_ASSET_SPRITE: return AssetKind::SPRITE;
            case ST_ASSET_DATA:   return AssetKind::DATA;
            default:              return AssetKind::UNKNOWN;
        }
    }
    
    // Convert AssetKind to STAssetType (best effort)
    STAssetType assetKindToType(AssetKind kind) {
        switch (kind) {
            case AssetKind::IMAGE:   return ST_ASSET_IMAGE;
            case AssetKind::SOUND:   return ST_ASSET_SOUND;
            case AssetKind::MUSIC:   return ST_ASSET_MUSIC;
            case AssetKind::FONT:    return ST_ASSET_FONT;
            case AssetKind::SPRITE:  return ST_ASSET_SPRITE;
            case AssetKind::DATA:    return ST_ASSET_DATA;
            default:                 return ST_ASSET_DATA;
        }
    }
    
    // Convert integer type to AssetKind
    AssetKind intToAssetKind(int type) {
        if (type == -1) return AssetKind::UNKNOWN;
        return assetTypeToKind((STAssetType)type);
    }
}

// =============================================================================
// Initialization
// =============================================================================

ST_API bool st_asset_init(const char* db_path, size_t max_cache_size) {
    if (!db_path) {
        ST_SET_ERROR("Database path is null");
        return false;
    }
    
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    if (!assets) {
        ST_SET_ERROR("AssetManager not available - not initialized in framework");
        return false;
    }
    
    if (assets->isInitialized()) {
        ST_SET_ERROR("Asset manager already initialized");
        return false;
    }
    
    AssetManagerConfig config;
    if (max_cache_size > 0) {
        config.maxCacheSize = max_cache_size;
    }
    
    if (!assets->initialize(db_path, config)) {
        ST_SET_ERROR(assets->getLastError().c_str());
        return false;
    }
    
    ST_CLEAR_ERROR();
    return true;
}

ST_API void st_asset_shutdown(void) {
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    if (!assets) {
        return;
    }
    
    assets->shutdown();
    ST_CLEAR_ERROR();
}

ST_API bool st_asset_is_initialized(void) {
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    if (!assets) {
        return false;
    }
    
    return assets->isInitialized();
}

// =============================================================================
// Loading / Unloading
// =============================================================================

ST_API STAssetID st_asset_load(const char* name) {
    if (!name) {
        ST_SET_ERROR("Asset name is null");
        return -1;
    }
    
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR_RET(assets, "AssetManager", -1);
    
    if (!assets->isInitialized()) {
        ST_SET_ERROR("Asset manager not initialized - call st_asset_init() first");
        return -1;
    }
    
    // Load asset from database
    AssetHandle handle = assets->loadAsset(name);
    if (handle == INVALID_ASSET_HANDLE) {
        ST_SET_ERROR(assets->getLastError().c_str());
        return -1;
    }
    
    // Register handle in context
    int32_t apiHandle = ST_CONTEXT.registerAsset(name);
    
    ST_CLEAR_ERROR();
    return apiHandle;
}

ST_API STAssetID st_asset_load_file(const char* path, STAssetType type) {
    if (!path) {
        ST_SET_ERROR("Asset path is null");
        return -1;
    }
    
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR_RET(assets, "AssetManager", -1);
    
    if (!assets->isInitialized()) {
        ST_SET_ERROR("Asset manager not initialized - call st_asset_init() first");
        return -1;
    }
    
    // Generate asset name from file path (use filename without extension)
    std::string pathStr(path);
    size_t lastSlash = pathStr.find_last_of("/\\");
    size_t lastDot = pathStr.find_last_of('.');
    std::string name = pathStr.substr(
        lastSlash == std::string::npos ? 0 : lastSlash + 1,
        lastDot == std::string::npos ? std::string::npos : lastDot - (lastSlash == std::string::npos ? 0 : lastSlash + 1)
    );
    
    // Import the asset
    AssetKind kind = assetTypeToKind(type);
    if (!assets->importAsset(path, name, kind)) {
        ST_SET_ERROR(assets->getLastError().c_str());
        return -1;
    }
    
    // Load the asset
    AssetHandle handle = assets->loadAsset(name);
    if (handle == INVALID_ASSET_HANDLE) {
        ST_SET_ERROR(assets->getLastError().c_str());
        return -1;
    }
    
    // Register handle in context
    int32_t apiHandle = ST_CONTEXT.registerAsset(name);
    
    ST_CLEAR_ERROR();
    return apiHandle;
}

ST_API STAssetID st_asset_load_builtin(const char* name, STAssetType type) {
    if (!name) {
        ST_SET_ERROR("Asset name is null");
        return -1;
    }
    
    ST_LOCK;
    
    // TODO: Implement builtin asset loading
    // This will require:
    // - Embedded asset data (compiled into binary)
    // - Asset registry/manifest
    // - Type-specific loaders
    
    ST_SET_ERROR("Builtin asset loading not yet implemented");
    return -1;
}

ST_API void st_asset_unload(STAssetID asset) {
    ST_LOCK;
    
    if (asset < 0) {
        ST_SET_ERROR("Invalid asset ID");
        return;
    }
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR(assets, "AssetManager");
    
    // Get asset name from handle
    std::string name = ST_CONTEXT.getAssetName(asset);
    if (name.empty()) {
        ST_SET_ERROR("Invalid asset handle");
        return;
    }
    
    // Unload from asset manager (this is actually done via handle, but we need the internal handle)
    // Since AssetManager returns its own handles, we need to get it
    const AssetMetadata* metadata = assets->getAssetMetadataByName(name);
    if (metadata) {
        // Note: AssetManager::unloadAsset takes its own handle type
        // For now, we just unregister from our tracking
        // The asset will be evicted from cache based on LRU policy
    }
    
    // Unregister from context
    ST_CONTEXT.unregisterAsset(asset);
    
    ST_CLEAR_ERROR();
}

ST_API bool st_asset_is_loaded(const char* name) {
    if (!name) {
        return false;
    }
    
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    if (!assets || !assets->isInitialized()) {
        return false;
    }
    
    return assets->isAssetLoaded(name);
}

// =============================================================================
// Import / Export
// =============================================================================

ST_API bool st_asset_import(const char* file_path, const char* asset_name, int type) {
    if (!file_path || !asset_name) {
        ST_SET_ERROR("File path or asset name is null");
        return false;
    }
    
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR_RET(assets, "AssetManager", false);
    
    if (!assets->isInitialized()) {
        ST_SET_ERROR("Asset manager not initialized - call st_asset_init() first");
        return false;
    }
    
    AssetKind kind = intToAssetKind(type);
    
    if (!assets->importAsset(file_path, asset_name, kind)) {
        ST_SET_ERROR(assets->getLastError().c_str());
        return false;
    }
    
    ST_CLEAR_ERROR();
    return true;
}

ST_API int st_asset_import_directory(const char* directory, bool recursive) {
    if (!directory) {
        ST_SET_ERROR("Directory path is null");
        return -1;
    }
    
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR_RET(assets, "AssetManager", -1);
    
    if (!assets->isInitialized()) {
        ST_SET_ERROR("Asset manager not initialized - call st_asset_init() first");
        return -1;
    }
    
    int count = assets->importDirectory(directory, recursive);
    if (count < 0) {
        ST_SET_ERROR(assets->getLastError().c_str());
        return -1;
    }
    
    ST_CLEAR_ERROR();
    return count;
}

ST_API bool st_asset_export(const char* asset_name, const char* file_path) {
    if (!asset_name || !file_path) {
        ST_SET_ERROR("Asset name or file path is null");
        return false;
    }
    
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR_RET(assets, "AssetManager", false);
    
    if (!assets->isInitialized()) {
        ST_SET_ERROR("Asset manager not initialized - call st_asset_init() first");
        return false;
    }
    
    if (!assets->exportAsset(asset_name, file_path)) {
        ST_SET_ERROR(assets->getLastError().c_str());
        return false;
    }
    
    ST_CLEAR_ERROR();
    return true;
}

ST_API bool st_asset_delete(const char* asset_name) {
    if (!asset_name) {
        ST_SET_ERROR("Asset name is null");
        return false;
    }
    
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR_RET(assets, "AssetManager", false);
    
    if (!assets->isInitialized()) {
        ST_SET_ERROR("Asset manager not initialized - call st_asset_init() first");
        return false;
    }
    
    if (!assets->deleteAsset(asset_name)) {
        ST_SET_ERROR(assets->getLastError().c_str());
        return false;
    }
    
    ST_CLEAR_ERROR();
    return true;
}

// =============================================================================
// Data Access
// =============================================================================

ST_API const void* st_asset_get_data(STAssetID asset) {
    ST_LOCK;
    
    if (asset < 0) {
        ST_SET_ERROR("Invalid asset ID");
        return nullptr;
    }
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR_RET(assets, "AssetManager", nullptr);
    
    // Get asset name from handle
    std::string name = ST_CONTEXT.getAssetName(asset);
    if (name.empty()) {
        ST_SET_ERROR("Invalid asset handle");
        return nullptr;
    }
    
    // Get metadata (which includes data if loaded)
    const AssetMetadata* metadata = assets->getAssetMetadataByName(name);
    if (!metadata) {
        ST_SET_ERROR("Asset not found or not loaded");
        return nullptr;
    }
    
    if (metadata->data.empty()) {
        ST_SET_ERROR("Asset has no data");
        return nullptr;
    }
    
    ST_CLEAR_ERROR();
    return metadata->data.data();
}

ST_API size_t st_asset_get_size(STAssetID asset) {
    ST_LOCK;
    
    if (asset < 0) {
        ST_SET_ERROR("Invalid asset ID");
        return 0;
    }
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR_RET(assets, "AssetManager", 0);
    
    // Get asset name from handle
    std::string name = ST_CONTEXT.getAssetName(asset);
    if (name.empty()) {
        ST_SET_ERROR("Invalid asset handle");
        return 0;
    }
    
    // Get metadata
    const AssetMetadata* metadata = assets->getAssetMetadataByName(name);
    if (!metadata) {
        ST_SET_ERROR("Asset not found");
        return 0;
    }
    
    ST_CLEAR_ERROR();
    return metadata->getDataSize();
}

ST_API int st_asset_get_type(STAssetID asset) {
    ST_LOCK;
    
    if (asset < 0) {
        ST_SET_ERROR("Invalid asset ID");
        return -1;
    }
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR_RET(assets, "AssetManager", -1);
    
    // Get asset name from handle
    std::string name = ST_CONTEXT.getAssetName(asset);
    if (name.empty()) {
        ST_SET_ERROR("Invalid asset handle");
        return -1;
    }
    
    // Get metadata
    const AssetMetadata* metadata = assets->getAssetMetadataByName(name);
    if (!metadata) {
        ST_SET_ERROR("Asset not found");
        return -1;
    }
    
    ST_CLEAR_ERROR();
    return (int)assetKindToType(metadata->kind);
}

ST_API const char* st_asset_get_name(STAssetID asset) {
    ST_LOCK;
    
    if (asset < 0) {
        ST_SET_ERROR("Invalid asset ID");
        return nullptr;
    }
    
    // Get asset name from handle
    g_lastReturnedString = ST_CONTEXT.getAssetName(asset);
    if (g_lastReturnedString.empty()) {
        ST_SET_ERROR("Invalid asset handle");
        return nullptr;
    }
    
    ST_CLEAR_ERROR();
    return g_lastReturnedString.c_str();
}

// =============================================================================
// Queries
// =============================================================================

ST_API bool st_asset_exists(const char* name) {
    if (!name) {
        return false;
    }
    
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    if (!assets || !assets->isInitialized()) {
        return false;
    }
    
    return assets->hasAsset(name);
}

ST_API int st_asset_list(int type, const char** names, int max_count) {
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR_RET(assets, "AssetManager", 0);
    
    if (!assets->isInitialized()) {
        ST_SET_ERROR("Asset manager not initialized");
        return 0;
    }
    
    AssetKind kind = intToAssetKind(type);
    std::vector<std::string> assetNames = assets->listAssets(kind);
    
    int count = (int)assetNames.size();
    
    // If names array provided, fill it
    if (names && max_count > 0) {
        // Store strings in thread-local storage to keep them alive
        g_lastReturnedNames.clear();
        g_lastReturnedNames.reserve(std::min(count, max_count));
        
        int copyCount = std::min(count, max_count);
        for (int i = 0; i < copyCount; i++) {
            g_lastReturnedNames.push_back(assetNames[i].c_str());
            names[i] = g_lastReturnedNames[i];
        }
    }
    
    ST_CLEAR_ERROR();
    return count;
}

ST_API int st_asset_list_builtin(STAssetType type, const char** names, int max_count) {
    ST_LOCK;
    
    // TODO: Implement builtin asset listing
    // This will enumerate all embedded assets of a given type
    // Examples:
    // - ST_ASSET_SOUND: "beep", "coin", "jump", etc.
    // - ST_ASSET_SPRITE: "player", "enemy", "bullet", etc.
    // - ST_ASSET_FONT: "default", "mono", "pixel", etc.
    
    // For now, return 0 (no assets)
    return 0;
}

ST_API int st_asset_search(const char* pattern, const char** names, int max_count) {
    if (!pattern) {
        ST_SET_ERROR("Search pattern is null");
        return 0;
    }
    
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR_RET(assets, "AssetManager", 0);
    
    if (!assets->isInitialized()) {
        ST_SET_ERROR("Asset manager not initialized");
        return 0;
    }
    
    std::vector<std::string> assetNames = assets->searchAssets(pattern);
    
    int count = (int)assetNames.size();
    
    // If names array provided, fill it
    if (names && max_count > 0) {
        // Store strings in thread-local storage to keep them alive
        g_lastReturnedNames.clear();
        g_lastReturnedNames.reserve(std::min(count, max_count));
        
        int copyCount = std::min(count, max_count);
        for (int i = 0; i < copyCount; i++) {
            g_lastReturnedNames.push_back(assetNames[i].c_str());
            names[i] = g_lastReturnedNames[i];
        }
    }
    
    ST_CLEAR_ERROR();
    return count;
}

ST_API int st_asset_get_count(int type) {
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR_RET(assets, "AssetManager", 0);
    
    if (!assets->isInitialized()) {
        ST_SET_ERROR("Asset manager not initialized");
        return 0;
    }
    
    AssetKind kind = intToAssetKind(type);
    int64_t count = assets->getAssetCount(kind);
    
    ST_CLEAR_ERROR();
    return (int)count;
}

// =============================================================================
// Cache Management
// =============================================================================

ST_API void st_asset_clear_cache(void) {
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR(assets, "AssetManager");
    
    if (!assets->isInitialized()) {
        ST_SET_ERROR("Asset manager not initialized");
        return;
    }
    
    assets->clearCache();
    ST_CLEAR_ERROR();
}

ST_API size_t st_asset_get_cache_size(void) {
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR_RET(assets, "AssetManager", 0);
    
    if (!assets->isInitialized()) {
        return 0;
    }
    
    return assets->getCacheSize();
}

ST_API int st_asset_get_cached_count(void) {
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR_RET(assets, "AssetManager", 0);
    
    if (!assets->isInitialized()) {
        return 0;
    }
    
    return (int)assets->getCachedAssetCount();
}

ST_API void st_asset_set_max_cache_size(size_t max_size) {
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR(assets, "AssetManager");
    
    if (!assets->isInitialized()) {
        ST_SET_ERROR("Asset manager not initialized");
        return;
    }
    
    assets->setMaxCacheSize(max_size);
    ST_CLEAR_ERROR();
}

// =============================================================================
// Statistics
// =============================================================================

ST_API double st_asset_get_hit_rate(void) {
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR_RET(assets, "AssetManager", 0.0);
    
    if (!assets->isInitialized()) {
        return 0.0;
    }
    
    AssetManagerStats stats = assets->getCacheStatistics();
    return stats.hitRate;
}

ST_API size_t st_asset_get_database_size(void) {
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    ST_CHECK_PTR_RET(assets, "AssetManager", 0);
    
    if (!assets->isInitialized()) {
        return 0;
    }
    
    AssetStatistics stats = assets->getDatabaseStatistics();
    return stats.totalDataSize;
}

// =============================================================================
// Error Handling
// =============================================================================

ST_API const char* st_asset_get_error(void) {
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    if (!assets) {
        return ST_CONTEXT.getLastError();
    }
    
    std::string error = assets->getLastError();
    if (error.empty()) {
        return ST_CONTEXT.getLastError();
    }
    
    g_lastReturnedString = error;
    return g_lastReturnedString.c_str();
}

ST_API void st_asset_clear_error(void) {
    ST_LOCK;
    
    AssetManager* assets = ST_CONTEXT.assets();
    if (assets) {
        assets->clearLastError();
    }
    
    ST_CLEAR_ERROR();
}