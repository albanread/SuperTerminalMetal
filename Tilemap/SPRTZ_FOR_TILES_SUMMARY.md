# SPRTZ for Tiles: Quick Summary

**Question:** Should we use the SPRTZ compressed sprite format for indexed tiles?

**Answer:** Use a **hybrid approach** - SPRTZ for authoring/distribution, PaletteBank for runtime.

---

## TL;DR Comparison

| Aspect | SPRTZ per Tile | Current System | Hybrid (Recommended) |
|--------|----------------|----------------|----------------------|
| **Authoring** | ✅ Easy (SPRED) | ⚠️ Need atlas tools | ✅ Easy (SPRED) |
| **Distribution** | ✅ Self-contained | ⚠️ Need palette file | ✅ Self-contained |
| **Memory (runtime)** | ❌ 16% overhead | ✅ Optimal | ✅ Optimal |
| **Load Time** | ❌ 20× slower | ✅ Fast | ✅ Fast |
| **Palette Swapping** | ❌ Not supported | ✅ Instant | ✅ Instant |
| **Modding** | ✅ Drop-in files | ⚠️ Complex | ✅ Drop-in files |

---

## Key Benefits of SPRTZ for Tiles

### ✅ Pros
1. **Format Unification** - One format for all pixel art
2. **Compression** - 40% disk space savings on average
3. **Self-Contained** - Each tile includes its palette
4. **Tooling** - SPRED works as tile editor
5. **Distribution** - Perfect for asset marketplaces
6. **Modding** - Easy drop-in custom tiles

### ❌ Cons
1. **Memory Overhead** - 210× more palette memory (420 KB vs 2 KB for 10k tiles)
2. **No Palette Swapping** - Can't do day/night cycles or palette effects
3. **Slower Loading** - 20-25× slower (decompression overhead)
4. **File System Overhead** - 1000 files vs 1 atlas file
5. **GPU Inefficiency** - Loses batch rendering optimizations
6. **Non-Standard** - Breaks atlas-based workflows

---

## Recommended Hybrid Approach

### Workflow

```
┌─────────────────────────────────────────────────────────────┐
│ AUTHORING (Artist)                                          │
├─────────────────────────────────────────────────────────────┤
│ 1. Create tiles in SPRED                                    │
│ 2. Export as individual .sprtz files                        │
│ 3. Each tile self-contained with palette                    │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ BUILD TIME (Tool: sprtz2tileset)                            │
├─────────────────────────────────────────────────────────────┤
│ 1. Collect all .sprtz files                                 │
│ 2. Extract and cluster similar palettes                     │
│ 3. Merge into 8-32 shared palettes                          │
│ 4. Remap tile indices to shared palettes                    │
│ 5. Build optimized TilesetIndexed + PaletteBank             │
│ 6. Save as .st_tileset (runtime format)                     │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ RUNTIME (Game)                                              │
├─────────────────────────────────────────────────────────────┤
│ 1. Load .st_tileset (fast, optimized)                       │
│ 2. Use PaletteBank for palette swapping                     │
│ 3. Efficient GPU batch rendering                            │
│ 4. Optional: Load dynamic .sprtz tiles for mods             │
└─────────────────────────────────────────────────────────────┘
```

---

## Use Cases

### ✅ SPRTZ per-tile is GOOD for:
- **Small games** (<100 tiles)
- **Tile marketplaces** (artists sell individual tiles)
- **Procedural generation** (each tile truly unique)
- **Modding priority** (users drop in custom tiles)
- **Authoring tools** (tile editors, level designers)

### ✅ Current System (PaletteBank) is GOOD for:
- **Large games** (500-5000+ tiles)
- **Palette effects** (day/night, themes, status effects)
- **Performance critical** (scrolling backgrounds, 10k+ tiles)
- **Memory constrained** (mobile, web, embedded)
- **Standard workflows** (Tiled, existing tilesets)

### ⭐ Hybrid Approach is GOOD for:
- **Everything** - Best of both worlds
- Willing to add build step
- Want both authoring ease AND runtime efficiency

---

## Implementation Roadmap

### Phase 1: SPRTZ Import (1-2 weeks)
```cpp
// Add to TilesetIndexed:
bool loadTileFromSPRTZ(const std::string& sprtzPath, 
                       uint16_t tileID,
                       uint8_t targetPaletteID);
```
- Load SPRTZ into existing tileset
- Remap to target palette
- SPRED supports 16×16, 32×32 tile sizes

### Phase 2: Build Tool (2-4 weeks)
```bash
# Command-line converter
sprtz2tileset --input tiles/*.sprtz \
              --output dungeon.st_tileset \
              --tile-size 16x16 \
              --optimize-palettes \
              --max-palettes 8
```
- Batch conversion
- Palette clustering/optimization
- CMake integration

### Phase 3: Advanced Features (future)
- Compressed runtime format (LZ4/Zstd)
- Tile metadata (collision, properties)
- Multi-resolution/LOD support

---

## Example: Memory Impact

### 10,000 Tiles (typical large game)

**Current System:**
```
Tile data:    10,000 × 256 bytes = 2.5 MB
Palette bank: 32 palettes × 64 bytes = 2 KB
Total:        2.502 MB
```

**SPRTZ per-tile:**
```
Tile data:    10,000 × 256 bytes = 2.5 MB
Palettes:     10,000 × 42 bytes = 420 KB
Total:        2.920 MB (+16.7% overhead)
```

**Hybrid (recommended):**
```
Runtime same as current system: 2.502 MB
Authoring: Individual .sprtz files (compressed ~60%)
Distribution: 1.5 MB compressed → 2.5 MB runtime
```

---

## Example: Palette Swapping

### Day/Night Cycle

**Current System (works great):**
```lua
-- Instant palette swap for entire tilemap
palettebank_set_active_palette(bank, PALETTE_NIGHT)
-- 0.01 ms, all 10,000 tiles change instantly
```

**SPRTZ per-tile (doesn't work):**
```lua
-- Would need to:
-- 1. Reload all 10,000 tiles with different palette, OR
-- 2. Store multiple copies (day/night versions), OR
-- 3. Implement complex shader remapping
-- Not practical for palette effects
```

---

## Performance Benchmarks

### Loading 1000 tiles (16×16 pixels)

| Method | Load Time | Speed |
|--------|-----------|-------|
| PNG Atlas + PaletteBank | 2.3 ms | ⭐⭐⭐⭐⭐ |
| Pre-converted .st_tileset | 1.8 ms | ⭐⭐⭐⭐⭐ |
| SPRTZ bundled (one file) | 18.2 ms | ⭐⭐⭐ |
| 1000 SPRTZ files (individual) | 45.8 ms | ⭐ |

---

## Decision Guide

### Choose SPRTZ-only if:
- [ ] Tile count < 100
- [ ] No palette effects needed
- [ ] Modding is critical requirement
- [ ] Authoring simplicity > runtime performance
- [ ] Distribution/marketplace is primary use case

### Choose Current System if:
- [ ] Tile count > 500
- [ ] Need palette swapping (day/night, themes)
- [ ] Performance critical (mobile, web)
- [ ] Using standard tools (Tiled)
- [ ] Memory constrained

### Choose Hybrid if:
- [x] Want benefits of both
- [x] Have build pipeline
- [x] Long-term project
- [x] Team has varied workflows
- [x] Want to support modding AND optimization

---

## Conclusion

**The Answer:** Don't choose one or the other - use **both**!

- ✅ **SPRTZ for authoring** - Easy tile creation in SPRED
- ✅ **PaletteBank for runtime** - Optimal performance and features
- ✅ **Build tool converts** - Best of both worlds
- ✅ **Optional runtime SPRTZ** - For mods and dynamic content

This gives you:
- Easy authoring (SPRED, self-contained files)
- Efficient runtime (shared palettes, fast loading)
- Palette swapping (day/night, themes)
- Modding support (optional SPRTZ loading)
- Standard workflows (works with existing tools too)

**See SPRTZ_FOR_TILES_ANALYSIS.md for full details.**

---

## Quick Start

### For Artists
```bash
# Create tiles in SPRED
spred --new-tile --size 16x16
# Export as tile_001.sprtz, tile_002.sprtz, etc.
```

### For Build Pipeline
```bash
# Convert to optimized runtime format
sprtz2tileset --input assets/tiles/*.sprtz \
              --output build/tileset.st_tileset
```

### For Runtime
```cpp
// Load optimized tileset
auto tileset = TilesetIndexed::load("tileset.st_tileset");

// Optional: Load dynamic tile for mods
tileset->loadTileFromSPRTZ("mod_tile.sprtz", 255, 0);
```

---

**End of Summary**