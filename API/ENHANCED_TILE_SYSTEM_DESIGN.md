# Enhanced Tile System Design - Flexible Palettes & Color Depth

## Overview

This document describes an enhanced tile system that provides full control over:
- **Tile color depth** (1-bit, 2-bit, 4-bit, 8-bit, 16-bit, 24-bit)
- **Tile size** (4×4 to 128×128 pixels)
- **Multiple palettes** (up to 256 palettes of 16/256 colors each)
- **Per-tile palette selection** (desert palette, rock palette, leaf palette, etc.)
- **Format flexibility** (indexed, direct color, or hybrid)

Inspired by classic console architectures (SNES, Genesis, GBA) while providing modern GPU acceleration.

---

## Tile Format System

### Supported Color Depths

| Bit Depth | Colors | Usable Colors | Palette Layout | Use Case |
|-----------|--------|---------------|----------------|----------|
| 1-bit | 2 | 0 | N/A (too small) | Not supported with transparency |
| 2-bit | 4 | 2 | 0=trans, 1=black, 2-3=colors | Minimal palettes |
| 4-bit | 16 | 14 | 0=trans, 1=black, 2-15=colors | Classic console (SNES/Genesis) ⭐ |
| 8-bit | 256 | 254 | 0=trans, 1=black, 2-255=colors | Rich indexed color |
| 16-bit | 65K | 65K | Direct (RGB565) + alpha bit | True color with transparency |
| 24-bit | 16M | 16M | Direct (RGB888) | Full color, no transparency |
| 32-bit | 16M | 16M | Direct (RGBA8888) | Full color with alpha |

### Tile Sizes

```
Supported: 4×4, 8×8, 16×16, 24×24, 32×32, 48×48, 64×64, 128×128
Default: 16×16 (classic)
Recommended: 8×8, 16×16, 32×32
```

---

## Palette System Architecture

### Multiple Palette Banks

```
┌──────────────────────────────────────────────────────────┐
│            Palette Memory (GPU-resident)                  │
├──────────────────────────────────────────────────────────┤
│ Bank 0: World Palettes (32 palettes × 16 colors each)   │
│   All palettes follow convention:                        │
│     Index 0:  Transparent black (RGBA: 0,0,0,0)         │
│     Index 1:  Opaque black (RGBA: 0,0,0,255)            │
│     Index 2-15: 14 usable colors                         │
│                                                          │
│   Palette 0:  Sky & clouds (blues, whites)              │
│   Palette 1:  Desert terrain (yellows, browns)          │
│   Palette 2:  Rocky mountains (grays, browns)           │
│   Palette 3:  Grass & trees (greens, browns)            │
│   Palette 4:  Water & ice (blues, cyans)                │
│   ...                                                    │
│   Palette 31: Special effects                            │
├──────────────────────────────────────────────────────────┤
│ Bank 1: Character Palettes (32 palettes × 16)           │
│   Palette 0:  Player 1 colors                           │
│   Palette 1:  Player 2 colors                           │
│   Palette 2:  Enemy type A                              │
│   ...                                                    │
├──────────────────────────────────────────────────────────┤
│ Bank 2: UI Palettes (16 palettes × 16)                  │
│ Bank 3: Effect Palettes (16 palettes × 16)              │
└──────────────────────────────────────────────────────────┘
```

### Palette Index Convention

**Every 16-color palette MUST follow this layout:**

```
Index 0:  Transparent black (R:0,   G:0,   B:0,   A:0)    ← See-through
Index 1:  Opaque black     (R:0,   G:0,   B:0,   A:255)  ← Visible black
Index 2:  Color 1          (user-defined)
Index 3:  Color 2          (user-defined)
Index 4:  Color 3          (user-defined)
...
Index 15: Color 14         (user-defined)
```

**Why this convention?**
- **Index 0 = transparent**: Sprites/tiles can have see-through areas
- **Index 1 = opaque black**: You can actually draw black (outlines, shadows)
- **Indices 2-15**: 14 colors for your actual palette
- **Consistent**: All palettes work the same way
- **Console-accurate**: Matches NES/SNES/Genesis behavior

### Palette Configuration

```c
typedef enum {
    ST_TILE_FORMAT_1BPP = 0,    // 2 colors per tile
    ST_TILE_FORMAT_2BPP,         // 4 colors per tile
    ST_TILE_FORMAT_4BPP,         // 16 colors per tile (most common)
    ST_TILE_FORMAT_8BPP,         // 256 colors per tile
    ST_TILE_FORMAT_RGB565,       // Direct color 16-bit
    ST_TILE_FORMAT_RGB888,       // Direct color 24-bit
    ST_TILE_FORMAT_RGBA8888      // Direct color 32-bit with alpha
} STTileFormat;

typedef struct {
    uint8_t r, g, b, a;          // RGBA color (0-255)
} STColor;

typedef struct {
    STColor colors[256];          // Up to 256 colors
    int32_t colorCount;           // Actual colors used (2/4/16/256)
    char name[64];                // Palette name
    bool isDirty;                 // Needs GPU upload
} STPalette;

// Palette index constants
#define ST_PALETTE_INDEX_TRANSPARENT  0    // Always transparent black
#define ST_PALETTE_INDEX_BLACK        1    // Always opaque black
#define ST_PALETTE_INDEX_FIRST_COLOR  2    // First user-defined color
#define ST_PALETTE_USABLE_COLORS      14   // For 16-color palettes (indices 2-15)
```

---

## Enhanced API

### Tileset Creation with Format

```c
// Create tileset with specific format
ST_API STTilesetID st_tileset_create_ex(
    const char* name,
    int32_t tileWidth,           // e.g., 32
    int32_t tileHeight,          // e.g., 32
    STTileFormat format,         // e.g., ST_TILE_FORMAT_4BPP
    int32_t maxTiles             // Max tiles in set
);

// Simpler version (defaults to 16×16, 4BPP)
ST_API STTilesetID st_tileset_create(const char* name);

// Load tileset from image with format detection
ST_API bool st_tileset_load_image(
    STTilesetID tilesetID,
    const char* imagePath,
    int32_t tileWidth,
    int32_t tileHeight,
    STTileFormat format          // Or ST_TILE_FORMAT_AUTO for detection
);
```

### Palette Management

```c
// ====================================================================
// PALETTE BANKS
// ====================================================================

// Create palette bank
ST_API STPaletteBankID st_palette_bank_create(
    const char* name,
    int32_t paletteCount,        // How many palettes (e.g., 32)
    int32_t colorsPerPalette     // Colors per palette (2/4/16/256)
);

// Set palette in bank
ST_API bool st_palette_bank_set_palette(
    STPaletteBankID bankID,
    int32_t paletteIndex,        // 0-31
    const STColor* colors,       // Array of colors
    int32_t colorCount           // Number of colors
);

// Get palette from bank
ST_API bool st_palette_bank_get_palette(
    STPaletteBankID bankID,
    int32_t paletteIndex,
    STColor* outColors,
    int32_t maxColors
);

// Load palette from file (PNG, ACT, PAL, GPL)
ST_API bool st_palette_load(
    STPaletteBankID bankID,
    int32_t paletteIndex,
    const char* filePath
);

// Save palette to file
ST_API bool st_palette_save(
    STPaletteBankID bankID,
    int32_t paletteIndex,
    const char* filePath
);

// ====================================================================
// PRESET PALETTES
// ====================================================================

// Load preset palette (similar to video modes)
ST_API bool st_palette_load_preset(
    STPaletteBankID bankID,
    int32_t paletteIndex,
    const char* presetName       // "desert", "forest", "cave", etc.
);

// Available presets:
//   "desert"      - Sandy yellows, browns, warm tones
//   "forest"      - Greens, browns, earth tones
//   "cave"        - Grays, blues, dark tones
//   "ice"         - Blues, whites, cool tones
//   "lava"        - Reds, oranges, hot tones
//   "night"       - Dark blues, purples
//   "water"       - Blue gradients
//   "metal"       - Grays, silvers
//   "crystal"     - Bright, saturated colors
//   "retro_nes"   - NES color palette
//   "retro_c64"   - Commodore 64 palette
//   "retro_cga"   - CGA 4-color modes
```

### Per-Tile Palette Selection

```c
// Set tile with palette selection
ST_API void st_tilemap_set_tile_ex(
    STLayerID layerID,
    int32_t x,
    int32_t y,
    uint16_t tileID,
    int32_t paletteIndex,        // Which palette to use (0-31)
    uint8_t flags                // Flip/rotate flags
);

// Get tile info including palette
ST_API bool st_tilemap_get_tile_ex(
    STLayerID layerID,
    int32_t x,
    int32_t y,
    uint16_t* outTileID,
    int32_t* outPaletteIndex,
    uint8_t* outFlags
);

// Set default palette for layer
ST_API void st_tilemap_layer_set_default_palette(
    STLayerID layerID,
    int32_t paletteIndex
);
```

### Tile Data Structure

```c
// Enhanced tile data (32-bit)
typedef struct {
    uint16_t tileID;             // Tile index (0-65535)
    uint8_t paletteIndex;        // Palette selection (0-255)
    uint8_t flags;               // Flip/rotate/priority
    /*
    Flags bits:
      0: Flip horizontal
      1: Flip vertical
      2: Rotate 90°
      3: Rotate 180°
      4-5: Priority layer (0-3)
      6-7: Reserved
    */
} STTileData;
```

---

## Usage Examples

### Example 1: Classic 16-Color Tiled World

```c
// Create tileset (32×32 tiles, 4-bit color = 16 colors per tile)
STTilesetID worldTiles = st_tileset_create_ex(
    "world_tiles",
    32, 32,                      // 32×32 pixel tiles
    ST_TILE_FORMAT_4BPP,         // 16 colors per tile
    256                          // 256 different tiles
);

// Load tileset image (will be converted to 4BPP indexed)
st_tileset_load_image(worldTiles, "assets/world_tiles.png", 
                      32, 32, ST_TILE_FORMAT_4BPP);

// Create palette bank for world tiles
STPaletteBankID worldPalettes = st_palette_bank_create(
    "world_palettes",
    32,                          // 32 different palettes
    16                           // 16 colors each
);

// Define desert palette (palette 0)
STColor desertColors[16] = {
    {0, 0, 0, 0},                // [0] Transparent black (REQUIRED)
    {0, 0, 0, 255},              // [1] Opaque black (REQUIRED)
    {240, 220, 130, 255},        // [2] Sandy yellow (base sand)
    {210, 180, 100, 255},        // [3] Dark sand
    {180, 150, 80, 255},         // [4] Darker sand
    {255, 200, 100, 255},        // [5] Bright sun-lit sand
    {160, 120, 60, 255},         // [6] Brown rock
    {200, 160, 100, 255},        // [7] Light rock
    {100, 80, 50, 255},          // [8] Dark rock
    {135, 206, 235, 255},        // [9] Sky blue
    {0, 100, 200, 255},          // [10] Deep sky
    {255, 255, 255, 255},        // [11] White (clouds/highlights)
    {50, 150, 50, 255},          // [12] Cactus green
    {200, 100, 50, 255},         // [13] Terracotta
    {150, 75, 25, 255},          // [14] Dark brown
    {255, 150, 50, 255}          // [15] Orange accent
};
st_palette_bank_set_palette(worldPalettes, 0, desertColors, 16);

// Define grass palette (palette 1)
STColor grassColors[16] = {
    {0, 0, 0, 0},                // [0] Transparent black (REQUIRED)
    {0, 0, 0, 255},              // [1] Opaque black (REQUIRED)
    {50, 180, 50, 255},          // [2] Grass green
    {40, 150, 40, 255},          // [3] Dark grass
    {60, 200, 60, 255},          // [4] Bright grass
    {100, 200, 100, 255},        // [5] Light grass
    {80, 120, 40, 255},          // [6] Olive/moss
    {120, 80, 40, 255},          // [7] Brown dirt
    {160, 120, 80, 255},         // [8] Light dirt
    {135, 206, 235, 255},        // [9] Sky blue
    {255, 255, 255, 255},        // [10] White
    {150, 100, 50, 255},         // [11] Tree bark
    {200, 150, 100, 255},        // [12] Light bark
    {255, 200, 50, 255},         // [13] Flower yellow
    {255, 100, 100, 255},        // [14] Flower red
    {200, 100, 200, 255}         // [15] Flower purple
};
st_palette_bank_set_palette(worldPalettes, 1, grassColors, 16);

// Define rock/cave palette (palette 2)
st_palette_load_preset(worldPalettes, 2, "cave");

// Create map
STTilemapID map = st_tilemap_create(100, 100, 32, 32);
STLayerID layer = st_tilemap_layer_create(map);

// Place desert tiles using desert palette
for (int x = 0; x < 30; x++) {
    for (int y = 20; y < 40; y++) {
        st_tilemap_set_tile_ex(layer, x, y, 
            5,      // Tile ID (sand tile)
            0,      // Palette 0 (desert)
            0       // No flip
        );
    }
}

// Place grass tiles using grass palette
for (int x = 30; x < 60; x++) {
    for (int y = 20; y < 40; y++) {
        st_tilemap_set_tile_ex(layer, x, y,
            5,      // Same tile ID!
            1,      // Palette 1 (grass) - different colors!
            0
        );
    }
}

// Place cave tiles using cave palette
for (int x = 60; x < 90; x++) {
    for (int y = 20; y < 40; y++) {
        st_tilemap_set_tile_ex(layer, x, y,
            5,      // Same tile again
            2,      // Palette 2 (cave)
            0
        );
    }
}

// Example: Sprite with transparency
// Tile uses index 0 for background (transparent)
// and indices 2-15 for the actual sprite colors
st_tilemap_set_tile_ex(layer, 50, 50,
    100,    // Sprite tile ID
    3,      // Character palette
    0
);
// Pixels with index 0 are transparent (see through to layer below)
// Pixels with index 1 are black (visible outline)
// Pixels with indices 2-15 show the character colors
```

**Key Points**: 
- The same tile (ID 5) appears in three different color schemes by selecting different palettes!
- Index 0 is always transparent, allowing sprites and tiles to have see-through areas
- Index 1 is opaque black for outlines and shadows

### Example 2: BASIC Language - Easy Palette Swapping

```basic
REM Initialize tilemap with 32×32 tiles, 16-color format
10 TILEMAP_INIT 640, 480
20 tileset = TILESET_CREATE_EX("world", 32, 32, TILE_FORMAT_4BPP, 256)
30 TILESET_LOAD_IMAGE tileset, "world_tiles.png", 32, 32

REM Create palette bank with 8 palettes
40 palettes = PALETTE_BANK_CREATE("world_pal", 8, 16)

REM Load preset palettes
50 PALETTE_LOAD_PRESET palettes, 0, "desert"
60 PALETTE_LOAD_PRESET palettes, 1, "forest"
70 PALETTE_LOAD_PRESET palettes, 2, "cave"
80 PALETTE_LOAD_PRESET palettes, 3, "ice"
90 PALETTE_LOAD_PRESET palettes, 4, "lava"

REM Create map
100 map = TILEMAP_CREATE(100, 100, 32, 32)
110 layer = TILEMAP_LAYER_CREATE(map)

REM Draw world with different biomes
120 REM Desert biome (left)
130 FOR x = 0 TO 24
140   FOR y = 0 TO 49
150     tile = INT(RND * 4) + 1  REM Random desert tile
160     TILEMAP_SET_TILE_EX layer, x, y, tile, 0, 0  REM Palette 0
170   NEXT y
180 NEXT x

190 REM Forest biome (middle)
200 FOR x = 25 TO 49
210   FOR y = 0 TO 49
220     tile = INT(RND * 4) + 1
230     TILEMAP_SET_TILE_EX layer, x, y, tile, 1, 0  REM Palette 1
240   NEXT y
250 NEXT x

260 REM Cave biome (right)
270 FOR x = 50 TO 74
280   FOR y = 0 TO 49
290     tile = INT(RND * 4) + 1
300     TILEMAP_SET_TILE_EX layer, x, y, tile, 2, 0  REM Palette 2
310   NEXT y
320 NEXT x

REM Camera follows player through different biomes
330 TILEMAP_SET_CAMERA playerX, playerY
```

### Example 3: Animated Palette Cycling

```c
// Create lava palette with color cycling
STColor lavaBase[16];
st_palette_load_preset(worldPalettes, 4, "lava");

// Animation loop
float time = 0;
while (running) {
    time += deltaTime;
    
    // Cycle lava colors for animation effect
    float cycle = sin(time * 2.0f) * 0.5f + 0.5f;  // 0-1
    
    STColor lavaCycled[16];
    st_palette_bank_get_palette(worldPalettes, 4, lavaBase, 16);
    
    // Brighten reds cyclically for "glowing" effect
    for (int i = 0; i < 8; i++) {
        lavaCycled[i].r = lavaBase[i].r + (int)(cycle * 50);
        lavaCycled[i].g = lavaBase[i].g;
        lavaCycled[i].b = lavaBase[i].b;
        lavaCycled[i].a = 255;
    }
    
    st_palette_bank_set_palette(worldPalettes, 4, lavaCycled, 16);
    
    // No need to redraw tiles - palette change updates all tiles instantly!
}
```

### Example 4: Day/Night Cycle with Palette Lerp

```c
// Interpolate between day and night palettes
void update_daynight_cycle(float timeOfDay) {
    // timeOfDay: 0.0 = midnight, 0.5 = noon, 1.0 = midnight
    
    STColor dayPalette[16];
    STColor nightPalette[16];
    STColor currentPalette[16];
    
    st_palette_bank_get_palette(worldPalettes, 0, dayPalette, 16);
    st_palette_bank_get_palette(worldPalettes, 1, nightPalette, 16);
    
    // Lerp between palettes
    float brightness = (sin(timeOfDay * 2 * M_PI) + 1.0f) / 2.0f;
    
    for (int i = 0; i < 16; i++) {
        currentPalette[i].r = dayPalette[i].r * brightness + 
                              nightPalette[i].r * (1.0f - brightness);
        currentPalette[i].g = dayPalette[i].g * brightness + 
                              nightPalette[i].g * (1.0f - brightness);
        currentPalette[i].b = dayPalette[i].b * brightness + 
                              nightPalette[i].b * (1.0f - brightness);
        currentPalette[i].a = 255;
    }
    
    // Update palette - entire world changes instantly!
    st_palette_bank_set_palette(worldPalettes, 0, currentPalette, 16);
}
```

---

## Metal Shader Implementation

### Palette Texture Format

```metal
// Palette stored as 2D texture (256 palettes × 256 colors)
texture2d<float> paletteTexture [[texture(1)]];

// OR for smaller palettes:
// 32 palettes × 16 colors = 512 pixels = 32×16 texture
```

### Fragment Shader with Palette Lookup

```metal
struct TileFragmentIn {
    float4 position [[position]];
    float2 texCoord;
    uint tileID;
    uint paletteIndex;
    uint flags;
};

fragment float4 tileFragmentShader(
    TileFragmentIn in [[stage_in]],
    texture2d<float> tileTexture [[texture(0)]],
    texture2d<float> paletteTexture [[texture(1)]],
    sampler texSampler [[sampler(0)]]
) {
    // Sample tile texture to get palette index
    float4 texel = tileTexture.sample(texSampler, in.texCoord);
    
    // For 4BPP: texel.r contains palette index (0-15)
    uint colorIndex = uint(texel.r * 255.0);
    
    // Look up color in palette
    // Palette texture coords: (colorIndex / 256, paletteIndex / 32)
    float2 paletteCoord = float2(
        (float(colorIndex) + 0.5) / 256.0,
        (float(in.paletteIndex) + 0.5) / 32.0
    );
    
    float4 finalColor = paletteTexture.sample(texSampler, paletteCoord);
    
    return finalColor;
}
```

---

## Preset Palette Library

### Desert Palette
```
0: Transparent  (0,0,0,0)     ← REQUIRED: See-through
1: #000000      (0,0,0,255)   ← REQUIRED: Opaque black
2: #F0DC82      Sandy yellow (base sand)
3: #D2B464      Dark sand
4: #B49650      Darker sand
5: #FFC864      Bright sun-lit sand
6: #A0783C      Brown rock
7: #C8A064      Light rock
8: #644832      Dark rock
9: #87CEEB      Sky blue
10: #0064C8     Deep sky
11: #FFFFFF     White (clouds/highlights)
12: #329632     Cactus green
13: #C86432     Terracotta
14: #964B19     Dark brown
15: #FF9632     Orange accent
```

### Forest Palette
```
0: Transparent  (0,0,0,0)     ← REQUIRED: See-through
1: #000000      (0,0,0,255)   ← REQUIRED: Opaque black
2: #32B432      Grass green
3: #289628      Dark grass
4: #3CC83C      Bright grass
5: #64C864      Light grass
6: #507828      Olive/moss
7: #785028      Brown dirt
8: #A07850      Light dirt
9: #87CEEB      Sky blue
10: #FFFFFF     White
11: #966432     Tree bark
12: #C89664     Light bark
13: #FFC832     Yellow flower
14: #FF6464     Red flower
15: #C864C8     Purple flower
```

### Cave Palette
```
0: Transparent  (0,0,0,0)     ← REQUIRED: See-through
1: #000000      (0,0,0,255)   ← REQUIRED: Opaque black
2: #404040      Dark gray stone
3: #303030      Darker stone
4: #505050      Light stone
5: #606060      Lighter stone
6: #202838      Blue-gray
7: #384050      Cool gray
8: #506478      Light cool gray
9: #4080C0      Crystal blue
10: #60A0E0     Bright crystal
11: #704020     Brown mineral
12: #906030     Orange mineral
13: #C08040     Gold ore
14: #E0A050     Bright gold
15: #FFFFFF     White crystal
```

### More Presets
- **Ice**: Blues, whites, cyan tones
- **Lava**: Reds, oranges, yellows, black
- **Night**: Dark blues, purples, muted colors
- **Water**: Blue gradients from dark to cyan
- **Metal**: Grays, silvers, metallic sheens
- **Crystal**: Saturated bright colors
- **Retro NES**: Authentic NES palette (54 colors)
- **Retro C64**: Commodore 64 palette (16 colors)
- **Retro CGA**: IBM CGA palettes (various 4-color modes)

---

## Memory Layout

### GPU Memory Usage

**4BPP Tileset (most common):**
```
Tile texture: 256 tiles × 32×32 pixels × 1 byte = 256 KB (indices)
Palette data: 32 palettes × 16 colors × 4 bytes = 2 KB
Total: ~258 KB per tileset

Compare to true color:
256 tiles × 32×32 × 4 bytes = 1 MB (4× larger!)
```

**Note on transparency:**
- Index 0 in tile data = lookup palette[0] = transparent (RGBA: 0,0,0,0)
- GPU alpha blending handles transparency automatically
- No special transparency flag needed per-tile

**Benefits:**
- 4× smaller texture memory
- Instant color changes via palette swap
- Easy theme variations
- Efficient for retro/stylized graphics

---

## Compatibility & Migration

### Existing Code
```c
// Old API still works (defaults to RGB888)
STTilesetID tiles = st_tileset_create("tiles");
st_tileset_load_image(tiles, "tiles.png", 16, 16, ST_TILE_FORMAT_AUTO);
```

### New Code
```c
// Opt-in to palette mode for efficiency
STTilesetID tiles = st_tileset_create_ex("tiles", 32, 32, 
                                          ST_TILE_FORMAT_4BPP, 256);
STPaletteBankID pals = st_palette_bank_create("pals", 32, 16);
```

---

## Advantages Over Current System

1. **Memory Efficient**: 4× smaller textures for indexed color
2. **Instant Color Changes**: Change palette, not pixels
3. **Theme Variations**: One tileset, many color schemes
4. **Classic Aesthetic**: Perfect for retro/pixel art games
5. **Performance**: Smaller textures = better cache usage
6. **Flexibility**: Can still use direct color (RGB) when needed
7. **Console Accurate**: Matches SNES/Genesis/GBA capabilities

---

## Implementation Roadmap

- [ ] Extend `STTileFormat` enum and structures
- [ ] Create `PaletteBank` class for GPU palette management
- [ ] Add palette texture to `TilemapRenderer`
- [ ] Update shaders for palette lookup
- [ ] Implement `st_palette_bank_*` API functions
- [ ] Add `st_tilemap_set_tile_ex` with palette parameter
- [ ] Create preset palette library
- [ ] Add tileset conversion tools (RGB → indexed)
- [ ] Write documentation and examples
- [ ] Add BASIC/Lua bindings
- [ ] Performance benchmarks

---

## Conclusion

This enhanced tile system provides the flexibility needed for efficient retro-style games while maintaining compatibility with modern true-color approaches. The palette system enables:

- **Efficient memory use** (4× reduction for 4BPP)
- **Instant visual themes** (desert, forest, cave from one tileset)
- **Dynamic effects** (day/night, lava glow, palette animation)
- **Console authenticity** (SNES/Genesis-style graphics)
- **Built-in transparency** (index 0 always transparent, index 1 always opaque black)
- **14 usable colors per palette** (indices 2-15)

Perfect for pixel art games, retro projects, or any game that needs efficient tile-based rendering with rich visual variety!

### Important Convention Summary

**Every 16-color palette must reserve:**
- **Index 0**: Transparent black (for see-through pixels)
- **Index 1**: Opaque black (for visible black pixels)
- **Indices 2-15**: Your 14 custom colors

This ensures consistent transparency behavior across all tiles and sprites while still providing opaque black when you need it!