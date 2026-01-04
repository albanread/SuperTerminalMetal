//
// WResBuffer.cpp
// SuperTerminal v2
//
// WRES (Wide Resolution) pixel buffer implementation
// Wide screen version of XRES. 256 colour low res mode.
// 432×240

#include "WResBuffer.h"
#include <algorithm>
#include <cstring>

namespace SuperTerminal {

WResBuffer::WResBuffer()
    : m_pixels(new uint8_t[PIXEL_COUNT])
    , m_dirty(true)
{
    // Initialize all pixels to 0 (transparent/background)
    std::memset(m_pixels.get(), 0, BUFFER_SIZE);
}

WResBuffer::~WResBuffer() {
    // Unique pointer handles cleanup
}

void WResBuffer::setPixel(int x, int y, uint8_t colorIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Bounds check
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return;
    }

    // Set pixel
    int index = y * WIDTH + x;
    m_pixels[index] = colorIndex;

    m_dirty = true;
}

uint8_t WResBuffer::getPixel(int x, int y) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Bounds check
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return 0;
    }

    // Get pixel
    int index = y * WIDTH + x;
    return m_pixels[index];
}

void WResBuffer::clear(uint8_t colorIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Fast fill using std::fill
    std::fill_n(m_pixels.get(), PIXEL_COUNT, colorIndex);

    m_dirty = true;
}

void WResBuffer::fillRect(int x, int y, int width, int height, uint8_t colorIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clip to buffer bounds
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > WIDTH) { width = WIDTH - x; }
    if (y + height > HEIGHT) { height = HEIGHT - y; }

    // Nothing to draw if dimensions are invalid
    if (width <= 0 || height <= 0) {
        return;
    }

    // Fill rectangle row by row
    for (int py = y; py < y + height; py++) {
        uint8_t* row = &m_pixels[py * WIDTH + x];
        std::fill_n(row, width, colorIndex);
    }

    m_dirty = true;
}

void WResBuffer::hline(int x, int y, int width, uint8_t colorIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Bounds check
    if (y < 0 || y >= HEIGHT) return;

    // Clip to buffer bounds
    if (x < 0) { width += x; x = 0; }
    if (x + width > WIDTH) { width = WIDTH - x; }

    if (width <= 0) return;

    // Fill horizontal line
    uint8_t* row = &m_pixels[y * WIDTH + x];
    std::fill_n(row, width, colorIndex);

    m_dirty = true;
}

void WResBuffer::vline(int x, int y, int height, uint8_t colorIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Bounds check
    if (x < 0 || x >= WIDTH) return;

    // Clip to buffer bounds
    if (y < 0) { height += y; y = 0; }
    if (y + height > HEIGHT) { height = HEIGHT - y; }

    if (height <= 0) return;

    // Fill vertical line
    for (int py = y; py < y + height; py++) {
        m_pixels[py * WIDTH + x] = colorIndex;
    }

    m_dirty = true;
}

void WResBuffer::blit(int src_x, int src_y, int width, int height, int dst_x, int dst_y) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clip source rectangle to buffer bounds
    if (src_x < 0) { width += src_x; dst_x -= src_x; src_x = 0; }
    if (src_y < 0) { height += src_y; dst_y -= src_y; src_y = 0; }
    if (src_x + width > WIDTH) { width = WIDTH - src_x; }
    if (src_y + height > HEIGHT) { height = HEIGHT - src_y; }

    // Clip destination rectangle to buffer bounds
    if (dst_x < 0) { width += dst_x; src_x -= dst_x; dst_x = 0; }
    if (dst_y < 0) { height += dst_y; src_y -= dst_y; dst_y = 0; }
    if (dst_x + width > WIDTH) { width = WIDTH - dst_x; }
    if (dst_y + height > HEIGHT) { height = HEIGHT - dst_y; }

    // Nothing to copy if dimensions are invalid
    if (width <= 0 || height <= 0) {
        return;
    }

    // Check for overlap and determine copy direction
    bool overlap = (src_y < dst_y + height && dst_y < src_y + height &&
                   src_x < dst_x + width && dst_x < src_x + width);

    if (overlap && dst_y > src_y) {
        // Copy bottom to top to avoid overwriting source
        for (int y = height - 1; y >= 0; --y) {
            uint8_t* src_row = &m_pixels[(src_y + y) * WIDTH + src_x];
            uint8_t* dst_row = &m_pixels[(dst_y + y) * WIDTH + dst_x];
            std::memmove(dst_row, src_row, width * sizeof(uint8_t));
        }
    } else {
        // Copy top to bottom (normal or non-overlapping)
        for (int y = 0; y < height; ++y) {
            uint8_t* src_row = &m_pixels[(src_y + y) * WIDTH + src_x];
            uint8_t* dst_row = &m_pixels[(dst_y + y) * WIDTH + dst_x];
            std::memmove(dst_row, src_row, width * sizeof(uint8_t));
        }
    }

    m_dirty = true;
}

void WResBuffer::blitTransparent(int src_x, int src_y, int width, int height,
                                 int dst_x, int dst_y) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clip source rectangle to buffer bounds
    if (src_x < 0) { width += src_x; dst_x -= src_x; src_x = 0; }
    if (src_y < 0) { height += src_y; dst_y -= src_y; src_y = 0; }
    if (src_x + width > WIDTH) { width = WIDTH - src_x; }
    if (src_y + height > HEIGHT) { height = HEIGHT - src_y; }

    // Clip destination rectangle to buffer bounds
    if (dst_x < 0) { width += dst_x; src_x -= dst_x; dst_x = 0; }
    if (dst_y < 0) { height += dst_y; src_y -= dst_y; dst_y = 0; }
    if (dst_x + width > WIDTH) { width = WIDTH - dst_x; }
    if (dst_y + height > HEIGHT) { height = HEIGHT - dst_y; }

    // Nothing to copy if dimensions are invalid
    if (width <= 0 || height <= 0) {
        return;
    }

    // Copy pixel by pixel, skipping transparent (0) pixels
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int src_index = (src_y + y) * WIDTH + (src_x + x);
            uint8_t pixel = m_pixels[src_index];

            // Skip transparent (color 0)
            if (pixel != 0) {
                int dst_index = (dst_y + y) * WIDTH + (dst_x + x);
                m_pixels[dst_index] = pixel;
            }
        }
    }

    m_dirty = true;
}

void WResBuffer::blitFrom(const WResBuffer* src, int src_x, int src_y, int width, int height,
                         int dst_x, int dst_y) {
    if (!src) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    std::lock_guard<std::mutex> src_lock(src->m_mutex);

    // Clip source rectangle to source buffer bounds
    if (src_x < 0) { width += src_x; dst_x -= src_x; src_x = 0; }
    if (src_y < 0) { height += src_y; dst_y -= src_y; src_y = 0; }
    if (src_x + width > WIDTH) { width = WIDTH - src_x; }
    if (src_y + height > HEIGHT) { height = HEIGHT - src_y; }

    // Clip destination rectangle to this buffer bounds
    if (dst_x < 0) { width += dst_x; src_x -= dst_x; dst_x = 0; }
    if (dst_y < 0) { height += dst_y; src_y -= dst_y; dst_y = 0; }
    if (dst_x + width > WIDTH) { width = WIDTH - dst_x; }
    if (dst_y + height > HEIGHT) { height = HEIGHT - dst_y; }

    // Nothing to copy if dimensions are invalid
    if (width <= 0 || height <= 0) {
        return;
    }

    // Copy row by row
    for (int y = 0; y < height; ++y) {
        const uint8_t* src_row = &src->m_pixels[(src_y + y) * WIDTH + src_x];
        uint8_t* dst_row = &m_pixels[(dst_y + y) * WIDTH + dst_x];
        std::memcpy(dst_row, src_row, width * sizeof(uint8_t));
    }

    m_dirty = true;
}

void WResBuffer::blitFromTransparent(const WResBuffer* src, int src_x, int src_y, int width, int height,
                                     int dst_x, int dst_y) {
    if (!src) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    std::lock_guard<std::mutex> src_lock(src->m_mutex);

    // Clip source rectangle to source buffer bounds
    if (src_x < 0) { width += src_x; dst_x -= src_x; src_x = 0; }
    if (src_y < 0) { height += src_y; dst_y -= src_y; src_y = 0; }
    if (src_x + width > WIDTH) { width = WIDTH - src_x; }
    if (src_y + height > HEIGHT) { height = HEIGHT - src_y; }

    // Clip destination rectangle to this buffer bounds
    if (dst_x < 0) { width += dst_x; src_x -= dst_x; dst_x = 0; }
    if (dst_y < 0) { height += dst_y; src_y -= dst_y; dst_y = 0; }
    if (dst_x + width > WIDTH) { width = WIDTH - dst_x; }
    if (dst_y + height > HEIGHT) { height = HEIGHT - dst_y; }

    // Nothing to copy if dimensions are invalid
    if (width <= 0 || height <= 0) {
        return;
    }

    // Copy pixel by pixel, skipping transparent (0) pixels
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int src_index = (src_y + y) * WIDTH + (src_x + x);
            uint8_t pixel = src->m_pixels[src_index];

            // Skip transparent (color 0)
            if (pixel != 0) {
                int dst_index = (dst_y + y) * WIDTH + (dst_x + x);
                m_pixels[dst_index] = pixel;
            }
        }
    }

    m_dirty = true;
}

bool WResBuffer::isDirty() const {
    // NOTE: Caller must hold mutex lock for consistency
    // This is called from rendering code that needs atomic isDirty+clearDirty
    return m_dirty;
}

void WResBuffer::clearDirty() {
    // NOTE: Caller must hold mutex lock
    // This function is called from rendering code that already holds the lock
    m_dirty = false;
}

void WResBuffer::clipRect(int& x, int& y, int& width, int& height) const {
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > WIDTH) { width = WIDTH - x; }
    if (y + height > HEIGHT) { height = HEIGHT - y; }
    if (width < 0) width = 0;
    if (height < 0) height = 0;
}

} // namespace SuperTerminal
