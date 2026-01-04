//
// LoResBuffer.cpp
// SuperTerminal v2
//
// Implementation of dynamic pixel buffer for chunky graphics modes.
// Chunky as in not planar, and also lo-res.

#include "LoResBuffer.h"
#include <algorithm>

namespace SuperTerminal {

LoResBuffer::LoResBuffer(int width, int height)
    : m_width(width)
    , m_height(height)
    , m_pixels(new uint8_t[width * height])
    , m_dirty(true)
{
    // Initialize all pixels to 0 (black)
    std::memset(m_pixels.get(), 0, m_width * m_height);
}

LoResBuffer::~LoResBuffer() {
    // unique_ptr handles cleanup automatically
}

void LoResBuffer::setPixel(int x, int y, uint8_t colorIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Bounds check
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return;
    }

    // Clamp color index to 0-15, set alpha to fully opaque (15)
    colorIndex &= 0x0F;

    // Set pixel with full alpha (0xF0 = alpha 15, fully opaque)
    int index = y * m_width + x;
    m_pixels[index] = (0xF0 | colorIndex);

    m_dirty = true;
}

void LoResBuffer::setPixelAlpha(int x, int y, uint8_t colorIndex, uint8_t alpha) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Bounds check
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return;
    }

    // Clamp color index and alpha to 0-15
    colorIndex &= 0x0F;
    alpha &= 0x0F;

    // Set pixel: upper 4 bits = alpha, lower 4 bits = color
    int index = y * m_width + x;
    m_pixels[index] = (alpha << 4) | colorIndex;

    m_dirty = true;
}

uint8_t LoResBuffer::getPixel(int x, int y) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Bounds check
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return 0;
    }

    // Get pixel color (lower 4 bits)
    int index = y * m_width + x;
    return m_pixels[index] & 0x0F;
}

uint8_t LoResBuffer::getPixelAlpha(int x, int y) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Bounds check
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return 15;  // Fully opaque by default
    }

    // Get pixel alpha (upper 4 bits)
    int index = y * m_width + x;
    return (m_pixels[index] >> 4) & 0x0F;
}

uint8_t LoResBuffer::getPixelRaw(int x, int y) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Bounds check
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return 0xF0;  // Fully opaque black
    }

    // Get full pixel value
    int index = y * m_width + x;
    return m_pixels[index];
}

void LoResBuffer::clear(uint8_t colorIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clamp color index and set full alpha
    colorIndex &= 0x0F;
    uint8_t pixelValue = 0xF0 | colorIndex;  // Alpha=15 (opaque), color=colorIndex

    // Fill all pixels
    std::memset(m_pixels.get(), pixelValue, m_width * m_height);

    m_dirty = true;
}

bool LoResBuffer::isDirty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_dirty;
}

void LoResBuffer::clearDirty() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dirty = false;
}

void LoResBuffer::resize(int width, int height) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Update dimensions
    m_width = width;
    m_height = height;

    // Reallocate buffer
    m_pixels.reset(new uint8_t[width * height]);

    // Clear to black
    std::memset(m_pixels.get(), 0, m_width * m_height);

    m_dirty = true;
}

void LoResBuffer::blit(int src_x, int src_y, int width, int height, int dst_x, int dst_y) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clamp source rectangle to buffer bounds
    if (src_x < 0) { width += src_x; dst_x -= src_x; src_x = 0; }
    if (src_y < 0) { height += src_y; dst_y -= src_y; src_y = 0; }
    if (src_x + width > m_width) { width = m_width - src_x; }
    if (src_y + height > m_height) { height = m_height - src_y; }

    // Clamp destination rectangle to buffer bounds
    if (dst_x < 0) { width += dst_x; src_x -= dst_x; dst_x = 0; }
    if (dst_y < 0) { height += dst_y; src_y -= dst_y; dst_y = 0; }
    if (dst_x + width > m_width) { width = m_width - dst_x; }
    if (dst_y + height > m_height) { height = m_height - dst_y; }

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
            int src_index = (src_y + y) * m_width + src_x;
            int dst_index = (dst_y + y) * m_width + dst_x;
            std::memmove(&m_pixels[dst_index], &m_pixels[src_index], width);
        }
    } else {
        // Copy top to bottom (normal or non-overlapping)
        for (int y = 0; y < height; ++y) {
            int src_index = (src_y + y) * m_width + src_x;
            int dst_index = (dst_y + y) * m_width + dst_x;
            std::memmove(&m_pixels[dst_index], &m_pixels[src_index], width);
        }
    }

    m_dirty = true;
}

void LoResBuffer::blitTransparent(int src_x, int src_y, int width, int height,
                                  int dst_x, int dst_y, uint8_t transparent_color) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clamp transparent color
    transparent_color &= 0x0F;

    // Clamp source rectangle to buffer bounds
    if (src_x < 0) { width += src_x; dst_x -= src_x; src_x = 0; }
    if (src_y < 0) { height += src_y; dst_y -= src_y; src_y = 0; }
    if (src_x + width > m_width) { width = m_width - src_x; }
    if (src_y + height > m_height) { height = m_height - src_y; }

    // Clamp destination rectangle to buffer bounds
    if (dst_x < 0) { width += dst_x; src_x -= dst_x; dst_x = 0; }
    if (dst_y < 0) { height += dst_y; src_y -= dst_y; dst_y = 0; }
    if (dst_x + width > m_width) { width = m_width - dst_x; }
    if (dst_y + height > m_height) { height = m_height - dst_y; }

    // Nothing to copy if dimensions are invalid
    if (width <= 0 || height <= 0) {
        return;
    }

    // Copy pixel by pixel, skipping transparent color
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int src_index = (src_y + y) * m_width + (src_x + x);
            uint8_t pixel = m_pixels[src_index] & 0x0F;

            if (pixel != transparent_color) {
                int dst_index = (dst_y + y) * m_width + (dst_x + x);
                m_pixels[dst_index] = pixel;
            }
        }
    }

    m_dirty = true;
}

void LoResBuffer::blitFrom(const LoResBuffer* src, int src_x, int src_y, int width, int height,
                           int dst_x, int dst_y) {
    if (!src) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    std::lock_guard<std::mutex> src_lock(src->m_mutex);

    // Clamp source rectangle to source buffer bounds
    if (src_x < 0) { width += src_x; dst_x -= src_x; src_x = 0; }
    if (src_y < 0) { height += src_y; dst_y -= src_y; src_y = 0; }
    if (src_x + width > src->m_width) { width = src->m_width - src_x; }
    if (src_y + height > src->m_height) { height = src->m_height - src_y; }

    // Clamp destination rectangle to this buffer bounds
    if (dst_x < 0) { width += dst_x; src_x -= dst_x; dst_x = 0; }
    if (dst_y < 0) { height += dst_y; src_y -= dst_y; dst_y = 0; }
    if (dst_x + width > m_width) { width = m_width - dst_x; }
    if (dst_y + height > m_height) { height = m_height - dst_y; }

    // Nothing to copy if dimensions are invalid
    if (width <= 0 || height <= 0) {
        return;
    }

    // Copy row by row
    for (int y = 0; y < height; ++y) {
        int src_index = (src_y + y) * src->m_width + src_x;
        int dst_index = (dst_y + y) * m_width + dst_x;
        std::memcpy(&m_pixels[dst_index], &src->m_pixels[src_index], width);
    }

    m_dirty = true;
}

void LoResBuffer::blitFromTransparent(const LoResBuffer* src, int src_x, int src_y, int width, int height,
                                      int dst_x, int dst_y, uint8_t transparent_color) {
    if (!src) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    std::lock_guard<std::mutex> src_lock(src->m_mutex);

    // Clamp transparent color
    transparent_color &= 0x0F;

    // Clamp source rectangle to source buffer bounds
    if (src_x < 0) { width += src_x; dst_x -= src_x; src_x = 0; }
    if (src_y < 0) { height += src_y; dst_y -= src_y; src_y = 0; }
    if (src_x + width > src->m_width) { width = src->m_width - src_x; }
    if (src_y + height > src->m_height) { height = src->m_height - src_y; }

    // Clamp destination rectangle to this buffer bounds
    if (dst_x < 0) { width += dst_x; src_x -= dst_x; dst_x = 0; }
    if (dst_y < 0) { height += dst_y; src_y -= dst_y; dst_y = 0; }
    if (dst_x + width > m_width) { width = m_width - dst_x; }
    if (dst_y + height > m_height) { height = m_height - dst_y; }

    // Nothing to copy if dimensions are invalid
    if (width <= 0 || height <= 0) {
        return;
    }

    // Copy pixel by pixel, skipping transparent color
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int src_index = (src_y + y) * src->m_width + (src_x + x);
            uint8_t pixel = src->m_pixels[src_index] & 0x0F;

            if (pixel != transparent_color) {
                int dst_index = (dst_y + y) * m_width + (dst_x + x);
                m_pixels[dst_index] = pixel;
            }
        }
    }

    m_dirty = true;
}

} // namespace SuperTerminal
