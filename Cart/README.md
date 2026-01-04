# FasterBASIC Cart System


## Overview

The Cart System allows developers to package FasterBASIC programs and all their assets (sprites, tilesets, sounds, music, data files) into a single distributable `.crt` file.

## What is a Cart File?

A `.crt` file is a **SQLite database** containing:
- BASIC program source code
- All required assets (sprites, sounds, music, tiles)
- Metadata (title, author, version, description)
- Version and compatibility information

Think of it like a game cartridge for a retro console - everything needed to run is in one file.

## Components

### 1. CartLoader (`CartLoader.h/cpp`)

Loads and manages cart files.

**Key Features:**
- Load/validate `.crt` files
- Extract program source
- Query cart metadata
- Load individual assets
- Validate cart integrity

**Usage:**
```cpp
#include "CartLoader.h"

SuperTerminal::CartLoader loader;

// Load cart
if (loader.loadCart("mygame.crt")) {
    // Get metadata
    auto metadata = loader.getMetadata();
    std::cout << "Title: " << metadata.title << std::endl;
    std::cout << "Author: " << metadata.author << std::endl;
    
    // Get program
    std::string program = loader.getProgramSource();
    
    // Load a sprite
    SuperTerminal::CartSprite sprite;
    if (loader.loadSprite("player", sprite)) {
        // Use sprite.data, sprite.width, sprite.height
    }
    
    // Clean up
    loader.unloadCart();
}
```

### 2. CartAssetProvider (`CartAssetProvider.h/cpp`)

Bridges cart assets with the existing AssetManager system.

**Key Features:**
- Transparent asset loading
- Integrates with AssetManager
- Automatic fallback to filesystem
- Format conversion
- Statistics tracking

**Usage:**
```cpp
#include "CartAssetProvider.h"

SuperTerminal::CartLoader loader;
loader.loadCart("mygame.crt");

// Create provider
SuperTerminal::CartAssetProvider provider(&loader);

// Check if asset exists
if (provider.hasAsset("player", SuperTerminal::AssetKind::SPRITE)) {
    SuperTerminal::AssetMetadata metadata;
    if (provider.loadAsset("player", SuperTerminal::AssetKind::SPRITE, metadata)) {
        // Use metadata.data
    }
}
```

## Integration 

**Quick Integration:**
1. Add cart sources to CMakeLists.txt
2. Link SQLite3
3. Add "Load Cart..." menu item
4. Create CartLoader instance
5. Modify asset loading to check cart provider first

## Cart Format Specification

See `docs/CART_FORMAT_SPEC.md` for complete technical specification.

**Database Schema:**
- `schema_version` - Format version tracking
- `metadata` - Cart metadata (title, author, etc.)
- `program` - BASIC source code
- `sprites` - Sprite assets (PNG/RGBA)
- `tilesets` - Tileset assets with tile dimensions
- `sounds` - Sound effects (WAV/AIFF)
- `music` - Music tracks (SID/MOD/XM)
- `data_files` - Arbitrary data files

## Creating Cart Files

Use the `fbcart.py` tool in `python_scripts/`:

```bash
# Create a new cart
./python_scripts/fbcart.py create mygame.crt \
    --title "My Game" \
    --author "John Doe" \
    --version "1.0.0" \
    --description "A cool game" \
    --program game.bas \
    --sprites player.png enemy.png \
    --sounds shoot.wav explosion.wav

# Add assets
./python_scripts/fbcart.py add mygame.crt --sprite boss.png

# List contents
./python_scripts/fbcart.py list mygame.crt --verbose

# Validate
./python_scripts/fbcart.py validate mygame.crt

# Show info
./python_scripts/fbcart.py info mygame.crt

# Extract for editing
./python_scripts/fbcart.py extract mygame.crt extracted/
```

## API Reference

### CartLoader

#### Loading
```cpp
bool loadCart(const std::string& cartPath);
void unloadCart();
bool isLoaded() const;
```

#### Metadata
```cpp
CartMetadata getMetadata() const;
std::string getMetadataValue(const std::string& key) const;
int getSchemaVersion() const;
```

#### Program
```cpp
CartProgram getProgram() const;
std::string getProgramSource() const;
```

#### Asset Queries
```cpp
std::vector<std::string> listSprites() const;
std::vector<std::string> listTilesets() const;
std::vector<std::string> listSounds() const;
std::vector<std::string> listMusic() const;
std::vector<std::string> listDataFiles() const;

bool hasSprite(const std::string& name) const;
bool hasTileset(const std::string& name) const;
bool hasSound(const std::string& name) const;
bool hasMusic(const std::string& name) const;
bool hasDataFile(const std::string& path) const;
```

#### Asset Loading
```cpp
bool loadSprite(const std::string& name, CartSprite& outSprite) const;
bool loadTileset(const std::string& name, CartTileset& outTileset) const;
bool loadSound(const std::string& name, CartSound& outSound) const;
bool loadMusic(const std::string& name, CartMusic& outMusic) const;
bool loadDataFile(const std::string& path, CartDataFile& outDataFile) const;
```

#### Statistics
```cpp
int getSpriteCount() const;
int getTilesetCount() const;
int getSoundCount() const;
int getMusicCount() const;
int getDataFileCount() const;
int64_t getCartSize() const;
int64_t getTotalAssetSize() const;
```

#### Validation
```cpp
static CartValidationResult validateCart(const std::string& cartPath);
CartValidationResult validate() const;
```

### CartAssetProvider

```cpp
bool hasAsset(const std::string& name, AssetKind kind) const;
bool loadAsset(const std::string& name, AssetKind kind, AssetMetadata& outMetadata) const;
std::vector<std::string> listAssets(AssetKind kind) const;
CartMetadata getCartMetadata() const;
bool isActive() const;
```

## Data Structures

### CartMetadata
```cpp
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
};
```

### CartSprite
```cpp
struct CartSprite {
    std::string name;
    std::vector<uint8_t> data;
    int32_t width;
    int32_t height;
    std::string format;  // "png", "rgba", "indexed"
    std::string notes;
};
```

### CartSound
```cpp
struct CartSound {
    std::string name;
    std::vector<uint8_t> data;
    std::string format;  // "wav", "raw", "aiff"
    int32_t sampleRate;
    int32_t channels;
    int32_t bitsPerSample;
    std::string notes;
};
```

### CartMusic
```cpp
struct CartMusic {
    std::string name;
    std::vector<uint8_t> data;
    std::string format;  // "sid", "mod", "xm", "s3m", "it"
    double durationSeconds;
    std::string notes;
};
```

## Thread Safety

- **CartLoader:** Not thread-safe, use from main thread only
- **CartAssetProvider:** Not thread-safe, use from main thread only
- **Asset Data:** Returned data is copied, safe to use from any thread

## Error Handling

Both classes provide error reporting:

```cpp
std::string getLastError() const;
void clearLastError();
```

Validation provides detailed results:

```cpp
CartValidationResult result = CartLoader::validateCart("mygame.crt");
if (!result.valid) {
    for (const auto& error : result.errors) {
        std::cerr << "Error: " << error << std::endl;
    }
    for (const auto& warning : result.warnings) {
        std::cerr << "Warning: " << warning << std::endl;
    }
}
```

## Performance Considerations

- **Cart file size:** Keep under 100MB for easy distribution
- **Asset loading:** Assets are loaded on-demand (lazy loading)
- **Caching:** Use AssetManager for caching frequently-used assets
- **SQLite indexes:** Cart tables are indexed for fast lookups
- **Validation:** Validate once at load time, not on every asset request

## Best Practices

### For Cart Creators
1. Optimize assets before packaging (compress PNGs, trim audio)
2. Use descriptive asset names
3. Test cart thoroughly before distribution
4. Version incrementally (1.0.0 → 1.0.1 → 1.1.0)
5. Document requirements in cart metadata

### For Integration
1. Always validate carts before loading
2. Show cart metadata to users
3. Handle errors gracefully with user-friendly messages
4. Allow users to exit cart mode easily
5. Clean up cart loader on program end

### For Asset Loading
1. Strip file extensions when loading (`"player"` not `"player.png"`)
2. Use consistent naming conventions
3. Handle missing assets gracefully
4. Provide fallbacks for optional assets
5. Cache frequently-used assets

## Examples

### Complete Cart Loading Example
```cpp
#include "CartLoader.h"
#include "CartAssetProvider.h"

// Validate cart
auto validation = SuperTerminal::CartLoader::validateCart("game.crt");
if (!validation.valid) {
    for (const auto& error : validation.errors) {
        std::cerr << "Error: " << error << std::endl;
    }
    return;
}

// Load cart
SuperTerminal::CartLoader loader;
if (!loader.loadCart("game.crt")) {
    std::cerr << "Failed to load: " << loader.getLastError() << std::endl;
    return;
}

// Show cart info
auto metadata = loader.getMetadata();
std::cout << "Title: " << metadata.title << std::endl;
std::cout << "Author: " << metadata.author << std::endl;
std::cout << "Version: " << metadata.version << std::endl;
std::cout << "Assets: " << loader.getSpriteCount() << " sprites, "
          << loader.getSoundCount() << " sounds" << std::endl;

// Setup asset provider
SuperTerminal::CartAssetProvider provider(&loader);

// Run program
std::string program = loader.getProgramSource();
// ... compile and run program ...

// Assets will be loaded automatically from cart via provider

// Cleanup
loader.unloadCart();
```

## Dependencies

- **SQLite3** (3.35.0 or later)
- **Standard C++ Library** (C++17)
- **AssetManager** (for CartAssetProvider integration)

## Building

Add to CMakeLists.txt:

```cmake
# Cart system sources
set(CART_SOURCES
    Cart/CartLoader.cpp
    Cart/CartAssetProvider.cpp
)

set(CART_HEADERS
    Cart/CartLoader.h
    Cart/CartAssetProvider.h
)

# Add to framework
target_sources(Framework PRIVATE
    ${CART_SOURCES}
    ${CART_HEADERS}
)

# Link SQLite
find_package(SQLite3 REQUIRED)
target_link_libraries(Framework PRIVATE SQLite3::SQLite3)
```

## Testing

### Manual Testing
1. Create test cart with fbcart.py
2. Validate cart structure
3. Load cart in application
4. Verify metadata displays correctly
5. Test asset loading from cart
6. Test program execution
7. Test cleanup/unload

### Automated Testing (Future)
- Unit tests for CartLoader
- Unit tests for CartAssetProvider
- Integration tests for asset loading
- Performance benchmarks

## Known Limitations

1. **No save data (v1.0)** - Carts are read-only
2. **No compression (v1.0)** - Assets stored uncompressed
3. **Single program** - One BASIC program per cart
4. **No streaming** - Entire cart must be accessible
5. **No dependencies** - Carts are self-contained

These will be addressed in future versions.

## Future Enhancements

### Version 1.1
- Save data support (companion `.sav` files)
- Compressed BLOBs (zlib)
- Multi-file programs (includes/modules)
- Localization support

### Version 2.0
- Cart library browser UI
- DLC/expansion support
- Digital signatures for verified publishers
- Cloud save sync
- Streaming asset loading

## Support

For issues, questions, or contributions:
- Documentation: `docs/CART_FORMAT_SPEC.md`
- Integration Guide: `docs/CART_INTEGRATION_GUIDE.md`
- Complete Status: `CART_SYSTEM_COMPLETE.md`

## License

Copyright © 2024 Alban Read, SuperTerminal. All rights reserved.

---

**Cart System Version:** 1.0  
**Implemented:** 2024
