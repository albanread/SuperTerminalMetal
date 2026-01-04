//
// st_api_polygons.h
// SuperTerminal v2 - Polygon Rendering API
//
// High-performance GPU-accelerated regular polygon rendering with gradients and patterns
//

#ifndef ST_API_POLYGONS_H
#define ST_API_POLYGONS_H

#include "superterminal_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Polygon Gradient/Pattern Modes
// =============================================================================

typedef enum {
    ST_POLYGON_SOLID = 0,
    ST_POLYGON_HORIZONTAL = 1,
    ST_POLYGON_VERTICAL = 2,
    ST_POLYGON_DIAGONAL_TL_BR = 3,
    ST_POLYGON_DIAGONAL_TR_BL = 4,
    ST_POLYGON_RADIAL = 5,
    ST_POLYGON_FOUR_CORNER = 6,
    ST_POLYGON_THREE_POINT = 7,
    
    // Procedural patterns
    ST_POLYGON_OUTLINE = 100,
    ST_POLYGON_DASHED_OUTLINE = 101,
    ST_POLYGON_HORIZONTAL_STRIPES = 102,
    ST_POLYGON_VERTICAL_STRIPES = 103,
    ST_POLYGON_DIAGONAL_STRIPES = 104,
    ST_POLYGON_CHECKERBOARD = 105,
    ST_POLYGON_DOTS = 106,
    ST_POLYGON_CROSSHATCH = 107,
    ST_POLYGON_GRID = 109,
} STPolygonGradientMode;

// =============================================================================
// ID-Based Polygon Management API
// =============================================================================

/// Create a solid-color polygon (returns polygon ID)
/// @param x Center X position in pixels
/// @param y Center Y position in pixels
/// @param radius Radius (distance from center to vertex) in pixels
/// @param numSides Number of sides (3-12)
/// @param color RGBA color (0xRRGGBBAA format)
/// @return Polygon ID (positive) or -1 on error
ST_API int st_polygon_create(float x, float y, float radius, int numSides, uint32_t color);

/// Create a gradient polygon (returns polygon ID)
/// @param x Center X position in pixels
/// @param y Center Y position in pixels
/// @param radius Radius in pixels
/// @param numSides Number of sides (3-12)
/// @param color1 First gradient color (0xRRGGBBAA format)
/// @param color2 Second gradient color (0xRRGGBBAA format)
/// @param mode Gradient mode (ST_POLYGON_*)
/// @return Polygon ID (positive) or -1 on error
ST_API int st_polygon_create_gradient(float x, float y, float radius, int numSides,
                                       uint32_t color1, uint32_t color2,
                                       STPolygonGradientMode mode);

/// Create a three-point gradient polygon (returns polygon ID)
/// @param x Center X position in pixels
/// @param y Center Y position in pixels
/// @param radius Radius in pixels
/// @param numSides Number of sides (3-12)
/// @param color1 First gradient color (0xRRGGBBAA format)
/// @param color2 Second gradient color (0xRRGGBBAA format)
/// @param color3 Third gradient color (0xRRGGBBAA format)
/// @param mode Gradient mode (ST_POLYGON_THREE_POINT recommended)
/// @return Polygon ID (positive) or -1 on error
ST_API int st_polygon_create_three_point(float x, float y, float radius, int numSides,
                                          uint32_t color1, uint32_t color2, uint32_t color3,
                                          STPolygonGradientMode mode);

/// Create a four-corner gradient polygon (returns polygon ID)
/// @param x Center X position in pixels
/// @param y Center Y position in pixels
/// @param radius Radius in pixels
/// @param numSides Number of sides (3-12)
/// @param topLeft Top-left corner color (0xRRGGBBAA format)
/// @param topRight Top-right corner color (0xRRGGBBAA format)
/// @param bottomRight Bottom-right corner color (0xRRGGBBAA format)
/// @param bottomLeft Bottom-left corner color (0xRRGGBBAA format)
/// @return Polygon ID (positive) or -1 on error
ST_API int st_polygon_create_four_corner(float x, float y, float radius, int numSides,
                                          uint32_t topLeft, uint32_t topRight,
                                          uint32_t bottomRight, uint32_t bottomLeft);

// =============================================================================
// Procedural Pattern Creation API
// =============================================================================

/// Create an outlined polygon (border around filled area)
ST_API int st_polygon_create_outline(float x, float y, float radius, int numSides,
                                      uint32_t fillColor, uint32_t outlineColor, float lineWidth);

/// Create a dashed outline polygon
ST_API int st_polygon_create_dashed_outline(float x, float y, float radius, int numSides,
                                             uint32_t fillColor, uint32_t outlineColor,
                                             float lineWidth, float dashLength);

/// Create a horizontal striped polygon
ST_API int st_polygon_create_horizontal_stripes(float x, float y, float radius, int numSides,
                                                 uint32_t color1, uint32_t color2, float stripeHeight);

/// Create a vertical striped polygon
ST_API int st_polygon_create_vertical_stripes(float x, float y, float radius, int numSides,
                                               uint32_t color1, uint32_t color2, float stripeWidth);

/// Create a diagonal striped polygon
ST_API int st_polygon_create_diagonal_stripes(float x, float y, float radius, int numSides,
                                               uint32_t color1, uint32_t color2,
                                               float stripeWidth, float angle);

/// Create a checkerboard pattern polygon
ST_API int st_polygon_create_checkerboard(float x, float y, float radius, int numSides,
                                           uint32_t color1, uint32_t color2, float cellSize);

/// Create a dot pattern polygon
ST_API int st_polygon_create_dots(float x, float y, float radius, int numSides,
                                   uint32_t dotColor, uint32_t backgroundColor,
                                   float dotRadius, float spacing);

/// Create a crosshatch pattern polygon
ST_API int st_polygon_create_crosshatch(float x, float y, float radius, int numSides,
                                         uint32_t lineColor, uint32_t backgroundColor,
                                         float lineWidth, float spacing);

/// Create a grid pattern polygon
ST_API int st_polygon_create_grid(float x, float y, float radius, int numSides,
                                   uint32_t lineColor, uint32_t backgroundColor,
                                   float lineWidth, float cellSize);

// =============================================================================
// Polygon Update API
// =============================================================================

/// Update polygon position
ST_API bool st_polygon_set_position(int id, float x, float y);

/// Update polygon radius
ST_API bool st_polygon_set_radius(int id, float radius);

/// Update polygon number of sides
ST_API bool st_polygon_set_sides(int id, int numSides);

/// Update polygon color (for solid or all gradient colors)
ST_API bool st_polygon_set_color(int id, uint32_t color);

/// Update polygon gradient colors
ST_API bool st_polygon_set_colors(int id, uint32_t color1, uint32_t color2, 
                                   uint32_t color3, uint32_t color4);

/// Update polygon gradient mode
ST_API bool st_polygon_set_mode(int id, STPolygonGradientMode mode);

/// Update polygon pattern parameters
ST_API bool st_polygon_set_parameters(int id, float param1, float param2, float param3);

/// Set polygon rotation
ST_API bool st_polygon_set_rotation(int id, float angleDegrees);

/// Show or hide a polygon
ST_API bool st_polygon_set_visible(int id, bool visible);

/// Check if polygon exists
ST_API bool st_polygon_exists(int id);

/// Check if polygon is visible
ST_API bool st_polygon_is_visible(int id);

/// Delete a polygon by ID
ST_API bool st_polygon_delete(int id);

/// Delete all managed polygons
ST_API void st_polygon_delete_all(void);

// =============================================================================
// General Polygon Statistics and Management
// =============================================================================

/// Get the total number of managed polygons
ST_API size_t st_polygon_count(void);

/// Check if there are no managed polygons
ST_API bool st_polygon_is_empty(void);

/// Set the maximum number of managed polygons
ST_API void st_polygon_set_max(size_t max);

/// Get the maximum number of managed polygons
ST_API size_t st_polygon_get_max(void);

#ifdef __cplusplus
}
#endif

#endif // ST_API_POLYGONS_H