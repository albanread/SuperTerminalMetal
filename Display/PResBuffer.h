//
// PResBuffer.h
// SuperTerminal v2
//
// PRES (Premium Resolution) pixel buffer for 1280×720 graphics with 256-color palette.
// Combines URES resolution with XRES palette capabilities.
//
// THREAD SAFETY:
// - All public methods are thread-safe
// - Internal state is protected by mutex
//

#ifndef SUPERTERMINAL_PRES_BUFFER_H
#define SUPERTERMINAL_PRES_BUFFER_H

#include <cstdint>
#include <mutex>
#include <memory>

namespace SuperTerminal {

/// PResBuffer: 1280×720 pixel buffer with 256-color palette (Premium Resolution)
///
/// Responsibilities:
/// - Store pixels at 1280×720 resolution (16:9 aspect ratio)
/// - Each pixel holds 8-bit palette index (0-255)
/// - Provide fast pixel read/write access
/// - Track dirty state for efficient rendering
/// - Thread-safe access for drawing
/// - Support 8 buffers for double buffering and sprite atlas
///
/// Resolution:
/// - PRES: 1280×720 pixels (921,600 pixels)
/// - Memory: 921,600 bytes (~900 KB per buffer)
/// - Total for 8 buffers: 7,372,800 bytes (~7 MB)
///
/// Pixel Format: 8-bit palette index
/// - 8-bit per pixel (1 byte)
/// - Value 0-255: Index into palette
/// - colour 0 is transparent to show lower layers.
/// - Palette structure: Mixed shared and per row.
///   - Colors 0-15: Per-row palette (720 rows × 16 colors = 11,520 entries)
///   - Colors 16-255: Global palette (240 colors, shared across all rows)
///
/// Palette Capabilities:
/// - Per-row colors (0-15): Classic raster effects, palette cycling, gradients
/// - Global colors (16-255): Sprites, UI, detailed artwork
/// - Total unique colors: 11,520 + 240 = 11,760 palette entries
///
/// Memory Layout:
/// - Format: uint8_t array (1 byte per pixel)
/// - Row-major order: pixels[y * width + x]
/// - Alignment: 16-byte aligned for SIMD operations
///
/// Multiple Buffers:
/// - Buffer 0: Front buffer (displayed)
/// - Buffer 1: Back buffer (drawing target, can flip with buffer 0)
/// - Buffers 2-7: Atlas/scratch space (sprites, tiles, temp storage)
class PResBuffer {
public:
    /// Resolution constants
    static constexpr int WIDTH = 1280;
    static constexpr int HEIGHT = 720;
    static constexpr int PIXEL_COUNT = WIDTH * HEIGHT;
    static constexpr size_t BUFFER_SIZE = PIXEL_COUNT * sizeof(uint8_t);

    /// Create a new PResBuffer
    /// Initializes all pixels to 0 (transparent/background)
    PResBuffer();

    /// Destructor
    ~PResBuffer();

    // Disable copy and move (due to mutex)
    PResBuffer(const PResBuffer&) = delete;
    PResBuffer& operator=(const PResBuffer&) = delete;
    PResBuffer(PResBuffer&&) = delete;
    PResBuffer& operator=(PResBuffer&&) = delete;

    /// Set a pixel color (8-bit palette index)
    /// @param x X coordinate (0-1279)
    /// @param y Y coordinate (0-719)
    /// @param colorIndex 8-bit palette index (0-255)
    /// @note Thread Safety: Safe to call from any thread
    /// @note Out of bounds coordinates are ignored
    void setPixel(int x, int y, uint8_t colorIndex);

    /// Get a pixel color
    /// @param x X coordinate (0-1279)
    /// @param y Y coordinate (0-719)
    /// @return 8-bit palette index, or 0 if out of bounds
    /// @note Thread Safety: Safe to call from any thread
    uint8_t getPixel(int x, int y) const;

    /// Clear all pixels to a specific color
    /// @param colorIndex 8-bit palette index to fill
    /// @note Thread Safety: Safe to call from any thread
    void clear(uint8_t colorIndex = 0);

    /// Fill a rectangular region with a color
    /// @param x X coordinate of top-left corner
    /// @param y Y coordinate of top-left corner
    /// @param width Width in pixels
    /// @param height Height in pixels
    /// @param colorIndex 8-bit palette index to fill
    /// @note Thread Safety: Safe to call from any thread
    /// @note Clips to buffer bounds automatically
    void fillRect(int x, int y, int width, int height, uint8_t colorIndex);

    /// Draw a horizontal line
    /// @param x X coordinate of start point
    /// @param y Y coordinate
    /// @param width Length of line in pixels
    /// @param colorIndex 8-bit palette index
    /// @note Thread Safety: Safe to call from any thread
    void hline(int x, int y, int width, uint8_t colorIndex);

    /// Draw a vertical line
    /// @param x X coordinate
    /// @param y Y coordinate of start point
    /// @param height Length of line in pixels
    /// @param colorIndex 8-bit palette index
    /// @note Thread Safety: Safe to call from any thread
    void vline(int x, int y, int height, uint8_t colorIndex);

    /// Draw a filled circle using Bresenham algorithm
    /// @param cx Center X coordinate
    /// @param cy Center Y coordinate
    /// @param radius Radius in pixels
    /// @param colorIndex 8-bit palette index
    /// @note Thread Safety: Safe to call from any thread
    void circle(int cx, int cy, int radius, uint8_t colorIndex);

    /// Draw a line using Bresenham algorithm
    /// @param x0 Start X coordinate
    /// @param y0 Start Y coordinate
    /// @param x1 End X coordinate
    /// @param y1 End Y coordinate
    /// @param colorIndex 8-bit palette index
    /// @note Thread Safety: Safe to call from any thread
    void line(int x0, int y0, int x1, int y1, uint8_t colorIndex);

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

    /// Copy rectangular region with transparency (skip color 0)
    /// @param src_x Source X coordinate
    /// @param src_y Source Y coordinate
    /// @param width Width in pixels
    /// @param height Height in pixels
    /// @param dst_x Destination X coordinate
    /// @param dst_y Destination Y coordinate
    /// @note Thread Safety: Safe to call from any thread
    /// @note Pixels with value 0 (transparent) are not copied
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
    void blitFrom(const PResBuffer* src, int src_x, int src_y, int width, int height,
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
    /// @note Pixels with value 0 are not copied
    void blitFromTransparent(const PResBuffer* src, int src_x, int src_y, int width, int height,
                             int dst_x, int dst_y);

    /// Get raw pixel data for rendering
    /// @return Pointer to pixel buffer (921,600 bytes)
    /// @note Thread Safety: Caller must hold lock via getMutex()
    /// @note Data format: uint8_t[1280*720] in row-major order
    const uint8_t* getPixelData() const { return m_pixels.get(); }

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
    /// @note Thread Safety: Caller should hold lock for consistency
    bool isDirty() const;

    /// Clear the dirty flag (call after rendering)
    /// @note Thread Safety: Caller must hold lock
    void clearDirty();

    /// Get mutex for external synchronization (e.g., during rendering)
    /// @return Reference to internal mutex
    std::mutex& getMutex() const { return m_mutex; }

private:
    /// Pixel storage: 1280×720 uint8_t array
    /// Each pixel stores 8-bit palette index (0-255)
    /// Memory: 921,600 bytes (~900 KB)
    std::unique_ptr<uint8_t[]> m_pixels;

    /// Dirty flag for tracking changes
    bool m_dirty;

    /// Mutex for thread-safe access
    mutable std::mutex m_mutex;

    /// Helper: Clip rectangle to buffer bounds
    void clipRect(int& x, int& y, int& width, int& height) const;
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_PRES_BUFFER_H
