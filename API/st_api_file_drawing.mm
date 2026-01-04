// =============================================================================
// st_api_file_drawing.mm
// SuperTerminal API - File Drawing Implementation
//
// Implements DrawToFile/EndDrawToFile for redirecting graphics to PNG files
// =============================================================================

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#include "superterminal_api.h"
#include "st_api_context.h"

using namespace STApi;

// =============================================================================
// File Drawing API Implementation
// =============================================================================

ST_API bool st_draw_to_file_begin(const char* filename, int width, int height) {
    NSLog(@"st_draw_to_file_begin: Called with filename='%s', width=%d, height=%d",
          filename ? filename : "NULL", width, height);

    if (!filename || strlen(filename) == 0) {
        ST_SET_ERROR("Invalid filename");
        NSLog(@"st_draw_to_file_begin: NULL or empty filename");
        return false;
    }

    if (width <= 0 || height <= 0) {
        ST_SET_ERROR("Invalid dimensions");
        NSLog(@"st_draw_to_file_begin: Invalid dimensions");
        return false;
    }

    ST_LOCK;

    // Check if already drawing to sprite or file
    if (ST_CONTEXT.isDrawingIntoSprite()) {
        ST_SET_ERROR("Already drawing into a sprite - call EndDrawIntoSprite first");
        NSLog(@"st_draw_to_file_begin: Already drawing into sprite");
        return false;
    }

    if (ST_CONTEXT.isDrawingToFile()) {
        ST_SET_ERROR("Already drawing to file - call EndDrawToFile first");
        NSLog(@"st_draw_to_file_begin: Already drawing to file");
        return false;
    }

    NSLog(@"st_draw_to_file_begin: Creating CGContext for file drawing");

    // Allocate bitmap data (RGBA, 4 bytes per pixel)
    size_t bytesPerRow = width * 4;
    size_t dataSize = bytesPerRow * height;
    void* bitmapData = calloc(dataSize, 1);

    if (!bitmapData) {
        ST_SET_ERROR("Failed to allocate bitmap memory");
        NSLog(@"st_draw_to_file_begin: Memory allocation failed");
        return false;
    }

    NSLog(@"st_draw_to_file_begin: Allocated %zu bytes for %dx%d bitmap", dataSize, width, height);

    // Create CGColorSpace
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

    // Create CGContext for drawing
    // Using RGBA format with premultiplied alpha
    CGContextRef context = CGBitmapContextCreate(
        bitmapData,
        width,
        height,
        8,  // bits per component
        bytesPerRow,
        colorSpace,
        kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast  // RGBA
    );

    CGColorSpaceRelease(colorSpace);

    if (!context) {
        free(bitmapData);
        ST_SET_ERROR("Failed to create CGContext");
        NSLog(@"st_draw_to_file_begin: CGContext creation failed");
        return false;
    }

    NSLog(@"st_draw_to_file_begin: CGContext created successfully");

    // Clear bitmap to transparent black
    CGContextClearRect(context, CGRectMake(0, 0, width, height));

    // Set initial drawing state
    CGContextSetShouldAntialias(context, true);
    CGContextSetAllowsAntialiasing(context, true);
    CGContextSetInterpolationQuality(context, kCGInterpolationHigh);

    // Flip coordinate system to match SuperTerminal coordinates (origin top-left)
    CGContextTranslateCTM(context, 0, height);
    CGContextScaleCTM(context, 1.0, -1.0);

    NSLog(@"st_draw_to_file_begin: CGContext configured, coordinate system flipped");

    // Store state in context
    ST_CONTEXT.beginFileDrawing(filename, width, height, context, bitmapData);

    NSLog(@"st_draw_to_file_begin: Success, file context ready for drawing");
    return true;
}

ST_API bool st_draw_to_file_end() {
    NSLog(@"st_draw_to_file_end: Called");

    ST_LOCK;

    if (!ST_CONTEXT.isDrawingToFile()) {
        ST_SET_ERROR("Not currently drawing to file");
        NSLog(@"st_draw_to_file_end: Not drawing to file");
        return false;
    }

    // Get drawing state
    CGContextRef context = ST_CONTEXT.getFileDrawContext();
    void* pixels = ST_CONTEXT.getFileDrawBitmapData();
    int width = ST_CONTEXT.getFileDrawWidth();
    int height = ST_CONTEXT.getFileDrawHeight();
    const char* filename = ST_CONTEXT.getFileDrawFilename();

    NSLog(@"st_draw_to_file_end: Finalizing file '%s' (%dx%d)", filename, width, height);

    // Flush the context to ensure all drawing is complete
    CGContextFlush(context);

    // Sample first few pixels for debugging
    const uint32_t* pixelData = (const uint32_t*)pixels;
    NSLog(@"st_draw_to_file_end: First 4 pixels: 0x%08X 0x%08X 0x%08X 0x%08X",
           pixelData[0], pixelData[1], pixelData[2], pixelData[3]);

    // Create CGImage from bitmap
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, pixels, width * height * 4, NULL);

    CGImageRef image = CGImageCreate(
        width,
        height,
        8,  // bits per component
        32, // bits per pixel
        width * 4,  // bytes per row
        colorSpace,
        kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast,
        provider,
        NULL,
        false,
        kCGRenderingIntentDefault
    );

    bool success = false;

    if (image) {
        // Convert filename to URL
        NSString* nsFilename = [NSString stringWithUTF8String:filename];

        // If filename is relative, make it absolute relative to current directory
        if (![nsFilename hasPrefix:@"/"]) {
            NSString* currentDir = [[NSFileManager defaultManager] currentDirectoryPath];
            nsFilename = [currentDir stringByAppendingPathComponent:nsFilename];
        }

        NSURL* url = [NSURL fileURLWithPath:nsFilename];

        NSLog(@"st_draw_to_file_end: Writing PNG to: %s", [nsFilename UTF8String]);

        // Create image destination (PNG)
        CGImageDestinationRef destination = CGImageDestinationCreateWithURL(
            (__bridge CFURLRef)url,
            kUTTypePNG,
            1,
            NULL
        );

        if (destination) {
            // Add image to destination
            CGImageDestinationAddImage(destination, image, NULL);

            // Finalize (write to disk)
            if (CGImageDestinationFinalize(destination)) {
                NSLog(@"st_draw_to_file_end: PNG file written successfully");
                success = true;
            } else {
                ST_SET_ERROR("Failed to write PNG file");
                NSLog(@"st_draw_to_file_end: Failed to finalize PNG");
            }

            CFRelease(destination);
        } else {
            ST_SET_ERROR("Failed to create image destination");
            NSLog(@"st_draw_to_file_end: Failed to create image destination");
        }

        CGImageRelease(image);
    } else {
        ST_SET_ERROR("Failed to create CGImage from bitmap");
        NSLog(@"st_draw_to_file_end: Failed to create CGImage");
    }

    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorSpace);

    // Release context and bitmap
    CGContextRelease(context);
    free(pixels);

    // Clear state
    ST_CONTEXT.endFileDrawing();

    NSLog(@"st_draw_to_file_end: Complete (success=%d)", success);
    return success;
}
