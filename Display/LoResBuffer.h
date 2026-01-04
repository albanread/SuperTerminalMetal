//
// LoResBuffer.h
// SuperTerminal v2
//
// Dedicated pixel buffer for chunky graphics modes.
// Supports LORES (160×75), MIDRES (320×150), and HIRES (640×300).
// 16 colour, but also - paletter per line :)
//
// THREAD SAFETY:
// - All public methods are thread-safe
// - Internal state is protected by mutex
//

#ifndef SUPERTERMINAL_LORES_BUFFER_H
#define SUPERTERMINAL_LORES_BUFFER_H

#include <cstdint>
#include <mutex>
#include <cstring>

namespace SuperTerminal {

/// LoResBuffer: Dynamic pixel buffer for chunky graphics modes
///
/// Responsibilities:
/// - Store pixels at various resolutions (LORES/MIDRES/HIRES)
/// - Each pixel holds a 4-bit color index (0-15)
/// - Provide fast pixel read/write access
/// - Track dirty state for efficient rendering
/// - Thread-safe access for drawing
///
/// Supported Resolutions:
/// - LORES: 160×75 pixels (12,000 bytes = 12 KB)
/// - MIDRES: 320×150 pixels (48,000 bytes = 48 KB)
/// - HIRES: 640×300 pixels (192,000 bytes = 192 KB)
///
/// Memory Layout:
/// - Format: 1 byte per pixel
///   - Lower 4 bits: color index (0-15)
///   - Upper 4 bits: alpha value (0-15, where 15=opaque, 0=transparent)
/// - Row-major order: pixels[y * width + x]
class LoResBuffer {
public:
    /// Resolution mode constants
    static constexpr int LORES_WIDTH = 160;
    static constexpr int LORES_HEIGHT = 75;
    static constexpr int MIDRES_WIDTH = 320;
    static constexpr int MIDRES_HEIGHT = 150;
    static constexpr int HIRES_WIDTH = 640;
    static constexpr int HIRES_HEIGHT = 300;
    static constexpr int MAX_PIXELS = HIRES_WIDTH * HIRES_HEIGHT;

    /// Create a new LoResBuffer with specified resolution
    /// @param width Width in pixels (default: LORES 160)
    /// @param height Height in pixels (default: LORES 75)
    /// Initializes all pixels to 0 (black)
    LoResBuffer(int width = LORES_WIDTH, int height = LORES_HEIGHT);

    /// Destructor
    ~LoResBuffer();

    // Disable copy and move (due to mutex)
    LoResBuffer(const LoResBuffer&) = delete;
    LoResBuffer& operator=(const LoResBuffer&) = delete;
    LoResBuffer(LoResBuffer&&) = delete;
    LoResBuffer& operator=(LoResBuffer&&) = delete;

    /// Set a pixel color
    /// @param x X coordinate (0 to width-1)
    /// @param y Y coordinate (0 to height-1)
    /// @param colorIndex Color index (0-15, will be clamped)
    /// @note Thread Safety: Safe to call from any thread
    void setPixel(int x, int y, uint8_t colorIndex);

    /// Set a pixel with color and alpha
    /// @param x X coordinate (0 to width-1)
    /// @param y Y coordinate (0 to height-1)
    /// @param colorIndex Color index (0-15, will be clamped)
    /// @param alpha Alpha value (0-15, where 15=opaque, 0=transparent)
    /// @note Thread Safety: Safe to call from any thread
    void setPixelAlpha(int x, int y, uint8_t colorIndex, uint8_t alpha);

    /// Get a pixel color
    /// @param x X coordinate (0 to width-1)
    /// @param y Y coordinate (0 to height-1)
    /// @return Color index (0-15), or 0 if out of bounds
    /// @note Thread Safety: Safe to call from any thread
    uint8_t getPixel(int x, int y) const;

    /// Get a pixel's alpha value
    /// @param x X coordinate (0 to width-1)
    /// @param y Y coordinate (0 to height-1)
    /// @return Alpha value (0-15), or 15 if out of bounds
    /// @note Thread Safety: Safe to call from any thread
    uint8_t getPixelAlpha(int x, int y) const;

    /// Get full pixel data (color + alpha)
    /// @param x X coordinate (0 to width-1)
    /// @param y Y coordinate (0 to height-1)
    /// @return Full 8-bit pixel value (upper 4 bits=alpha, lower 4 bits=color)
    /// @note Thread Safety: Safe to call from any thread
    uint8_t getPixelRaw(int x, int y) const;

    /// Clear all pixels to a specific color
    /// @param colorIndex Color index to fill (0-15)
    /// @note Thread Safety: Safe to call from any thread
    void clear(uint8_t colorIndex = 0);

    /// Get raw pixel data for rendering
    /// @return Pointer to pixel buffer
    /// @note Thread Safety: Caller must hold lock via getMutex()
    const uint8_t* getPixelData() const { return m_pixels.get(); }

    /// Get buffer dimensions
    /// @param width Output width
    /// @param height Output height
    void getSize(int& width, int& height) const {
        width = m_width;
        height = m_height;
    }

    /// Get buffer width
    /// @return Current width in pixels
    int getWidth() const { return m_width; }

    /// Get buffer height
    /// @return Current height in pixels
    int getHeight() const { return m_height; }

    /// Resize the buffer to new dimensions
    /// @param width New width in pixels
    /// @param height New height in pixels
    /// @note Clears all pixel data
    void resize(int width, int height);

    /// Copy rectangular region within this buffer
    /// @param src_x Source X coordinate
    /// @param src_y Source Y coordinate
    /// @param width Width in pixels
    /// @param height Height in pixels
    /// @param dst_x Destination X coordinate
    /// @param dst_y Destination Y coordinate
    /// @note Thread Safety: Safe to call from any thread
    void blit(int src_x, int src_y, int width, int height, int dst_x, int dst_y);

    /// Copy rectangular region with transparency (cookie-cut blitting)
    /// @param src_x Source X coordinate
    /// @param src_y Source Y coordinate
    /// @param width Width in pixels
    /// @param height Height in pixels
    /// @param dst_x Destination X coordinate
    /// @param dst_y Destination Y coordinate
    /// @param transparent_color Color index to skip (0-15)
    /// @note Thread Safety: Safe to call from any thread
    void blitTransparent(int src_x, int src_y, int width, int height,
                         int dst_x, int dst_y, uint8_t transparent_color);

    /// Copy rectangular region from another buffer
    /// @param src Source buffer
    /// @param src_x Source X coordinate
    /// @param src_y Source Y coordinate
    /// @param width Width in pixels
    /// @param height Height in pixels
    /// @param dst_x Destination X coordinate
    /// @param dst_y Destination Y coordinate
    /// @note Thread Safety: Safe to call from any thread
    void blitFrom(const LoResBuffer* src, int src_x, int src_y, int width, int height,
                  int dst_x, int dst_y);

    /// Copy rectangular region from another buffer with transparency
    /// @param src Source buffer
    /// @param src_x Source X coordinate
    /// @param src_y Source Y coordinate
    /// @param width Width in pixels
    /// @param height Height in pixels
    /// @param dst_x Destination X coordinate
    /// @param dst_y Destination Y coordinate
    /// @param transparent_color Color index to skip (0-15)
    /// @note Thread Safety: Safe to call from any thread
    void blitFromTransparent(const LoResBuffer* src, int src_x, int src_y, int width, int height,
                             int dst_x, int dst_y, uint8_t transparent_color);

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
    /// Current buffer dimensions
    int m_width;
    int m_height;

    /// Pixel storage: dynamically allocated
    /// Each pixel stores color index (0-15) in lower 4 bits
    std::unique_ptr<uint8_t[]> m_pixels;

    /// Dirty flag for tracking changes
    bool m_dirty;

    /// Mutex for thread-safe access
    mutable std::mutex m_mutex;
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_LORES_BUFFER_H
