//
// TilemapManager.cpp
// SuperTerminal Framework - Tilemap System
//
// TilemapManager implementation
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "TilemapManager.h"
#include "Tilemap.h"
#include "Tileset.h"
#include "TilemapLayer.h"
#include "Camera.h"
#include "TilemapFormat.h"
#include "../Assets/AssetManager.h"
#include "../Assets/AssetMetadata.h"
#include <algorithm>

namespace SuperTerminal {

// =================================================================
// Construction
// =================================================================

TilemapManager::TilemapManager()
    : m_initialized(false)
    , m_viewportWidth(800.0f)
    , m_viewportHeight(600.0f)
    , m_nextTilemapID(1)
    , m_nextTilesetID(1)
    , m_nextLayerID(1)
    , m_layerOrderDirty(true)
{
}

TilemapManager::~TilemapManager() {
    shutdown();
}

TilemapManager::TilemapManager(TilemapManager&& other) noexcept
    : m_initialized(other.m_initialized)
    , m_viewportWidth(other.m_viewportWidth)
    , m_viewportHeight(other.m_viewportHeight)
    , m_camera(std::move(other.m_camera))
    , m_tilemaps(std::move(other.m_tilemaps))
    , m_tilesets(std::move(other.m_tilesets))
    , m_layers(std::move(other.m_layers))
    , m_nextTilemapID(other.m_nextTilemapID)
    , m_nextTilesetID(other.m_nextTilesetID)
    , m_nextLayerID(other.m_nextLayerID)
    , m_layerOrder(std::move(other.m_layerOrder))
    , m_layerOrderDirty(other.m_layerOrderDirty)
{
    other.m_initialized = false;
}

TilemapManager& TilemapManager::operator=(TilemapManager&& other) noexcept {
    if (this != &other) {
        shutdown();
        
        m_initialized = other.m_initialized;
        m_viewportWidth = other.m_viewportWidth;
        m_viewportHeight = other.m_viewportHeight;
        m_camera = std::move(other.m_camera);
        m_tilemaps = std::move(other.m_tilemaps);
        m_tilesets = std::move(other.m_tilesets);
        m_layers = std::move(other.m_layers);
        m_nextTilemapID = other.m_nextTilemapID;
        m_nextTilesetID = other.m_nextTilesetID;
        m_nextLayerID = other.m_nextLayerID;
        m_layerOrder = std::move(other.m_layerOrder);
        m_layerOrderDirty = other.m_layerOrderDirty;
        
        other.m_initialized = false;
    }
    return *this;
}

// =================================================================
// Initialization
// =================================================================

void TilemapManager::initialize(float viewportWidth, float viewportHeight) {
    if (m_initialized) {
        return;
    }
    
    m_viewportWidth = viewportWidth;
    m_viewportHeight = viewportHeight;
    
    // Create camera
    m_camera = std::make_shared<Camera>(viewportWidth, viewportHeight);
    
    m_initialized = true;
}

void TilemapManager::shutdown() {
    clear();
    m_camera.reset();
    m_initialized = false;
}

// =================================================================
// Tilemap Management
// =================================================================

int32_t TilemapManager::createTilemap(int32_t width, int32_t height,
                                      int32_t tileWidth, int32_t tileHeight,
                                      const std::string& name)
{
    auto tilemap = std::make_shared<Tilemap>(width, height, tileWidth, tileHeight);
    tilemap->setName(name);
    
    int32_t id = m_nextTilemapID++;
    m_tilemaps[id] = tilemap;
    
    return id;
}

std::shared_ptr<Tilemap> TilemapManager::getTilemap(int32_t tilemapID) const {
    auto it = m_tilemaps.find(tilemapID);
    return (it != m_tilemaps.end()) ? it->second : nullptr;
}

void TilemapManager::removeTilemap(int32_t tilemapID) {
    m_tilemaps.erase(tilemapID);
}

std::vector<int32_t> TilemapManager::getTilemapIDs() const {
    std::vector<int32_t> ids;
    ids.reserve(m_tilemaps.size());
    
    for (const auto& pair : m_tilemaps) {
        ids.push_back(pair.first);
    }
    
    return ids;
}

// =================================================================
// Asset Management
// =================================================================

int32_t TilemapManager::loadTilemapFromAsset(const std::string& assetName) {
    clearError();
    
    // TODO: Access AssetManager from context
    // For now, return error
    setError("Asset loading requires AssetManager integration");
    return -1;
}

bool TilemapManager::saveTilemapToAsset(int32_t tilemapID, const std::string& assetName,
                                        const std::vector<int32_t>* layerIDs,
                                        bool saveCamera) {
    clearError();
    
    // TODO: Access AssetManager from context
    // For now, return error
    setError("Asset saving requires AssetManager integration");
    return false;
}

int32_t TilemapManager::loadTilemapFromFile(const std::string& filePath,
                                            std::vector<int32_t>& outLayers) {
    clearError();
    
    std::shared_ptr<Tilemap> tilemap;
    std::vector<std::shared_ptr<TilemapLayer>> layers;
    CameraState cameraState;
    
    // Detect format and load
    std::string format = TilemapFormat::detectFormat(filePath);
    
    bool success = false;
    if (format == "binary") {
        success = TilemapFormat::loadBinary(filePath, tilemap, layers, &cameraState);
    } else if (format == "json") {
        success = TilemapFormat::loadJSON(filePath, tilemap, layers, &cameraState);
    } else if (format == "csv") {
        success = TilemapFormat::importCSV(filePath, tilemap);
        // CSV doesn't include layers, create a default one
        if (success && tilemap) {
            auto layer = std::make_shared<TilemapLayer>("default");
            layer->setTilemap(tilemap);
            layers.push_back(layer);
        }
    } else {
        setError("Unknown or unsupported file format: " + format);
        return -1;
    }
    
    if (!success) {
        setError("Failed to load tilemap: " + TilemapFormat::getLastError());
        return -1;
    }
    
    if (!tilemap) {
        setError("Failed to create tilemap from file");
        return -1;
    }
    
    // Register tilemap
    int32_t tilemapID = m_nextTilemapID++;
    m_tilemaps[tilemapID] = tilemap;
    
    // Register layers
    outLayers.clear();
    for (auto& layer : layers) {
        int32_t layerID = m_nextLayerID++;
        layer->setID(layerID);
        m_layers[layerID] = layer;
        outLayers.push_back(layerID);
        m_layerOrderDirty = true;
    }
    
    // Apply camera state if available
    if (m_camera && (cameraState.maxX > 0 || cameraState.maxY > 0)) {
        m_camera->setPosition(cameraState.x, cameraState.y);
        m_camera->setZoom(cameraState.zoom);
        if (cameraState.maxX > 0 && cameraState.maxY > 0) {
            m_camera->setBounds(cameraState.minX, cameraState.minY, 
                               cameraState.maxX, cameraState.maxY);
        }
    }
    
    return tilemapID;
}

bool TilemapManager::saveTilemapToFile(int32_t tilemapID, const std::string& filePath,
                                       const std::vector<int32_t>* layerIDs,
                                       bool saveCamera) {
    clearError();
    
    auto tilemap = getTilemap(tilemapID);
    if (!tilemap) {
        setError("Invalid tilemap ID");
        return false;
    }
    
    // Collect layers to save
    std::vector<TilemapLayer*> layersToSave;
    
    if (layerIDs) {
        // Save specific layers
        for (int32_t layerID : *layerIDs) {
            auto layer = getLayer(layerID);
            if (layer) {
                layersToSave.push_back(layer.get());
            }
        }
    } else {
        // Save all layers
        for (auto& pair : m_layers) {
            layersToSave.push_back(pair.second.get());
        }
    }
    
    // Detect format and save
    std::string format = TilemapFormat::detectFormat(filePath);
    
    TilemapSaveOptions options;
    options.saveCamera = saveCamera;
    options.prettyPrint = true;
    
    bool success = false;
    if (format == "binary") {
        success = TilemapFormat::saveBinary(*tilemap, filePath, &layersToSave, 
                                           m_camera.get(), options);
    } else if (format == "json") {
        success = TilemapFormat::saveJSON(*tilemap, filePath, &layersToSave, 
                                         m_camera.get(), options);
    } else if (format == "csv") {
        success = TilemapFormat::exportCSV(*tilemap, filePath, 0);
    } else {
        setError("Unknown or unsupported file format: " + format);
        return false;
    }
    
    if (!success) {
        setError("Failed to save tilemap: " + TilemapFormat::getLastError());
        return false;
    }
    
    return true;
}

// =================================================================
// Tileset Management
// =================================================================

int32_t TilemapManager::createTileset(const std::string& name) {
    auto tileset = std::make_shared<Tileset>();
    tileset->setName(name);
    
    int32_t id = m_nextTilesetID++;
    m_tilesets[id] = tileset;
    
    return id;
}

std::shared_ptr<Tileset> TilemapManager::getTileset(int32_t tilesetID) const {
    auto it = m_tilesets.find(tilesetID);
    return (it != m_tilesets.end()) ? it->second : nullptr;
}

void TilemapManager::removeTileset(int32_t tilesetID) {
    m_tilesets.erase(tilesetID);
}

std::vector<int32_t> TilemapManager::getTilesetIDs() const {
    std::vector<int32_t> ids;
    ids.reserve(m_tilesets.size());
    
    for (const auto& pair : m_tilesets) {
        ids.push_back(pair.first);
    }
    
    return ids;
}

// =================================================================
// Layer Management
// =================================================================

int32_t TilemapManager::createLayer(const std::string& name) {
    auto layer = std::make_shared<TilemapLayer>(name);
    
    int32_t id = m_nextLayerID++;
    layer->setID(id);
    m_layers[id] = layer;
    
    m_layerOrderDirty = true;
    
    return id;
}

std::shared_ptr<TilemapLayer> TilemapManager::getLayer(int32_t layerID) const {
    auto it = m_layers.find(layerID);
    return (it != m_layers.end()) ? it->second : nullptr;
}

void TilemapManager::removeLayer(int32_t layerID) {
    m_layers.erase(layerID);
    m_layerOrderDirty = true;
}

std::vector<int32_t> TilemapManager::getLayerIDs() const {
    if (m_layerOrderDirty) {
        // Need to sort - cast away const (not ideal but acceptable for caching)
        const_cast<TilemapManager*>(this)->sortLayers();
    }
    
    return m_layerOrder;
}

void TilemapManager::setLayerTilemap(int32_t layerID, int32_t tilemapID) {
    auto layer = getLayer(layerID);
    auto tilemap = getTilemap(tilemapID);
    
    if (layer && tilemap) {
        layer->setTilemap(tilemap);
    }
}

void TilemapManager::setLayerTileset(int32_t layerID, int32_t tilesetID) {
    auto layer = getLayer(layerID);
    auto tileset = getTileset(tilesetID);
    
    if (layer && tileset) {
        layer->setTileset(tileset);
    }
}

void TilemapManager::sortLayers() {
    m_layerOrder.clear();
    m_layerOrder.reserve(m_layers.size());
    
    // Collect all layer IDs
    for (const auto& pair : m_layers) {
        m_layerOrder.push_back(pair.first);
    }
    
    // Sort by Z-order
    std::sort(m_layerOrder.begin(), m_layerOrder.end(),
              [this](int32_t a, int32_t b) {
                  auto layerA = m_layers.at(a);
                  auto layerB = m_layers.at(b);
                  return layerA->getZOrder() < layerB->getZOrder();
              });
    
    m_layerOrderDirty = false;
}

// =================================================================
// Camera
// =================================================================

void TilemapManager::setCameraPosition(float x, float y) {
    if (m_camera) {
        m_camera->setPosition(x, y);
    }
}

void TilemapManager::moveCamera(float dx, float dy) {
    if (m_camera) {
        m_camera->move(dx, dy);
    }
}

void TilemapManager::setCameraZoom(float zoom) {
    if (m_camera) {
        m_camera->setZoom(zoom);
    }
}

void TilemapManager::cameraFollow(float targetX, float targetY) {
    if (m_camera) {
        m_camera->follow(targetX, targetY);
    }
}

void TilemapManager::setCameraBounds(float x, float y, float width, float height) {
    if (m_camera) {
        m_camera->setBounds(x, y, width, height);
    }
}

void TilemapManager::cameraShake(float magnitude, float duration) {
    if (m_camera) {
        m_camera->shake(magnitude, duration);
    }
}

// =================================================================
// Update
// =================================================================

void TilemapManager::update(float dt) {
    // Update camera
    if (m_camera) {
        m_camera->update(dt);
    }
    
    // Update all layers
    for (auto& pair : m_layers) {
        pair.second->update(dt);
    }
}

// =================================================================
// Rendering
// =================================================================

std::vector<std::shared_ptr<TilemapLayer>> TilemapManager::getRenderableLayers() const {
    std::vector<std::shared_ptr<TilemapLayer>> result;
    result.reserve(m_layers.size());
    
    // Get layer IDs in Z-order
    auto layerIDs = getLayerIDs();
    
    // Collect renderable layers
    for (int32_t id : layerIDs) {
        auto layer = getLayer(id);
        if (layer && layer->shouldRender()) {
            result.push_back(layer);
        }
    }
    
    return result;
}

// =================================================================
// Utilities
// =================================================================

void TilemapManager::setViewportSize(float width, float height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
    
    if (m_camera) {
        m_camera->setViewportSize(width, height);
    }
}

void TilemapManager::clear() {
    m_tilemaps.clear();
    m_tilesets.clear();
    m_layers.clear();
    m_layerOrder.clear();
    m_layerOrderDirty = true;
}

void TilemapManager::getStats(int32_t& tilemapCount, int32_t& tilesetCount, int32_t& layerCount) const {
    tilemapCount = (int32_t)m_tilemaps.size();
    tilesetCount = (int32_t)m_tilesets.size();
    layerCount = (int32_t)m_layers.size();
}

std::string TilemapManager::getLastError() const {
    return m_lastError;
}

void TilemapManager::setError(const std::string& error) const {
    m_lastError = error;
}

void TilemapManager::clearError() const {
    m_lastError.clear();
}

} // namespace SuperTerminal