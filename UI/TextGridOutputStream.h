//
// TextGridOutputStream.h
// SuperTerminal Framework - Text Grid Output Stream
//
// Provides scrolling text output for TextGrid, routing PRINT/PRINTLN commands
// to a scrollable text buffer with automatic line wrapping and scrolling.
// Designed for interactive BASIC shell output.
//

#pragma once

#include <string>
#include <deque>
#include <memory>
#include <mutex>

namespace SuperTerminal {

// Forward declarations
class TextGrid;

/// Scrolling text output stream for TextGrid
/// Manages a scrollable text buffer with automatic wrapping and scrolling
class TextGridOutputStream {
public:
    /// Constructor
    /// @param textGrid Shared pointer to TextGrid for rendering
    /// @param topRow Top row of output area (0-based, inclusive)
    /// @param bottomRow Bottom row of output area (0-based, inclusive)
    TextGridOutputStream(std::shared_ptr<TextGrid> textGrid,
                        int topRow, int bottomRow);
    
    /// Destructor
    ~TextGridOutputStream();
    
    /// Print text without newline
    /// Text will wrap if it exceeds line width
    /// @param text Text to print
    void print(const std::string& text);
    
    /// Print text with newline
    /// @param text Text to print
    void println(const std::string& text);
    
    /// Print a newline
    void println();
    
    /// Clear the output area
    void clear();
    
    /// Scroll up one line
    void scroll();
    
    /// Get current cursor position in output area
    /// @return Pair of (column, row) relative to output area
    std::pair<int, int> getCursor() const;
    
    /// Set cursor position for AT/LOCATE commands
    /// @param x Column position (0-based, relative to output area)
    /// @param y Row position (0-based, relative to output area)
    void setCursor(int x, int y);
    
    /// Move cursor to home position (0, 0)
    void home();
    
    /// Set colors for output
    /// @param foreground Foreground color (RGBA)
    /// @param background Background color (RGBA)
    void setColors(uint32_t foreground, uint32_t background);
    
    /// Get number of rows in output area
    /// @return Number of rows available
    int getRows() const { return bottomRow_ - topRow_ + 1; }
    
    /// Get width of output area (from TextGrid)
    /// @return Width in characters
    int getWidth() const;
    
    /// Render the output buffer to TextGrid
    void render();
    
    /// Enable/disable auto-scroll
    /// @param enabled If true, automatically scroll when new lines are added
    void setAutoScroll(bool enabled) { autoScroll_ = enabled; }
    
    /// Get auto-scroll state
    /// @return true if auto-scroll is enabled
    bool getAutoScroll() const { return autoScroll_; }
    
    /// Get scroll position (for manual scrolling)
    /// @return Current scroll offset (0 = showing most recent lines)
    int getScrollPosition() const { return scrollOffset_; }
    
    /// Set scroll position (for manual scrolling)
    /// @param offset Scroll offset (0 = showing most recent lines)
    void setScrollPosition(int offset);
    
    /// Scroll up by N lines
    /// @param lines Number of lines to scroll up
    void scrollUp(int lines = 1);
    
    /// Scroll down by N lines
    /// @param lines Number of lines to scroll down
    void scrollDown(int lines = 1);

private:
    // Components
    std::shared_ptr<TextGrid> textGrid_;
    
    // Display area
    int topRow_;        // Top row in TextGrid (inclusive)
    int bottomRow_;     // Bottom row in TextGrid (inclusive)
    
    // Colors
    uint32_t foregroundColor_;
    uint32_t backgroundColor_;
    
    // Buffer state
    std::deque<std::string> lines_;  // Scrollback buffer
    int currentCol_;                 // Current column position in current line
    int currentRow_;                 // Current row position (relative to topRow_)
    
    // Scrolling state
    bool autoScroll_;                // Auto-scroll to bottom when new lines added
    int scrollOffset_;               // Scroll position (0 = bottom)
    size_t maxLines_;                // Maximum lines in scrollback buffer
    
    // Thread safety
    mutable std::mutex mutex_;
    
    // Private methods
    void addLine(const std::string& line);
    void wrapAndAddLine(const std::string& text);
    void ensureCurrentLine();
    void trimBuffer();
    std::string getCurrentLine() const;
    void updateCurrentLine(const std::string& newContent);
};

} // namespace SuperTerminal