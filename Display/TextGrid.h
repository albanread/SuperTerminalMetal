// TextGrid.h
// SuperTerminal v2.0 - Text Grid Display System
//

#ifndef SUPERTERMINAL_TEXTGRID_H
#define SUPERTERMINAL_TEXTGRID_H

#include <vector>
#include <mutex>
#include <cstdint>
#include <string>

namespace SuperTerminal {

// Forward declarations
class MetalRenderer;

/// @brief A single character cell in the text grid
struct Cell {
    char32_t character;      ///< UTF-32 character code point
    uint32_t foreground;     ///< RGBA foreground color
    uint32_t background;     ///< RGBA background color

    /// Default constructor - empty cell with default colors
    Cell()
        : character(U' ')
        , foreground(0xFFFFFFFF)  // White
        , background(0x00000000)  // Transparent (allows tilemaps/graphics to show through)
    {}

    /// Constructor with values
    Cell(char32_t ch, uint32_t fg, uint32_t bg)
        : character(ch)
        , foreground(fg)
        , background(bg)
    {}

    /// Check if cell is empty (space with default colors)
    bool isEmpty() const {
        return character == U' ' &&
               foreground == 0xFFFFFFFF &&
               background == 0x000000FF;
    }
};

/// @brief Text grid for character-based rendering
///
/// TextGrid manages a 2D grid of character cells, each with independent
/// foreground and background colors. It provides thread-safe operations
/// for modifying and rendering the grid.
///
/// Design principles:
/// - Thread-safe: All public methods use mutex protection
/// - Immediate mode: Changes visible on next render
/// - Unicode support: Full UTF-32 character support
/// - Configurable size: Can be resized dynamically
class TextGrid {
public:
    /// Constructor
    /// @param width Grid width in characters (default 80)
    /// @param height Grid height in characters (default 25)
    TextGrid(int width = 80, int height = 25);

    /// Destructor
    ~TextGrid();

    // Disable copy and move (mutex is not movable)
    TextGrid(const TextGrid&) = delete;
    TextGrid& operator=(const TextGrid&) = delete;
    TextGrid(TextGrid&&) = delete;
    TextGrid& operator=(TextGrid&&) = delete;

    // =========================================================================
    // Character Operations
    // =========================================================================

    /// Put a single character at position
    /// @param x Column position (0-based)
    /// @param y Row position (0-based)
    /// @param character UTF-32 character to display
    /// @param foreground Foreground color (RGBA)
    /// @param background Background color (RGBA)
    void putChar(int x, int y, char32_t character, uint32_t foreground, uint32_t background);

    /// Put a UTF-8 string at position
    /// @param x Column position (0-based)
    /// @param y Row position (0-based)
    /// @param text UTF-8 encoded string
    /// @param foreground Foreground color (RGBA)
    /// @param background Background color (RGBA)
    void putString(int x, int y, const char* text, uint32_t foreground, uint32_t background);

    /// Get character at position
    /// @param x Column position (0-based)
    /// @param y Row position (0-based)
    /// @return Cell at position, or empty cell if out of bounds
    Cell getCell(int x, int y) const;

    // =========================================================================
    // Grid Operations
    // =========================================================================

    /// Clear entire grid (fill with spaces and default colors)
    void clear();

    /// Clear a rectangular region
    /// @param x Starting column
    /// @param y Starting row
    /// @param width Region width
    /// @param height Region height
    void clearRegion(int x, int y, int width, int height);

    /// Scroll the grid vertically
    /// @param lines Number of lines to scroll (positive = up, negative = down)
    void scroll(int lines);

    /// Fill region with character and colors
    /// @param x Starting column
    /// @param y Starting row
    /// @param width Region width
    /// @param height Region height
    /// @param character Character to fill with
    /// @param foreground Foreground color
    /// @param background Background color
    void fillRegion(int x, int y, int width, int height,
                   char32_t character, uint32_t foreground, uint32_t background);

    // =========================================================================
    // Size and Configuration
    // =========================================================================

    /// Resize the grid (clears content)
    /// @param width New width in characters
    /// @param height New height in characters
    void resize(int width, int height);

    /// Get grid width
    /// @return Width in characters
    int getWidth() const { return width_; }

    /// Get grid height
    /// @return Height in characters
    int getHeight() const { return height_; }

    /// Check if coordinates are in bounds
    /// @param x Column position
    /// @param y Row position
    /// @return true if position is valid
    bool isInBounds(int x, int y) const {
        return x >= 0 && x < width_ && y >= 0 && y < height_;
    }

    // =========================================================================
    // Rendering
    // =========================================================================

    /// Render the text grid
    /// This is called by the rendering system to display the grid
    /// @note Thread-safe, can be called from render thread
    void render();

    /// Get direct access to cell buffer (for rendering)
    /// @warning Only call from render thread with proper locking
    /// @return Const reference to cell buffer
    const std::vector<Cell>& getCells() const { return cells_; }

    /// Lock the grid for batch operations
    /// @return Unique lock that will automatically unlock when destroyed
    std::unique_lock<std::mutex> lockForBatchOperation() {
        return std::unique_lock<std::mutex>(mutex_);
    }

    // =========================================================================
    // Statistics and Debugging
    // =========================================================================

    /// Get number of non-empty cells
    /// @return Count of cells that are not default space
    size_t getNonEmptyCellCount() const;

    /// Get total cell count
    /// @return Total number of cells (width * height)
    size_t getTotalCellCount() const {
        return cells_.size();
    }

    /// Dump grid contents to string (for debugging)
    /// @return String representation of grid
    std::string toString() const;

    // =========================================================================
    // Dirty Tracking (Performance Optimization)
    // =========================================================================

    /// Check if grid has been modified since last render
    /// @return true if grid has pending changes
    bool isDirty() const { return dirty_; }

    /// Mark entire grid as dirty (needs re-render)
    void markDirty() {
        dirty_ = true;
        dirtyRegionStart_ = 0;
        dirtyRegionEnd_ = height_;
    }

    /// Mark a specific row as dirty
    /// @param row Row index to mark dirty
    void markRowDirty(int row) {
        if (row >= 0 && row < height_) {
            dirty_ = true;
            dirtyRows_[row] = true;
            dirtyRegionStart_ = std::min(dirtyRegionStart_, row);
            dirtyRegionEnd_ = std::max(dirtyRegionEnd_, row + 1);
        }
    }

    /// Clear dirty flag (called after successful render)
    void clearDirty() {
        dirty_ = false;
        std::fill(dirtyRows_.begin(), dirtyRows_.end(), false);
        dirtyRegionStart_ = height_;
        dirtyRegionEnd_ = 0;
    }

    /// Get dirty region for partial updates
    /// @param startRow Output: first dirty row
    /// @param endRow Output: last dirty row (exclusive)
    void getDirtyRegion(int& startRow, int& endRow) const {
        startRow = dirtyRegionStart_;
        endRow = dirtyRegionEnd_;
    }

    /// Check if specific row is dirty
    /// @param row Row index to check
    /// @return true if row has been modified
    bool isRowDirty(int row) const {
        return row >= 0 && row < height_ && dirtyRows_[row];
    }

private:
    // =========================================================================
    // Private Helper Methods
    // =========================================================================

    /// Convert (x, y) to linear index
    /// @param x Column position
    /// @param y Row position
    /// @return Linear index into cells_ array
    inline size_t indexAt(int x, int y) const {
        return static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x);
    }

    /// Convert UTF-8 string to UTF-32
    /// @param utf8 Input UTF-8 string
    /// @return Vector of UTF-32 code points
    std::vector<char32_t> utf8ToUtf32(const char* utf8) const;

    // =========================================================================
    // Member Variables
    // =========================================================================

    int width_;                      ///< Grid width in characters
    int height_;                     ///< Grid height in characters
    std::vector<Cell> cells_;        ///< Linear array of cells (row-major)
    mutable std::mutex mutex_;       ///< Mutex for thread-safe access

    // Dirty tracking for optimization
    bool dirty_;                     ///< True if grid has been modified
    std::vector<bool> dirtyRows_;    ///< Per-row dirty flags
    int dirtyRegionStart_;           ///< First dirty row (inclusive)
    int dirtyRegionEnd_;             ///< Last dirty row (exclusive)
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_TEXTGRID_H
