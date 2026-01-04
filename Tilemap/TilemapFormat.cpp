//
// TilemapFormat.cpp
// SuperTerminal Framework - Tilemap System
//
// Tilemap file format implementation
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "TilemapFormat.h"
#include "Tilemap.h"
#include "TilemapLayer.h"
#include "Camera.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <ctime>

namespace SuperTerminal {

// Static error storage
std::string TilemapFormat::s_lastError;

// =============================================================================
// Error Handling
// =============================================================================

void TilemapFormat::setError(const std::string& error) {
    s_lastError = error;
}

const std::string& TilemapFormat::getLastError() {
    return s_lastError;
}

void TilemapFormat::clearError() {
    s_lastError.clear();
}

// =============================================================================
// Binary Format Helpers
// =============================================================================

bool TilemapFormat::writeU32(std::ofstream& file, uint32_t value) {
    file.write(reinterpret_cast<const char*>(&value), sizeof(value));
    return file.good();
}

bool TilemapFormat::readU32(std::ifstream& file, uint32_t& value) {
    file.read(reinterpret_cast<char*>(&value), sizeof(value));
    return file.good();
}

bool TilemapFormat::writeU16(std::ofstream& file, uint16_t value) {
    file.write(reinterpret_cast<const char*>(&value), sizeof(value));
    return file.good();
}

bool TilemapFormat::readU16(std::ifstream& file, uint16_t& value) {
    file.read(reinterpret_cast<char*>(&value), sizeof(value));
    return file.good();
}

bool TilemapFormat::writeString(std::ofstream& file, const std::string& str) {
    uint32_t len = static_cast<uint32_t>(str.length());
    if (!writeU32(file, len)) return false;
    if (len > 0) {
        file.write(str.c_str(), len);
        return file.good();
    }
    return true;
}

bool TilemapFormat::readString(std::ifstream& file, std::string& str) {
    uint32_t len;
    if (!readU32(file, len)) return false;
    if (len > 0) {
        str.resize(len);
        file.read(&str[0], len);
        return file.good();
    }
    str.clear();
    return true;
}

bool TilemapFormat::writeFloat(std::ofstream& file, float value) {
    file.write(reinterpret_cast<const char*>(&value), sizeof(value));
    return file.good();
}

bool TilemapFormat::readFloat(std::ifstream& file, float& value) {
    file.read(reinterpret_cast<char*>(&value), sizeof(value));
    return file.good();
}

// =============================================================================
// Binary Format (.stmap)
// =============================================================================

bool TilemapFormat::saveBinary(const Tilemap& tilemap,
                               const std::string& filePath,
                               const std::vector<TilemapLayer*>* layers,
                               const Camera* camera,
                               const TilemapSaveOptions& options)
{
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        setError("Failed to open file for writing: " + filePath);
        return false;
    }
    
    // Write magic number
    if (!writeU32(file, TILEMAP_MAGIC)) {
        setError("Failed to write magic number");
        return false;
    }
    
    // Write version
    if (!writeU32(file, TILEMAP_FORMAT_VERSION)) {
        setError("Failed to write version");
        return false;
    }
    
    // Write tilemap metadata
    if (!writeU32(file, tilemap.getWidth())) return false;
    if (!writeU32(file, tilemap.getHeight())) return false;
    if (!writeU32(file, tilemap.getTileWidth())) return false;
    if (!writeU32(file, tilemap.getTileHeight())) return false;
    
    if (!writeString(file, tilemap.getName())) return false;
    
    // Write layer count
    uint32_t layerCount = layers ? static_cast<uint32_t>(layers->size()) : 0;
    if (!writeU32(file, layerCount)) return false;
    
    // Write compression type
    uint8_t compression = static_cast<uint8_t>(options.compression);
    file.write(reinterpret_cast<const char*>(&compression), 1);
    
    // Write camera data if requested
    uint8_t hasCamera = (camera && options.saveCamera) ? 1 : 0;
    file.write(reinterpret_cast<const char*>(&hasCamera), 1);
    
    if (hasCamera) {
        if (!writeFloat(file, camera->getX())) return false;
        if (!writeFloat(file, camera->getY())) return false;
        if (!writeFloat(file, camera->getZoom())) return false;
        
        // Write camera bounds
        Rect bounds = camera->getWorldBounds();
        if (!writeFloat(file, bounds.x)) return false;
        if (!writeFloat(file, bounds.y)) return false;
        if (!writeFloat(file, bounds.width)) return false;
        if (!writeFloat(file, bounds.height)) return false;
    }
    
    // Write layer data
    if (layers) {
        for (const auto* layer : *layers) {
            if (!layer) continue;
            
            // Layer metadata
            if (!writeString(file, layer->getName())) return false;
            if (!writeU32(file, layer->getID())) return false;
            if (!writeU32(file, layer->getZOrder())) return false;
            
            float px, py;
            layer->getParallax(px, py);
            if (!writeFloat(file, px)) return false;
            if (!writeFloat(file, py)) return false;
            if (!writeFloat(file, layer->getOpacity())) return false;
            
            if (!writeFloat(file, layer->getOffsetX())) return false;
            if (!writeFloat(file, layer->getOffsetY())) return false;
            
            if (!writeFloat(file, layer->getAutoScrollX())) return false;
            if (!writeFloat(file, layer->getAutoScrollY())) return false;
            
            uint8_t visible = layer->isVisible() ? 1 : 0;
            file.write(reinterpret_cast<const char*>(&visible), 1);
            
            // Layer tile data
            auto layerTilemap = layer->getTilemap();
            if (layerTilemap) {
                size_t tileCount = layerTilemap->getTileCount();
                if (!writeU32(file, static_cast<uint32_t>(tileCount))) return false;
                
                const TileData* tiles = layerTilemap->getTileData();
                
                // Write raw tile data (16-bit tile IDs)
                for (size_t i = 0; i < tileCount; ++i) {
                    if (!writeU16(file, tiles[i].getTileID())) return false;
                }
            } else {
                if (!writeU32(file, 0)) return false; // No tiles
            }
        }
    }
    
    file.close();
    clearError();
    return true;
}

bool TilemapFormat::loadBinary(const std::string& filePath,
                               std::shared_ptr<Tilemap>& outTilemap,
                               std::vector<std::shared_ptr<TilemapLayer>>& outLayers,
                               CameraState* outCamera)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        setError("Failed to open file for reading: " + filePath);
        return false;
    }
    
    // Read magic number
    uint32_t magic;
    if (!readU32(file, magic)) {
        setError("Failed to read magic number");
        return false;
    }
    
    if (magic != TILEMAP_MAGIC) {
        setError("Invalid file format (bad magic number)");
        return false;
    }
    
    // Read version
    uint32_t version;
    if (!readU32(file, version)) {
        setError("Failed to read version");
        return false;
    }
    
    if (version != TILEMAP_FORMAT_VERSION) {
        setError("Unsupported file version");
        return false;
    }
    
    // Read tilemap metadata
    uint32_t width, height, tileWidth, tileHeight;
    if (!readU32(file, width)) return false;
    if (!readU32(file, height)) return false;
    if (!readU32(file, tileWidth)) return false;
    if (!readU32(file, tileHeight)) return false;
    
    std::string name;
    if (!readString(file, name)) return false;
    
    // Create tilemap
    outTilemap = std::make_shared<Tilemap>(width, height, tileWidth, tileHeight);
    outTilemap->setName(name);
    
    // Read layer count
    uint32_t layerCount;
    if (!readU32(file, layerCount)) return false;
    
    // Read compression type
    uint8_t compression;
    file.read(reinterpret_cast<char*>(&compression), 1);
    
    // Read camera flag
    uint8_t hasCamera;
    file.read(reinterpret_cast<char*>(&hasCamera), 1);
    
    if (hasCamera && outCamera) {
        if (!readFloat(file, outCamera->x)) return false;
        if (!readFloat(file, outCamera->y)) return false;
        if (!readFloat(file, outCamera->zoom)) return false;
        if (!readFloat(file, outCamera->minX)) return false;
        if (!readFloat(file, outCamera->minY)) return false;
        if (!readFloat(file, outCamera->maxX)) return false;
        if (!readFloat(file, outCamera->maxY)) return false;
    } else if (hasCamera) {
        // Skip camera data
        float dummy;
        for (int i = 0; i < 7; ++i) {
            if (!readFloat(file, dummy)) return false;
        }
    }
    
    // Read layers
    outLayers.clear();
    outLayers.reserve(layerCount);
    
    for (uint32_t i = 0; i < layerCount; ++i) {
        std::string layerName;
        if (!readString(file, layerName)) return false;
        
        uint32_t layerID, zOrder;
        if (!readU32(file, layerID)) return false;
        if (!readU32(file, zOrder)) return false;
        
        float px, py, opacity, ox, oy, sx, sy;
        if (!readFloat(file, px)) return false;
        if (!readFloat(file, py)) return false;
        if (!readFloat(file, opacity)) return false;
        if (!readFloat(file, ox)) return false;
        if (!readFloat(file, oy)) return false;
        if (!readFloat(file, sx)) return false;
        if (!readFloat(file, sy)) return false;
        
        uint8_t visible;
        file.read(reinterpret_cast<char*>(&visible), 1);
        
        // Create layer
        auto layer = std::make_shared<TilemapLayer>(layerName);
        layer->setID(layerID);
        layer->setZOrder(zOrder);
        layer->setParallax(px, py);
        layer->setOpacity(opacity);
        layer->setOffset(ox, oy);
        layer->setAutoScroll(sx, sy);
        layer->setVisible(visible != 0);
        
        // Read tile data
        uint32_t tileCount;
        if (!readU32(file, tileCount)) return false;
        
        if (tileCount > 0) {
            // Create layer tilemap
            auto layerTilemap = std::make_shared<Tilemap>(width, height, tileWidth, tileHeight);
            
            // Read tile data
            TileData* tiles = layerTilemap->getTileData();
            for (uint32_t j = 0; j < tileCount; ++j) {
                uint16_t tileID;
                if (!readU16(file, tileID)) return false;
                tiles[j] = TileData(tileID);
            }
            
            layer->setTilemap(layerTilemap);
        }
        
        outLayers.push_back(layer);
    }
    
    file.close();
    clearError();
    return true;
}

// =============================================================================
// JSON Format (.json)
// =============================================================================

bool TilemapFormat::saveJSON(const Tilemap& tilemap,
                             const std::string& filePath,
                             const std::vector<TilemapLayer*>* layers,
                             const Camera* camera,
                             const TilemapSaveOptions& options)
{
    std::ofstream file(filePath);
    if (!file.is_open()) {
        setError("Failed to open file for writing: " + filePath);
        return false;
    }
    
    const char* indent = options.prettyPrint ? "  " : "";
    const char* nl = options.prettyPrint ? "\n" : "";
    
    file << "{" << nl;
    file << indent << "\"version\": " << TILEMAP_FORMAT_VERSION << "," << nl;
    file << indent << "\"name\": \"" << tilemap.getName() << "\"," << nl;
    file << indent << "\"width\": " << tilemap.getWidth() << "," << nl;
    file << indent << "\"height\": " << tilemap.getHeight() << "," << nl;
    file << indent << "\"tileWidth\": " << tilemap.getTileWidth() << "," << nl;
    file << indent << "\"tileHeight\": " << tilemap.getTileHeight() << "," << nl;
    
    // Camera
    if (camera && options.saveCamera) {
        file << indent << "\"camera\": {" << nl;
        file << indent << indent << "\"x\": " << camera->getX() << "," << nl;
        file << indent << indent << "\"y\": " << camera->getY() << "," << nl;
        file << indent << indent << "\"zoom\": " << camera->getZoom() << nl;
        file << indent << "}," << nl;
    }
    
    // Layers
    file << indent << "\"layers\": [" << nl;
    if (layers) {
        for (size_t i = 0; i < layers->size(); ++i) {
            const auto* layer = (*layers)[i];
            if (!layer) continue;
            
            file << indent << indent << "{" << nl;
            file << indent << indent << indent << "\"name\": \"" << layer->getName() << "\"," << nl;
            file << indent << indent << indent << "\"id\": " << layer->getID() << "," << nl;
            file << indent << indent << indent << "\"zOrder\": " << layer->getZOrder() << "," << nl;
            
            float px, py;
            layer->getParallax(px, py);
            file << indent << indent << indent << "\"parallaxX\": " << px << "," << nl;
            file << indent << indent << indent << "\"parallaxY\": " << py << "," << nl;
            file << indent << indent << indent << "\"opacity\": " << layer->getOpacity() << "," << nl;
            file << indent << indent << indent << "\"visible\": " << (layer->isVisible() ? "true" : "false") << "," << nl;
            
            // Tile data as array
            auto layerTilemap = layer->getTilemap();
            file << indent << indent << indent << "\"tiles\": [";
            if (layerTilemap) {
                const TileData* tiles = layerTilemap->getTileData();
                size_t tileCount = layerTilemap->getTileCount();
                for (size_t j = 0; j < tileCount; ++j) {
                    if (j > 0) file << ",";
                    if (options.prettyPrint && j % 20 == 0) file << nl << indent << indent << indent << indent;
                    file << tiles[j].getTileID();
                }
            }
            file << "]" << nl;
            
            file << indent << indent << "}";
            if (i < layers->size() - 1) file << ",";
            file << nl;
        }
    }
    file << indent << "]" << nl;
    file << "}" << nl;
    
    file.close();
    clearError();
    return true;
}

bool TilemapFormat::loadJSON(const std::string& filePath,
                             std::shared_ptr<Tilemap>& outTilemap,
                             std::vector<std::shared_ptr<TilemapLayer>>& outLayers,
                             CameraState* outCamera)
{
    setError("JSON loading not yet implemented - use binary format");
    return false;
}

// =============================================================================
// CSV Format (.csv)
// =============================================================================

bool TilemapFormat::exportCSV(const Tilemap& tilemap,
                              const std::string& filePath,
                              int32_t layerIndex)
{
    std::ofstream file(filePath);
    if (!file.is_open()) {
        setError("Failed to open file for writing: " + filePath);
        return false;
    }
    
    const TileData* tiles = tilemap.getTileData();
    int32_t width = tilemap.getWidth();
    int32_t height = tilemap.getHeight();
    
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            if (x > 0) file << ",";
            file << tiles[y * width + x].getTileID();
        }
        file << "\n";
    }
    
    file.close();
    clearError();
    return true;
}

bool TilemapFormat::importCSV(const std::string& filePath,
                              std::shared_ptr<Tilemap>& outTilemap)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        setError("Failed to open file for reading: " + filePath);
        return false;
    }
    
    // Read all lines to determine size
    std::vector<std::vector<uint16_t>> rows;
    std::string line;
    int32_t maxWidth = 0;
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::vector<uint16_t> row;
        std::stringstream ss(line);
        std::string cell;
        
        while (std::getline(ss, cell, ',')) {
            uint16_t tileID = static_cast<uint16_t>(std::stoi(cell));
            row.push_back(tileID);
        }
        
        maxWidth = std::max(maxWidth, static_cast<int32_t>(row.size()));
        rows.push_back(row);
    }
    
    file.close();
    
    if (rows.empty() || maxWidth == 0) {
        setError("Empty or invalid CSV file");
        return false;
    }
    
    // Create tilemap
    int32_t height = static_cast<int32_t>(rows.size());
    outTilemap = std::make_shared<Tilemap>(maxWidth, height, 16, 16);
    
    // Fill tilemap
    TileData* tiles = outTilemap->getTileData();
    for (int32_t y = 0; y < height; ++y) {
        const auto& row = rows[y];
        for (int32_t x = 0; x < static_cast<int32_t>(row.size()); ++x) {
            tiles[y * maxWidth + x] = TileData(row[x]);
        }
    }
    
    clearError();
    return true;
}

// =============================================================================
// Tiled TMX Format (.tmx)
// =============================================================================

bool TilemapFormat::importTiledTMX(const std::string& filePath,
                                   std::shared_ptr<Tilemap>& outTilemap,
                                   std::vector<std::shared_ptr<TilemapLayer>>& outLayers,
                                   std::vector<TilesetReference>& outTilesets)
{
    setError("Tiled TMX import not yet implemented");
    return false;
}

bool TilemapFormat::exportTiledTMX(const Tilemap& tilemap,
                                   const std::string& filePath,
                                   const std::vector<TilemapLayer*>& layers,
                                   const std::vector<TilesetReference>& tilesets)
{
    setError("Tiled TMX export not yet implemented");
    return false;
}

// =============================================================================
// Utility Functions
// =============================================================================

TilemapFile TilemapFormat::buildFile(const Tilemap& tilemap,
                                     const std::vector<TilemapLayer*>& layers,
                                     const Camera* camera,
                                     const TilemapSaveOptions& options)
{
    TilemapFile file;
    
    // Metadata
    file.metadata.version = TILEMAP_FORMAT_VERSION;
    file.metadata.width = tilemap.getWidth();
    file.metadata.height = tilemap.getHeight();
    file.metadata.tileWidth = tilemap.getTileWidth();
    file.metadata.tileHeight = tilemap.getTileHeight();
    file.metadata.name = tilemap.getName();
    file.metadata.layerCount = static_cast<uint32_t>(layers.size());
    file.metadata.compression = options.compression;
    file.metadata.encoding = options.encoding;
    file.metadata.createdAt = std::time(nullptr);
    file.metadata.modifiedAt = std::time(nullptr);
    
    // Camera
    if (camera && options.saveCamera) {
        file.camera.x = camera->getX();
        file.camera.y = camera->getY();
        file.camera.zoom = camera->getZoom();
        Rect bounds = camera->getWorldBounds();
        file.camera.minX = bounds.x;
        file.camera.minY = bounds.y;
        file.camera.maxX = bounds.x + bounds.width;
        file.camera.maxY = bounds.y + bounds.height;
    }
    
    // Layers
    for (const auto* layer : layers) {
        if (!layer) continue;
        
        LayerMetadata layerMeta;
        layerMeta.name = layer->getName();
        layerMeta.id = layer->getID();
        layerMeta.zOrder = layer->getZOrder();
        layer->getParallax(layerMeta.parallaxX, layerMeta.parallaxY);
        layerMeta.opacity = layer->getOpacity();
        layerMeta.offsetX = layer->getOffsetX();
        layerMeta.offsetY = layer->getOffsetY();
        layerMeta.autoScrollX = layer->getAutoScrollX();
        layerMeta.autoScrollY = layer->getAutoScrollY();
        layerMeta.visible = layer->isVisible();
        
        file.layers.push_back(layerMeta);
        
        // Layer tile data
        auto layerTilemap = layer->getTilemap();
        if (layerTilemap) {
            const TileData* tiles = layerTilemap->getTileData();
            size_t tileCount = layerTilemap->getTileCount();
            
            std::vector<uint16_t> tileData;
            tileData.reserve(tileCount);
            for (size_t i = 0; i < tileCount; ++i) {
                tileData.push_back(tiles[i].getTileID());
            }
            
            file.layerData.push_back(tileData);
        } else {
            file.layerData.push_back(std::vector<uint16_t>());
        }
    }
    
    return file;
}

bool TilemapFormat::createFromFile(const TilemapFile& file,
                                   std::shared_ptr<Tilemap>& outTilemap,
                                   std::vector<std::shared_ptr<TilemapLayer>>& outLayers,
                                   CameraState* outCamera)
{
    // Create tilemap
    outTilemap = std::make_shared<Tilemap>(
        file.metadata.width, 
        file.metadata.height,
        file.metadata.tileWidth,
        file.metadata.tileHeight
    );
    outTilemap->setName(file.metadata.name);
    
    // Camera
    if (outCamera) {
        *outCamera = file.camera;
    }
    
    // Layers
    outLayers.clear();
    outLayers.reserve(file.layers.size());
    
    for (size_t i = 0; i < file.layers.size(); ++i) {
        const auto& layerMeta = file.layers[i];
        
        auto layer = std::make_shared<TilemapLayer>(layerMeta.name);
        layer->setID(layerMeta.id);
        layer->setZOrder(layerMeta.zOrder);
        layer->setParallax(layerMeta.parallaxX, layerMeta.parallaxY);
        layer->setOpacity(layerMeta.opacity);
        layer->setOffset(layerMeta.offsetX, layerMeta.offsetY);
        layer->setAutoScroll(layerMeta.autoScrollX, layerMeta.autoScrollY);
        layer->setVisible(layerMeta.visible);
        
        // Tile data
        if (i < file.layerData.size() && !file.layerData[i].empty()) {
            auto layerTilemap = std::make_shared<Tilemap>(
                file.metadata.width,
                file.metadata.height,
                file.metadata.tileWidth,
                file.metadata.tileHeight
            );
            
            TileData* tiles = layerTilemap->getTileData();
            const auto& tileData = file.layerData[i];
            for (size_t j = 0; j < tileData.size(); ++j) {
                tiles[j] = TileData(tileData[j]);
            }
            
            layer->setTilemap(layerTilemap);
        }
        
        outLayers.push_back(layer);
    }
    
    clearError();
    return true;
}

bool TilemapFormat::compressData(const std::vector<uint16_t>& data,
                                 CompressionType type,
                                 std::vector<uint8_t>& output)
{
    if (type == CompressionType::None) {
        output.resize(data.size() * 2);
        std::memcpy(output.data(), data.data(), data.size() * 2);
        return true;
    }
    
    // RLE compression
    if (type == CompressionType::RLE) {
        output.clear();
        if (data.empty()) return true;
        
        uint16_t currentTile = data[0];
        uint16_t count = 1;
        
        for (size_t i = 1; i < data.size(); ++i) {
            if (data[i] == currentTile && count < 65535) {
                count++;
            } else {
                // Write run
                output.push_back(count & 0xFF);
                output.push_back((count >> 8) & 0xFF);
                output.push_back(currentTile & 0xFF);
                output.push_back((currentTile >> 8) & 0xFF);
                
                currentTile = data[i];
                count = 1;
            }
        }
        
        // Write last run
        output.push_back(count & 0xFF);
        output.push_back((count >> 8) & 0xFF);
        output.push_back(currentTile & 0xFF);
        output.push_back((currentTile >> 8) & 0xFF);
        
        return true;
    }
    
    setError("Compression type not implemented");
    return false;
}

bool TilemapFormat::decompressData(const std::vector<uint8_t>& input,
                                   CompressionType type,
                                   std::vector<uint16_t>& output,
                                   size_t expectedSize)
{
    if (type == CompressionType::None) {
        output.resize(input.size() / 2);
        std::memcpy(output.data(), input.data(), input.size());
        return true;
    }
    
    // RLE decompression
    if (type == CompressionType::RLE) {
        output.clear();
        output.reserve(expectedSize);
        
        for (size_t i = 0; i + 3 < input.size(); i += 4) {
            uint16_t count = input[i] | (input[i + 1] << 8);
            uint16_t tileID = input[i + 2] | (input[i + 3] << 8);
            
            for (uint16_t j = 0; j < count; ++j) {
                output.push_back(tileID);
            }
        }
        
        return output.size() == expectedSize;
    }
    
    setError("Compression type not implemented");
    return false;
}

std::string TilemapFormat::detectFormat(const std::string& filePath) {
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos == std::string::npos) {
        return "unknown";
    }
    
    std::string ext = filePath.substr(dotPos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == "stmap") return "binary";
    if (ext == "json") return "json";
    if (ext == "tmx") return "tmx";
    if (ext == "csv") return "csv";
    
    return "unknown";
}

bool TilemapFormat::validate(const std::string& filePath, std::string& outError) {
    std::string format = detectFormat(filePath);
    
    if (format == "binary") {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            outError = "Cannot open file";
            return false;
        }
        
        uint32_t magic;
        if (!readU32(file, magic)) {
            outError = "Cannot read magic number";
            return false;
        }
        
        if (magic != TILEMAP_MAGIC) {
            outError = "Invalid magic number";
            return false;
        }
        
        uint32_t version;
        if (!readU32(file, version)) {
            outError = "Cannot read version";
            return false;
        }
        
        if (version != TILEMAP_FORMAT_VERSION) {
            outError = "Unsupported version: " + std::to_string(version);
            return false;
        }
        
        file.close();
        return true;
    }
    
    outError = "Format validation not implemented for: " + format;
    return false;
}

} // namespace SuperTerminal