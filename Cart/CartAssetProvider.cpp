//
//  CartAssetProvider.cpp
//  SuperTerminal Framework - Cart Asset Provider
//
//  Implementation of CartAssetProvider for asset integration
//  Copyright © 2024-2026 Alban Read, SuperTerminal. All rights reserved.
//

#include "CartAssetProvider.h"
#include <algorithm>
#include <cctype>

namespace SuperTerminal {

// =============================================================================
// Constructor / Destructor
// =============================================================================

CartAssetProvider::CartAssetProvider(CartLoader* cartLoader)
    : m_cartLoader(cartLoader)
    , m_requestCount(0)
    , m_cacheHits(0)
{
}

CartAssetProvider::~CartAssetProvider() {
    // Don't own cartLoader, just clear reference
    m_cartLoader = nullptr;
}

CartAssetProvider::CartAssetProvider(CartAssetProvider&& other) noexcept
    : m_cartLoader(other.m_cartLoader)
    , m_requestCount(other.m_requestCount)
    , m_cacheHits(other.m_cacheHits)
{
    other.m_cartLoader = nullptr;
    other.m_requestCount = 0;
    other.m_cacheHits = 0;
}

CartAssetProvider& CartAssetProvider::operator=(CartAssetProvider&& other) noexcept {
    if (this != &other) {
        m_cartLoader = other.m_cartLoader;
        m_requestCount = other.m_requestCount;
        m_cacheHits = other.m_cacheHits;

        other.m_cartLoader = nullptr;
        other.m_requestCount = 0;
        other.m_cacheHits = 0;
    }
    return *this;
}

// =============================================================================
// Asset Resolution
// =============================================================================

bool CartAssetProvider::hasAsset(const std::string& name, AssetKind kind) const {
    if (!isActive()) {
        return false;
    }

    m_requestCount++;

    std::string cleanName = stripExtension(name);

    switch (kind) {
        case AssetKind::SPRITE:
        case AssetKind::IMAGE:
            return m_cartLoader->hasSprite(cleanName);

        case AssetKind::TILESET:
            return m_cartLoader->hasTileset(cleanName);

        case AssetKind::SOUND:
            return m_cartLoader->hasSound(cleanName);

        case AssetKind::MUSIC:
            return m_cartLoader->hasMusic(cleanName);

        case AssetKind::DATA:
            return m_cartLoader->hasDataFile(name); // Keep full path for data files

        case AssetKind::UNKNOWN:
            // Try all types
            return m_cartLoader->hasSprite(cleanName) ||
                   m_cartLoader->hasTileset(cleanName) ||
                   m_cartLoader->hasSound(cleanName) ||
                   m_cartLoader->hasMusic(cleanName) ||
                   m_cartLoader->hasDataFile(name);

        default:
            return false;
    }
}

bool CartAssetProvider::loadAsset(const std::string& name, AssetKind kind,
                                   AssetMetadata& outMetadata) const {
    if (!isActive()) {
        return false;
    }

    m_requestCount++;

    std::string cleanName = stripExtension(name);

    // Try to load based on kind
    switch (kind) {
        case AssetKind::SPRITE:
        case AssetKind::IMAGE: {
            CartSprite sprite;
            if (m_cartLoader->loadSprite(cleanName, sprite)) {
                m_cacheHits++;
                return convertSprite(sprite, outMetadata);
            }
            break;
        }

        case AssetKind::TILESET: {
            CartTileset tileset;
            if (m_cartLoader->loadTileset(cleanName, tileset)) {
                m_cacheHits++;
                return convertTileset(tileset, outMetadata);
            }
            break;
        }

        case AssetKind::SOUND: {
            CartSound sound;
            if (m_cartLoader->loadSound(cleanName, sound)) {
                m_cacheHits++;
                return convertSound(sound, outMetadata);
            }
            break;
        }

        case AssetKind::MUSIC: {
            CartMusic music;
            if (m_cartLoader->loadMusic(cleanName, music)) {
                m_cacheHits++;
                return convertMusic(music, outMetadata);
            }
            break;
        }

        case AssetKind::DATA: {
            CartDataFile dataFile;
            if (m_cartLoader->loadDataFile(name, dataFile)) {
                m_cacheHits++;
                outMetadata.name = dataFile.path;
                outMetadata.kind = AssetKind::DATA;
                outMetadata.format = AssetFormat::RAW;
                outMetadata.data = dataFile.data;
                return true;
            }
            break;
        }

        case AssetKind::UNKNOWN: {
            // Try each type in order
            CartSprite sprite;
            if (m_cartLoader->loadSprite(cleanName, sprite)) {
                m_cacheHits++;
                return convertSprite(sprite, outMetadata);
            }

            CartTileset tileset;
            if (m_cartLoader->loadTileset(cleanName, tileset)) {
                m_cacheHits++;
                return convertTileset(tileset, outMetadata);
            }

            CartSound sound;
            if (m_cartLoader->loadSound(cleanName, sound)) {
                m_cacheHits++;
                return convertSound(sound, outMetadata);
            }

            CartMusic music;
            if (m_cartLoader->loadMusic(cleanName, music)) {
                m_cacheHits++;
                return convertMusic(music, outMetadata);
            }

            CartDataFile dataFile;
            if (m_cartLoader->loadDataFile(name, dataFile)) {
                m_cacheHits++;
                outMetadata.name = dataFile.path;
                outMetadata.kind = AssetKind::DATA;
                outMetadata.format = AssetFormat::RAW;
                outMetadata.data = dataFile.data;
                return true;
            }
            break;
        }

        default:
            break;
    }

    return false;
}

std::vector<std::string> CartAssetProvider::listAssets(AssetKind kind) const {
    if (!isActive()) {
        return {};
    }

    switch (kind) {
        case AssetKind::SPRITE:
        case AssetKind::IMAGE:
            return m_cartLoader->listSprites();

        case AssetKind::TILESET:
            return m_cartLoader->listTilesets();

        case AssetKind::SOUND:
            return m_cartLoader->listSounds();

        case AssetKind::MUSIC:
            return m_cartLoader->listMusic();

        case AssetKind::DATA:
            return m_cartLoader->listDataFiles();

        case AssetKind::UNKNOWN: {
            // Return all assets
            std::vector<std::string> all;

            auto sprites = m_cartLoader->listSprites();
            all.insert(all.end(), sprites.begin(), sprites.end());

            auto tilesets = m_cartLoader->listTilesets();
            all.insert(all.end(), tilesets.begin(), tilesets.end());

            auto sounds = m_cartLoader->listSounds();
            all.insert(all.end(), sounds.begin(), sounds.end());

            auto music = m_cartLoader->listMusic();
            all.insert(all.end(), music.begin(), music.end());

            auto data = m_cartLoader->listDataFiles();
            all.insert(all.end(), data.begin(), data.end());

            return all;
        }

        default:
            return {};
    }
}

CartMetadata CartAssetProvider::getCartMetadata() const {
    if (!isActive()) {
        return CartMetadata();
    }

    return m_cartLoader->getMetadata();
}

// =============================================================================
// Provider State
// =============================================================================

void CartAssetProvider::setCartLoader(CartLoader* cartLoader) {
    m_cartLoader = cartLoader;
}

// =============================================================================
// Statistics
// =============================================================================

void CartAssetProvider::resetStats() {
    m_requestCount = 0;
    m_cacheHits = 0;
}

// =============================================================================
// Conversion Helpers
// =============================================================================

bool CartAssetProvider::convertSprite(const CartSprite& sprite,
                                       AssetMetadata& outMetadata) const {
    outMetadata.name = sprite.name;
    outMetadata.kind = AssetKind::SPRITE;
    outMetadata.format = stringToFormat(sprite.format);
    outMetadata.data = sprite.data;
    outMetadata.width = sprite.width;
    outMetadata.height = sprite.height;

    // Add custom properties
    if (!sprite.notes.empty()) {
        outMetadata.tags.push_back(sprite.notes);
    }

    return true;
}

bool CartAssetProvider::convertTileset(const CartTileset& tileset,
                                        AssetMetadata& outMetadata) const {
    outMetadata.name = tileset.name;
    outMetadata.kind = AssetKind::TILESET;
    outMetadata.format = stringToFormat(tileset.format);
    outMetadata.data = tileset.data;
    outMetadata.width = tileset.tilesAcross * tileset.tileWidth;
    outMetadata.height = tileset.tilesDown * tileset.tileHeight;

    // Store tileset-specific info in custom data (could extend AssetMetadata)
    // For now, just store basic info
    if (!tileset.notes.empty()) {
        outMetadata.tags.push_back(tileset.notes);
    }

    return true;
}

bool CartAssetProvider::convertSound(const CartSound& sound,
                                      AssetMetadata& outMetadata) const {
    outMetadata.name = sound.name;
    outMetadata.kind = AssetKind::SOUND;
    outMetadata.format = stringToFormat(sound.format);
    outMetadata.data = sound.data;

    // Sound metadata
    if (!sound.notes.empty()) {
        outMetadata.tags.push_back(sound.notes);
    }

    return true;
}

bool CartAssetProvider::convertMusic(const CartMusic& music,
                                      AssetMetadata& outMetadata) const {
    outMetadata.name = music.name;
    outMetadata.kind = AssetKind::MUSIC;
    outMetadata.format = stringToFormat(music.format);
    outMetadata.data = music.data;

    // Music metadata
    if (!music.notes.empty()) {
        outMetadata.tags.push_back(music.notes);
    }

    return true;
}

std::string CartAssetProvider::stripExtension(const std::string& name) {
    size_t dotPos = name.find_last_of('.');
    if (dotPos != std::string::npos) {
        // Check if it's actually an extension (not just a dot in the path)
        size_t slashPos = name.find_last_of("/\\");
        if (slashPos == std::string::npos || dotPos > slashPos) {
            return name.substr(0, dotPos);
        }
    }
    return name;
}

AssetFormat CartAssetProvider::stringToFormat(const std::string& formatStr) {
    std::string lower = formatStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "png") return AssetFormat::PNG;
    if (lower == "jpeg" || lower == "jpg") return AssetFormat::JPEG;
    if (lower == "bmp") return AssetFormat::BMP;
    if (lower == "wav") return AssetFormat::WAV;
    if (lower == "mp3") return AssetFormat::MP3;
    if (lower == "ogg") return AssetFormat::OGG;
    if (lower == "json") return AssetFormat::JSON;
    if (lower == "xml") return AssetFormat::XML;
    if (lower == "txt" || lower == "text") return AssetFormat::TEXT;
    if (lower == "rgba" || lower == "raw") return AssetFormat::RAW;
    if (lower == "sid") return AssetFormat::RAW; // SID is binary
    if (lower == "mod" || lower == "xm" || lower == "s3m" || lower == "it") {
        return AssetFormat::RAW; // Module formats
    }

    return AssetFormat::UNKNOWN;
}

} // namespace SuperTerminal
