//
// UResBuffer.h
// SuperTerminal v2
//
// URES (Ultra Resolution) mode pixel buffer for 1280×720 graphics.
// Uses 16-bit ARGB4444 format (4 bits per channel) for direct color.
//
// THREAD SAFETY:
// - All public methods are thread-safe
// - Internal state is protected by mutex
//

#ifndef SUPERTERMINAL_URES_BUFFER_H
#define SUPERTERMINAL_URES_BUFFER_H

#include <cstdint>
#include <mutex>
#include <memory>

namespace SuperTerminal {

/// UResBuffer: High-resolution pixel buffer with direct 12-bit color + 4-bit alpha
///
/// Responsibilities:
/// - Store pixels at 1280×720 resolution (16:9 aspect ratio, 720p)
/// - Each pixel holds 16-bit ARGB4444 (4 bits per channel)
/// - Provide fast pixel read/write access
/// - Track dirty state for efficient rendering
/// - Thread-safe access for drawing
///
/// Resolution:
/// - URES: 1280×720 pixels (921,600 pixels)
/// - Memory: 1,843,200 bytes (1.8 MB per buffer)
///
/// Pixel Format: ARGB4444
/// - 16-bit per pixel (2 bytes)
/// - Bits [15-12]: Alpha (0-15, 0=transparent, 15=opaque)
/// - Bits [11-8]:  Red (0-15)
/// - Bits [7-4]:   Green (0-15)
/// - Bits [3-0]:   Blue (0-15)
/// - Hex format: 0xARGB
/// - Example: 0xF00F = opaque blue (A=15, R=0, G=0, B=15)
/// - Special: 0x0000 = transparent black (acts like color 0 in palette modes)
///
/// Color Capabilities:
/// - 4,096 simultaneous colors (12-bit RGB)
/// - 16 alpha levels per pixel
/// - Total combinations: 65,536 (including alpha)
///
/// Memory Layout:
/// - Format: uint16_t array (2 bytes per pixel)
/// - Row-major order: pixels[y * width + x]
/// - Alignment: 16-byte aligned for SIMD operations
class UResBuffer {
public:
    /// Resolution constants
    static constexpr int WIDTH = 1280;
    static constexpr int HEIGHT = 720;
    static constexpr int PIXEL_COUNT = WIDTH * HEIGHT;
    static constexpr size_t BUFFER_SIZE = PIXEL_COUNT * sizeof(uint16_t);
    
    /// Create a new UResBuffer
    /// Initializes all pixels to 0x0000 (transparent black)
    UResBuffer();
    
    /// Destructor
    ~UResBuffer();
    
    // Disable copy and move (due to mutex)
    UResBuffer(const UResBuffer&) = delete;
    UResBuffer& operator=(const UResBuffer&) = delete;
    UResBuffer(UResBuffer&&) = delete;
    UResBuffer& operator=(UResBuffer&&) = delete;
    
    /// Set a pixel color (ARGB4444 format)
    /// @param x X coordinate (0-1279)
    /// @param y Y coordinate (0-719)
    /// @param color 16-bit ARGB4444 color (0xARGB)
    /// @note Thread Safety: Safe to call from any thread
    /// @note Out of bounds coordinates are ignored
    void setPixel(int x, int y, uint16_t color);
    
    /// Get a pixel color
    /// @param x X coordinate (0-1279)
    /// @param y Y coordinate (0-719)
    /// @return 16-bit ARGB4444 color, or 0x0000 if out of bounds
    /// @note Thread Safety: Safe to call from any thread
    uint16_t getPixel(int x, int y) const;
    
    /// Clear all pixels to a specific color
    /// @param color 16-bit ARGB4444 color to fill
    /// @note Thread Safety: Safe to call from any thread
    void clear(uint16_t color = 0x0000);
    
    /// Fill a rectangular region with a color
    /// @param x X coordinate of top-left corner
    /// @param y Y coordinate of top-left corner
    /// @param width Width in pixels
    /// @param height Height in pixels
    /// @param color 16-bit ARGB4444 color to fill
    /// @note Thread Safety: Safe to call from any thread
    /// @note Clips to buffer bounds automatically
    void fillRect(int x, int y, int width, int height, uint16_t color);
    
    /// Draw a horizontal line
    /// @param x X coordinate of start point
    /// @param y Y coordinate
    /// @param width Length of line in pixels
    /// @param color 16-bit ARGB4444 color
    /// @note Thread Safety: Safe to call from any thread
    void hline(int x, int y, int width, uint16_t color);
    
    /// Draw a vertical line
    /// @param x X coordinate
    /// @param y Y coordinate of start point
    /// @param height Length of line in pixels
    /// @param color 16-bit ARGB4444 color
    /// @note Thread Safety: Safe to call from any thread
    void vline(int x, int y, int height, uint16_t color);
    
    /// Copy rectangular region within this buffer
    /// @param src_x Source X coordinate
    /// @param src_y Source Y coordinate
    /// @param width Width in pixels
    /// @param height Height in pixels
    /// @param dst_x Destination X coordinate
    /// @param dst_y Destination Y coordinate
    /// @note Thread Safety: Safe to call from any thread
    /// @note Handles overlapping regions correctly
    void blit(int src_x, int src_y, int width, int height, int dst_x, int dst_y);
    
    /// Copy rectangular region with transparency (skip 0x0000)
    /// @param src_x Source X coordinate
    /// @param src_y Source Y coordinate
    /// @param width Width in pixels
    /// @param height Height in pixels
    /// @param dst_x Destination X coordinate
    /// @param dst_y Destination Y coordinate
    /// @note Thread Safety: Safe to call from any thread
    /// @note Pixels with value 0x0000 (transparent black) are not copied
    void blitTransparent(int src_x, int src_y, int width, int height, 
                         int dst_x, int dst_y);
    
    /// Copy rectangular region from another buffer
    /// @param src Source buffer
    /// @param src_x Source X coordinate
    /// @param src_y Source Y coordinate
    /// @param width Width in pixels
    /// @param height Height in pixels
    /// @param dst_x Destination X coordinate
    /// @param dst_y Destination Y coordinate
    /// @note Thread Safety: Safe to call from any thread
    void blitFrom(const UResBuffer* src, int src_x, int src_y, int width, int height, 
                  int dst_x, int dst_y);
    
    /// Copy rectangular region from another buffer with transparency
    /// @param src Source buffer
    /// @param src_x Source X coordinate
    /// @param src_y Source Y coordinate
    /// @param width Width in pixels
    /// @param height Height in pixels
    /// @param dst_x Destination X coordinate
    /// @param dst_y Destination Y coordinate
    /// @note Thread Safety: Safe to call from any thread
    /// @note Pixels with value 0x0000 are not copied
    void blitFromTransparent(const UResBuffer* src, int src_x, int src_y, int width, int height,
                             int dst_x, int dst_y);
    
    /// Get raw pixel data for rendering
    /// @return Pointer to pixel buffer (1,843,200 bytes)
    /// @note Thread Safety: Caller must hold lock via getMutex()
    /// @note Data format: uint16_t[1280*720] in row-major order
    const uint16_t* getPixelData() const { return m_pixels.get(); }
    
    /// Get buffer dimensions
    /// @param width Output width (always 1280)
    /// @param height Output height (always 720)
    void getSize(int& width, int& height) const {
        width = WIDTH;
        height = HEIGHT;
    }
    
    /// Get buffer width
    /// @return Width in pixels (1280)
    int getWidth() const { return WIDTH; }
    
    /// Get buffer height
    /// @return Height in pixels (720)
    int getHeight() const { return HEIGHT; }
    
    /// Check if buffer has changed since last clearDirty()
    /// @return true if buffer has been modified
    /// @note Thread Safety: Safe to call from any thread
    bool isDirty() const;
    
    /// Clear the dirty flag (call after rendering)
    /// @note Thread Safety: Safe to call from any thread
    void clearDirty();
    
    /// Get mutex for external synchronization (e.g., during rendering)
    /// @return Reference to internal mutex
    std::mutex& getMutex() const { return m_mutex; }

private:
    /// Pixel storage: 1280×720 uint16_t array
    /// Each pixel stores ARGB4444 (16-bit)
    /// Memory: 1,843,200 bytes (1.8 MB)
    std::unique_ptr<uint16_t[]> m_pixels;
    
    /// Dirty flag for tracking changes
    bool m_dirty;
    
    /// Mutex for thread-safe access
    mutable std::mutex m_mutex;
    
    /// Helper: Clip rectangle to buffer bounds
    void clipRect(int& x, int& y, int& width, int& height) const;
};

/// Color utility functions for ARGB4444 format
namespace UResColor {
    /// Make 16-bit color from 4-bit components (0-15 each)
    /// @param r Red component (0-15)
    /// @param g Green component (0-15)
    /// @param b Blue component (0-15)
    /// @param a Alpha component (0-15, default 15=opaque)
    /// @return 16-bit ARGB4444 color
    inline uint16_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 15) {
        return ((a & 0xF) << 12) | ((r & 0xF) << 8) | ((g & 0xF) << 4) | (b & 0xF);
    }
    
    /// Make 16-bit color from RGB components (alpha=15, opaque)
    /// @param r Red component (0-15)
    /// @param g Green component (0-15)
    /// @param b Blue component (0-15)
    /// @return 16-bit ARGB4444 color with full opacity
    inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
        return rgba(r, g, b, 15);
    }
    
    /// Extract red component
    /// @param color 16-bit ARGB4444 color
    /// @return Red component (0-15)
    inline uint8_t getRed(uint16_t color) {
        return (color >> 8) & 0xF;
    }
    
    /// Extract green component
    /// @param color 16-bit ARGB4444 color
    /// @return Green component (0-15)
    inline uint8_t getGreen(uint16_t color) {
        return (color >> 4) & 0xF;
    }
    
    /// Extract blue component
    /// @param color 16-bit ARGB4444 color
    /// @return Blue component (0-15)
    inline uint8_t getBlue(uint16_t color) {
        return color & 0xF;
    }
    
    /// Extract alpha component
    /// @param color 16-bit ARGB4444 color
    /// @return Alpha component (0-15)
    inline uint8_t getAlpha(uint16_t color) {
        return (color >> 12) & 0xF;
    }
    
    /// Convert 24-bit RGB to 12-bit RGB (no dithering)
    /// @param rgb24 24-bit color (0xRRGGBB)
    /// @return 16-bit ARGB4444 color with full opacity
    inline uint16_t fromRGB24(uint32_t rgb24) {
        uint8_t r = ((rgb24 >> 16) & 0xFF) >> 4;  // Down-sample 8→4 bits
        uint8_t g = ((rgb24 >> 8) & 0xFF) >> 4;
        uint8_t b = (rgb24 & 0xFF) >> 4;
        return rgba(r, g, b, 15);
    }
    
    /// Convert 32-bit RGBA to 16-bit ARGB4444 (no dithering)
    /// @param rgba32 32-bit color (0xRRGGBBAA)
    /// @return 16-bit ARGB4444 color
    inline uint16_t fromRGBA32(uint32_t rgba32) {
        uint8_t r = ((rgba32 >> 24) & 0xFF) >> 4;
        uint8_t g = ((rgba32 >> 16) & 0xFF) >> 4;
        uint8_t b = ((rgba32 >> 8) & 0xFF) >> 4;
        uint8_t a = (rgba32 & 0xFF) >> 4;
        return rgba(r, g, b, a);
    }
    
    /// Convert 16-bit ARGB4444 to 32-bit RGBA (expand to 8-bit)
    /// @param color 16-bit ARGB4444 color
    /// @return 32-bit RGBA color (0xRRGGBBAA)
    inline uint32_t toRGBA32(uint16_t color) {
        uint8_t r = getRed(color);
        uint8_t g = getGreen(color);
        uint8_t b = getBlue(color);
        uint8_t a = getAlpha(color);
        
        // Expand 4-bit to 8-bit by duplicating high nibble
        uint8_t r8 = (r << 4) | r;
        uint8_t g8 = (g << 4) | g;
        uint8_t b8 = (b << 4) | b;
        uint8_t a8 = (a << 4) | a;
        
        return (r8 << 24) | (g8 << 16) | (b8 << 8) | a8;
    }
    
    /// Set alpha component of a color
    /// @param color Original 16-bit ARGB4444 color
    /// @param alpha New alpha value (0-15)
    /// @return Color with updated alpha
    inline uint16_t setAlpha(uint16_t color, uint8_t alpha) {
        return (color & 0x0FFF) | ((alpha & 0xF) << 12);
    }
    
    /// Blend two colors linearly
    /// @param c1 First color
    /// @param c2 Second color
    /// @param t Blend factor (0.0=c1, 1.0=c2)
    /// @return Blended color
    inline uint16_t blend(uint16_t c1, uint16_t c2, float t) {
        if (t <= 0.0f) return c1;
        if (t >= 1.0f) return c2;
        
        uint8_t r1 = getRed(c1), g1 = getGreen(c1), b1 = getBlue(c1), a1 = getAlpha(c1);
        uint8_t r2 = getRed(c2), g2 = getGreen(c2), b2 = getBlue(c2), a2 = getAlpha(c2);
        
        uint8_t r = static_cast<uint8_t>(r1 + (r2 - r1) * t);
        uint8_t g = static_cast<uint8_t>(g1 + (g2 - g1) * t);
        uint8_t b = static_cast<uint8_t>(b1 + (b2 - b1) * t);
        uint8_t a = static_cast<uint8_t>(a1 + (a2 - a1) * t);
        
        return rgba(r, g, b, a);
    }
}

} // namespace SuperTerminal

#endif // SUPERTERMINAL_URES_BUFFER_H