//
//  CartAssetProvider.h
//  SuperTerminal Framework - Cart Asset Provider
//
//  Provides assets from cart files to the AssetManager
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef CART_ASSET_PROVIDER_H
#define CART_ASSET_PROVIDER_H

#include "CartLoader.h"
#include "../Assets/AssetMetadata.h"
#include <string>
#include <vector>
#include <memory>

namespace SuperTerminal {

// Forward declarations
class AssetManager;

/// CartAssetProvider: Bridges cart assets with AssetManager
///
/// Responsibilities:
/// - Intercept asset load requests and check cart first
/// - Convert cart assets to AssetMetadata format
/// - Provide transparent fallback to filesystem
/// - Cache converted assets for performance
///
/// Integration:
/// - Installed into AssetManager via setCartProvider()
/// - AssetManager queries provider before filesystem
/// - Provider returns AssetMetadata compatible with existing system
///
/// Thread Safety:
/// - Not thread-safe, use from main thread only
/// - AssetManager handles thread safety for cache
///
class CartAssetProvider {
public:
    /// Constructor
    /// @param cartLoader Pointer to active CartLoader (must remain valid)
    explicit CartAssetProvider(CartLoader* cartLoader);
    
    /// Destructor
    ~CartAssetProvider();
    
    // Disable copy
    CartAssetProvider(const CartAssetProvider&) = delete;
    CartAssetProvider& operator=(const CartAssetProvider&) = delete;
    
    // Allow move
    CartAssetProvider(CartAssetProvider&& other) noexcept;
    CartAssetProvider& operator=(CartAssetProvider&& other) noexcept;
    
    // === ASSET RESOLUTION ===
    
    /// Check if provider can supply the requested asset
    /// @param name Asset name (with or without extension)
    /// @param kind Asset kind hint
    /// @return true if asset exists in cart
    bool hasAsset(const std::string& name, AssetKind kind) const;
    
    /// Load asset from cart
    /// @param name Asset name
    /// @param kind Asset kind
    /// @param outMetadata Output metadata structure
    /// @return true if loaded successfully
    bool loadAsset(const std::string& name, AssetKind kind, AssetMetadata& outMetadata) const;
    
    /// List all assets of a specific kind
    /// @param kind Asset kind
    /// @return Vector of asset names
    std::vector<std::string> listAssets(AssetKind kind) const;
    
    /// Get cart metadata
    /// @return Cart metadata structure
    CartMetadata getCartMetadata() const;
    
    // === PROVIDER STATE ===
    
    /// Check if provider is active (has valid cart loader)
    /// @return true if active
    bool isActive() const { return m_cartLoader != nullptr && m_cartLoader->isLoaded(); }
    
    /// Get associated cart loader
    /// @return Pointer to cart loader or nullptr
    CartLoader* getCartLoader() const { return m_cartLoader; }
    
    /// Set cart loader (nullptr to disable)
    /// @param cartLoader New cart loader
    void setCartLoader(CartLoader* cartLoader);
    
    // === STATISTICS ===
    
    /// Get number of asset requests served
    /// @return Request count
    int64_t getRequestCount() const { return m_requestCount; }
    
    /// Get number of cache hits
    /// @return Hit count
    int64_t getCacheHits() const { return m_cacheHits; }
    
    /// Reset statistics
    void resetStats();
    
private:
    // Cart loader reference
    CartLoader* m_cartLoader;
    
    // Statistics
    mutable int64_t m_requestCount;
    mutable int64_t m_cacheHits;
    
    // === CONVERSION HELPERS ===
    
    /// Convert CartSprite to AssetMetadata
    /// @param sprite Cart sprite
    /// @param outMetadata Output metadata
    /// @return true if converted successfully
    bool convertSprite(const CartSprite& sprite, AssetMetadata& outMetadata) const;
    
    /// Convert CartTileset to AssetMetadata
    /// @param tileset Cart tileset
    /// @param outMetadata Output metadata
    /// @return true if converted successfully
    bool convertTileset(const CartTileset& tileset, AssetMetadata& outMetadata) const;
    
    /// Convert CartSound to AssetMetadata
    /// @param sound Cart sound
    /// @param outMetadata Output metadata
    /// @return true if converted successfully
    bool convertSound(const CartSound& sound, AssetMetadata& outMetadata) const;
    
    /// Convert CartMusic to AssetMetadata
    /// @param music Cart music
    /// @param outMetadata Output metadata
    /// @return true if converted successfully
    bool convertMusic(const CartMusic& music, AssetMetadata& outMetadata) const;
    
    /// Strip file extension from name
    /// @param name Name with possible extension
    /// @return Name without extension
    static std::string stripExtension(const std::string& name);
    
    /// Convert format string to AssetFormat enum
    /// @param formatStr Format string ("png", "wav", etc.)
    /// @return AssetFormat enum value
    static AssetFormat stringToFormat(const std::string& formatStr);
};

} // namespace SuperTerminal

#endif // CART_ASSET_PROVIDER_H