//
// st_sprite_drawing_utils.h
// SuperTerminal v2
//
// Utility functions for rendering graphics primitives directly to CGContext
// Used by DrawIntoSprite feature to render graphics commands to sprite textures
//

#ifndef ST_SPRITE_DRAWING_UTILS_H
#define ST_SPRITE_DRAWING_UTILS_H

#ifdef __OBJC__
    #import <CoreGraphics/CoreGraphics.h>
#else
    typedef struct CGContext* CGContextRef;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// Render a filled rectangle to a CGContext
/// @param context CoreGraphics context to render to
/// @param x X coordinate (top-left)
/// @param y Y coordinate (top-left)
/// @param width Rectangle width
/// @param height Rectangle height
/// @param r Red (0-1)
/// @param g Green (0-1)
/// @param b Blue (0-1)
/// @param a Alpha (0-1)
void st_sprite_draw_rect(CGContextRef context, float x, float y, float width, float height,
                         float r, float g, float b, float a);

/// Render a rectangle outline to a CGContext
/// @param context CoreGraphics context to render to
/// @param x X coordinate (top-left)
/// @param y Y coordinate (top-left)
/// @param width Rectangle width
/// @param height Rectangle height
/// @param r Red (0-1)
/// @param g Green (0-1)
/// @param b Blue (0-1)
/// @param a Alpha (0-1)
/// @param lineWidth Line thickness
void st_sprite_draw_rect_outline(CGContextRef context, float x, float y, float width, float height,
                                  float r, float g, float b, float a, float lineWidth);

/// Render a filled circle to a CGContext
/// @param context CoreGraphics context to render to
/// @param x Center X coordinate
/// @param y Center Y coordinate
/// @param radius Circle radius
/// @param r Red (0-1)
/// @param g Green (0-1)
/// @param b Blue (0-1)
/// @param a Alpha (0-1)
void st_sprite_draw_circle(CGContextRef context, float x, float y, float radius,
                           float r, float g, float b, float a);

/// Render a circle outline to a CGContext
/// @param context CoreGraphics context to render to
/// @param x Center X coordinate
/// @param y Center Y coordinate
/// @param radius Circle radius
/// @param r Red (0-1)
/// @param g Green (0-1)
/// @param b Blue (0-1)
/// @param a Alpha (0-1)
/// @param lineWidth Line thickness
void st_sprite_draw_circle_outline(CGContextRef context, float x, float y, float radius,
                                    float r, float g, float b, float a, float lineWidth);

/// Render an arc outline to a CGContext
/// @param context CoreGraphics context to render to
/// @param x Center X coordinate
/// @param y Center Y coordinate
/// @param radius Arc radius
/// @param start_angle Start angle in radians
/// @param end_angle End angle in radians
/// @param r Red (0-1)
/// @param g Green (0-1)
/// @param b Blue (0-1)
/// @param a Alpha (0-1)
void st_sprite_draw_arc(CGContextRef context, float x, float y, float radius,
                        float start_angle, float end_angle,
                        float r, float g, float b, float a);

/// Render a filled arc (pie slice) to a CGContext
/// @param context CoreGraphics context to render to
/// @param x Center X coordinate
/// @param y Center Y coordinate
/// @param radius Arc radius
/// @param start_angle Start angle in radians
/// @param end_angle End angle in radians
/// @param r Red (0-1)
/// @param g Green (0-1)
/// @param b Blue (0-1)
/// @param a Alpha (0-1)
void st_sprite_draw_arc_filled(CGContextRef context, float x, float y, float radius,
                               float start_angle, float end_angle,
                               float r, float g, float b, float a);

/// Render a line to a CGContext
/// @param context CoreGraphics context to render to
/// @param x1 Start X coordinate
/// @param y1 Start Y coordinate
/// @param x2 End X coordinate
/// @param y2 End Y coordinate
/// @param r Red (0-1)
/// @param g Green (0-1)
/// @param b Blue (0-1)
/// @param a Alpha (0-1)
/// @param lineWidth Line thickness
void st_sprite_draw_line(CGContextRef context, float x1, float y1, float x2, float y2,
                         float r, float g, float b, float a, float lineWidth);

/// Render a single pixel/point to a CGContext
/// @param context CoreGraphics context to render to
/// @param x X coordinate
/// @param y Y coordinate
/// @param r Red (0-1)
/// @param g Green (0-1)
/// @param b Blue (0-1)
/// @param a Alpha (0-1)
void st_sprite_draw_pixel(CGContextRef context, float x, float y,
                          float r, float g, float b, float a);

/// Clear a CGContext to transparent
/// @param context CoreGraphics context to clear
/// @param width Context width
/// @param height Context height
void st_sprite_clear(CGContextRef context, int width, int height);

#ifdef __cplusplus
}
#endif

#endif // ST_SPRITE_DRAWING_UTILS_H