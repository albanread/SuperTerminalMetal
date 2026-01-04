//
// st_sprite_drawing_utils.mm
// SuperTerminal v2
//
// Implementation of utility functions for rendering graphics primitives to CGContext
// Used by DrawIntoSprite feature
//

#include "st_sprite_drawing_utils.h"
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

// =============================================================================
// Rectangle Drawing
// =============================================================================

void st_sprite_draw_rect(CGContextRef context, float x, float y, float width, float height,
                         float r, float g, float b, float a) {
    NSLog(@"[st_sprite_draw_rect] Called: context=%p x=%.1f y=%.1f w=%.1f h=%.1f rgba=(%.3f,%.3f,%.3f,%.3f)",
          context, x, y, width, height, r, g, b, a);

    if (!context) {
        NSLog(@"[st_sprite_draw_rect] ERROR: context is NULL!");
        return;
    }

    CGContextSaveGState(context);

    CGContextSetRGBFillColor(context, r, g, b, a);
    NSLog(@"[st_sprite_draw_rect] Set fill color");

    CGContextFillRect(context, CGRectMake(x, y, width, height));
    NSLog(@"[st_sprite_draw_rect] Filled rect");

    CGContextRestoreGState(context);
    NSLog(@"[st_sprite_draw_rect] Complete");
}

void st_sprite_draw_rect_outline(CGContextRef context, float x, float y, float width, float height,
                                  float r, float g, float b, float a, float lineWidth) {
    if (!context) return;

    CGContextSaveGState(context);

    CGContextSetRGBStrokeColor(context, r, g, b, a);
    CGContextSetLineWidth(context, lineWidth);
    CGContextStrokeRect(context, CGRectMake(x, y, width, height));

    CGContextRestoreGState(context);
}

// =============================================================================
// Circle Drawing
// =============================================================================

void st_sprite_draw_circle(CGContextRef context, float x, float y, float radius,
                           float r, float g, float b, float a) {
    if (!context) return;

    CGContextSaveGState(context);

    CGContextSetRGBFillColor(context, r, g, b, a);
    CGContextBeginPath(context);
    CGContextAddArc(context, x, y, radius, 0, 2 * M_PI, 0);
    CGContextClosePath(context);
    CGContextFillPath(context);

    CGContextRestoreGState(context);
}

void st_sprite_draw_circle_outline(CGContextRef context, float x, float y, float radius,
                                    float r, float g, float b, float a, float lineWidth) {
    if (!context) return;

    CGContextSaveGState(context);

    CGContextSetRGBStrokeColor(context, r, g, b, a);
    CGContextSetLineWidth(context, lineWidth);
    CGContextBeginPath(context);
    CGContextAddArc(context, x, y, radius, 0, 2 * M_PI, 0);
    CGContextClosePath(context);
    CGContextStrokePath(context);

    CGContextRestoreGState(context);
}

// =============================================================================
// Arc Drawing
// =============================================================================

void st_sprite_draw_arc(CGContextRef context, float x, float y, float radius,
                        float start_angle, float end_angle,
                        float r, float g, float b, float a) {
    if (!context) return;

    CGContextSaveGState(context);

    CGContextSetRGBStrokeColor(context, r, g, b, a);
    CGContextSetLineWidth(context, 1.0f);
    CGContextBeginPath(context);
    CGContextAddArc(context, x, y, radius, start_angle, end_angle, 0);
    CGContextStrokePath(context);

    CGContextRestoreGState(context);
}

void st_sprite_draw_arc_filled(CGContextRef context, float x, float y, float radius,
                               float start_angle, float end_angle,
                               float r, float g, float b, float a) {
    if (!context) return;

    CGContextSaveGState(context);

    CGContextSetRGBFillColor(context, r, g, b, a);
    CGContextBeginPath(context);
    CGContextMoveToPoint(context, x, y);
    CGContextAddArc(context, x, y, radius, start_angle, end_angle, 0);
    CGContextClosePath(context);
    CGContextFillPath(context);

    CGContextRestoreGState(context);
}

// =============================================================================
// Line Drawing
// =============================================================================

void st_sprite_draw_line(CGContextRef context, float x1, float y1, float x2, float y2,
                         float r, float g, float b, float a, float lineWidth) {
    if (!context) return;

    CGContextSaveGState(context);

    CGContextSetRGBStrokeColor(context, r, g, b, a);
    CGContextSetLineWidth(context, lineWidth);
    CGContextSetLineCap(context, kCGLineCapRound);
    CGContextBeginPath(context);
    CGContextMoveToPoint(context, x1, y1);
    CGContextAddLineToPoint(context, x2, y2);
    CGContextStrokePath(context);

    CGContextRestoreGState(context);
}

// =============================================================================
// Pixel Drawing
// =============================================================================

void st_sprite_draw_pixel(CGContextRef context, float x, float y,
                          float r, float g, float b, float a) {
    if (!context) return;

    // Draw a 1x1 filled rectangle for a pixel
    CGContextSaveGState(context);

    CGContextSetRGBFillColor(context, r, g, b, a);
    CGContextFillRect(context, CGRectMake(x, y, 1, 1));

    CGContextRestoreGState(context);
}

// =============================================================================
// Clear
// =============================================================================

void st_sprite_clear(CGContextRef context, int width, int height) {
    if (!context) return;

    CGContextSaveGState(context);
    CGContextSetBlendMode(context, kCGBlendModeClear);
    CGContextFillRect(context, CGRectMake(0, 0, width, height));
    CGContextRestoreGState(context);
}
