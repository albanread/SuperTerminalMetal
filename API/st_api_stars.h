//
// st_api_stars.h
// SuperTerminal v2 - Star Rendering API Header
//
// High-performance GPU-accelerated star rendering with gradients, rotation, and outlines
//

#ifndef SUPERTERMINAL_API_STARS_H
#define SUPERTERMINAL_API_STARS_H

#include "superterminal_api.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// ID-Based Star Management API
// =============================================================================

/// Create a solid color star with default inner radius
/// @param x X position (center)
/// @param y Y position (center)
/// @param outerRadius Star outer radius (distance from center to points)
/// @param numPoints Number of star points (3-12)
/// @param color RGBA color (0xRRGGBBAA)
/// @return Star ID (>= 0) or -1 on error
int st_star_create(float x, float y, float outerRadius, int numPoints, uint32_t color);

/// Create a star with custom inner radius
/// @param x X position (center)
/// @param y Y position (center)
/// @param outerRadius Star outer radius
/// @param innerRadius Star inner radius (ratio 0.0-1.0, where 0.382 is classic 5-point star)
/// @param numPoints Number of star points (3-12)
/// @param color RGBA color
/// @return Star ID or -1 on error
int st_star_create_custom(float x, float y, float outerRadius, float innerRadius,
                          int numPoints, uint32_t color);

/// Create a star with gradient
/// @param x X position (center)
/// @param y Y position (center)
/// @param outerRadius Star outer radius
/// @param numPoints Number of star points (3-12)
/// @param color1 First gradient color (center or alternating)
/// @param color2 Second gradient color (points or alternating)
/// @param mode Gradient mode (radial, alternating)
/// @return Star ID or -1 on error
int st_star_create_gradient(float x, float y, float outerRadius, int numPoints,
                            uint32_t color1, uint32_t color2,
                            STStarGradientMode mode);

/// Create a star with outline
/// @param x X position (center)
/// @param y Y position (center)
/// @param outerRadius Star outer radius
/// @param numPoints Number of star points (3-12)
/// @param fillColor Fill color
/// @param outlineColor Outline color
/// @param lineWidth Outline width in pixels
/// @return Star ID or -1 on error
int st_star_create_outline(float x, float y, float outerRadius, int numPoints,
                           uint32_t fillColor, uint32_t outlineColor,
                           float lineWidth);

// =============================================================================
// Star Update API
// =============================================================================

/// Update star position
/// @param id Star ID
/// @param x New X position
/// @param y New Y position
/// @return true on success, false if star doesn't exist
bool st_star_set_position(int id, float x, float y);

/// Update star outer radius
/// @param id Star ID
/// @param outerRadius New outer radius
/// @return true on success, false if star doesn't exist
bool st_star_set_radius(int id, float outerRadius);

/// Update star radii (outer and inner)
/// @param id Star ID
/// @param outerRadius New outer radius
/// @param innerRadius New inner radius (ratio 0.0-1.0)
/// @return true on success, false if star doesn't exist
bool st_star_set_radii(int id, float outerRadius, float innerRadius);

/// Update number of star points
/// @param id Star ID
/// @param numPoints New number of points (3-12)
/// @return true on success, false if star doesn't exist
bool st_star_set_points(int id, int numPoints);

/// Update star color (for solid stars)
/// @param id Star ID
/// @param color New RGBA color
/// @return true on success, false if star doesn't exist
bool st_star_set_color(int id, uint32_t color);

/// Update star gradient colors
/// @param id Star ID
/// @param color1 First gradient color
/// @param color2 Second gradient color
/// @return true on success, false if star doesn't exist
bool st_star_set_colors(int id, uint32_t color1, uint32_t color2);

/// Update star rotation
/// @param id Star ID
/// @param angleDegrees Rotation angle in degrees (0-360)
/// @return true on success, false if star doesn't exist
bool st_star_set_rotation(int id, float angleDegrees);

/// Set star visibility
/// @param id Star ID
/// @param visible true to show, false to hide
/// @return true on success, false if star doesn't exist
bool st_star_set_visible(int id, bool visible);

// =============================================================================
// Star Query/Management API
// =============================================================================

/// Check if a star exists
/// @param id Star ID
/// @return true if star exists
bool st_star_exists(int id);

/// Check if a star is visible
/// @param id Star ID
/// @return true if star is visible
bool st_star_is_visible(int id);

/// Delete a specific star
/// @param id Star ID
/// @return true on success, false if star doesn't exist
bool st_star_delete(int id);

/// Delete all stars
void st_star_delete_all(void);

/// Get the number of active stars
/// @return Number of stars
size_t st_star_count(void);

/// Check if there are no stars
/// @return true if no stars exist
bool st_star_is_empty(void);

#ifdef __cplusplus
}
#endif

#endif // SUPERTERMINAL_API_STARS_H