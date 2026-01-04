//
// TilemapManager.h
// SuperTerminal Framework - Tilemap System
//
// TilemapManager class for high-level tilemap management
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef TILEMAP_MANAGER_H
#define TILEMAP_MANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace SuperTerminal {

// Forward declarations
class Tilemap;
class Tileset;
class TilemapLayer;
class Camera;

/// TilemapManager: High-level management of tilemaps, layers, and camera
///
/// Features:
/// - Manage multiple tilemap instances
/// - Layer creation and ordering
/// - Camera management
/// - Centralized update and rendering coordination
/// - Resource management (tilemaps, tilesets)
///
/// Thread Safety: Not thread-safe. Should be used from render thread only.
///
class TilemapManager {
public:
    // =================================================================
    // Construction
    // =================================================================
    
    TilemapManager();
    ~TilemapManager();
    
    // Disable copy, allow move
    TilemapManager(const TilemapManager&) = delete;
    TilemapManager& operator=(const TilemapManager&) = delete;
    TilemapManager(TilemapManager&&) noexcept;
    TilemapManager& operator=(TilemapManager&&) noexcept;
    
    // =================================================================
    // Initialization
    // =================================================================
    
    /// Initialize manager with viewport size
    void initialize(float viewportWidth, float viewportHeight);
    
    /// Shutdown and cleanup
    void shutdown();
    
    /// Check if initialized
    inline bool isInitialized() const { return m_initialized; }
    
    // =================================================================
    // Tilemap Management
    // =================================================================
    
    /// Create new tilemap
    /// @param width Width in tiles
    /// @param height Height in tiles
    /// @param tileWidth Tile width in pixels
    /// @param tileHeight Tile height in pixels
    /// @param name Tilemap name
    /// @return Tilemap handle ID
    int32_t createTilemap(int32_t width, int32_t height, 
                          int32_t tileWidth, int32_t tileHeight,
                          const std::string& name = "");
    
    /// Get tilemap by ID
    std::shared_ptr<Tilemap> getTilemap(int32_t tilemapID) const;
    
    /// Remove tilemap
    void removeTilemap(int32_t tilemapID);
    
    /// Get all tilemap IDs
    std::vector<int32_t> getTilemapIDs() const;
    
    // =================================================================
    // Asset Management
    // =================================================================
    
    /// Load tilemap from asset database
    /// @param assetName Asset name in database
    /// @return Tilemap handle ID or -1 on error
    int32_t loadTilemapFromAsset(const std::string& assetName);
    
    /// Save tilemap to asset database
    /// @param tilemapID Tilemap handle ID
    /// @param assetName Asset name to save as
    /// @param layers Layers to save (optional, saves all if null)
    /// @param saveCamera Whether to save camera state
    /// @return true on success
    bool saveTilemapToAsset(int32_t tilemapID, const std::string& assetName,
                            const std::vector<int32_t>* layerIDs = nullptr,
                            bool saveCamera = true);
    
    /// Load tilemap from file
    /// @param filePath File path (.stmap, .json, .csv)
    /// @param outLayers Output layer IDs (created layers)
    /// @return Tilemap handle ID or -1 on error
    int32_t loadTilemapFromFile(const std::string& filePath,
                                std::vector<int32_t>& outLayers);
    
    /// Save tilemap to file
    /// @param tilemapID Tilemap handle ID
    /// @param filePath File path (.stmap, .json, .csv)
    /// @param layerIDs Layers to save (optional, saves all if null)
    /// @param saveCamera Whether to save camera state
    /// @return true on success
    bool saveTilemapToFile(int32_t tilemapID, const std::string& filePath,
                           const std::vector<int32_t>* layerIDs = nullptr,
                           bool saveCamera = true);
    
    // =================================================================
    // Tileset Management
    // =================================================================
    
    /// Create tileset (placeholder - will be implemented with renderer)
    /// @param name Tileset name
    /// @return Tileset handle ID
    int32_t createTileset(const std::string& name);
    
    /// Get tileset by ID
    std::shared_ptr<Tileset> getTileset(int32_t tilesetID) const;
    
    /// Remove tileset
    void removeTileset(int32_t tilesetID);
    
    /// Get all tileset IDs
    std::vector<int32_t> getTilesetIDs() const;
    
    // =================================================================
    // Layer Management
    // =================================================================
    
    /// Create layer
    /// @param name Layer name
    /// @return Layer handle ID
    int32_t createLayer(const std::string& name = "");
    
    /// Get layer by ID
    std::shared_ptr<TilemapLayer> getLayer(int32_t layerID) const;
    
    /// Remove layer
    void removeLayer(int32_t layerID);
    
    /// Get all layer IDs (sorted by Z-order)
    std::vector<int32_t> getLayerIDs() const;
    
    /// Get layer count
    inline size_t getLayerCount() const { return m_layers.size(); }
    
    /// Assign tilemap to layer
    void setLayerTilemap(int32_t layerID, int32_t tilemapID);
    
    /// Assign tileset to layer
    void setLayerTileset(int32_t layerID, int32_t tilesetID);
    
    /// Sort layers by Z-order
    void sortLayers();
    
    // =================================================================
    // Camera
    // =================================================================
    
    /// Get camera
    inline std::shared_ptr<Camera> getCamera() const { return m_camera; }
    
    /// Set camera position
    void setCameraPosition(float x, float y);
    
    /// Move camera
    void moveCamera(float dx, float dy);
    
    /// Set camera zoom
    void setCameraZoom(float zoom);
    
    /// Camera follow target
    void cameraFollow(float targetX, float targetY);
    
    /// Set camera bounds
    void setCameraBounds(float x, float y, float width, float height);
    
    /// Camera shake
    void cameraShake(float magnitude, float duration);
    
    // =================================================================
    // Update
    // =================================================================
    
    /// Update all layers and camera
    /// @param dt Delta time in seconds
    void update(float dt);
    
    // =================================================================
    // Rendering (placeholder for now)
    // =================================================================
    
    /// Get layers ready for rendering (sorted by Z-order)
    std::vector<std::shared_ptr<TilemapLayer>> getRenderableLayers() const;
    
    // =================================================================
    // Utilities
    // =================================================================
    
    /// Set viewport size
    void setViewportSize(float width, float height);
    
    /// Get viewport size
    inline float getViewportWidth() const { return m_viewportWidth; }
    inline float getViewportHeight() const { return m_viewportHeight; }
    
    /// Clear all data
    void clear();
    
    /// Get statistics
    void getStats(int32_t& tilemapCount, int32_t& tilesetCount, int32_t& layerCount) const;
    
    /// Get last error message
    std::string getLastError() const;
    
private:
    // Initialization flag
    bool m_initialized = false;
    
    // Viewport
    float m_viewportWidth = 800.0f;
    float m_viewportHeight = 600.0f;
    
    // Camera
    std::shared_ptr<Camera> m_camera;
    
    // Resources
    std::unordered_map<int32_t, std::shared_ptr<Tilemap>> m_tilemaps;
    std::unordered_map<int32_t, std::shared_ptr<Tileset>> m_tilesets;
    std::unordered_map<int32_t, std::shared_ptr<TilemapLayer>> m_layers;
    
    // ID generators
    int32_t m_nextTilemapID = 1;
    int32_t m_nextTilesetID = 1;
    int32_t m_nextLayerID = 1;
    
    // Layer order cache
    std::vector<int32_t> m_layerOrder;
    bool m_layerOrderDirty = true;
    
    // Error handling
    mutable std::string m_lastError;
    
    // Helper methods
    void setError(const std::string& error) const;
    void clearError() const;
};

} // namespace SuperTerminal

#endif // TILEMAP_MANAGER_H