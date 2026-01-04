# Standard Palette Library

**Version:** 1.0  
**Format:** JSON and Binary (.pal)  
**Colors per Palette:** 16 (4-bit indexed)  
**Total Palettes:** 32

---

## Overview

This library contains 32 predefined palettes for use with SPRTZ sprites and indexed tiles. By referencing a standard palette, sprites and tiles can save 42 bytes of storage (97% palette data savings) and automatically share palettes when converted to tilesets.

**Files:**
- `standard_palettes.json` - Human-readable JSON format
- `standard_palettes.pal` - Binary format for fast loading (2064 bytes)

---

## Palette Categories

### 🕹️ Retro Platforms (IDs 0-7)

Classic video game system palettes for authentic retro aesthetics.

- **0: C64** - Commodore 64 classic palette
- **1: CGA** - IBM CGA Mode 4 Palette 1 (Magenta/Cyan)
- **2: CGA_ALT** - IBM CGA Mode 4 Palette 2 (Green/Red/Brown)
- **3: ZX_SPECTRUM** - ZX Spectrum classic colors
- **4: NES** - Nintendo Entertainment System palette
- **5: GAMEBOY** - Original Game Boy greenscale
- **6: GAMEBOY_COLOR** - Game Boy Color vibrant palette
- **7: APPLE_II** - Apple II hi-res colors

### 🌍 Natural Biomes (IDs 8-15)

Environment-themed palettes for terrain and backgrounds.

- **8: FOREST** - Green forest and nature theme
- **9: DESERT** - Sandy desert and arid theme
- **10: ICE** - Cold ice and snow theme
- **11: OCEAN** - Water and underwater theme
- **12: LAVA** - Hot lava and fire theme
- **13: SWAMP** - Murky swamp and bog theme
- **14: CAVE** - Dark cave and underground theme
- **15: MOUNTAIN** - Rocky mountain terrain theme

### 🎨 Themed Palettes (IDs 16-23)

Stylistic and mood-based color schemes.

- **16: DUNGEON** - Dark dungeon stone theme
- **17: NEON** - Bright neon cyberpunk colors
- **18: PASTEL** - Soft pastel colors
- **19: EARTH** - Natural earth tones
- **20: METAL** - Metallic grays and silvers
- **21: CRYSTAL** - Gem and crystal colors
- **22: TOXIC** - Radioactive greens and yellows
- **23: BLOOD** - Red gore and violence theme

### 🔧 Utility Palettes (IDs 24-31)

Special-purpose palettes for specific needs.

- **24: GRAYSCALE** - 16-level grayscale
- **25: SEPIA** - Vintage sepia tones
- **26: BLUE_TINT** - Blue monochrome
- **27: GREEN_TINT** - Green monochrome (like old monitors)
- **28: RED_TINT** - Red monochrome
- **29: HIGH_CONTRAST** - Maximum contrast for visibility
- **30: COLORBLIND_SAFE** - Deuteranopia-friendly palette
- **31: NIGHT_MODE** - Dark mode optimized for low light

---

## Usage

### In SPRED (Sprite Editor)

1. Create new sprite or open existing
2. Select **Palette Mode: Standard**
3. Choose palette from dropdown (0-31)
4. Draw using the standard palette colors
5. Save - only 1 byte palette reference stored!

### In SPRTZ Format (v2)

**Standard palette reference:**
```
Header (16 bytes)
Palette Mode: 0x00-0x1F (1 byte) ← Palette ID
Compressed pixels (variable)

Total overhead: 1 byte for palette
```

**Custom palette (backward compatible):**
```
Header (16 bytes)
Palette Mode: 0xFF (1 byte)
Custom RGB data: 42 bytes
Compressed pixels (variable)

Total overhead: 43 bytes for palette
```

### In Build Tools

When converting SPRTZ tiles to tilesets, the build tool automatically groups tiles by standard palette:

```bash
sprtz2tileset --input tiles/*.sprtz --output tileset.st_tileset

# Output:
# - 800 tiles using palette #8 (FOREST) → Share single palette
# - 150 tiles using palette #9 (DESERT) → Share single palette
# - 50 tiles with custom palettes → Optimized/merged
#
# Result: 3 palettes instead of 1000!
```

### At Runtime

The framework automatically loads the standard palette library at initialization:

```cpp
// Framework loads automatically
StandardPaletteLibrary::loadFromFile("standard_palettes.pal");

// Sprites/tiles referencing standard palettes resolve automatically
auto sprite = SpriteManager::loadSpriteFromSPRTZ("hero.sprtz");
// If hero.sprtz references palette #8, colors loaded from library
```

---

## Benefits

### Space Savings

**Per sprite/tile:**
- Standard palette: 1 byte
- Custom palette: 43 bytes
- **Savings: 42 bytes (97.7%)**

**For 1000 forest tiles:**
- All use standard palette #8 (FOREST)
- Storage: 1000 bytes (1 byte each)
- vs. Custom: 43,000 bytes (43 bytes each)
- **Total savings: 42 KB**

### Automatic Palette Sharing

Tiles using the same standard palette automatically share at runtime:

```
Runtime memory (1000 tiles):
- Tile data: 250 KB
- Palette #8: 64 bytes (shared by all 1000 tiles)
- Total: 250 KB + 64 bytes

vs. Custom palettes:
- Tile data: 250 KB
- 1000 palettes × 64 bytes: 64 KB
- Total: 314 KB (+25% overhead)
```

### Easy Palette Swapping

Games can swap entire biome themes by changing the palette bank:

```lua
-- Day mode: Use palette #8 (FOREST - bright greens)
palettebank_set_palette(bank, 0, STANDARD_PALETTE_FOREST)

-- Night mode: Use palette #31 (NIGHT_MODE - dark blues)
palettebank_set_palette(bank, 0, STANDARD_PALETTE_NIGHT_MODE)

-- All tiles using palette 0 instantly change appearance!
```

---

## Color Convention

**All palettes follow this convention:**

- **Index 0:** Transparent black `(0, 0, 0, 0)` - Always transparent
- **Index 1:** Opaque black `(0, 0, 0, 255)` - Always solid black
- **Indices 2-15:** Variable colors (14 usable colors)

This ensures consistency across all sprites and tiles.

---

## Adding Custom Palettes

If you need a palette not in the standard library, you can:

1. **Use custom palette mode** (0xFF) in SPRTZ v2
2. **Propose addition** to standard library (for next version)
3. **Create project-specific palette bank** at runtime

---

## File Format

### JSON Format (`standard_palettes.json`)

Human-readable format for editing:

```json
{
  "version": "1.0",
  "palettes": [
    {
      "id": 0,
      "name": "C64",
      "description": "Commodore 64 classic palette",
      "category": "retro",
      "colors": [
        {"r": 0, "g": 0, "b": 0, "a": 0},
        {"r": 0, "g": 0, "b": 0, "a": 255},
        ...
      ]
    },
    ...
  ]
}
```

### Binary Format (`standard_palettes.pal`)

Optimized for fast loading:

```
Header (16 bytes):
  Magic:   "STPL" (4 bytes)
  Version: 1 (uint16)
  Count:   32 (uint16)
  Reserved: (8 bytes)

Palette Data (2048 bytes):
  32 palettes × 16 colors × 4 bytes (RGBA) = 2048 bytes

Total: 2064 bytes
```

---

## Best Practices

### For Tile Artists

✅ **DO:**
- Use standard palettes for common terrain (grass, stone, water)
- Choose biome-appropriate palettes (#8-15 for natural terrain)
- Stick to one standard palette per tileset for automatic sharing

❌ **DON'T:**
- Mix multiple standard palettes in same tileset (unless needed for variety)
- Use custom palettes unless tile truly needs unique colors

### For Sprite Artists

✅ **DO:**
- Use custom palettes for unique characters (heroes, bosses)
- Use standard palettes for common objects (coins, crates, pickups)
- Use retro palettes (#0-7) for authentic retro games

❌ **DON'T:**
- Force unique sprites into standard palettes (quality over file size)

### For Game Developers

✅ **DO:**
- Load standard palette library at game startup
- Group assets by standard palette for optimization
- Use build tools to automatically optimize palette usage

❌ **DON'T:**
- Manually manage individual sprite palettes (let build tools handle it)
- Forget to bundle `standard_palettes.pal` with your game

---

## Version History

### Version 1.0 (2024)
- Initial release
- 32 standard palettes
- 4 categories: Retro, Biome, Themed, Utility
- JSON and binary formats

---

## License

Copyright © 2024 SuperTerminal Framework  
Standard palette library is part of the SuperTerminal Framework.

**Usage:**
- Free to use in games and applications
- May not be redistributed as standalone palette library
- Attribution appreciated but not required

---

## See Also

- `SPRTZ_V2_STANDARD_PALETTES.md` - Full proposal document
- `SPRTZ_FORMAT.md` - SPRTZ file format specification
- `PaletteBank.h` - Runtime palette management
- `SpriteCompression.h` - SPRTZ compression/decompression

---

**Questions or suggestions?** Contact the SuperTerminal team.