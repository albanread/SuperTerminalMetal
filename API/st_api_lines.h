//
//  st_api_lines.h
//  SuperTerminal Framework - Line Management C API
//
//  ID-based line management with GPU-accelerated rendering
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef ST_API_LINES_H
#define ST_API_LINES_H

#include "superterminal_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Line Creation Functions (return line ID)
// =============================================================================

/// Create a solid color line
/// @param x1 Start X coordinate in pixels
/// @param y1 Start Y coordinate in pixels
/// @param x2 End X coordinate in pixels
/// @param y2 End Y coordinate in pixels
/// @param color Line color (RGBA8888)
/// @param thickness Line thickness in pixels
/// @return Line ID (positive integer) or -1 on failure
ST_API int st_line_create(float x1, float y1, float x2, float y2, uint32_t color, float thickness);

/// Create a gradient line (color interpolates from start to end)
/// @param x1 Start X coordinate in pixels
/// @param y1 Start Y coordinate in pixels
/// @param x2 End X coordinate in pixels
/// @param y2 End Y coordinate in pixels
/// @param color1 Start color (RGBA8888)
/// @param color2 End color (RGBA8888)
/// @param thickness Line thickness in pixels
/// @return Line ID (positive integer) or -1 on failure
ST_API int st_line_create_gradient(float x1, float y1, float x2, float y2,
                                    uint32_t color1, uint32_t color2, float thickness);

/// Create a dashed line
/// @param x1 Start X coordinate in pixels
/// @param y1 Start Y coordinate in pixels
/// @param x2 End X coordinate in pixels
/// @param y2 End Y coordinate in pixels
/// @param color Line color (RGBA8888)
/// @param thickness Line thickness in pixels
/// @param dashLength Length of each dash in pixels
/// @param gapLength Length of gap between dashes in pixels
/// @return Line ID (positive integer) or -1 on failure
ST_API int st_line_create_dashed(float x1, float y1, float x2, float y2,
                                  uint32_t color, float thickness,
                                  float dashLength, float gapLength);

/// Create a dotted line
/// @param x1 Start X coordinate in pixels
/// @param y1 Start Y coordinate in pixels
/// @param x2 End X coordinate in pixels
/// @param y2 End Y coordinate in pixels
/// @param color Line color (RGBA8888)
/// @param thickness Line thickness in pixels
/// @param dotSpacing Distance between dot centers in pixels
/// @return Line ID (positive integer) or -1 on failure
ST_API int st_line_create_dotted(float x1, float y1, float x2, float y2,
                                  uint32_t color, float thickness,
                                  float dotSpacing);

// =============================================================================
// Line Update Functions
// =============================================================================

/// Update line endpoints
/// @param id Line ID
/// @param x1 New start X coordinate
/// @param y1 New start Y coordinate
/// @param x2 New end X coordinate
/// @param y2 New end Y coordinate
/// @return true if successful, false if line not found
ST_API bool st_line_set_endpoints(int id, float x1, float y1, float x2, float y2);

/// Update line thickness
/// @param id Line ID
/// @param thickness New thickness in pixels
/// @return true if successful, false if line not found
ST_API bool st_line_set_thickness(int id, float thickness);

/// Update line color (solid color)
/// @param id Line ID
/// @param color New color (RGBA8888)
/// @return true if successful, false if line not found
ST_API bool st_line_set_color(int id, uint32_t color);

/// Update line colors (for gradient lines)
/// @param id Line ID
/// @param color1 New start color (RGBA8888)
/// @param color2 New end color (RGBA8888)
/// @return true if successful, false if line not found
ST_API bool st_line_set_colors(int id, uint32_t color1, uint32_t color2);

/// Update dash pattern (for dashed lines)
/// @param id Line ID
/// @param dashLength Length of each dash in pixels
/// @param gapLength Length of gap between dashes in pixels
/// @return true if successful, false if line not found
ST_API bool st_line_set_dash_pattern(int id, float dashLength, float gapLength);

/// Show or hide a line
/// @param id Line ID
/// @param visible 1 to show, 0 to hide
/// @return true if successful, false if line not found
ST_API bool st_line_set_visible(int id, bool visible);

// =============================================================================
// Line Query Functions
// =============================================================================

/// Check if a line exists
/// @param id Line ID
/// @return true if line exists, false otherwise
ST_API bool st_line_exists(int id);

/// Check if a line is visible
/// @param id Line ID
/// @return true if line is visible, false if hidden or not found
ST_API bool st_line_is_visible(int id);

// =============================================================================
// Line Management Functions
// =============================================================================

/// Delete a specific line
/// @param id Line ID
/// @return true if deleted, false if not found
ST_API bool st_line_delete(int id);

/// Delete all lines
ST_API void st_line_delete_all(void);

/// Get total number of lines
/// @return Number of lines currently managed
ST_API size_t st_line_count(void);

/// Check if there are no lines
/// @return true if no lines exist, false otherwise
ST_API bool st_line_is_empty(void);

/// Get maximum number of lines that can be created
/// @return Maximum line capacity
ST_API size_t st_line_get_max(void);

/// Set maximum number of lines
/// @param max New maximum capacity
ST_API void st_line_set_max(size_t max);

#ifdef __cplusplus
}
#endif

#endif // ST_API_LINES_H