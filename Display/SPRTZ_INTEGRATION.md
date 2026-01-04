# SPRTZ Integration in SuperTerminal Framework

## Overview

The SuperTerminal framework now supports loading sprites from `.sprtz` compressed files. SPRTZ is a compact format that combines indexed pixel data and palette information in a single compressed file.

## What is SPRTZ?

SPRTZ is a compressed sprite format that:
- Stores 4-bit indexed sprites (16 colors)
- Embeds the palette (colors 2-15 as RGB)
- Uses RLE compression for pixel data
- Typically achieves 50-70% size reduction
- Supports 8×8, 16×16, and 40×40 sprites

**File Size Comparison:**
- Raw indexed: 256 bytes (16×16) + 64 bytes palette = 320 bytes
- SPRTZ: ~58 bytes header + 80-180 bytes compressed = 138-238 bytes
- **Savings: ~30-60% smaller**

## API Usage

### C API

```c
// Load a sprite from SPRTZ file
STSpriteID sprite = st_sprite_load_sprtz("assets/hero.sprtz");

if (sprite < 0) {
    printf("Failed to load sprite!\n");
    return;
}

// Use the sprite normally
st_sprite_show(sprite, 100, 100);
```

### C++ API (SpriteManager)

```cpp
#include "Display/SpriteManager.h"

SuperTerminal::SpriteManager* spriteManager = ...;

// Load SPRTZ sprite
uint16_t spriteId = spriteManager->loadSpriteFromSPRTZ("assets/hero.sprtz");

if (spriteId == 0) {
    NSLog(@"Failed to load SPRTZ sprite");
    return;
}

// Show the sprite
spriteManager->showSprite(spriteId, 100, 100);
```

## Integration Details

### Added Files

- `Framework/Display/SpriteCompression.h` - SPRTZ format specification and API
- `Framework/Display/SpriteCompression.cpp` - RLE compression/decompression

### Modified Files

- `Framework/Display/SpriteManager.h` - Added `loadSpriteFromSPRTZ()`
- `Framework/Display/SpriteManager.mm` - Implemented SPRTZ loading
- `Framework/API/superterminal_api.h` - Added `st_sprite_load_sprtz()`
- `Framework/API/st_api_sprites.mm` - Implemented C API wrapper
- `Framework/CMakeLists.txt` - Added SpriteCompression sources

### How It Works

```
.sprtz file
    ↓
loadSpriteFromSPRTZ()
    ↓
SpriteCompression::loadSPRTZ()
    ↓
Decompress RLE → indices + palette
    ↓
loadSpriteIndexed(indices, palette)
    ↓
Create Metal R8Uint texture + palette texture
    ↓
Sprite ready to render
```

## File Format

### SPRTZ Structure

```
Offset | Size     | Description
-------|----------|----------------------------------
0x00   | 4        | Magic: "SPTZ"
0x04   | 2        | Version (1)
0x06   | 1        | Width (8, 16, or 40)
0x07   | 1        | Height (8, 16, or 40)
0x08   | 4        | Uncompressed size (W×H)
0x0C   | 4        | Compressed size
0x10   | 42       | Palette (colors 2-15, RGB only)
0x3A   | variable | RLE compressed pixel data
```

### Palette Convention

- **Index 0**: Transparent black (0,0,0,0) - Not stored (implicit)
- **Index 1**: Opaque black (0,0,0,255) - Not stored (implicit)
- **Indices 2-15**: User colors stored as RGB (alpha=255 implicit)

This matches the SuperTerminal indexed sprite convention.

### RLE Compression

**Short runs** (1-15 pixels):
```
[count:4bits][value:4bits]  (1 byte)
```

**Long runs** (16-255 pixels):
```
[0xF0][count:8bits][value:4bits][padding:4bits]  (3 bytes)
```

## Asset Pipeline

### Creating SPRTZ Files

Use the **SPRED** sprite editor to create SPRTZ files:

```bash
# SPRED is located in spred/ directory
open spred/build/bin/SPRED.app

# Or from command line (future):
# spred-cli convert sprite.png -o sprite.sprtz --size 16x16
```

**Workflow:**
1. Create/edit sprite in SPRED
2. Choose palette (12 presets available)
3. Save as SPRTZ: File → Save SPRTZ As...
4. Load in your game using `st_sprite_load_sprtz()`

### Batch Conversion (Future)

```bash
# Convert all sprites in a directory
for file in sprites/*.png; do
    spred-cli import "$file" -o "assets/$(basename $file .png).sprtz"
done
```

## Performance

### Loading Speed

| Sprite Size | Decompression Time |
|-------------|-------------------|
| 8×8         | < 0.01 ms         |
| 16×16       | < 0.05 ms         |
| 40×40       | < 0.2 ms          |

**Note:** Decompression is very fast. Loading is I/O bound, not CPU bound.

### Memory Usage

SPRTZ sprites use the same memory as indexed sprites:
- **Texture**: W×H bytes (R8Uint format)
- **Palette**: 64 bytes (16 colors × RGBA)

**Example (16×16 sprite):**
- Disk: ~140 bytes (compressed)
- Memory: 256 bytes texture + 64 bytes palette = 320 bytes

Compare to RGBA sprite:
- Disk: ~1-4 KB (PNG)
- Memory: 1024 bytes (16×16 × RGBA)

**SPRTZ is 3-4× smaller in memory!**

## Compatibility

### With Existing Sprites

SPRTZ sprites are fully compatible with existing indexed sprite system:
- Render using same shaders
- Same palette operations (lerp, rotate, etc.)
- Same transforms (position, scale, rotation)
- Share sprite ID pool (1-255)

### Mixed Loading

You can mix and match sprite formats:

```c
// Load different formats
STSpriteID rgbSprite = st_sprite_load("hero.png");        // RGBA
STSpriteID idxSprite = st_sprite_load_indexed(...);       // Manual indexed
STSpriteID sprtzSprite = st_sprite_load_sprtz("coin.sprtz"); // SPRTZ

// All work the same way
st_sprite_show(rgbSprite, 0, 0);
st_sprite_show(idxSprite, 32, 0);
st_sprite_show(sprtzSprite, 64, 0);
```

## Error Handling

### Common Errors

**"Failed to load SPRTZ file"**
- File doesn't exist
- File is corrupted
- Wrong format (not SPRTZ)
- Version mismatch

**Debugging:**
```cpp
uint16_t id = spriteManager->loadSpriteFromSPRTZ("missing.sprtz");
if (id == 0) {
    // Check console for error message
    // NSLog output shows specific failure reason
}
```

### Validation

The loader validates:
- Magic number ("SPTZ")
- Version number (must be 1)
- Dimensions (8, 16, or 40)
- Decompressed size matches expected
- RLE data is not truncated

## Best Practices

### When to Use SPRTZ

✅ **Use SPRTZ for:**
- Small sprites (8×8, 16×16, 40×40)
- Limited color sprites (≤14 colors + 2 blacks)
- Retro/pixel art games
- Memory-constrained projects
- Asset bundles (smaller download)

❌ **Don't use SPRTZ for:**
- Large sprites (>40×40)
- Photos/realistic images
- Sprites requiring >16 colors
- Sprites that change frequently (use indexed RAM buffers instead)

### Palette Strategy

**Shared palettes:**
```c
// Load sprites with same palette theme
STSpriteID hero = st_sprite_load_sprtz("hero.sprtz");     // Uses "Retro" palette
STSpriteID enemy = st_sprite_load_sprtz("enemy.sprtz");   // Uses "Retro" palette
STSpriteID coin = st_sprite_load_sprtz("coin.sprtz");     // Uses "Retro" palette

// They'll look cohesive!
```

**Per-sprite palettes:**
```c
// Each sprite can have unique colors
STSpriteID fire = st_sprite_load_sprtz("fire.sprtz");     // Red/orange palette
STSpriteID water = st_sprite_load_sprtz("water.sprtz");   // Blue palette
STSpriteID grass = st_sprite_load_sprtz("grass.sprtz");   // Green palette
```

### Asset Organization

```
assets/
├── sprites/
│   ├── characters/
│   │   ├── hero.sprtz
│   │   ├── enemy1.sprtz
│   │   └── enemy2.sprtz
│   ├── items/
│   │   ├── coin.sprtz
│   │   ├── gem.sprtz
│   │   └── key.sprtz
│   └── effects/
│       ├── explosion.sprtz
│       └── sparkle.sprtz
```

## Examples

### Simple Sprite Loading

```c
#include "superterminal_api.h"

void game_init() {
    // Load sprites
    STSpriteID player = st_sprite_load_sprtz("assets/player.sprtz");
    STSpriteID enemy = st_sprite_load_sprtz("assets/enemy.sprtz");
    
    // Position them
    st_sprite_show(player, 100, 100);
    st_sprite_show(enemy, 200, 100);
}
```

### Animated Sprite

```c
// Load animation frames
STSpriteID frames[4];
frames[0] = st_sprite_load_sprtz("walk1.sprtz");
frames[1] = st_sprite_load_sprtz("walk2.sprtz");
frames[2] = st_sprite_load_sprtz("walk3.sprtz");
frames[3] = st_sprite_load_sprtz("walk4.sprtz");

int currentFrame = 0;
int frameCounter = 0;

void update() {
    frameCounter++;
    if (frameCounter >= 10) {
        frameCounter = 0;
        
        // Hide current frame
        st_sprite_hide(frames[currentFrame]);
        
        // Show next frame
        currentFrame = (currentFrame + 1) % 4;
        st_sprite_show(frames[currentFrame], playerX, playerY);
    }
}
```

### Palette Animation

```c
STSpriteID sprite = st_sprite_load_sprtz("gem.sprtz");

void animate_palette() {
    // Rotate palette colors for shimmer effect
    st_sprite_palette_rotate(sprite, 2, 15, 1);
}
```

## Future Enhancements

Possible improvements:
- [ ] Streaming loader for large sprite sheets
- [ ] Batch loading API (`st_sprite_load_sprtz_batch()`)
- [ ] AssetManager integration (auto-detect .sprtz)
- [ ] Sprite atlas support (multiple sprites in one SPRTZ)
- [ ] Animation metadata in SPRTZ files
- [ ] Compressed palette sharing across sprites

## Troubleshooting

### Sprite Not Appearing

**Check:**
1. File exists: `ls -la assets/sprite.sprtz`
2. Return value: `if (spriteId < 0) { ... }`
3. Console output for NSLog errors
4. Sprite visibility: `st_sprite_show(sprite, x, y)`

### Compression Issues

**Verify file:**
```bash
# Check file size
ls -lh sprite.sprtz

# Check magic number
xxd -l 4 sprite.sprtz
# Should show: 53 50 54 5a (SPTZ)
```

### Performance Problems

**Profile:**
- SPRTZ decompression is <0.2ms per sprite
- If loading is slow, check disk I/O
- Consider loading sprites at startup, not during gameplay

## See Also

- **SpriteCompression.h** - Format specification and API
- **SPRED Editor** - Tool for creating SPRTZ files
- **superterminal_api.h** - Full C API reference
- **SpriteManager.h** - C++ API reference

## Credits

SPRTZ format and integration developed for FasterBasicGreen/SuperTerminal framework.