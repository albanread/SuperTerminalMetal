//
// st_api_collision.cpp
// SuperTerminal API - Collision Detection Implementation
//
// Optimized collision detection algorithms for game development.
// All functions avoid expensive operations like sqrt when possible.
//

#include "st_api_collision.h"
#include <cmath>
#include <algorithm>

// =============================================================================
// Circle-Circle Collision Detection
// =============================================================================

int st_collision_circle_circle(float x1, float y1, float r1,
                                float x2, float y2, float r2) {
    // Calculate distance between centers (squared)
    float dx = x2 - x1;
    float dy = y2 - y1;
    float distanceSquared = dx * dx + dy * dy;
    
    // Calculate sum of radii (squared to avoid sqrt)
    float radiusSum = r1 + r2;
    float radiusSumSquared = radiusSum * radiusSum;
    
    // Circles collide if distance is less than sum of radii
    return distanceSquared <= radiusSumSquared ? 1 : 0;
}

// =============================================================================
// Circle-Rectangle Collision Detection
// =============================================================================

int st_collision_circle_rect(float cx, float cy, float radius,
                              float rx, float ry, float rw, float rh) {
    // Find the closest point on the rectangle to the circle center
    // Clamp circle center to rectangle bounds
    float closestX = std::max(rx, std::min(cx, rx + rw));
    float closestY = std::max(ry, std::min(cy, ry + rh));
    
    // Calculate distance from circle center to closest point
    float dx = cx - closestX;
    float dy = cy - closestY;
    float distanceSquared = dx * dx + dy * dy;
    
    // Circle collides if distance to closest point is less than radius
    // Using squared values avoids expensive sqrt operation
    return distanceSquared <= (radius * radius) ? 1 : 0;
}

int st_collision_circle_rect_bottom(float cx, float cy, float radius,
                                     float rx, float ry, float rw, float rh) {
    // Check if the bottom of the circle (cy + radius) has reached or passed
    // the top of rectangle (ry) AND the circle center is horizontally
    // within the rectangle bounds
    
    float circleBottom = cy + radius;
    float rectTop = ry;
    float rectLeft = rx;
    float rectRight = rx + rw;
    
    // Vertical check: bottom of circle at or below top of rectangle
    if (circleBottom < rectTop) {
        return 0;
    }
    
    // Also check we haven't passed through the rectangle
    float circleTop = cy - radius;
    float rectBottom = ry + rh;
    if (circleTop > rectBottom) {
        return 0;
    }
    
    // Horizontal check: circle center is within rectangle horizontal bounds
    if (cx < rectLeft || cx > rectRight) {
        return 0;
    }
    
    return 1;
}

// =============================================================================
// Rectangle-Rectangle Collision Detection (AABB)
// =============================================================================

int st_collision_rect_rect(float x1, float y1, float w1, float h1,
                            float x2, float y2, float w2, float h2) {
    // Axis-Aligned Bounding Box (AABB) collision detection
    // No collision if one rectangle is completely to the left, right,
    // above, or below the other
    
    // Check if rectangles are separated on X axis
    if (x1 + w1 < x2 || x2 + w2 < x1) {
        return 0;
    }
    
    // Check if rectangles are separated on Y axis
    if (y1 + h1 < y2 || y2 + h2 < y1) {
        return 0;
    }
    
    // If not separated on either axis, they must be colliding
    return 1;
}

// =============================================================================
// Point Containment Tests
// =============================================================================

int st_collision_point_in_circle(float px, float py, 
                                  float cx, float cy, float radius) {
    float dx = px - cx;
    float dy = py - cy;
    float distanceSquared = dx * dx + dy * dy;
    return distanceSquared <= (radius * radius) ? 1 : 0;
}

int st_collision_point_in_rect(float px, float py,
                                float rx, float ry, float rw, float rh) {
    return (px >= rx && px <= rx + rw && py >= ry && py <= ry + rh) ? 1 : 0;
}

// =============================================================================
// Advanced Collision Detection with Physics Info
// =============================================================================

void st_collision_circle_rect_info(float cx, float cy, float radius,
                                    float rx, float ry, float rw, float rh,
                                    st_collision_info* info) {
    info->colliding = 0;
    info->penetrationDepth = 0.0f;
    info->normalX = 0.0f;
    info->normalY = 0.0f;
    
    // Find the closest point on the rectangle to the circle center
    float closestX = std::max(rx, std::min(cx, rx + rw));
    float closestY = std::max(ry, std::min(cy, ry + rh));
    
    // Calculate distance from circle center to closest point
    float dx = cx - closestX;
    float dy = cy - closestY;
    float distanceSquared = dx * dx + dy * dy;
    float radiusSquared = radius * radius;
    
    if (distanceSquared > radiusSquared) {
        return; // No collision
    }
    
    info->colliding = 1;
    
    // If circle center is inside rectangle
    if (cx >= rx && cx <= rx + rw && cy >= ry && cy <= ry + rh) {
        // Find closest edge
        float leftDist = cx - rx;
        float rightDist = (rx + rw) - cx;
        float topDist = cy - ry;
        float bottomDist = (ry + rh) - cy;
        
        float minDist = std::min({leftDist, rightDist, topDist, bottomDist});
        
        if (minDist == leftDist) {
            info->normalX = -1.0f;
            info->normalY = 0.0f;
            info->penetrationDepth = radius + leftDist;
        } else if (minDist == rightDist) {
            info->normalX = 1.0f;
            info->normalY = 0.0f;
            info->penetrationDepth = radius + rightDist;
        } else if (minDist == topDist) {
            info->normalX = 0.0f;
            info->normalY = -1.0f;
            info->penetrationDepth = radius + topDist;
        } else {
            info->normalX = 0.0f;
            info->normalY = 1.0f;
            info->penetrationDepth = radius + bottomDist;
        }
    } else {
        // Circle center is outside rectangle
        float distance = std::sqrt(distanceSquared);
        
        if (distance > 0.0f) {
            info->normalX = dx / distance;
            info->normalY = dy / distance;
            info->penetrationDepth = radius - distance;
        }
    }
}

float st_collision_circle_circle_penetration(float x1, float y1, float r1,
                                              float x2, float y2, float r2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float distance = std::sqrt(dx * dx + dy * dy);
    float radiusSum = r1 + r2;
    
    if (distance >= radiusSum) {
        return 0.0f; // No collision
    }
    
    return radiusSum - distance;
}

void st_collision_rect_rect_overlap(float x1, float y1, float w1, float h1,
                                     float x2, float y2, float w2, float h2,
                                     float* overlapX, float* overlapY) {
    if (!st_collision_rect_rect(x1, y1, w1, h1, x2, y2, w2, h2)) {
        *overlapX = 0.0f;
        *overlapY = 0.0f;
        return;
    }
    
    // Calculate overlap on each axis
    float right1 = x1 + w1;
    float right2 = x2 + w2;
    float bottom1 = y1 + h1;
    float bottom2 = y2 + h2;
    
    // X axis overlap
    float leftOverlap = right1 - x2;
    float rightOverlap = right2 - x1;
    *overlapX = std::min(leftOverlap, rightOverlap);
    
    // Y axis overlap
    float topOverlap = bottom1 - y2;
    float bottomOverlap = bottom2 - y1;
    *overlapY = std::min(topOverlap, bottomOverlap);
}

// =============================================================================
// Swept Collision Detection (Continuous Collision)
// =============================================================================

int st_collision_swept_circle_rect(float cx, float cy, float radius,
                                    float vx, float vy,
                                    float rx, float ry, float rw, float rh) {
    // This function checks if a moving circle intersects with a rectangle
    // along its movement path. This prevents tunneling at high velocities.
    
    // First, check if already colliding at start position
    if (st_collision_circle_rect(cx, cy, radius, rx, ry, rw, rh)) {
        return 1;
    }
    
    // Check if colliding at end position
    float endX = cx + vx;
    float endY = cy + vy;
    if (st_collision_circle_rect(endX, endY, radius, rx, ry, rw, rh)) {
        return 1;
    }
    
    // Now we need to check if the path between start and end crosses the rectangle
    // We'll use a line-sweep approach specifically optimized for paddles (horizontal rects)
    
    // For paddle collision, we care most about the bottom of the circle hitting the top of the rect
    // This is the common case in breakout-style games
    
    if (vy != 0.0f) {
        // Calculate when the bottom of the circle crosses the top of the rectangle
        float circleBottom = cy + radius;
        float rectTop = ry;
        
        // If moving downward and we'll cross the top edge
        if (vy > 0.0f && circleBottom <= rectTop) {
            // Calculate the time (0 to 1) when bottom of circle reaches top of rect
            float tVertical = (rectTop - circleBottom) / vy;
            
            if (tVertical >= 0.0f && tVertical <= 1.0f) {
                // Calculate the X position of the circle center at this time
                float xAtCollision = cx + vx * tVertical;
                
                // Check if this X position is within the rectangle bounds
                float rectLeft = rx;
                float rectRight = rx + rw;
                
                if (xAtCollision >= rectLeft && xAtCollision <= rectRight) {
                    return 1; // Path crosses the paddle!
                }
            }
        }
        
        // Also check if moving upward and crossing the bottom edge
        if (vy < 0.0f) {
            float circleTop = cy - radius;
            float rectBottom = ry + rh;
            
            if (circleTop >= rectBottom) {
                float tVertical = (rectBottom - circleTop) / vy;
                
                if (tVertical >= 0.0f && tVertical <= 1.0f) {
                    float xAtCollision = cx + vx * tVertical;
                    float rectLeft = rx;
                    float rectRight = rx + rw;
                    
                    if (xAtCollision >= rectLeft && xAtCollision <= rectRight) {
                        return 1;
                    }
                }
            }
        }
    }
    
    // Check horizontal edge crossings (left and right sides of rectangle)
    if (vx != 0.0f) {
        float rectLeft = rx;
        float rectRight = rx + rw;
        float rectTop = ry;
        float rectBottom = ry + rh;
        
        // Check left edge
        if (vx > 0.0f) {
            float circleRight = cx + radius;
            if (circleRight <= rectLeft) {
                float tHorizontal = (rectLeft - circleRight) / vx;
                
                if (tHorizontal >= 0.0f && tHorizontal <= 1.0f) {
                    float yAtCollision = cy + vy * tHorizontal;
                    
                    if (yAtCollision >= rectTop && yAtCollision <= rectBottom) {
                        return 1;
                    }
                }
            }
        }
        
        // Check right edge
        if (vx < 0.0f) {
            float circleLeft = cx - radius;
            if (circleLeft >= rectRight) {
                float tHorizontal = (rectRight - circleLeft) / vx;
                
                if (tHorizontal >= 0.0f && tHorizontal <= 1.0f) {
                    float yAtCollision = cy + vy * tHorizontal;
                    
                    if (yAtCollision >= rectTop && yAtCollision <= rectBottom) {
                        return 1;
                    }
                }
            }
        }
    }
    
    // Check corner collisions (circle path intersects rectangle corners)
    // We'll check if the line segment from (cx,cy) to (endX,endY) comes within
    // radius distance of any corner
    
    float corners[4][2] = {
        {rx, ry},           // Top-left
        {rx + rw, ry},      // Top-right
        {rx, ry + rh},      // Bottom-left
        {rx + rw, ry + rh}  // Bottom-right
    };
    
    for (int i = 0; i < 4; i++) {
        float cornerX = corners[i][0];
        float cornerY = corners[i][1];
        
        // Calculate closest point on line segment to corner
        float dx = vx;
        float dy = vy;
        float fx = cx - cornerX;
        float fy = cy - cornerY;
        
        float a = dx * dx + dy * dy;
        float b = 2.0f * (fx * dx + fy * dy);
        float c = fx * fx + fy * fy - radius * radius;
        
        if (a == 0.0f) continue; // Not moving
        
        float discriminant = b * b - 4.0f * a * c;
        
        if (discriminant >= 0.0f) {
            float sqrtDisc = std::sqrt(discriminant);
            float t1 = (-b - sqrtDisc) / (2.0f * a);
            float t2 = (-b + sqrtDisc) / (2.0f * a);
            
            // Check if either intersection point is within [0,1]
            if ((t1 >= 0.0f && t1 <= 1.0f) || (t2 >= 0.0f && t2 <= 1.0f)) {
                return 1;
            }
        }
    }
    
    return 0; // No collision along the path
}