//
// st_api_collision.h
// SuperTerminal API - Collision Detection
//
// Provides optimized collision detection functions for game development:
// - Circle vs Circle collision
// - Circle vs Rectangle collision (with special bottom-edge detection)
// - Rectangle vs Rectangle collision (AABB)
// - Point containment tests
//
// All functions use optimized algorithms (avoiding sqrt when possible)
//

#ifndef ST_API_COLLISION_H
#define ST_API_COLLISION_H

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Circle-Circle Collision Detection
// =============================================================================

// Returns 1 if two circles intersect, 0 otherwise
// Parameters:
//   x1, y1, r1: Center and radius of first circle
//   x2, y2, r2: Center and radius of second circle
int st_collision_circle_circle(float x1, float y1, float r1,
                                float x2, float y2, float r2);

// =============================================================================
// Circle-Rectangle Collision Detection
// =============================================================================

// Returns 1 if a circle intersects with a rectangle, 0 otherwise
// Uses closest point on rectangle algorithm
// Parameters:
//   cx, cy, radius: Circle center and radius
//   rx, ry, rw, rh: Rectangle position (top-left corner) and dimensions
int st_collision_circle_rect(float cx, float cy, float radius,
                              float rx, float ry, float rw, float rh);

// Special version that checks if bottom of circle hits top of rectangle
// Useful for platform/paddle collision where you want specific edge detection
// Returns 1 if:
//   1. Bottom of circle (cy + radius) has reached or passed top of rectangle (ry)
//   2. Circle center is horizontally within rectangle bounds
// This is the correct way to detect ball-paddle collisions!
int st_collision_circle_rect_bottom(float cx, float cy, float radius,
                                     float rx, float ry, float rw, float rh);

// Swept collision detection - checks if a moving circle intersects with a rectangle
// along its movement path between frames. This prevents tunneling at high speeds.
// Parameters:
//   cx, cy, radius: Circle center and radius at START position
//   vx, vy: Circle velocity (movement per frame)
//   rx, ry, rw, rh: Rectangle position (top-left corner) and dimensions
// Returns 1 if the circle's path intersects the rectangle, 0 otherwise
// This is essential for fast-moving objects like balls hitting paddles!
int st_collision_swept_circle_rect(float cx, float cy, float radius,
                                    float vx, float vy,
                                    float rx, float ry, float rw, float rh);

// =============================================================================
// Rectangle-Rectangle Collision Detection (AABB)
// =============================================================================

// Returns 1 if two rectangles intersect, 0 otherwise
// Uses Axis-Aligned Bounding Box (AABB) collision detection
// Parameters:
//   x1, y1, w1, h1: First rectangle position (top-left corner) and dimensions
//   x2, y2, w2, h2: Second rectangle position (top-left corner) and dimensions
int st_collision_rect_rect(float x1, float y1, float w1, float h1,
                            float x2, float y2, float w2, float h2);

// =============================================================================
// Point Containment Tests
// =============================================================================

// Returns 1 if a point is inside a circle, 0 otherwise
int st_collision_point_in_circle(float px, float py, 
                                  float cx, float cy, float radius);

// Returns 1 if a point is inside a rectangle, 0 otherwise
int st_collision_point_in_rect(float px, float py,
                                float rx, float ry, float rw, float rh);

// =============================================================================
// Advanced Collision Detection with Physics Info
// =============================================================================

// Collision info structure for physics response
typedef struct {
    int colliding;           // 1 if collision detected, 0 otherwise
    float penetrationDepth;  // How far objects overlap
    float normalX;           // Collision normal X component
    float normalY;           // Collision normal Y component
} st_collision_info;

// Returns detailed collision info for circle-rectangle collision
// Includes penetration depth and collision normal for physics response
// Result is written to the info parameter
void st_collision_circle_rect_info(float cx, float cy, float radius,
                                    float rx, float ry, float rw, float rh,
                                    st_collision_info* info);

// Get the penetration depth for circle-circle collision
// Returns 0.0 if no collision, positive value = overlap amount
float st_collision_circle_circle_penetration(float x1, float y1, float r1,
                                              float x2, float y2, float r2);

// Get the overlap amounts for rectangle-rectangle collision
// Results are written to overlapX and overlapY parameters
// Returns 0.0 for both if no collision
void st_collision_rect_rect_overlap(float x1, float y1, float w1, float h1,
                                     float x2, float y2, float w2, float h2,
                                     float* overlapX, float* overlapY);

#ifdef __cplusplus
}
#endif

#endif // ST_API_COLLISION_H