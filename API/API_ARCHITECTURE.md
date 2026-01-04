# SuperTerminal v2 C API Architecture

## Overview

The SuperTerminal v2 C API provides a clean, language-agnostic interface for all scripting language runtimes (JavaScript, Lua, Scheme, BASIC, etc.). The API is designed to be simple, thread-safe, and immediate-mode, with no retained state in the API layer itself.

## Design Principles

1. **Simple C Types Only**: No C++ objects exposed; all types are plain C types (int, float, pointers)
2. **Thread-Safe**: All API functions can be called from any thread safely
3. **Immediate Mode**: Functions take effect on the next frame; no complex state management
4. **No Retained State**: All state lives in the framework components, not the API layer
5. **Modular Implementation**: API functions are organized by service for maintainability

## Modular Structure

The API implementation is split across multiple files for better organization:

```
Framework/API/
├── superterminal_api.h          # Public C API header (all declarations)
├── superterminal_api.cpp        # Main entry point (documentation only)
├── st_api_context.h             # Internal context manager (singleton)
├── st_api_context.cpp           # Context implementation
├── st_api_display.cpp           # Text, graphics, layers, screen
├── st_api_sprites.cpp           # Sprite management
├── st_api_audio.cpp             # Sound effects, music, synthesis
├── st_api_input.cpp             # Keyboard and mouse input
├── st_api_assets.cpp            # Asset loading (future)
└── st_api_utils.cpp             # Utilities, frame control, version, debug
```

### File Responsibilities

#### `superterminal_api.h`
- Public C header with all function declarations
- Type definitions (STColor, STSoundID, STSpriteID, etc.)
- Enumerations (key codes, mouse buttons, asset types, etc.)
- Documentation for all API functions
- Included by all language runtime bindings

#### `st_api_context.h/cpp`
- Internal singleton context manager
- Stores references to framework components (TextGrid, AudioManager, etc.)
- Manages resource handle mappings (API handles → internal IDs)
- Provides error handling (last error string)
- Tracks frame count and timing
- Thread-safe access via mutexes

#### `st_api_display.cpp`
- **Text Layer**: `st_text_*` functions for character grid
- **Graphics Layer**: `st_gfx_*` functions for primitives
- **Layers**: `st_layer_*` functions for visibility/alpha/ordering
- **Screen**: `st_display_*` and `st_cell_*` functions

#### `st_api_sprites.cpp`
- **Loading**: `st_sprite_load`, `st_sprite_load_builtin`
- **Display**: `st_sprite_show`, `st_sprite_hide`
- **Transform**: `st_sprite_transform` (position, rotation, scale)
- **Effects**: `st_sprite_tint`
- **Lifecycle**: `st_sprite_unload`

#### `st_api_audio.cpp`
- **Sound Effects**: `st_sound_*` functions for playing builtin/loaded sounds
- **Music**: `st_music_*` functions for ABC notation playback
- **Synthesis**: `st_synth_*` functions for generating tones

#### `st_api_input.cpp`
- **Keyboard**: `st_key_*` functions for key state and character input
- **Mouse**: `st_mouse_*` functions for position, buttons, wheel

#### `st_api_assets.cpp`
- **Loading**: `st_asset_load`, `st_asset_load_builtin`
- **Management**: `st_asset_unload`
- **Discovery**: `st_asset_list_builtin`
- **Status**: Stub implementation; full asset system to be implemented later

#### `st_api_utils.cpp`
- **Color Utilities**: `st_rgb`, `st_rgba`, `st_hsv`, `st_color_components`
- **Frame Control**: `st_wait_frame`, `st_frame_count`, `st_time`, `st_delta_time`
- **Version Info**: `st_version`, `st_version_string`
- **Debug**: `st_debug_print`, `st_get_last_error`, `st_clear_error`

## Context Management

The API Context (`STApi::Context`) is a singleton that bridges the C API to the C++ framework:

### Component Registration

Framework components are registered with the context during app initialization:

```cpp
// In main.mm or app initialization code:
auto& ctx = STApi::Context::instance();
ctx.setTextGrid(textGrid);
ctx.setGraphics(graphicsLayer);
ctx.setSprites(spriteManager);
ctx.setAudio(audioManager);
ctx.setInput(inputManager);
ctx.setDisplay(displayManager);
```

### Resource Handle Mapping

The context maintains mappings between API handles (opaque int32_t) and internal resource IDs:

- **Sprites**: API returns `STSpriteID` → internally maps to `uint16_t` sprite ID
- **Sounds**: API returns `STSoundID` → internally maps to sound name string
- **Assets**: API returns `STAssetID` → internally maps to asset registry (future)

This indirection allows:
1. Language runtimes to pass simple integers
2. Framework to use optimal internal representations
3. Handle validation and error checking at API boundary
4. Resource lifecycle tracking

### Thread Safety

All API functions use the `ST_LOCK` macro to acquire a mutex before accessing shared state:

```cpp
ST_API void st_text_put(int x, int y, const char* text, STColor fg, STColor bg) {
    ST_LOCK;  // Acquires context mutex
    ST_CHECK_PTR(ST_CONTEXT.textGrid(), "TextGrid");
    ST_CONTEXT.textGrid()->putString(x, y, text, fg, bg);
}
```

### Error Handling

The context stores the last error message, accessible via:

```cpp
ST_SET_ERROR("Error message");           // Set error
const char* err = st_get_last_error();   // Get error
st_clear_error();                        // Clear error
```

## API Patterns

### Immediate Mode

API calls take effect immediately or on the next frame. No complex state is retained:

```c
// Draw a rectangle - takes effect on next render
st_gfx_rect(10, 10, 100, 50, st_rgb(255, 0, 0));

// Play a sound - starts playing immediately
STSoundID coin = st_sound_load_builtin("coin");
st_sound_play(coin, 1.0f);
```

### Resource Lifecycle

Resources follow a load/use/unload pattern:

```c
// Load
STSpriteID player = st_sprite_load("player.png");
STSoundID music = st_sound_load_builtin("beep");

// Use
st_sprite_show(player, 100, 100);
st_sound_play(music, 0.8f);

// Unload
st_sprite_unload(player);
st_sound_unload(music);
```

### Error Checking

Check for errors after operations that can fail:

```c
STSpriteID sprite = st_sprite_load("missing.png");
if (sprite < 0) {
    const char* error = st_get_last_error();
    printf("Failed to load sprite: %s\n", error);
    st_clear_error();
}
```

## Type Definitions

### Color Type

Colors are 32-bit RGBA values:

```c
typedef uint32_t STColor;

STColor red = st_rgb(255, 0, 0);           // Opaque red
STColor semi = st_rgba(255, 0, 0, 128);    // 50% transparent red
STColor hue = st_hsv(240.0f, 1.0f, 1.0f);  // Blue via HSV
```

Format: `0xRRGGBBAA` (red, green, blue, alpha)

### Handle Types

Opaque integer handles for resources:

```c
typedef int32_t STSoundID;
typedef int32_t STSpriteID;
typedef int32_t STAssetID;
typedef int32_t STLayerID;
```

Invalid handles are typically `-1` or `0`.

### Enumerations

#### Key Codes
Based on USB HID codes for portability:

```c
typedef enum {
    ST_KEY_A = 4, ST_KEY_B, ..., ST_KEY_Z,
    ST_KEY_0 = 30, ST_KEY_1, ..., ST_KEY_9,
    ST_KEY_ENTER = 40,
    ST_KEY_SPACE = 44,
    ST_KEY_UP = 82, ST_KEY_DOWN = 81, ...
} STKeyCode;
```

#### Mouse Buttons
```c
typedef enum {
    ST_MOUSE_LEFT = 0,
    ST_MOUSE_RIGHT = 1,
    ST_MOUSE_MIDDLE = 2,
} STMouseButton;
```

## Integration with Framework

### Initialization Flow

1. App creates framework components (DisplayManager, AudioManager, etc.)
2. App registers components with API context
3. App initializes display and starts render loop
4. API functions are now ready to use

### Frame Flow

1. App calls `beginFrame()` on components
2. Runtime executes user script (calls API functions)
3. API functions queue commands/update state
4. App calls `renderFrame()` to draw
5. App calls `endFrame()` on components
6. API context updates frame count and timing

### Example Integration

```cpp
// main.mm
int main() {
    // Create components
    auto display = std::make_shared<DisplayManager>();
    auto textGrid = std::make_shared<TextGrid>(80, 25);
    auto audio = std::make_shared<AudioManager>();
    
    // Register with API context
    auto& ctx = STApi::Context::instance();
    ctx.setDisplay(display);
    ctx.setTextGrid(textGrid);
    ctx.setAudio(audio);
    
    // Runtime can now call API functions
    st_text_put(0, 0, "Hello World!", st_rgb(255, 255, 255), st_rgb(0, 0, 0));
    st_music_play("C D E F G A B c");
    
    // Render loop
    while (running) {
        input->beginFrame();
        // ... execute user script ...
        display->renderFrame();
        input->endFrame();
        ctx.incrementFrame();
    }
}
```

## Future Improvements

### Phase 2 (Current)
- Complete asset loading system
- Add sprite atlas support
- Implement layer visibility/alpha/ordering
- Add more synthesis options (instrument selection)

### Phase 3 (Future)
- Animation system (sprite sheets, tweening)
- Particle effects
- Advanced audio (mixing, effects, streaming)
- Font loading and custom fonts
- Tilemaps and scrolling

### Phase 4 (Long-term)
- Save/load system
- Network API (multiplayer)
- Physics (optional)
- 3D support (optional)

## Language Runtime Bindings

Each language runtime wraps the C API in an idiomatic interface:

### JavaScript Example
```javascript
// JavaScript wraps C API with objects
Text.put(0, 0, "Hello", Color.white, Color.black);
Sound.play("coin", 1.0);
Music.play("C D E F G");
```

### Lua Example
```lua
-- Lua wraps C API with tables
st.text.put(0, 0, "Hello", st.color.white, st.color.black)
st.sound.play("coin", 1.0)
st.music.play("C D E F G")
```

Each runtime is responsible for:
1. Calling C API functions via FFI
2. Converting language types to C types
3. Providing idiomatic error handling
4. Managing resource cleanup (via GC finalizers)

## Summary

The modular C API architecture provides:

- ✅ Clean separation between API and implementation
- ✅ Easy to extend with new services
- ✅ Thread-safe access to framework components
- ✅ Simple resource management via handles
- ✅ Consistent error handling
- ✅ Language-agnostic design
- ✅ Maintainable code organization

This design enables SuperTerminal v2 to support multiple scripting languages while maintaining a single, consistent framework implementation.