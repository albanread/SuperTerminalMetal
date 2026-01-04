# Collision Detection API

## Overview

The Collision Detection API provides optimized, game-ready collision detection functions for SuperTerminal/FasterBASIC. All functions are implemented in C for maximum performance and exposed to Lua for easy use in games.

**Key Features:**
- **Optimized algorithms** - Avoids expensive operations like `sqrt()` when possible
- **Multiple collision types** - Circle-circle, circle-rectangle, rectangle-rectangle
- **Special edge detection** - Bottom-edge collision for platform/paddle games
- **Physics-ready** - Optional penetration depth and collision normals
- **Zero-copy** - Direct C implementation, no overhead

## Function Reference

### Circle-Circle Collision

```lua
collision_circle_circle(x1, y1, r1, x2, y2, r2) -> boolean
```

Tests if two circles overlap.

**Parameters:**
- `x1, y1` - Center position of first circle
- `r1` - Radius of first circle  
- `x2, y2` - Center position of second circle
- `r2` - Radius of second circle

**Returns:** `true` if circles overlap, `false` otherwise

**Example:**
```lua
-- Check if two balls collide
local ball1_x, ball1_y, ball1_r = 100, 100, 20
local ball2_x, ball2_y, ball2_r = 130, 110, 25

if collision_circle_circle(ball1_x, ball1_y, ball1_r, 
                           ball2_x, ball2_y, ball2_r) then
    print("Balls collided!")
end
```

---

### Circle-Rectangle Collision

```lua
collision_circle_rect(cx, cy, radius, rx, ry, rw, rh) -> boolean
```

Tests if a circle overlaps with a rectangle using the closest-point algorithm.

**Parameters:**
- `cx, cy` - Circle center position
- `radius` - Circle radius
- `rx, ry` - Rectangle top-left corner position
- `rw, rh` - Rectangle width and height

**Returns:** `true` if circle overlaps rectangle, `false` otherwise

**Example:**
```lua
-- Check if ball hits a brick
local ball_x, ball_y, ball_r = 400, 300, 15
local brick_x, brick_y, brick_w, brick_h = 380, 200, 60, 20

if collision_circle_rect(ball_x, ball_y, ball_r,
                         brick_x, brick_y, brick_w, brick_h) then
    print("Ball hit brick!")
end
```

---

### Circle-Rectangle Bottom Edge Collision

```lua
collision_circle_rect_bottom(cx, cy, radius, rx, ry, rw, rh) -> boolean
```

**Special collision detection for platform/paddle games.** Tests if the **bottom edge** of a circle has hit the **top edge** of a rectangle. This is the **correct way** to detect ball-paddle collisions.

**Parameters:**
- `cx, cy` - Circle center position
- `radius` - Circle radius
- `rx, ry` - Rectangle top-left corner position
- `rw, rh` - Rectangle width and height

**Returns:** `true` if bottom of circle touches top of rectangle AND circle center is horizontally within rectangle bounds

**Why use this instead of `collision_circle_rect`?**
- Prevents unrealistic side/bottom collisions on paddles
- Only triggers when ball descends onto paddle from above
- Ball center must be horizontally aligned with paddle
- Perfect for Pong, Breakout, Arkanoid-style games

**Example:**
```lua
-- Proper ball-paddle collision in Breakout
local ball_x, ball_y, ball_radius = 400, 890, 20
local paddle_x, paddle_y, paddle_w, paddle_h = 350, 900, 100, 20

if collision_circle_rect_bottom(ball_x, ball_y, ball_radius,
                                 paddle_x, paddle_y, paddle_w, paddle_h) then
    -- Ball hit paddle from above - bounce it!
    ball_vy = -math.abs(ball_vy)
    ball_y = paddle_y - ball_radius  -- Prevent tunneling
end
```

**Collision Detection:**
```
     ╭─────╮        ball.y (center)
     │  O  │
     ╰─────╯        ball.y + radius (BOTTOM EDGE) ← Checked!
       ↓
       ↓
───────────────     paddle.y (TOP EDGE) ← Compared here!
│   PADDLE   │
───────────────

Collision occurs when:
1. ball.y + radius >= paddle.y (bottom touches top)
2. ball.x >= paddle.x AND ball.x <= paddle.x + paddle.w (horizontally aligned)
```

---

### Rectangle-Rectangle Collision (AABB)

```lua
collision_rect_rect(x1, y1, w1, h1, x2, y2, w2, h2) -> boolean
```

Tests if two axis-aligned rectangles overlap using AABB (Axis-Aligned Bounding Box) collision detection.

**Parameters:**
- `x1, y1` - First rectangle top-left position
- `w1, h1` - First rectangle width and height
- `x2, y2` - Second rectangle top-left position
- `w2, h2` - Second rectangle width and height

**Returns:** `true` if rectangles overlap, `false` otherwise

**Example:**
```lua
-- Check if player collides with wall
local player_x, player_y, player_w, player_h = 100, 100, 32, 48
local wall_x, wall_y, wall_w, wall_h = 130, 90, 64, 128

if collision_rect_rect(player_x, player_y, player_w, player_h,
                       wall_x, wall_y, wall_w, wall_h) then
    print("Hit wall!")
end
```

---

### Point-in-Circle Test

```lua
collision_point_in_circle(px, py, cx, cy, radius) -> boolean
```

Tests if a point is inside a circle.

**Parameters:**
- `px, py` - Point coordinates
- `cx, cy` - Circle center
- `radius` - Circle radius

**Returns:** `true` if point is inside circle, `false` otherwise

**Example:**
```lua
-- Check if mouse click hit a circular button
local mouse_x, mouse_y = 450, 300
local button_x, button_y, button_r = 400, 280, 50

if collision_point_in_circle(mouse_x, mouse_y,
                             button_x, button_y, button_r) then
    print("Button clicked!")
end
```

---

### Point-in-Rectangle Test

```lua
collision_point_in_rect(px, py, rx, ry, rw, rh) -> boolean
```

Tests if a point is inside a rectangle.

**Parameters:**
- `px, py` - Point coordinates
- `rx, ry` - Rectangle top-left position
- `rw, rh` - Rectangle width and height

**Returns:** `true` if point is inside rectangle, `false` otherwise

**Example:**
```lua
-- Check if cursor is over a button
local cursor_x, cursor_y = 520, 420
local button_x, button_y, button_w, button_h = 500, 400, 100, 50

if collision_point_in_rect(cursor_x, cursor_y,
                           button_x, button_y, button_w, button_h) then
    print("Hovering over button")
end
```

---

## Advanced Functions (Physics Response)

### Circle-Rectangle Collision Info

```lua
collision_circle_rect_info(cx, cy, radius, rx, ry, rw, rh) -> table
```

Returns detailed collision information including penetration depth and collision normal. Useful for realistic physics response.

**Parameters:** Same as `collision_circle_rect`

**Returns:** Table with fields:
- `colliding` (boolean) - Whether collision occurred
- `penetrationDepth` (number) - How far circle penetrated rectangle
- `normalX` (number) - X component of collision normal (-1 to 1)
- `normalY` (number) - Y component of collision normal (-1 to 1)

**Example:**
```lua
local info = collision_circle_rect_info(ball_x, ball_y, ball_r,
                                        box_x, box_y, box_w, box_h)

if info.colliding then
    -- Bounce based on collision normal
    if math.abs(info.normalX) > math.abs(info.normalY) then
        ball_vx = -ball_vx  -- Horizontal bounce
    else
        ball_vy = -ball_vy  -- Vertical bounce
    end
    
    -- Push ball out of collision
    ball_x = ball_x + info.normalX * info.penetrationDepth
    ball_y = ball_y + info.normalY * info.penetrationDepth
end
```

---

### Circle-Circle Penetration Depth

```lua
collision_circle_circle_penetration(x1, y1, r1, x2, y2, r2) -> number
```

Returns how much two circles overlap. Useful for separating colliding objects.

**Parameters:** Same as `collision_circle_circle`

**Returns:** Penetration depth (0.0 if no collision, positive value if colliding)

**Example:**
```lua
local depth = collision_circle_circle_penetration(ball1_x, ball1_y, ball1_r,
                                                   ball2_x, ball2_y, ball2_r)

if depth > 0 then
    -- Calculate separation direction
    local dx = ball1_x - ball2_x
    local dy = ball1_y - ball2_y
    local dist = math.sqrt(dx*dx + dy*dy)
    
    if dist > 0 then
        local nx, ny = dx/dist, dy/dist
        
        -- Push balls apart
        ball1_x = ball1_x + nx * depth * 0.5
        ball1_y = ball1_y + ny * depth * 0.5
        ball2_x = ball2_x - nx * depth * 0.5
        ball2_y = ball2_y - ny * depth * 0.5
    end
end
```

---

### Rectangle-Rectangle Overlap

```lua
collision_rect_rect_overlap(x1, y1, w1, h1, x2, y2, w2, h2) -> overlapX, overlapY
```

Returns how much two rectangles overlap on each axis. Useful for determining separation direction.

**Parameters:** Same as `collision_rect_rect`

**Returns:** Two numbers:
- `overlapX` - Overlap amount on X axis (0.0 if no collision)
- `overlapY` - Overlap amount on Y axis (0.0 if no collision)

**Example:**
```lua
local ox, oy = collision_rect_rect_overlap(player_x, player_y, player_w, player_h,
                                           wall_x, wall_y, wall_w, wall_h)

if ox > 0 and oy > 0 then
    -- Separate on axis with smallest overlap
    if ox < oy then
        -- Push horizontally
        if player_x < wall_x then
            player_x = player_x - ox
        else
            player_x = player_x + ox
        end
    else
        -- Push vertically
        if player_y < wall_y then
            player_y = player_y - oy
        else
            player_y = player_y + oy
        end
    end
end
```

---

## Performance Notes

All collision detection functions are implemented in **optimized C code** and are extremely fast:

1. **Circle-circle collision** - Uses squared distances, avoids `sqrt()`
2. **Circle-rectangle collision** - Closest-point algorithm, minimal operations
3. **Rectangle-rectangle collision** - Simple axis separation test, very fast
4. **Zero overhead** - Direct C implementation, no Lua table creation (except `_info` functions)

**Typical performance:** 
- Millions of collision checks per second
- Suitable for games with hundreds of objects
- No garbage collection pressure (except `_info` functions)

---

## Common Patterns

### Ball-Paddle Collision (Breakout/Pong)

```lua
-- CORRECT: Use collision_circle_rect_bottom for paddle hits
if collision_circle_rect_bottom(ball.x, ball.y, ball.radius,
                                 paddle.x, paddle.y, paddle.width, paddle.height) then
    ball.vy = -math.abs(ball.vy)  -- Bounce up
    ball.y = paddle.y - ball.radius  -- Prevent sticking
    
    -- Optional: Add spin based on hit position
    local hit_offset = (ball.x - (paddle.x + paddle.width/2)) / (paddle.width/2)
    ball.vx = ball.vx + hit_offset * 2
end
```

### Brick Breaking

```lua
-- Use general circle-rect collision for bricks
for i, brick in ipairs(bricks) do
    if brick.active and collision_circle_rect(ball.x, ball.y, ball.radius,
                                               brick.x, brick.y, brick.w, brick.h) then
        brick.active = false
        
        -- Bounce ball
        local info = collision_circle_rect_info(ball.x, ball.y, ball.radius,
                                                 brick.x, brick.y, brick.w, brick.h)
        if math.abs(info.normalX) > math.abs(info.normalY) then
            ball.vx = -ball.vx
        else
            ball.vy = -ball.vy
        end
    end
end
```

### Circular Enemies

```lua
-- Check collisions between all enemies
for i = 1, #enemies do
    for j = i+1, #enemies do
        if collision_circle_circle(enemies[i].x, enemies[i].y, enemies[i].r,
                                   enemies[j].x, enemies[j].y, enemies[j].r) then
            -- Separate overlapping enemies
            local depth = collision_circle_circle_penetration(
                enemies[i].x, enemies[i].y, enemies[i].r,
                enemies[j].x, enemies[j].y, enemies[j].r)
            
            if depth > 0 then
                local dx = enemies[i].x - enemies[j].x
                local dy = enemies[i].y - enemies[j].y
                local dist = math.sqrt(dx*dx + dy*dy)
                if dist > 0 then
                    local nx, ny = dx/dist, dy/dist
                    enemies[i].x = enemies[i].x + nx * depth * 0.5
                    enemies[i].y = enemies[i].y + ny * depth * 0.5
                    enemies[j].x = enemies[j].x - nx * depth * 0.5
                    enemies[j].y = enemies[j].y - ny * depth * 0.5
                end
            end
        end
    end
end
```

---

## Implementation Details

The collision detection system is implemented in:
- **C API:** `Framework/API/st_api_collision.h` and `st_api_collision.cpp`
- **Lua Bindings:** `LuaRunner2/LuaBindings.cpp`

All algorithms use **industry-standard** collision detection techniques optimized for real-time games.

---

## See Also

- [Circle API](st_api_circles.h) - For drawing circles
- [Rectangle API](st_api_rectangles.h) - For drawing rectangles
- [Examples/Lua/breakout_game.lua](../../Examples/Lua/breakout_game.lua) - Complete game using collision detection
- [Examples/Lua/test_collision_detection.lua](../../Examples/Lua/test_collision_detection.lua) - API test suite