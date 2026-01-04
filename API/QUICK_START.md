# SuperTerminal v2 C API - Quick Start Guide

## Introduction

This guide will get you started using the SuperTerminal C API in 5 minutes. The API provides simple, immediate-mode functions for creating retro-style games and applications.

---

## Basic Concepts

### Immediate Mode
API calls take effect immediately or on the next frame. No complex state management needed.

```c
st_text_put(0, 0, "Hello!", COLOR_WHITE, COLOR_BLACK);  // Draws immediately
st_gfx_circle(100, 100, 50, COLOR_RED);                 // Draws on next frame
```

### Color Format
Colors are 32-bit RGBA values (0xRRGGBBAA):

```c
STColor white = st_rgb(255, 255, 255);           // Opaque white
STColor red50 = st_rgba(255, 0, 0, 128);         // 50% transparent red
STColor blue = st_hsv(240.0f, 1.0f, 1.0f);       // Pure blue via HSV
```

### Handle Types
Resources return integer handles:

```c
STSpriteID player = st_sprite_load("player.png");  // Returns handle (e.g., 1)
STSoundID coin = st_sound_load_builtin("coin");    // Returns handle (e.g., 1)
```

---

## Hello World

```c
#include "superterminal_api.h"

void my_game_loop() {
    // Clear screen
    st_text_clear();
    st_gfx_clear();
    
    // Draw text
    st_text_put(10, 5, "Hello SuperTerminal!", 
                st_rgb(255, 255, 255),  // White text
                st_rgb(0, 0, 0));       // Black background
    
    // Draw a circle
    st_gfx_circle(400, 300, 50, st_rgb(0, 255, 0));
}
```

---

## Common Tasks

### 1. Text Rendering

```c
// Put text at grid position (x, y)
st_text_put(0, 0, "Score: 100", st_rgb(255, 255, 0), st_rgb(0, 0, 0));

// Put single character
st_text_putchar(10, 5, 'X', st_rgb(255, 0, 0), st_rgb(0, 0, 0));

// Clear entire grid
st_text_clear();

// Clear region
st_text_clear_region(0, 0, 20, 10);  // x, y, width, height
```

### 2. Drawing Shapes

```c
// Filled rectangle
st_gfx_rect(10, 10, 100, 50, st_rgb(255, 0, 0));

// Rectangle outline
st_gfx_rect_outline(10, 10, 100, 50, st_rgb(255, 255, 0), 2);

// Filled circle
st_gfx_circle(100, 100, 30, st_rgb(0, 255, 0));

// Circle outline
st_gfx_circle_outline(100, 100, 30, st_rgb(0, 255, 255), 2);

// Line
st_gfx_line(0, 0, 100, 100, st_rgb(255, 255, 255), 2);

// Single pixel
st_gfx_point(50, 50, st_rgb(255, 0, 0));
```

### 3. Sprites

```c
// Load sprite
STSpriteID player = st_sprite_load("player.png");
if (player < 0) {
    printf("Failed to load sprite: %s\n", st_get_last_error());
}

// Show sprite at position
st_sprite_show(player, 100, 100);

// Transform: position, rotation (radians), scale_x, scale_y
st_sprite_transform(player, 150, 120, 0.5f, 2.0f, 1.5f);

// Tint sprite
st_sprite_tint(player, st_rgba(255, 100, 100, 255));

// Hide sprite
st_sprite_hide(player);

// Unload sprite (free memory)
st_sprite_unload(player);
```

### 4. Sound Effects

```c
// Load builtin sound
STSoundID coin = st_sound_load_builtin("coin");

// Play sound (volume 0.0-1.0)
st_sound_play(coin, 1.0f);

// Available builtin sounds:
// beep, bang, coin, jump, powerup, hurt, shoot, click
// explode, big_explosion, small_explosion, zap, pickup, blip
// sweep_up, sweep_down, random_beep
```

### 5. Music (ABC Notation)

```c
// Play ABC notation
const char* melody = 
    "X:1\n"
    "T:My Song\n"
    "M:4/4\n"
    "L:1/8\n"
    "K:C\n"
    "C D E F | G A B c |\n";

st_music_play(melody);

// Control playback
st_music_set_volume(0.7f);
st_music_pause();
st_music_resume();
st_music_stop();

// Check status
if (st_music_is_playing()) {
    printf("Music is playing\n");
}
```

### 6. Synthesized Notes

```c
// Play MIDI note (0-127), duration (seconds), volume (0-1)
st_synth_note(60, 0.5f, 0.8f);  // Middle C, 0.5 seconds, 80% volume

// Play raw frequency
st_synth_frequency(440.0f, 1.0f, 0.5f);  // A4, 1 second, 50% volume
```

### 7. Keyboard Input

```c
// Check if key is currently pressed
if (st_key_pressed(ST_KEY_SPACE)) {
    printf("Space is held down\n");
}

// Check if key was just pressed this frame
if (st_key_just_pressed(ST_KEY_ENTER)) {
    printf("Enter was just pressed\n");
}

// Check if key was just released
if (st_key_just_released(ST_KEY_ESCAPE)) {
    printf("Escape was released\n");
}

// Get character input (for text entry)
uint32_t ch = st_key_get_char();
if (ch != 0) {
    printf("Typed: %c\n", (char)ch);
}

// Clear character buffer
st_key_clear_buffer();
```

### 8. Mouse Input

```c
// Get mouse position in pixels
int mx, my;
st_mouse_position(&mx, &my);
printf("Mouse: (%d, %d)\n", mx, my);

// Get mouse position in grid coordinates
int grid_x, grid_y;
st_mouse_grid_position(&grid_x, &grid_y);

// Check mouse buttons
if (st_mouse_button(ST_MOUSE_LEFT)) {
    printf("Left button is pressed\n");
}

if (st_mouse_button_just_pressed(ST_MOUSE_RIGHT)) {
    printf("Right button just clicked\n");
}

// Get mouse wheel
float dx, dy;
st_mouse_wheel(&dx, &dy);
```

### 9. Frame Timing

```c
// Get current frame count
uint64_t frame = st_frame_count();

// Get elapsed time in seconds
double time = st_time();

// Get time since last frame (delta time)
double delta = st_delta_time();

printf("Frame %llu, Time: %.2f, Delta: %.3f\n", frame, time, delta);
```

---

## Complete Example

```c
#include "superterminal_api.h"
#include <stdio.h>
#include <math.h>

typedef struct {
    int x, y;
    int score;
    STSoundID coin_sound;
} GameState;

void init_game(GameState* state) {
    state->x = 400;
    state->y = 300;
    state->score = 0;
    state->coin_sound = st_sound_load_builtin("coin");
}

void update_game(GameState* state) {
    // Move with arrow keys
    if (st_key_pressed(ST_KEY_LEFT))  state->x -= 5;
    if (st_key_pressed(ST_KEY_RIGHT)) state->x += 5;
    if (st_key_pressed(ST_KEY_UP))    state->y -= 5;
    if (st_key_pressed(ST_KEY_DOWN))  state->y += 5;
    
    // Collect coin with space
    if (st_key_just_pressed(ST_KEY_SPACE)) {
        st_sound_play(state->coin_sound, 1.0f);
        state->score += 10;
    }
}

void draw_game(GameState* state) {
    // Clear
    st_text_clear();
    st_gfx_clear();
    
    // Draw score
    char score_text[64];
    snprintf(score_text, sizeof(score_text), "Score: %d", state->score);
    st_text_put(2, 1, score_text, st_rgb(255, 255, 0), st_rgb(0, 0, 0));
    
    // Draw player (pulsing circle)
    double time = st_time();
    float radius = 20.0f + sin(time * 5.0f) * 5.0f;
    st_gfx_circle(state->x, state->y, (int)radius, st_rgb(0, 255, 0));
    
    // Draw mouse cursor
    int mx, my;
    st_mouse_position(&mx, &my);
    st_gfx_line(mx - 10, my, mx + 10, my, st_rgb(255, 0, 0), 2);
    st_gfx_line(mx, my - 10, mx, my + 10, st_rgb(255, 0, 0), 2);
}

void game_loop() {
    static GameState state;
    static bool initialized = false;
    
    if (!initialized) {
        init_game(&state);
        initialized = true;
    }
    
    update_game(&state);
    draw_game(&state);
}
```

---

## Key Codes Reference

### Letters
`ST_KEY_A` through `ST_KEY_Z`

### Numbers
`ST_KEY_0` through `ST_KEY_9`

### Special Keys
- `ST_KEY_SPACE`
- `ST_KEY_ENTER`
- `ST_KEY_ESCAPE`
- `ST_KEY_BACKSPACE`
- `ST_KEY_TAB`

### Arrow Keys
- `ST_KEY_UP`
- `ST_KEY_DOWN`
- `ST_KEY_LEFT`
- `ST_KEY_RIGHT`

### Function Keys
`ST_KEY_F1` through `ST_KEY_F12`

---

## Color Helpers

```c
// RGB (opaque)
STColor red = st_rgb(255, 0, 0);

// RGBA (with alpha)
STColor transparent = st_rgba(255, 0, 0, 128);  // 50% transparent red

// HSV to RGB
STColor rainbow = st_hsv(hue, 1.0f, 1.0f);  // hue: 0-360

// Extract components
uint8_t r, g, b, a;
st_color_components(color, &r, &g, &b, &a);
```

---

## Error Handling

```c
STSpriteID sprite = st_sprite_load("missing.png");
if (sprite < 0) {
    const char* error = st_get_last_error();
    printf("Error: %s\n", error);
    st_clear_error();
}
```

---

## Tips & Best Practices

1. **Check return values**: Negative values usually indicate errors
2. **Unload resources**: Always call `st_sprite_unload()`, `st_sound_unload()`, etc.
3. **Use delta time**: For smooth animation independent of frame rate
4. **Clear each frame**: Call `st_text_clear()` and `st_gfx_clear()` each frame
5. **Clamp coordinates**: Keep positions within screen bounds
6. **Volume range**: Sound volume is 0.0 (silent) to 1.0 (full)
7. **Color format**: Remember it's 0xRRGGBBAA, not ARGB

---

## Common Pitfalls

❌ **Forgetting to clear**
```c
// BAD: Text accumulates
st_text_put(0, 0, "Score: 100", white, black);
st_text_put(0, 0, "Score: 110", white, black);  // Overwrites
```

✅ **Clear first**
```c
// GOOD: Clear before drawing
st_text_clear();
st_text_put(0, 0, "Score: 100", white, black);
```

❌ **Not checking errors**
```c
// BAD: Assume it worked
STSpriteID sprite = st_sprite_load("sprite.png");
st_sprite_show(sprite, 100, 100);  // Might crash if load failed
```

✅ **Check errors**
```c
// GOOD: Check return value
STSpriteID sprite = st_sprite_load("sprite.png");
if (sprite >= 0) {
    st_sprite_show(sprite, 100, 100);
} else {
    printf("Load failed: %s\n", st_get_last_error());
}
```

---

## Next Steps

1. **Read the full API**: See `superterminal_api.h` for all functions
2. **Check examples**: Look in `Examples/` directory
3. **Read architecture docs**: See `API_ARCHITECTURE.md` for design details
4. **Implementation status**: See `API_IMPLEMENTATION_STATUS.md` for what's available

---

## Getting Help

- **Error messages**: Use `st_get_last_error()` after failed operations
- **Debug output**: Use `st_debug_print("message")` for logging
- **Version info**: Use `st_version_string()` to check API version

---

## Quick Reference Card

| Category | Function | Purpose |
|----------|----------|---------|
| **Text** | `st_text_put(x, y, text, fg, bg)` | Draw text |
| | `st_text_clear()` | Clear screen |
| **Graphics** | `st_gfx_rect(x, y, w, h, color)` | Draw rectangle |
| | `st_gfx_circle(x, y, r, color)` | Draw circle |
| | `st_gfx_line(x1, y1, x2, y2, color, w)` | Draw line |
| **Sprites** | `st_sprite_load(path)` | Load sprite |
| | `st_sprite_show(id, x, y)` | Show sprite |
| | `st_sprite_transform(id, x, y, r, sx, sy)` | Transform |
| **Audio** | `st_sound_play(id, vol)` | Play sound |
| | `st_music_play(abc)` | Play music |
| | `st_synth_note(note, dur, vol)` | Synthesize note |
| **Input** | `st_key_pressed(key)` | Check key |
| | `st_mouse_position(&x, &y)` | Get mouse pos |
| **Utility** | `st_rgb(r, g, b)` | Create color |
| | `st_frame_count()` | Get frame # |
| | `st_time()` | Get elapsed time |

---

**Happy coding with SuperTerminal!** 🎮