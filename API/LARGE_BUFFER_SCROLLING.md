# Large Buffer Scrolling Design

## The Challenge

Standard video mode buffers are the same size as the screen resolution:
- **LORES**: 160×120 pixels per buffer
- **XRES**: 640×480 pixels per buffer  
- **WRES**: 1280×720 pixels per buffer
- **PRES**: 1920×1080 pixels per buffer
- **URES**: 2560×1440 pixels per buffer

**But games need larger worlds!** A platformer might have a 4-screen-wide level (2560×480 for XRES), or an RPG might have a massive 10-screen map (6400×4800).

## Solution Overview

We provide **three complementary approaches**, each optimized for different use cases:

### 1. **Oversized Buffers** (Best for small-medium worlds)
Allocate buffers larger than screen resolution, scroll viewport over them.

### 2. **Buffer Atlases** (Best for complex scenes)
Pack multiple screens/sprites into one large texture, select regions dynamically.

### 3. **Tilemap Integration** (Best for huge worlds)
Use existing tilemap system for tile-based scrolling, video buffers for entities.

---

## Approach 1: Oversized Buffers

### Concept

Allow buffers to be allocated at custom sizes larger than the screen:

```c
// Create a buffer 4x larger than screen for a wide level
st_xres_create_large_buffer(5,    // buffer ID
                            2560,  // width (4× screen width)
                            480);  // height (same as screen)

// Draw to the entire large buffer
st_xres_set_active_buffer(5);
for (int x = 0; x < 2560; x += 32) {
    st_xres_rect_fill_gpu(5, x, 400, 32, 80, 10); // Draw platforms
}

// Create scroll layer that views a portion of the large buffer
STScrollLayerConfig config = {0};
config.bufferID = 5;
config.sourceX = 0;        // Start at left edge of buffer
config.sourceY = 0;
config.sourceWidth = 640;  // View 640 pixels wide (screen size)
config.sourceHeight = 480; // View 480 pixels tall (screen size)
config.viewportX = 0;      // Display at top-left of screen
config.viewportY = 0;
config.viewportWidth = 640;
config.viewportHeight = 480;

STScrollLayerID layer = st_scroll_layer_create(&config);

// Scroll by moving the source region
st_scroll_layer_set_source(layer, cameraX, cameraY, 640, 480);
```

### Architecture Changes

#### New API Functions

```c
// Create oversized buffer (beyond standard 8 buffers)
ST_API bool st_video_create_large_buffer(
    int32_t bufferID,      // 8-15 for large buffers
    int32_t width,         // Width in pixels
    int32_t height         // Height in pixels
);

// Destroy large buffer and free GPU memory
ST_API void st_video_destroy_large_buffer(int32_t bufferID);

// Query large buffer dimensions
ST_API bool st_video_get_buffer_size(
    int32_t bufferID,
    int32_t* width,
    int32_t* height
);

// Check if buffer is oversized
ST_API bool st_video_is_large_buffer(int32_t bufferID);
```

#### Metal Implementation

```metal
// Fragment shader samples from arbitrary buffer region
fragment float4 scrollFragmentShader(
    VertexOut in [[stage_in]],
    texture2d<float> bufferTexture [[texture(0)]],
    constant ScrollUniforms& uniforms [[buffer(0)]]
) {
    // Map viewport coordinates to buffer coordinates
    float2 bufferCoord = in.texCoord * uniforms.sourceSize + uniforms.sourcePos;
    
    // Apply wrap mode if needed
    if (uniforms.wrapMode & WRAP_X_BIT) {
        bufferCoord.x = fmod(bufferCoord.x, uniforms.bufferSize.x);
    }
    if (uniforms.wrapMode & WRAP_Y_BIT) {
        bufferCoord.y = fmod(bufferCoord.y, uniforms.bufferSize.y);
    }
    
    // Clamp to buffer bounds
    bufferCoord = clamp(bufferCoord, float2(0), uniforms.bufferSize);
    
    // Sample texture at buffer coordinates
    float2 uv = bufferCoord / uniforms.bufferSize;
    return bufferTexture.sample(sampler, uv);
}
```

### Memory Considerations

**GPU Memory Usage:**
```
XRES standard buffer:  640 × 480 × 1 byte  = 307 KB
XRES 4x wide:         2560 × 480 × 1 byte  = 1.2 MB
XRES 4x4 screens:     2560 × 1920 × 1 byte = 4.9 MB
```

**Limits:**
- Mobile GPU: ~100-200 MB texture memory safe
- Desktop GPU: ~500 MB - 2 GB texture memory
- Recommendation: Keep individual buffers under 16 MB

### BASIC API

```basic
REM Create large buffer for wide level
XRES_CREATE_LARGE_BUFFER 5, 2560, 480

REM Draw entire level to large buffer
XRES_SET_ACTIVE_BUFFER 5
FOR x = 0 TO 2560 STEP 32
    XRES_RECT_FILL_GPU 5, x, 400, 32, 80, 10
NEXT x

REM Create scroll layer with viewport into large buffer
layer = SCROLL_LAYER_CREATE()
SCROLL_LAYER_SET_BUFFER layer, 5
SCROLL_LAYER_SET_SOURCE layer, 0, 0, 640, 480

REM Scroll by changing source region
cameraX = 0
WHILE running
    cameraX = cameraX + scrollSpeed
    SCROLL_LAYER_SET_SOURCE layer, cameraX, 0, 640, 480
    SCROLL_UPDATE DELTA_TIME
WEND
```

---

## Approach 2: Buffer Atlases

### Concept

Pack multiple screens or sprites into a single large texture, then render different regions:

```
┌─────────────────────────────────────────┐
│ 2048 × 2048 Atlas Buffer                │
│                                         │
│ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐      │
│ │Screen│Screen│Screen│Screen│      │
│ │  1  │  2  │  3  │  4  │      │
│ └─────┘ └─────┘ └─────┘ └─────┘      │
│                                         │
│ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐      │
│ │Screen│Screen│Screen│Screen│      │
│ │  5  │  6  │  7  │  8  │      │
│ └─────┘ └─────┘ └─────┘ └─────┘      │
│                                         │
│ ┌───┐┌───┐┌───┐┌───┐┌───┐┌───┐       │
│ │Spr││Spr││Spr││Spr││Spr││Spr│       │
│ └───┘└───┘└───┘└───┘└───┘└───┘       │
│ ... (more sprites) ...                 │
└─────────────────────────────────────────┘
```

### API

```c
// Create atlas buffer (power-of-2 sizes best for GPU)
st_video_create_atlas(8, 2048, 2048);

// Define atlas regions
typedef struct {
    int32_t x, y, width, height;
    const char* name;  // Optional identifier
} STAtlasRegion;

// Register screen regions
STAtlasRegion screen1 = {0, 0, 640, 480, "level1_screen1"};
STAtlasRegion screen2 = {640, 0, 640, 480, "level1_screen2"};
st_video_atlas_add_region(8, &screen1);
st_video_atlas_add_region(8, &screen2);

// Switch between screens by changing source region
st_scroll_layer_set_source_region(layer, "level1_screen2");
// Or by coordinates:
st_scroll_layer_set_source(layer, 640, 0, 640, 480);
```

### Use Case: Room-Based Games

Perfect for games like **Zelda** or **Metroid** with discrete rooms:

```basic
REM Create 2048×2048 atlas for 12 rooms (4×3 grid of 640×480 rooms)
XRES_CREATE_ATLAS 8, 2048, 2048

REM Draw all rooms to atlas
FOR room = 0 TO 11
    roomX = (room MOD 4) * 640
    roomY = INT(room / 4) * 480
    XRES_SET_ATLAS_REGION 8, roomX, roomY, 640, 480
    GOSUB 1000 + room * 100  REM Draw room content
NEXT room

REM Display one room at a time
currentRoom = 0
layer = SCROLL_LAYER_CREATE()
SCROLL_LAYER_SET_BUFFER layer, 8

SUB ShowRoom(roomNum)
    roomX = (roomNum MOD 4) * 640
    roomY = INT(roomNum / 4) * 480
    SCROLL_LAYER_SET_SOURCE layer, roomX, roomY, 640, 480
END SUB

REM Transition between rooms
SUB TransitionTo(newRoom)
    REM Could do smooth scrolling transition here
    FOR step = 0 TO 60
        t = step / 60.0
        oldX = (currentRoom MOD 4) * 640
        oldY = INT(currentRoom / 4) * 480
        newX = (newRoom MOD 4) * 640
        newY = INT(newRoom / 4) * 480
        interpX = oldX + (newX - oldX) * t
        interpY = oldY + (newY - oldY) * t
        SCROLL_LAYER_SET_SOURCE layer, interpX, interpY, 640, 480
        WAIT 0.016  REM 60 fps
    NEXT step
    currentRoom = newRoom
END SUB
```

---

## Approach 3: Tilemap Integration

### Concept

**Best approach for huge worlds**: Use the existing tilemap system for background/terrain, use video mode buffers for entities.

```
Layer Stack (bottom to top):
┌────────────────────────────────────┐
│ Tilemap Layer 0: Background tiles  │ ← Huge world (100×100 screens)
├────────────────────────────────────┤
│ Tilemap Layer 1: Main terrain      │ ← Scrolls with camera
├────────────────────────────────────┤
│ Video Buffer Layer: Entities       │ ← Dynamic content (enemies, particles)
├────────────────────────────────────┤
│ Tilemap Layer 2: Foreground        │ ← Parallax front layer
├────────────────────────────────────┤
│ Video Buffer Layer: UI/HUD         │ ← Static overlay
└────────────────────────────────────┘
```

### Hybrid Architecture

```c
// Initialize both systems
st_xres_mode();
st_tilemap_init(640, 480);
st_scroll_init();

// Create huge tilemap world (1000×1000 tiles)
STTilemapID worldMap = st_tilemap_create(1000, 1000, 32, 32);

// Create video buffer layer for entities
STScrollLayerConfig entityConfig = {0};
entityConfig.bufferID = 0;  // Standard XRES buffer
entityConfig.depth = 100;   // Above tilemap
STScrollLayerID entityLayer = st_scroll_layer_create(&entityConfig);

// Synchronize scrolling between systems
void update_camera(float dt) {
    // Update player position
    update_player(&playerX, &playerY);
    
    // Both systems follow same camera
    st_tilemap_set_camera(playerX, playerY);
    st_scroll_layer_set_offset(entityLayer, playerX - 320, playerY - 240);
    
    // Clear and redraw entities each frame
    st_xres_clear_gpu(0, 0);  // Transparent
    draw_enemies();
    draw_particles();
    draw_projectiles();
    
    st_tilemap_update(dt);
    st_scroll_update(dt);
}
```

### Benefits of Hybrid Approach

1. **Unlimited world size**: Tilemaps can be enormous (10,000+ screens)
2. **Memory efficient**: Tiles reused, only visible tiles in GPU memory
3. **Fast collision**: Tilemap has built-in collision detection
4. **Dynamic entities**: Video buffers for objects that move/animate
5. **Separate layers**: Different scroll speeds for parallax

---

## Comparison of Approaches

| Feature | Oversized Buffers | Buffer Atlases | Tilemap Hybrid |
|---------|-------------------|----------------|----------------|
| **Max World Size** | ~16 screens | ~64 rooms | Unlimited |
| **Memory Usage** | High | Medium | Low |
| **GPU Efficiency** | Excellent | Excellent | Good |
| **Best For** | Linear levels | Room-based | Open worlds |
| **Collision** | Manual | Manual | Built-in |
| **Dynamic Content** | Excellent | Good | Excellent |
| **Setup Complexity** | Low | Medium | Medium |

---

## Implementation Examples

### Example 1: Wide Platformer Level (Oversized Buffer)

```basic
REM 4-screen-wide platformer level
10 XRES_MODE
20 SCROLL_INIT

30 REM Create large buffer (2560×480 = 4 screens wide)
40 XRES_CREATE_LARGE_BUFFER 5, 2560, 480

50 REM Draw entire level
60 XRES_SET_ACTIVE_BUFFER 5
70 GOSUB 5000  REM Draw level geometry

80 REM Create viewport into large buffer
90 layer = SCROLL_LAYER_CREATE()
100 SCROLL_LAYER_SET_BUFFER layer, 5
110 SCROLL_LAYER_SET_SOURCE layer, 0, 0, 640, 480
120 SCROLL_CAMERA_SET_BOUNDS layer, 0, 0, 2560, 480

130 REM Game loop
140 WHILE running
150   REM Update player
160   playerX = playerX + playerSpeed * DELTA_TIME
170   
180   REM Camera follows player
190   SCROLL_CAMERA_FOLLOW layer, playerX, 240, 0.1
200   
210   REM Get camera position to determine visible region
220   camX = SCROLL_LAYER_GET_OFFSET_X(layer)
230   
240   REM Update source region based on camera
250   SCROLL_LAYER_SET_SOURCE layer, camX, 0, 640, 480
260   
270   SCROLL_UPDATE DELTA_TIME
280 WEND
```

### Example 2: 16-Room Dungeon (Atlas)

```basic
REM 4×4 grid of rooms in 2560×1920 atlas
10 XRES_MODE
20 SCROLL_INIT

30 REM Create atlas buffer
40 XRES_CREATE_ATLAS 8, 2560, 1920

50 REM Draw all 16 rooms to atlas
60 FOR roomY = 0 TO 3
70   FOR roomX = 0 TO 4
80     room = roomY * 4 + roomX
90     atlasX = roomX * 640
100    atlasY = roomY * 480
110    XRES_SET_ATLAS_REGION 8, atlasX, atlasY, 640, 480
120    GOSUB 1000 + room * 10  REM Draw room
130  NEXT roomX
140 NEXT roomY

150 REM Create layer showing one room
160 layer = SCROLL_LAYER_CREATE()
170 SCROLL_LAYER_SET_BUFFER layer, 8
180 currentRoom = 0

190 REM Function to switch rooms
200 DEF PROC ShowRoom(room)
210   roomX = (room MOD 4) * 640
220   roomY = INT(room / 4) * 480
230   SCROLL_LAYER_SET_SOURCE layer, roomX, roomY, 640, 480
240 ENDPROC

250 REM Door transitions
260 IF playerX < 0 AND currentRoom MOD 4 > 0 THEN
270   currentRoom = currentRoom - 1
280   playerX = 600
290   ShowRoom(currentRoom)
300 ENDIF
```

### Example 3: Huge RPG World (Tilemap)

```basic
REM Massive RPG with 100×100 screen world
10 XRES_MODE
20 TILEMAP_INIT 640, 480
30 SCROLL_INIT

40 REM Create huge tilemap (3200×2400 tiles = 100×75 screens)
50 worldMap = TILEMAP_CREATE(3200, 2400, 32, 32)
60 layer = TILEMAP_LAYER_CREATE(worldMap)

70 REM Load world data
80 TILEMAP_LOAD_FILE worldMap, "rpg_world.tmx"

90 REM Create entity buffer for characters
100 entityLayer = SCROLL_LAYER_CREATE()
110 SCROLL_LAYER_SET_BUFFER entityLayer, 0
120 SCROLL_LAYER_SET_DEPTH entityLayer, 100

130 REM Game loop
140 WHILE running
150   REM Update player
160   GOSUB 2000
170   
180   REM Both cameras follow player
190   TILEMAP_SET_CAMERA playerX, playerY
200   camX = playerX - 320
210   camY = playerY - 240
220   SCROLL_LAYER_SET_OFFSET entityLayer, camX, camY
230   
240   REM Clear and redraw dynamic entities
250   XRES_CLEAR_GPU 0, 0
260   GOSUB 3000  REM Draw NPCs
270   GOSUB 4000  REM Draw monsters
280   GOSUB 5000  REM Draw particles
290   
300   REM Update both systems
310   TILEMAP_UPDATE DELTA_TIME
320   SCROLL_UPDATE DELTA_TIME
330 WEND
```

---

## Performance Optimization

### Culling Off-Screen Content

```c
// Only draw entities visible in current viewport
void draw_entities() {
    float camX, camY;
    st_scroll_layer_get_offset(layer, &camX, &camY);
    
    float viewLeft = camX;
    float viewRight = camX + 640;
    float viewTop = camY;
    float viewBottom = camY + 480;
    
    for (int i = 0; i < numEntities; i++) {
        Entity* e = &entities[i];
        
        // Cull if outside viewport (with margin)
        if (e->x < viewLeft - 100) continue;
        if (e->x > viewRight + 100) continue;
        if (e->y < viewTop - 100) continue;
        if (e->y > viewBottom + 100) continue;
        
        // Draw entity
        draw_entity(e);
    }
}
```

### Lazy Loading for Huge Buffers

```c
// Stream in level chunks as camera moves
void update_level_streaming() {
    int currentChunk = (int)(cameraX / CHUNK_WIDTH);
    
    // Load next chunk if approaching edge
    if (!isChunkLoaded(currentChunk + 1)) {
        loadChunkToBuffer(currentChunk + 1, largeBuffer);
    }
    
    // Unload far chunks to free memory
    if (isChunkLoaded(currentChunk - 2)) {
        unloadChunk(currentChunk - 2);
    }
}
```

---

## Recommended Patterns

### Small Game (< 4 screens): **Oversized Buffer**
```
Single 2560×480 buffer, simple camera follow
```

### Medium Game (4-20 rooms): **Buffer Atlas**
```
One 2048×2048 atlas, room-based transitions
```

### Large Game (20+ screens): **Tilemap + Entities**
```
Tilemap for terrain, video buffers for dynamic content
```

### Huge Game (100+ screens): **Tilemap + Streaming**
```
Tilemap for world, stream entities in/out based on camera
```

---

## Summary

**Key Insights:**

1. **Oversized buffers** are perfect for linear levels up to ~16 screens
2. **Buffer atlases** excel at discrete room-based games
3. **Tilemap hybrid** is best for massive open worlds
4. All three approaches work with the same scroll layer API
5. GPU memory is the main constraint (watch texture sizes!)

**Choose based on:**
- World size needed
- Memory budget
- Game structure (linear vs rooms vs open world)
- Need for tile-based collision

The scroll layer API supports all three approaches seamlessly!