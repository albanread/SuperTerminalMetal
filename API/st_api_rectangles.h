//
// st_api_rectangles.h
// SuperTerminal v2 - Rectangle Rendering API
//
// High-performance GPU-accelerated rectangle rendering with gradients
//

#ifndef ST_API_RECTANGLES_H
#define ST_API_RECTANGLES_H

#include "superterminal_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// ID-Based Rectangle Management API
// =============================================================================

/// Create a solid-color rectangle (returns rectangle ID)
/// @param x X position in pixels
/// @param y Y position in pixels
/// @param width Width in pixels
/// @param height Height in pixels
/// @param color RGBA color (0xRRGGBBAA format)
/// @return Rectangle ID (positive) or -1 on error
ST_API int st_rect_create(float x, float y, float width, float height, uint32_t color);

/// Create a gradient rectangle (returns rectangle ID)
/// @param x X position in pixels
/// @param y Y position in pixels
/// @param width Width in pixels
/// @param height Height in pixels
/// @param color1 First gradient color (0xRRGGBBAA format)
/// @param color2 Second gradient color (0xRRGGBBAA format)
/// @param mode Gradient mode (ST_GRADIENT_*)
/// @return Rectangle ID (positive) or -1 on error
ST_API int st_rect_create_gradient(float x, float y, float width, float height,
                                     uint32_t color1, uint32_t color2,
                                     STRectangleGradientMode mode);

/// Create a three-point gradient rectangle (returns rectangle ID)
/// @param x X position in pixels
/// @param y Y position in pixels
/// @param width Width in pixels
/// @param height Height in pixels
/// @param color1 First gradient color (0xRRGGBBAA format)
/// @param color2 Second gradient color (0xRRGGBBAA format)
/// @param color3 Third gradient color (0xRRGGBBAA format)
/// @param mode Gradient mode (ST_GRADIENT_THREE_POINT recommended)
/// @return Rectangle ID (positive) or -1 on error
ST_API int st_rect_create_three_point(float x, float y, float width, float height,
                                        uint32_t color1, uint32_t color2, uint32_t color3,
                                        STRectangleGradientMode mode);

/// Create a four-corner gradient rectangle (returns rectangle ID)
/// @param x X position in pixels
/// @param y Y position in pixels
/// @param width Width in pixels
/// @param height Height in pixels
/// @param topLeft Top-left corner color (0xRRGGBBAA format)
/// @param topRight Top-right corner color (0xRRGGBBAA format)
/// @param bottomRight Bottom-right corner color (0xRRGGBBAA format)
/// @param bottomLeft Bottom-left corner color (0xRRGGBBAA format)
/// @return Rectangle ID (positive) or -1 on error
ST_API int st_rect_create_four_corner(float x, float y, float width, float height,
                                        uint32_t topLeft, uint32_t topRight,
                                        uint32_t bottomRight, uint32_t bottomLeft);

// =============================================================================
// Procedural Pattern Creation API
// =============================================================================

/// Create an outlined rectangle (border around filled area)
/// @param x X position in pixels
/// @param y Y position in pixels
/// @param width Width in pixels
/// @param height Height in pixels
/// @param fillColor Fill color (0xRRGGBBAA format)
/// @param outlineColor Outline color (0xRRGGBBAA format)
/// @param lineWidth Outline width in pixels (default: 2.0)
/// @return Rectangle ID (positive) or -1 on error
ST_API int st_rect_create_outline(float x, float y, float width, float height,
                                   uint32_t fillColor, uint32_t outlineColor, float lineWidth);

/// Create a dashed outline rectangle
/// @param x X position in pixels
/// @param y Y position in pixels
/// @param width Width in pixels
/// @param height Height in pixels
/// @param fillColor Fill color (0xRRGGBBAA format)
/// @param outlineColor Outline color (0xRRGGBBAA format)
/// @param lineWidth Outline width in pixels (default: 2.0)
/// @param dashLength Length of each dash in pixels (default: 10.0)
/// @return Rectangle ID (positive) or -1 on error
ST_API int st_rect_create_dashed_outline(float x, float y, float width, float height,
                                          uint32_t fillColor, uint32_t outlineColor,
                                          float lineWidth, float dashLength);

/// Create a horizontal striped rectangle
/// @param x X position in pixels
/// @param y Y position in pixels
/// @param width Width in pixels
/// @param height Height in pixels
/// @param color1 First stripe color (0xRRGGBBAA format)
/// @param color2 Second stripe color (0xRRGGBBAA format)
/// @param stripeHeight Height of each stripe in pixels (default: 10.0)
/// @return Rectangle ID (positive) or -1 on error
ST_API int st_rect_create_horizontal_stripes(float x, float y, float width, float height,
                                              uint32_t color1, uint32_t color2, float stripeHeight);

/// Create a vertical striped rectangle
/// @param x X position in pixels
/// @param y Y position in pixels
/// @param width Width in pixels
/// @param height Height in pixels
/// @param color1 First stripe color (0xRRGGBBAA format)
/// @param color2 Second stripe color (0xRRGGBBAA format)
/// @param stripeWidth Width of each stripe in pixels (default: 10.0)
/// @return Rectangle ID (positive) or -1 on error
ST_API int st_rect_create_vertical_stripes(float x, float y, float width, float height,
                                            uint32_t color1, uint32_t color2, float stripeWidth);

/// Create a diagonal striped rectangle
/// @param x X position in pixels
/// @param y Y position in pixels
/// @param width Width in pixels
/// @param height Height in pixels
/// @param color1 First stripe color (0xRRGGBBAA format)
/// @param color2 Second stripe color (0xRRGGBBAA format)
/// @param stripeWidth Width of each stripe in pixels (default: 10.0)
/// @param angle Rotation angle in degrees (default: 45.0)
/// @return Rectangle ID (positive) or -1 on error
ST_API int st_rect_create_diagonal_stripes(float x, float y, float width, float height,
                                            uint32_t color1, uint32_t color2,
                                            float stripeWidth, float angle);

/// Create a checkerboard pattern rectangle
/// @param x X position in pixels
/// @param y Y position in pixels
/// @param width Width in pixels
/// @param height Height in pixels
/// @param color1 First checker color (0xRRGGBBAA format)
/// @param color2 Second checker color (0xRRGGBBAA format)
/// @param cellSize Size of each checker cell in pixels (default: 10.0)
/// @return Rectangle ID (positive) or -1 on error
ST_API int st_rect_create_checkerboard(float x, float y, float width, float height,
                                        uint32_t color1, uint32_t color2, float cellSize);

/// Create a dot pattern rectangle
/// @param x X position in pixels
/// @param y Y position in pixels
/// @param width Width in pixels
/// @param height Height in pixels
/// @param dotColor Dot color (0xRRGGBBAA format)
/// @param backgroundColor Background color (0xRRGGBBAA format)
/// @param dotRadius Radius of each dot in pixels (default: 3.0)
/// @param spacing Spacing between dot centers in pixels (default: 10.0)
/// @return Rectangle ID (positive) or -1 on error
ST_API int st_rect_create_dots(float x, float y, float width, float height,
                                uint32_t dotColor, uint32_t backgroundColor,
                                float dotRadius, float spacing);

/// Create a crosshatch pattern rectangle
/// @param x X position in pixels
/// @param y Y position in pixels
/// @param width Width in pixels
/// @param height Height in pixels
/// @param lineColor Line color (0xRRGGBBAA format)
/// @param backgroundColor Background color (0xRRGGBBAA format)
/// @param lineWidth Width of crosshatch lines in pixels (default: 1.0)
/// @param spacing Spacing between lines in pixels (default: 10.0)
/// @return Rectangle ID (positive) or -1 on error
ST_API int st_rect_create_crosshatch(float x, float y, float width, float height,
                                      uint32_t lineColor, uint32_t backgroundColor,
                                      float lineWidth, float spacing);

/// Create a rounded corner rectangle
/// @param x X position in pixels
/// @param y Y position in pixels
/// @param width Width in pixels
/// @param height Height in pixels
/// @param color Fill color (0xRRGGBBAA format)
/// @param cornerRadius Corner radius in pixels (default: 10.0)
/// @return Rectangle ID (positive) or -1 on error
ST_API int st_rect_create_rounded_corners(float x, float y, float width, float height,
                                           uint32_t color, float cornerRadius);

/// Create a grid pattern rectangle
/// @param x X position in pixels
/// @param y Y position in pixels
/// @param width Width in pixels
/// @param height Height in pixels
/// @param lineColor Grid line color (0xRRGGBBAA format)
/// @param backgroundColor Background color (0xRRGGBBAA format)
/// @param lineWidth Width of grid lines in pixels (default: 1.0)
/// @param cellSize Size of each grid cell in pixels (default: 10.0)
/// @return Rectangle ID (positive) or -1 on error
ST_API int st_rect_create_grid(float x, float y, float width, float height,
                                uint32_t lineColor, uint32_t backgroundColor,
                                float lineWidth, float cellSize);

// =============================================================================
// Rectangle Update API
// =============================================================================

/// Update rectangle position
/// @param id Rectangle ID
/// @param x New X position in pixels
/// @param y New Y position in pixels
/// @return true on success, false if ID not found
ST_API bool st_rect_set_position(int id, float x, float y);

/// Update rectangle size
/// @param id Rectangle ID
/// @param width New width in pixels
/// @param height New height in pixels
/// @return true on success, false if ID not found
ST_API bool st_rect_set_size(int id, float width, float height);

/// Update rectangle color (for solid or all gradient colors)
/// @param id Rectangle ID
/// @param color RGBA color (0xRRGGBBAA format)
/// @return true on success, false if ID not found
ST_API bool st_rect_set_color(int id, uint32_t color);

/// Update rectangle gradient colors
/// @param id Rectangle ID
/// @param color1 First gradient color
/// @param color2 Second gradient color
/// @param color3 Third gradient color
/// @param color4 Fourth gradient color
/// @return true on success, false if ID not found
ST_API bool st_rect_set_colors(int id, uint32_t color1, uint32_t color2, 
                                 uint32_t color3, uint32_t color4);

/// Update rectangle gradient mode
/// @param id Rectangle ID
/// @param mode Gradient mode (ST_GRADIENT_*)
/// @return true on success, false if ID not found
ST_API bool st_rect_set_mode(int id, STRectangleGradientMode mode);

/// Update rectangle pattern parameters
/// @param id Rectangle ID
/// @param param1 First parameter (e.g., line width, stripe size)
/// @param param2 Second parameter (e.g., spacing, corner radius)
/// @param param3 Third parameter (e.g., angle, grid size)
/// @return true on success, false if ID not found
ST_API bool st_rect_set_parameters(int id, float param1, float param2, float param3);

/// Set rectangle rotation
/// @param id Rectangle ID
/// @param angleDegrees Rotation angle in degrees (clockwise)
/// @return true on success, false if ID not found
ST_API bool st_rect_set_rotation(int id, float angleDegrees);

/// Show or hide a rectangle
/// @param id Rectangle ID
/// @param visible true to show, false to hide
/// @return true on success, false if ID not found
ST_API bool st_rect_set_visible(int id, bool visible);

/// Check if rectangle exists
/// @param id Rectangle ID
/// @return true if rectangle exists
ST_API bool st_rect_exists(int id);

/// Check if rectangle is visible
/// @param id Rectangle ID
/// @return true if rectangle is visible, false if hidden or doesn't exist
ST_API bool st_rect_is_visible(int id);

/// Delete a rectangle by ID
/// @param id Rectangle ID
/// @return true on success, false if ID not found
ST_API bool st_rect_delete(int id);

/// Delete all managed rectangles
ST_API void st_rect_delete_all(void);

// =============================================================================
// General Rectangle Statistics and Management
// =============================================================================

/// Get the total number of managed rectangles
/// @return Total number of rectangles
ST_API size_t st_rect_count(void);

/// Check if there are no managed rectangles
/// @return true if no rectangles exist
ST_API bool st_rect_is_empty(void);

/// Set the maximum number of managed rectangles
/// @param max Maximum number of rectangles (default: 10000)
ST_API void st_rect_set_max(size_t max);

/// Get the maximum number of managed rectangles
/// @return Maximum rectangle capacity
ST_API size_t st_rect_get_max(void);

#ifdef __cplusplus
}
#endif

#endif // ST_API_RECTANGLES_H