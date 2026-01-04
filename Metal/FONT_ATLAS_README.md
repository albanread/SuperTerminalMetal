# Font Atlas Generation

## Overview

The font rendering system uses pre-generated bitmap font atlases for pixel-perfect, artifact-free text rendering. This approach eliminates the antialiasing artifacts that occurred with CoreText's dynamic font rendering.

## Current Font

**PetMe128** - A high-resolution C64-style bitmap font from Kreative Korp
- Source: `Assets/font/petme/PetMe128.ttf`
- License: Kreative Software Relay Fonts Free Use License (see `Assets/font/petme/FreeLicense.txt`)
- Character set: ASCII 32-126 (95 printable characters)

## Font Atlas Specifications

- **Cell Size**: 16×16 pixels per character
- **Atlas Dimensions**: 256×128 pixels (power-of-2 for GPU efficiency)
- **Layout**: 16 glyphs per row, 6 rows
- **Format**: 8-bit grayscale PNG (R8Unorm Metal texture)
- **Baseline Alignment**: Characters aligned to proper baseline with descenders extending below

## Generating a Font Atlas

### Prerequisites

```bash
pip install Pillow
```

### Basic Usage

```bash
cd Framework/Metal
python3 generate_font_atlas.py <font.ttf> <output.png> <cell_width> <cell_height>
```

### Example: Current Font

```bash
python3 generate_font_atlas.py \
    ../../Assets/font/petme/PetMe128.ttf \
    font_atlas_petme128_16x16.png \
    16 16
```

This generates:
- `font_atlas_petme128_16x16.png` - The texture atlas
- `font_atlas_petme128_16x16_metadata.h` - C header with dimensions

### Installing the Atlas

1. Generate the atlas as shown above
2. Copy to app bundle:
   ```bash
   cp font_atlas_petme128_16x16.png build/bin/FasterBASIC.app/Contents/Resources/
   ```
3. Update `FontAtlas.mm` to reference the correct filename (line ~701)
4. Update glyph dimensions in `FontAtlas.mm` (lines ~731-732)
5. Rebuild the app

## How It Works

### Generation Process

1. **Load Font**: PIL loads the TrueType font at the requested cell height
2. **Calculate Metrics**: Extract font ascent/descent for baseline positioning
3. **Create Atlas**: Allocate a grayscale image with power-of-2 dimensions
4. **Render Glyphs**: 
   - Each character is centered horizontally in its cell
   - Characters are aligned to a consistent baseline
   - PIL's `draw.text()` handles baseline positioning automatically
   - Descenders (g, j, p, q, y) extend below the baseline naturally
5. **Save Output**: Write PNG and metadata header file

### Runtime Loading

The `FontAtlas::loadTrueTypeFont()` method:

1. Checks for pre-rendered PNG atlas in app bundle
2. If found:
   - Loads PNG using NSImage/NSBitmapImageRep
   - Extracts grayscale pixel data
   - Creates Metal R8Unorm texture
   - Generates glyph UV coordinates based on grid layout
3. If not found:
   - Falls back to CoreText dynamic rendering (Menlo font)

### Glyph Lookup

Character → Glyph mapping:
```cpp
int charIndex = character - firstChar_;  // e.g., 'A' (65) - 32 = 33
int row = charIndex / glyphsPerRow_;     // 33 / 16 = 2
int col = charIndex % glyphsPerRow_;     // 33 % 16 = 1
float atlasX = (col * glyphWidth_) / atlasWidth_;
float atlasY = (row * glyphHeight_) / atlasHeight_;
```

## Advantages Over Dynamic Rendering

1. **No Artifacts**: Bitmap rendering eliminates antialiasing bleed between characters
2. **Consistent Quality**: Same appearance on all displays (Retina or not)
3. **Performance**: No runtime font rasterization overhead
4. **Control**: Complete control over character positioning and appearance
5. **Pixel Perfect**: Ideal for retro/terminal aesthetics

## Alternative Fonts

To use a different font:

1. Obtain a TrueType (.ttf) font file
2. Generate atlas with desired cell dimensions:
   ```bash
   python3 generate_font_atlas.py path/to/font.ttf output.png 16 16
   ```
3. Copy PNG to app bundle
4. Update `FontAtlas.mm` to load the new atlas
5. Update glyph dimensions to match your cell size

### Recommended Cell Sizes

- **8×8**: Classic bitmap font look (requires small font size)
- **16×16**: Good balance (current default)
- **16×32**: Tall cells for better readability
- **32×32**: Large, crisp characters

## Troubleshooting

### Characters Look Scrambled
- Check that glyph dimensions in `FontAtlas.mm` match the generated atlas metadata
- Verify `glyphsPerRow_` is set correctly (should be 16)

### Characters Too Thin/Thick
- Adjust the cell size when generating the atlas
- Larger cells = more room for thicker strokes

### Baseline Misalignment
- The script uses PIL's automatic baseline handling
- All characters should align properly with descenders below baseline
- If not, regenerate the atlas (don't modify the script's text positioning)

### Missing Characters
- Default range is ASCII 32-126 (printable characters)
- To include more characters, modify `first_char` and `last_char` in the script

## Technical Details

### Atlas Layout

```
Character 32 (space) at (0, 0)
Character 33 (!) at (16, 0)
...
Character 47 (/) at (240, 0)
Character 48 (0) at (0, 16)
...
Character 126 (~) at (224, 80)
```

### Metal Texture Format

- **Pixel Format**: `MTLPixelFormatR8Unorm`
- **Single channel**: 8-bit grayscale
- **Value range**: 0 (black/transparent) to 255 (white/opaque)
- **Filtering**: Point sampling (no interpolation) for pixel-perfect rendering

### UV Coordinate Calculation

Normalized texture coordinates (0.0 to 1.0):
```
u0 = (col * cellWidth) / atlasWidth
v0 = (row * cellHeight) / atlasHeight
u1 = u0 + (cellWidth / atlasWidth)
v1 = v0 + (cellHeight / atlasHeight)
```

## Future Improvements

- Support for multiple font sizes at runtime
- Extended character sets (Unicode, box drawing)
- Color emoji support
- Dynamic font atlas generation at app startup
- Font atlas compression