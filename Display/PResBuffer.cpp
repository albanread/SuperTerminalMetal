//
// PResBuffer.cpp
// SuperTerminal v2
//
// Implementation of PRES mode pixel buffer for 1280×720 graphics with 256-color palette.
// Indexed colours into mixed palette. See PResPalletteManager.

#include "PResBuffer.h"
#include <algorithm>
#include <cstring>

namespace SuperTerminal {

PResBuffer::PResBuffer()
    : m_pixels(new uint8_t[PIXEL_COUNT])
    , m_dirty(true)
{
    // Initialize all pixels to 0 (transparent/background)
    std::memset(m_pixels.get(), 0, BUFFER_SIZE);
}

PResBuffer::~PResBuffer() {
    // unique_ptr handles cleanup automatically
}

void PResBuffer::setPixel(int x, int y, uint8_t colorIndex) {
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

uint8_t PResBuffer::getPixel(int x, int y) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Bounds check
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return 0;
    }

    // Get pixel
    int index = y * WIDTH + x;
    return m_pixels[index];
}

void PResBuffer::clear(uint8_t colorIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Fast fill using std::fill
    std::fill_n(m_pixels.get(), PIXEL_COUNT, colorIndex);

    m_dirty = true;
}

void PResBuffer::fillRect(int x, int y, int width, int height, uint8_t colorIndex) {
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

void PResBuffer::hline(int x, int y, int width, uint8_t colorIndex) {
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

void PResBuffer::vline(int x, int y, int height, uint8_t colorIndex) {
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

void PResBuffer::circle(int cx, int cy, int radius, uint8_t colorIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (radius < 0) return;

    // Bresenham circle algorithm with filled scanlines
    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y) {
        // Draw horizontal lines for each octant
        // Top and bottom halves
        for (int i = cx - x; i <= cx + x; i++) {
            if (i >= 0 && i < WIDTH) {
                if (cy + y >= 0 && cy + y < HEIGHT) {
                    m_pixels[(cy + y) * WIDTH + i] = colorIndex;
                }
                if (cy - y >= 0 && cy - y < HEIGHT && y != 0) {
                    m_pixels[(cy - y) * WIDTH + i] = colorIndex;
                }
            }
        }

        // Left and right sides
        if (x != y) {
            for (int i = cx - y; i <= cx + y; i++) {
                if (i >= 0 && i < WIDTH) {
                    if (cy + x >= 0 && cy + x < HEIGHT) {
                        m_pixels[(cy + x) * WIDTH + i] = colorIndex;
                    }
                    if (cy - x >= 0 && cy - x < HEIGHT) {
                        m_pixels[(cy - x) * WIDTH + i] = colorIndex;
                    }
                }
            }
        }

        y += 1;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0) {
            x -= 1;
            err += 1 - 2 * x;
        }
    }

    m_dirty = true;
}

void PResBuffer::line(int x0, int y0, int x1, int y1, uint8_t colorIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Bresenham line algorithm
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        // Set pixel if within bounds
        if (x0 >= 0 && x0 < WIDTH && y0 >= 0 && y0 < HEIGHT) {
            m_pixels[y0 * WIDTH + x0] = colorIndex;
        }

        // Check if we've reached the end
        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }

    m_dirty = true;
}

void PResBuffer::blit(int src_x, int src_y, int width, int height, int dst_x, int dst_y) {
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

void PResBuffer::blitTransparent(int src_x, int src_y, int width, int height,
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

void PResBuffer::blitFrom(const PResBuffer* src, int src_x, int src_y, int width, int height,
                          int dst_x, int dst_y) {
    if (!src) return;

    // Lock both buffers (lock this first to avoid deadlock)
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

void PResBuffer::blitFromTransparent(const PResBuffer* src, int src_x, int src_y, int width, int height,
                                     int dst_x, int dst_y) {
    if (!src) return;

    // Lock both buffers (lock this first to avoid deadlock)
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

bool PResBuffer::isDirty() const {
    return m_dirty;
}

void PResBuffer::clearDirty() {
    m_dirty = false;
}

void PResBuffer::clipRect(int& x, int& y, int& width, int& height) const {
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > WIDTH) { width = WIDTH - x; }
    if (y + height > HEIGHT) { height = HEIGHT - y; }

    if (width < 0) width = 0;
    if (height < 0) height = 0;
}

} // namespace SuperTerminal
