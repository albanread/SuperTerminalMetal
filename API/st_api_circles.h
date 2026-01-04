//
// st_api_circles.h
// SuperTerminal v2 - Circle Rendering API
//
// High-performance GPU-accelerated circle rendering with gradients
//

#ifndef ST_API_CIRCLES_H
#define ST_API_CIRCLES_H

#include "superterminal_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// ID-Based Circle Management API
// =============================================================================

/// Create a solid-color circle (returns circle ID)
/// @param x X position in pixels (center)
/// @param y Y position in pixels (center)
/// @param radius Radius in pixels
/// @param color RGBA color (0xRRGGBBAA format)
/// @return Circle ID (positive) or -1 on error
ST_API int st_circle_create(float x, float y, float radius, uint32_t color);

/// Create a radial gradient circle (returns circle ID)
/// @param x X position in pixels (center)
/// @param y Y position in pixels (center)
/// @param radius Radius in pixels
/// @param centerColor Center color (0xRRGGBBAA format)
/// @param edgeColor Edge color (0xRRGGBBAA format)
/// @return Circle ID (positive) or -1 on error
ST_API int st_circle_create_radial(float x, float y, float radius,
                                    uint32_t centerColor, uint32_t edgeColor);

/// Create a three-color radial gradient circle (returns circle ID)
/// @param x X position in pixels (center)
/// @param y Y position in pixels (center)
/// @param radius Radius in pixels
/// @param color1 Center color (0xRRGGBBAA format)
/// @param color2 Middle color (0xRRGGBBAA format)
/// @param color3 Edge color (0xRRGGBBAA format)
/// @return Circle ID (positive) or -1 on error
ST_API int st_circle_create_radial_3(float x, float y, float radius,
                                      uint32_t color1, uint32_t color2, uint32_t color3);

/// Create a four-color radial gradient circle (returns circle ID)
/// @param x X position in pixels (center)
/// @param y Y position in pixels (center)
/// @param radius Radius in pixels
/// @param color1 Center color (0xRRGGBBAA format)
/// @param color2 First ring color (0xRRGGBBAA format)
/// @param color3 Second ring color (0xRRGGBBAA format)
/// @param color4 Edge color (0xRRGGBBAA format)
/// @return Circle ID (positive) or -1 on error
ST_API int st_circle_create_radial_4(float x, float y, float radius,
                                      uint32_t color1, uint32_t color2,
                                      uint32_t color3, uint32_t color4);

// =============================================================================
// Procedural Pattern Creation API
// =============================================================================

/// Create an outlined circle (border around filled area)
/// @param x X position in pixels (center)
/// @param y Y position in pixels (center)
/// @param radius Radius in pixels
/// @param fillColor Fill color (0xRRGGBBAA format)
/// @param outlineColor Outline color (0xRRGGBBAA format)
/// @param lineWidth Outline width in pixels (default: 2.0)
/// @return Circle ID (positive) or -1 on error
ST_API int st_circle_create_outline(float x, float y, float radius,
                                     uint32_t fillColor, uint32_t outlineColor, float lineWidth);

/// Create a dashed outline circle
/// @param x X position in pixels (center)
/// @param y Y position in pixels (center)
/// @param radius Radius in pixels
/// @param fillColor Fill color (0xRRGGBBAA format)
/// @param outlineColor Outline color (0xRRGGBBAA format)
/// @param lineWidth Outline width in pixels (default: 2.0)
/// @param dashLength Length of each dash in pixels (default: 10.0)
/// @return Circle ID (positive) or -1 on error
ST_API int st_circle_create_dashed_outline(float x, float y, float radius,
                                            uint32_t fillColor, uint32_t outlineColor,
                                            float lineWidth, float dashLength);

/// Create a ring (hollow circle)
/// @param x X position in pixels (center)
/// @param y Y position in pixels (center)
/// @param outerRadius Outer radius in pixels
/// @param innerRadius Inner radius in pixels
/// @param color Ring color (0xRRGGBBAA format)
/// @return Circle ID (positive) or -1 on error
ST_API int st_circle_create_ring(float x, float y, float outerRadius, float innerRadius,
                                  uint32_t color);

/// Create a pie slice (for charts)
/// @param x X position in pixels (center)
/// @param y Y position in pixels (center)
/// @param radius Radius in pixels
/// @param startAngle Start angle in radians
/// @param endAngle End angle in radians
/// @param color Slice color (0xRRGGBBAA format)
/// @return Circle ID (positive) or -1 on error
ST_API int st_circle_create_pie_slice(float x, float y, float radius,
                                       float startAngle, float endAngle, uint32_t color);

/// Create an arc segment
/// @param x X position in pixels (center)
/// @param y Y position in pixels (center)
/// @param radius Radius in pixels
/// @param startAngle Start angle in radians
/// @param endAngle End angle in radians
/// @param color Arc color (0xRRGGBBAA format)
/// @param lineWidth Arc thickness in pixels (default: 2.0)
/// @return Circle ID (positive) or -1 on error
ST_API int st_circle_create_arc(float x, float y, float radius,
                                 float startAngle, float endAngle,
                                 uint32_t color, float lineWidth);

/// Create a ring of dots
/// @param x X position in pixels (center)
/// @param y Y position in pixels (center)
/// @param radius Radius of the ring in pixels
/// @param dotColor Dot color (0xRRGGBBAA format)
/// @param backgroundColor Background color (0xRRGGBBAA format)
/// @param dotRadius Radius of each dot in pixels (default: 3.0)
/// @param numDots Number of dots around the ring (default: 12)
/// @return Circle ID (positive) or -1 on error
ST_API int st_circle_create_dots_ring(float x, float y, float radius,
                                       uint32_t dotColor, uint32_t backgroundColor,
                                       float dotRadius, int numDots);

/// Create a star burst pattern
/// @param x X position in pixels (center)
/// @param y Y position in pixels (center)
/// @param radius Radius in pixels
/// @param color1 First ray color (0xRRGGBBAA format)
/// @param color2 Second ray color (0xRRGGBBAA format)
/// @param numRays Number of rays (default: 8)
/// @return Circle ID (positive) or -1 on error
ST_API int st_circle_create_star_burst(float x, float y, float radius,
                                        uint32_t color1, uint32_t color2, int numRays);

// =============================================================================
// Circle Update API
// =============================================================================

/// Update circle position
/// @param id Circle ID
/// @param x New X position in pixels (center)
/// @param y New Y position in pixels (center)
/// @return true on success, false if ID not found
ST_API bool st_circle_set_position(int id, float x, float y);

/// Update circle radius
/// @param id Circle ID
/// @param radius New radius in pixels
/// @return true on success, false if ID not found
ST_API bool st_circle_set_radius(int id, float radius);

/// Update circle color (for solid circles)
/// @param id Circle ID
/// @param color RGBA color (0xRRGGBBAA format)
/// @return true on success, false if ID not found
ST_API bool st_circle_set_color(int id, uint32_t color);

/// Update circle gradient colors
/// @param id Circle ID
/// @param color1 First color
/// @param color2 Second color
/// @param color3 Third color
/// @param color4 Fourth color
/// @return true on success, false if ID not found
ST_API bool st_circle_set_colors(int id, uint32_t color1, uint32_t color2,
                                  uint32_t color3, uint32_t color4);

/// Update circle pattern parameters
/// @param id Circle ID
/// @param param1 First parameter (e.g., line width, inner radius)
/// @param param2 Second parameter (e.g., start angle, dash length)
/// @param param3 Third parameter (e.g., end angle, num dots)
/// @return true on success, false if ID not found
ST_API bool st_circle_set_parameters(int id, float param1, float param2, float param3);

/// Show or hide a circle
/// @param id Circle ID
/// @param visible true to show, false to hide
/// @return true on success, false if ID not found
ST_API bool st_circle_set_visible(int id, bool visible);

/// Check if circle exists
/// @param id Circle ID
/// @return true if circle exists
ST_API bool st_circle_exists(int id);

/// Check if circle is visible
/// @param id Circle ID
/// @return true if circle is visible, false if hidden or doesn't exist
ST_API bool st_circle_is_visible(int id);

/// Delete a circle by ID
/// @param id Circle ID
/// @return true on success, false if ID not found
ST_API bool st_circle_delete(int id);

/// Delete all managed circles
ST_API void st_circle_delete_all(void);

// =============================================================================
// General Circle Statistics and Management
// =============================================================================

/// Get the total number of managed circles
/// @return Total number of circles
ST_API size_t st_circle_count(void);

/// Check if there are no managed circles
/// @return true if no circles exist
ST_API bool st_circle_is_empty(void);

/// Set the maximum number of managed circles
/// @param max Maximum number of circles (default: 10000)
ST_API void st_circle_set_max(size_t max);

/// Get the maximum number of managed circles
/// @return Maximum circle capacity
ST_API size_t st_circle_get_max(void);

#ifdef __cplusplus
}
#endif

#endif // ST_API_CIRCLES_H