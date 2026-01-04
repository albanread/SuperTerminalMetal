# Indexed Color Tile System Integration Design

## Overview

This document describes the integration of indexed color (palette-based) tiles into the existing SuperTerminal tilemap system. The goal is to add 16-color palette support with 32 palette banks while maintaining backward compatibility with the existing RGB/RGBA tile system.

---

## Current Architecture

### Existing Components

1. **TileData** (16-bit)
   - 2048 tile IDs
   - Flip X/Y, rotation (0°/90°/180°/270°)
   - Collision flag
   - 2 bytes per tile

2. **Tilemap**
   - Grid of TileData
   - Row-major storage
   - Dirty tracking

3. **Tileset**
   - Texture atlas (RGB/RGBA)
   - Margin and spacing support
   - Animation support
   - Tile properties

4. **TilemapLayer**
   - Combines Tilemap + Tileset
   - Parallax, opacity, blending
   - Auto-scroll
   - Z-order per layer

5. **TilemapRenderer**
   - GPU-accelerated instanced rendering
   - Frustum culling
   - Metal shaders

---

## New Requirements

### Indexed Color Tiles Must Support:

1. **4-bit color depth** (16 colors per tile)
2. **256 palette banks** (32 is practical target)
3. **Per-tile palette selection** (same tile, different colors)
4. **Per-tile z-order** (8 priority levels: 0-7)
5. **Palette convention**:
   - Index 0: Transparent black (RGBA: 0,0,0,0)
   - Index 1: Opaque black (RGBA: 0,0,0,255)
   - Index 2-15: 14 usable colors
6. **GPU storage**:
   - Tile atlas: 4-bit indexed pixels
   - Palette texture: 32×16 RGBA colors (2 KB)
7. **Backward compatibility** with existing 16-bit TileData

---

## Integration Strategy

### Phase 1: Extended Tile Data ✅ COMPLETED

**New Component: TileDataEx** (32-bit)
- 4096 tile IDs (12 bits)
- 256 palette indices (8 bits)
- 8 z-order levels (3 bits)
- Flip X/Y, rotation (4 bits)
- Collision flag (1 bit)
- Reserved bits (4 bits)

**Total: 32 bits / 4 bytes per tile**

**Bit Layout:**
```
[31:20] Tile ID (12 bits)
[19:12] Palette Index (8 bits)
[11:9]  Z-Order (3 bits)
[8]     Flip Y
[7]     Flip X
[6:5]   Rotation (2 bits)
[4]     Collision
[3:0]   Reserved
```

### Phase 2: Tilemap Extension

**New Component: TilemapEx**

```cpp
class TilemapEx : public Tilemap {
public:
    // Constructor
    TilemapEx(int32_t width, int32_t height, 
              int32_t tileWidth, int32_t tileHeight);
    
    // Extended tile access
    TileDataEx getTileEx(int32_t x, int32_t y) const;
    void setTileEx(int32_t x, int32_t y, const TileDataEx& tile);
    
    // Palette-aware operations
    void setTileWithPalette(int32_t x, int32_t y, 
                           uint16_t tileID, 
                           uint8_t paletteIndex,
                           uint8_t zOrder = 3);
    
    // Bulk palette operations
    void setPaletteForRegion(int32_t x, int32_t y, 
                            int32_t width, int32_t height,
                            uint8_t paletteIndex);
    
    // Query
    bool usesIndexedColor() const { return true; }
    
private:
    std::vector<TileDataEx> m_tilesEx;  // 32-bit tiles
};
```

**Key Design Decision:**
- Separate class (`TilemapEx`) to avoid breaking existing code
- Can still use base `Tilemap` methods (internally converts)
- Clear distinction between 16-bit and 32-bit modes

### Phase 3: Palette Management

**New Component: PaletteBank**

```cpp
struct PaletteColor {
    uint8_t r, g, b, a;
};

class PaletteBank {
public:
    // Constructor
    PaletteBank(int32_t paletteCount = 32, 
                int32_t colorsPerPalette = 16);
    
    // Palette operations
    void setPalette(int32_t index, const PaletteColor* colors, int32_t count);
    void getPalette(int32_t index, PaletteColor* colors, int32_t count) const;
    
    // Preset palettes
    void loadPreset(int32_t index, const char* presetName);
    // Presets: "desert", "forest", "cave", "ice", "lava", 
    //          "night", "water", "metal", "crystal",
    //          "retro_nes", "retro_c64", "retro_cga"
    
    // File I/O
    bool loadFromFile(int32_t index, const char* path);
    bool saveToFile(int32_t index, const char* path) const;
    
    // GPU access
    MTLTexturePtr getPaletteTexture() const;
    void uploadToGPU();
    bool isDirty() const { return m_dirty; }
    
    // Validation
    void enforceConvention();  // Ensures index 0=transparent, 1=black
    
private:
    int32_t m_paletteCount;
    int32_t m_colorsPerPalette;
    std::vector<PaletteColor> m_data;  // paletteCount × colorsPerPalette
    MTLTexturePtr m_texture;
    bool m_dirty;
};
```

**Palette Texture Layout:**
```
32 palettes × 16 colors = 512 pixels
Texture format: MTLPixelFormatRGBA8Unorm
Texture size: 16 (width) × 32 (height)
Memory: 16 × 32 × 4 bytes = 2 KB

Access: texture.sample(u: colorIndex/16, v: paletteIndex/32)
```

### Phase 4: Indexed Tileset

**New Component: TilesetIndexed**

```cpp
class TilesetIndexed : public Tileset {
public:
    // Constructor
    TilesetIndexed();
    
    // Initialize with indexed color
    void initializeIndexed(int32_t tileWidth, int32_t tileHeight,
                          int32_t tileCount,
                          const uint8_t* indexedData,  // 4-bit packed
                          size_t dataSize);
    
    // Load from image (converts RGB to indexed)
    bool loadImageIndexed(const char* path,
                         int32_t tileWidth, int32_t tileHeight,
                         const PaletteColor* referencePalette = nullptr);
    
    // Get tile format
    enum class TileFormat {
        RGB888,      // Legacy
        RGBA8888,    // Legacy
        Indexed4Bit, // New
        Indexed8Bit  // Future
    };
    
    TileFormat getFormat() const { return m_format; }
    bool isIndexed() const { return m_format == TileFormat::Indexed4Bit; }
    
    // GPU texture (4-bit indices stored in R8 texture)
    MTLTexturePtr getIndexTexture() const { return m_indexTexture; }
    
private:
    TileFormat m_format;
    MTLTexturePtr m_indexTexture;  // R8 texture with 0-15 values
    std::vector<uint8_t> m_indexedData;  // CPU copy
};
```

**Index Texture Format:**
```
Pixel format: MTLPixelFormatR8Uint
Each pixel stores: 0-15 (palette index)
256 tiles × 32×32 pixels = 262,144 pixels
Memory: 262,144 bytes = 256 KB

Note: Could pack 2 pixels per byte (128 KB) but adds shader complexity
```

### Phase 5: Renderer Extension

**Extended Component: TilemapRenderer**

```cpp
// Add to TilemapRenderer class:

// Indexed rendering support
bool renderLayerIndexed(const TilemapLayer& layer, 
                       const Camera& camera,
                       const PaletteBank& palettes,
                       float time);

// Pipeline state for indexed rendering
MTLRenderPipelineStatePtr m_indexedPipeline;

// Palette texture binding
void setPaletteTexture(MTLTexturePtr paletteTexture);
```

**Metal Shader: Indexed Tile Fragment Shader**

```metal
struct TileFragmentIn {
    float4 position [[position]];
    float2 texCoord;           // UV into tile atlas
    uint tileID;
    uint paletteIndex;         // Which palette (0-31)
    uint zOrder;               // Z-order (0-7)
    uint flags;                // Flip/rotate flags
};

fragment float4 tileFragmentIndexed(
    TileFragmentIn in [[stage_in]],
    texture2d<uint, access::sample> indexTexture [[texture(0)]],   // R8Uint
    texture2d<float> paletteTexture [[texture(1)]],                // RGBA8
    sampler texSampler [[sampler(0)]]
) {
    // Step 1: Sample tile atlas to get palette index (0-15)
    uint colorIndex = indexTexture.sample(texSampler, in.texCoord).r;
    
    // Step 2: Look up actual color in palette texture
    // Palette texture layout: 16 colors wide, 32 palettes tall
    float2 paletteCoord = float2(
        (float(colorIndex) + 0.5) / 16.0,        // Which color (x)
        (float(in.paletteIndex) + 0.5) / 32.0   // Which palette (y)
    );
    
    float4 finalColor = paletteTexture.sample(texSampler, paletteCoord);
    
    // finalColor.a will be 0 for colorIndex 0 (transparent black)
    // Alpha blending happens automatically
    
    return finalColor;
}
```

### Phase 6: TilemapLayer Extension

**No new class needed** - extend existing TilemapLayer:

```cpp
// Add to TilemapLayer class:

// Palette bank for indexed tiles
void setPaletteBank(std::shared_ptr<PaletteBank> palettes);
std::shared_ptr<PaletteBank> getPaletteBank() const;

// Check if layer uses indexed color
bool isIndexedColor() const;

// Z-order sorting (if using per-tile z-order)
enum class ZOrderMode {
    LayerOnly,     // Use layer z-order only (current behavior)
    PerTile,       // Use per-tile z-order (requires sorting)
    LayerAndTile   // Combine layer + tile z-order
};

void setZOrderMode(ZOrderMode mode);
ZOrderMode getZOrderMode() const;

private:
    std::shared_ptr<PaletteBank> m_paletteBank;
    ZOrderMode m_zOrderMode = ZOrderMode::LayerOnly;
```

---

## API Design

### C API Functions

```c
// Palette Bank Management
ST_API STPaletteBankID st_palette_bank_create(
    int32_t paletteCount,      // e.g., 32
    int32_t colorsPerPalette   // e.g., 16
);

ST_API void st_palette_bank_destroy(STPaletteBankID bankID);

ST_API bool st_palette_bank_set_palette(
    STPaletteBankID bankID,
    int32_t paletteIndex,
    const STColor* colors,     // Array of RGBA colors
    int32_t colorCount
);

ST_API bool st_palette_bank_get_palette(
    STPaletteBankID bankID,
    int32_t paletteIndex,
    STColor* outColors,
    int32_t maxColors
);

ST_API bool st_palette_load_preset(
    STPaletteBankID bankID,
    int32_t paletteIndex,
    const char* presetName
);

// Indexed Tileset Creation
ST_API STTilesetID st_tileset_create_indexed(
    const char* name,
    int32_t tileWidth,
    int32_t tileHeight,
    int32_t maxTiles
);

ST_API bool st_tileset_load_image_indexed(
    STTilesetID tilesetID,
    const char* imagePath,
    int32_t tileWidth,
    int32_t tileHeight
);

// Extended Tilemap Creation
ST_API STTilemapID st_tilemap_create_indexed(
    int32_t width,
    int32_t height,
    int32_t tileWidth,
    int32_t tileHeight
);

// Extended Tile Setting
ST_API void st_tilemap_set_tile_indexed(
    STLayerID layerID,
    int32_t x,
    int32_t y,
    uint16_t tileID,
    uint8_t paletteIndex,
    uint8_t zOrder,
    uint8_t flags  // Flip/rotate
);

// Layer Palette Assignment
ST_API void st_tilemap_layer_set_palette_bank(
    STLayerID layerID,
    STPaletteBankID bankID
);
```

---

## Memory Layout

### Per-Tile Memory

**Legacy TileData (16-bit):**
```
2 bytes per tile
100×100 tiles = 20 KB
```

**New TileDataEx (32-bit):**
```
4 bytes per tile
100×100 tiles = 40 KB
```

### GPU Memory

**Indexed Tileset:**
```
Tile atlas (4-bit):  256 tiles × 32×32 × 1 byte = 256 KB
Palette data:        32 palettes × 16 colors × 4 bytes = 2 KB
Total:               ~258 KB

Compare to RGB:
Tile atlas (RGB888): 256 tiles × 32×32 × 4 bytes = 1 MB
Savings: ~75% memory reduction!
```

**Example Scene:**
```
3 indexed layers: 3 × 258 KB = 774 KB
vs.
3 RGB layers: 3 × 1 MB = 3 MB

Savings: 2.2 MB (74% reduction)
```

---

## Rendering Pipeline

### Render Flow for Indexed Tiles

1. **CPU Side:**
   ```
   TilemapManager::update()
   ├─ Update palette banks (if dirty)
   ├─ Sort tiles by z-order (if per-tile z-order enabled)
   └─ Build instance buffer (TileDataEx → GPU instance data)
   ```

2. **GPU Upload:**
   ```
   - Upload instance buffer (position, tileID, paletteIndex, flags)
   - Upload palette texture (if dirty)
   - Bind index texture (tile atlas)
   - Bind palette texture
   ```

3. **GPU Rendering:**
   ```
   Vertex Shader:
   ├─ Transform tile position
   ├─ Calculate UV coordinates
   └─ Pass through paletteIndex, zOrder, flags
   
   Fragment Shader:
   ├─ Sample index texture → get colorIndex (0-15)
   ├─ Sample palette texture at (colorIndex, paletteIndex)
   └─ Return RGBA color (with alpha for transparency)
   ```

4. **Alpha Blending:**
   ```
   GPU automatically blends based on alpha:
   - colorIndex 0 → alpha=0 → transparent
   - colorIndex 1 → alpha=255 → opaque black
   - colorIndex 2-15 → alpha=255 → opaque colors
   ```

### Z-Order Handling

**Option 1: Layer-Only (Current - Fast)**
```
Render layers in z-order
No per-tile sorting needed
```

**Option 2: Per-Tile (New - Flexible)**
```
Sort tiles by (layer.zOrder * 8 + tile.zOrder)
Render in sorted order
Requires CPU sorting or GPU depth buffer
```

**Option 3: Multiple Passes (Compromise)**
```
Render each z-order level (0-7) in separate pass
8 draw calls per layer
Good for modest z-order usage
```

---

## Backward Compatibility

### Migration Strategy

**Existing Code:**
```cpp
// Works as before
Tilemap tilemap(100, 100, 32, 32);
tilemap.setTile(x, y, TileData(42));
```

**New Indexed Code:**
```cpp
// Use extended types
TilemapEx tilemapEx(100, 100, 32, 32);
tilemapEx.setTileEx(x, y, TileDataEx(42, 3, 5));  // tile 42, palette 3, z-order 5
```

**Mixed Usage:**
```cpp
// Layer 0: Legacy RGB tiles
layer0->setTileset(rgbTileset);

// Layer 1: Indexed tiles
layer1->setTileset(indexedTileset);
layer1->setPaletteBank(paletteBank);

// Renderer handles both automatically
```

### Detection

```cpp
// Renderer detects tile format
if (layer->getTileset()->isIndexed()) {
    renderLayerIndexed(layer, camera, *layer->getPaletteBank(), time);
} else {
    renderLayer(layer, camera, time);  // Legacy path
}
```

---

## Implementation Checklist

### Phase 1: Data Structures ✅
- [x] Create TileDataEx.h (32-bit tile data)
- [ ] Add unit tests for TileDataEx
- [ ] Verify bit packing/unpacking

### Phase 2: Palette System
- [ ] Create PaletteBank class
- [ ] Implement palette texture upload
- [ ] Create preset palette library
- [ ] Add palette file I/O (PNG, ACT, GPL formats)

### Phase 3: Indexed Tileset
- [ ] Create TilesetIndexed class
- [ ] Implement RGB → indexed conversion
- [ ] Add 4-bit texture packing
- [ ] Test with sample tile images

### Phase 4: Extended Tilemap
- [ ] Create TilemapEx class
- [ ] Implement 32-bit tile storage
- [ ] Add palette-aware operations
- [ ] Migration utilities (TileData → TileDataEx)

### Phase 5: Renderer
- [ ] Create indexed tile Metal shaders
- [ ] Add indexed rendering pipeline
- [ ] Implement z-order sorting (optional)
- [ ] Performance testing

### Phase 6: API Bindings
- [ ] C API implementation
- [ ] Lua bindings
- [ ] BASIC commands
- [ ] Documentation and examples

### Phase 7: Testing
- [ ] Unit tests for all components
- [ ] Integration tests (full render pipeline)
- [ ] Performance benchmarks
- [ ] Visual tests (palette switching, z-order)

### Phase 8: Documentation
- [ ] API reference documentation
- [ ] Tutorial: Creating indexed tiles
- [ ] Tutorial: Using multiple palettes
- [ ] Example programs (C, Lua, BASIC)

---

## Performance Expectations

### Memory Savings
```
4-bit indexed: 75% smaller than RGB888
Per-tile overhead: 2× (16-bit → 32-bit)
Net savings for typical scene: ~50-60%
```

### Rendering Performance
```
Expected: Same or better than RGB
- Smaller textures = better cache usage
- Double texture lookup adds ~5% cost
- Instanced rendering still efficient
Target: 60 FPS with 10,000+ tiles
```

### Palette Operations
```
Palette change: ~0.001ms (GPU upload)
Palette animation: Zero CPU cost
Color cycling: Instant (no tile redraw)
```

---

## Future Enhancements

### Phase 2 Features
- [ ] 8-bit indexed color (256 colors per tile)
- [ ] 2-bit indexed color (4 colors per tile)
- [ ] Per-tile opacity (use reserved bits)
- [ ] Texture compression (PVRTC, ASTC)

### Phase 3 Features
- [ ] Palette interpolation (smooth color transitions)
- [ ] HDR palettes (16-bit per channel)
- [ ] Animated palettes (built-in cycling)
- [ ] Palette effects (brightness, contrast, saturation)

---

## Conclusion

This design integrates indexed color tiles into the existing tilemap system while:

✅ Maintaining full backward compatibility
✅ Adding powerful palette system (32 banks × 16 colors)
✅ Supporting per-tile z-order (8 priority levels)
✅ Reducing GPU memory by ~75%
✅ Enabling instant palette changes (day/night, themes)
✅ Following console conventions (index 0=transparent, 1=black)

The phased approach allows incremental implementation and testing. Each component can be developed and tested independently before integration.

**Estimated Implementation Time:** 2-3 weeks for core system, 1 week for bindings and documentation.