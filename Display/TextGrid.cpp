// TextGrid.cpp
// SuperTerminal v2.0 - Text Grid Display System Implementation
// This is the text mode layer, for editing etc.

#include "TextGrid.h"
#include "CharacterMapping.h"
#include <algorithm>
#include <sstream>
#include <codecvt>
#include <locale>

namespace SuperTerminal {

// =============================================================================
// Constructor / Destructor
// =============================================================================

TextGrid::TextGrid(int width, int height)
    : width_(width)
    , height_(height)
    , cells_(static_cast<size_t>(width) * static_cast<size_t>(height))
    , dirty_(true)
    , dirtyRows_(height, true)
    , dirtyRegionStart_(0)
    , dirtyRegionEnd_(height)
{
    // All cells initialized to default (space, white on black)
    // Mark as dirty initially so first render includes all content
}

TextGrid::~TextGrid() {
    // RAII: mutex and vector handle cleanup automatically
}

// =============================================================================
// Character Operations
// =============================================================================

void TextGrid::putChar(int x, int y, char32_t character, uint32_t foreground, uint32_t background) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isInBounds(x, y)) {
        return;  // Silently ignore out of bounds
    }

    // Map 8-bit ASCII (128-255) to Unicode box drawing characters
    char32_t mappedChar = character;
    if (character >= 128 && character <= 255) {
        mappedChar = CharacterMapping::mapAsciiToUnicode(static_cast<uint8_t>(character));
    }

    size_t index = indexAt(x, y);
    cells_[index].character = mappedChar;
    cells_[index].foreground = foreground;
    cells_[index].background = background;

    // Mark row as dirty
    markRowDirty(y);
}

void TextGrid::putString(int x, int y, const char* text, uint32_t foreground, uint32_t background) {
    if (!text) {
        return;
    }

    // Convert UTF-8 to UTF-32
    std::vector<char32_t> chars = utf8ToUtf32(text);

    std::lock_guard<std::mutex> lock(mutex_);

    int currentX = x;
    for (char32_t ch : chars) {
        if (!isInBounds(currentX, y)) {
            break;  // Stop at edge of screen
        }

        // Handle special characters
        if (ch == U'\n') {
            // Newline not supported in single-line putString
            // Could be extended in future
            break;
        } else if (ch == U'\t') {
            // Tab = 4 spaces
            for (int i = 0; i < 4 && isInBounds(currentX, y); ++i) {
                size_t index = indexAt(currentX, y);
                cells_[index].character = U' ';
                cells_[index].foreground = foreground;
                cells_[index].background = background;
                currentX++;
            }
        } else {
            // Normal character
            size_t index = indexAt(currentX, y);
            cells_[index].character = ch;
            cells_[index].foreground = foreground;
            cells_[index].background = background;
            currentX++;
        }
    }

    // Mark row as dirty
    markRowDirty(y);
}

Cell TextGrid::getCell(int x, int y) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isInBounds(x, y)) {
        return Cell();  // Return empty cell for out of bounds
    }

    return cells_[indexAt(x, y)];
}

// =============================================================================
// Grid Operations
// =============================================================================

void TextGrid::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Reset all cells to default
    for (auto& cell : cells_) {
        cell = Cell();
    }

    // Mark entire grid as dirty
    markDirty();
}

void TextGrid::clearRegion(int x, int y, int width, int height) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Clamp to grid bounds
    int startX = std::max(0, x);
    int startY = std::max(0, y);
    int endX = std::min(width_, x + width);
    int endY = std::min(height_, y + height);

    for (int row = startY; row < endY; ++row) {
        for (int col = startX; col < endX; ++col) {
            cells_[indexAt(col, row)] = Cell();
        }
        // Mark each modified row as dirty
        markRowDirty(row);
    }
}

void TextGrid::scroll(int lines) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (lines == 0) {
        return;
    }

    if (lines > 0) {
        // Scroll up (move content up, clear bottom)
        int scrollLines = std::min(lines, height_);

        // Move rows up
        for (int row = 0; row < height_ - scrollLines; ++row) {
            for (int col = 0; col < width_; ++col) {
                cells_[indexAt(col, row)] = cells_[indexAt(col, row + scrollLines)];
            }
        }

        // Clear bottom rows
        for (int row = height_ - scrollLines; row < height_; ++row) {
            for (int col = 0; col < width_; ++col) {
                cells_[indexAt(col, row)] = Cell();
            }
        }
    } else {
        // Scroll down (move content down, clear top)
        int scrollLines = std::min(-lines, height_);

        // Move rows down
        for (int row = height_ - 1; row >= scrollLines; --row) {
            for (int col = 0; col < width_; ++col) {
                cells_[indexAt(col, row)] = cells_[indexAt(col, row - scrollLines)];
            }
        }

        // Clear top rows
        for (int row = 0; row < scrollLines; ++row) {
            for (int col = 0; col < width_; ++col) {
                cells_[indexAt(col, row)] = Cell();
            }
        }
    }

    // Mark entire grid as dirty after scrolling
    markDirty();
}

void TextGrid::fillRegion(int x, int y, int width, int height,
                         char32_t character, uint32_t foreground, uint32_t background) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Clamp to grid bounds
    int startX = std::max(0, x);
    int startY = std::max(0, y);
    int endX = std::min(width_, x + width);
    int endY = std::min(height_, y + height);

    for (int row = startY; row < endY; ++row) {
        for (int col = startX; col < endX; ++col) {
            size_t index = indexAt(col, row);
            cells_[index].character = character;
            cells_[index].foreground = foreground;
            cells_[index].background = background;
        }
        // Mark each modified row as dirty
        markRowDirty(row);
    }
}

// =============================================================================
// Size and Configuration
// =============================================================================

void TextGrid::resize(int width, int height) {
    std::lock_guard<std::mutex> lock(mutex_);

    width_ = width;
    height_ = height;

    // Resize and clear
    cells_.clear();
    cells_.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

    // Reinitialize dirty tracking for new size
    dirtyRows_.clear();
    dirtyRows_.resize(height, true);
    dirty_ = true;
    dirtyRegionStart_ = 0;
    dirtyRegionEnd_ = height;
}

// =============================================================================
// Rendering
// =============================================================================

void TextGrid::render() {
    std::lock_guard<std::mutex> lock(mutex_);

    // TODO Phase 1 Week 1-2: Implement Metal rendering
    // This will be called by MetalRenderer to display the grid
    // For now, this is a placeholder that will be filled in when
    // we create the Metal shader and rendering pipeline
}

// =============================================================================
// Statistics and Debugging
// =============================================================================

size_t TextGrid::getNonEmptyCellCount() const {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t count = 0;
    for (const auto& cell : cells_) {
        if (!cell.isEmpty()) {
            count++;
        }
    }
    return count;
}

std::string TextGrid::toString() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream oss;

    for (int row = 0; row < height_; ++row) {
        for (int col = 0; col < width_; ++col) {
            const Cell& cell = cells_[indexAt(col, row)];

            // Convert UTF-32 to UTF-8 for output
            if (cell.character <= 0x7F) {
                // ASCII - direct output
                oss << static_cast<char>(cell.character);
            } else if (cell.character <= 0x7FF) {
                // 2-byte UTF-8
                oss << static_cast<char>(0xC0 | (cell.character >> 6));
                oss << static_cast<char>(0x80 | (cell.character & 0x3F));
            } else if (cell.character <= 0xFFFF) {
                // 3-byte UTF-8
                oss << static_cast<char>(0xE0 | (cell.character >> 12));
                oss << static_cast<char>(0x80 | ((cell.character >> 6) & 0x3F));
                oss << static_cast<char>(0x80 | (cell.character & 0x3F));
            } else {
                // 4-byte UTF-8
                oss << static_cast<char>(0xF0 | (cell.character >> 18));
                oss << static_cast<char>(0x80 | ((cell.character >> 12) & 0x3F));
                oss << static_cast<char>(0x80 | ((cell.character >> 6) & 0x3F));
                oss << static_cast<char>(0x80 | (cell.character & 0x3F));
            }
        }
        oss << '\n';
    }

    return oss.str();
}

// =============================================================================
// Private Helper Methods
// =============================================================================

std::vector<char32_t> TextGrid::utf8ToUtf32(const char* utf8) const {
    std::vector<char32_t> result;

    if (!utf8) {
        return result;
    }

    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(utf8);
    size_t i = 0;

    while (bytes[i] != 0) {
        char32_t codepoint = 0;

        if ((bytes[i] & 0x80) == 0) {
            // 1-byte character (ASCII)
            codepoint = bytes[i];
            i++;
        } else if ((bytes[i] & 0xE0) == 0xC0) {
            // 2-byte character
            if (bytes[i + 1] == 0) break;
            codepoint = ((bytes[i] & 0x1F) << 6) | (bytes[i + 1] & 0x3F);
            i += 2;
        } else if ((bytes[i] & 0xF0) == 0xE0) {
            // 3-byte character
            if (bytes[i + 1] == 0 || bytes[i + 2] == 0) break;
            codepoint = ((bytes[i] & 0x0F) << 12) |
                       ((bytes[i + 1] & 0x3F) << 6) |
                       (bytes[i + 2] & 0x3F);
            i += 3;
        } else if ((bytes[i] & 0xF8) == 0xF0) {
            // 4-byte character
            if (bytes[i + 1] == 0 || bytes[i + 2] == 0 || bytes[i + 3] == 0) break;
            codepoint = ((bytes[i] & 0x07) << 18) |
                       ((bytes[i + 1] & 0x3F) << 12) |
                       ((bytes[i + 2] & 0x3F) << 6) |
                       (bytes[i + 3] & 0x3F);
            i += 4;
        } else {
            // Invalid UTF-8, skip byte
            i++;
            continue;
        }

        result.push_back(codepoint);
    }

    return result;
}

} // namespace SuperTerminal
