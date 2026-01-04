//
//  Cursor.h
//  SuperTerminal Framework - Text Cursor
//
//  Cursor position, selection, and movement for text editor
//  Supports keyboard and mouse input
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef CURSOR_H
#define CURSOR_H

#include <cstdint>
#include <utility>

namespace SuperTerminal {

// Forward declaration
class TextBuffer;

// =============================================================================
// Position - Simple line/column coordinate
// =============================================================================

struct Position {
    size_t line;
    size_t column;
    
    Position() : line(0), column(0) {}
    Position(size_t l, size_t c) : line(l), column(c) {}
    
    bool operator==(const Position& other) const {
        return line == other.line && column == other.column;
    }
    
    bool operator!=(const Position& other) const {
        return !(*this == other);
    }
    
    bool operator<(const Position& other) const {
        if (line != other.line) return line < other.line;
        return column < other.column;
    }
};

// =============================================================================
// Cursor - Text editor cursor with selection support
// =============================================================================

class Cursor {
public:
    // Constructor
    Cursor();
    
    // Destructor
    ~Cursor();
    
    // =========================================================================
    // Position
    // =========================================================================
    
    /// Get cursor position
    Position getPosition() const { return Position(m_line, m_column); }
    
    /// Get cursor line
    size_t getLine() const { return m_line; }
    
    /// Get cursor column
    size_t getColumn() const { return m_column; }
    
    /// Set cursor position (with bounds checking)
    void setPosition(size_t line, size_t column, const TextBuffer& buffer);
    
    /// Set position without bounds checking (internal use)
    void setPositionUnchecked(size_t line, size_t column);
    
    // =========================================================================
    // Movement (Keyboard)
    // =========================================================================
    
    /// Move cursor up one line
    void moveUp(const TextBuffer& buffer);
    
    /// Move cursor down one line
    void moveDown(const TextBuffer& buffer);
    
    /// Move cursor left one character
    void moveLeft(const TextBuffer& buffer);
    
    /// Move cursor right one character
    void moveRight(const TextBuffer& buffer);
    
    /// Move to start of current line
    void moveToLineStart();
    
    /// Move to end of current line
    void moveToLineEnd(const TextBuffer& buffer);
    
    /// Move to start of document
    void moveToDocumentStart();
    
    /// Move to end of document
    void moveToDocumentEnd(const TextBuffer& buffer);
    
    /// Move to previous word
    void moveWordLeft(const TextBuffer& buffer);
    
    /// Move to next word
    void moveWordRight(const TextBuffer& buffer);
    
    /// Move up one page
    void movePageUp(const TextBuffer& buffer, int pageLines);
    
    /// Move down one page
    void movePageDown(const TextBuffer& buffer, int pageLines);
    
    // =========================================================================
    // Mouse Support
    // =========================================================================
    
    /// Set cursor position from mouse click (grid coordinates)
    /// @param gridX Column in text grid
    /// @param gridY Row in text grid
    /// @param buffer Text buffer to validate against
    void setPositionFromMouse(int gridX, int gridY, const TextBuffer& buffer);
    
    /// Start selection from mouse click
    void startMouseSelection(int gridX, int gridY, const TextBuffer& buffer);
    
    /// Extend selection while dragging mouse
    void extendMouseSelection(int gridX, int gridY, const TextBuffer& buffer);
    
    /// End mouse selection
    void endMouseSelection();
    
    /// Check if currently selecting with mouse
    bool isMouseSelecting() const { return m_mouseSelecting; }
    
    // =========================================================================
    // Selection
    // =========================================================================
    
    /// Check if text is selected
    bool hasSelection() const { return m_hasSelection; }
    
    /// Start selection at current cursor position
    void startSelection();
    
    /// Extend selection with cursor movement
    void extendSelection(size_t newLine, size_t newColumn);
    
    /// Clear selection
    void clearSelection();
    
    /// Get selection range (returns normalized start and end)
    /// @return pair of (start, end) positions
    std::pair<Position, Position> getSelection() const;
    
    /// Get selection start position (may be after end if selecting backwards)
    Position getSelectionStart() const { return Position(m_selectStartLine, m_selectStartColumn); }
    
    /// Get selection end position
    Position getSelectionEnd() const { return Position(m_selectEndLine, m_selectEndColumn); }
    
    /// Select all text
    void selectAll(const TextBuffer& buffer);
    
    /// Select current line
    void selectLine(const TextBuffer& buffer);
    
    /// Select word at cursor
    void selectWord(const TextBuffer& buffer);
    
    // =========================================================================
    // Visual Properties
    // =========================================================================
    
    /// Set cursor visible (for blinking animation)
    void setVisible(bool visible) { m_visible = visible; }
    
    /// Check if cursor is visible
    bool isVisible() const { return m_visible; }
    
    /// Get cursor blink timer
    double getBlinkTime() const { return m_blinkTime; }
    
    /// Update blink timer
    /// @param deltaTime Time since last update (seconds)
    void updateBlink(double deltaTime);
    
    /// Reset blink timer (make cursor visible)
    void resetBlink();
    
    /// Set blink rate (seconds per blink)
    void setBlinkRate(double rate) { m_blinkRate = rate; }
    
    /// Get blink rate
    double getBlinkRate() const { return m_blinkRate; }
    
    // =========================================================================
    // Preferred Column (for vertical movement)
    // =========================================================================
    
    /// Get preferred column for vertical movement
    /// When moving up/down, cursor tries to maintain this column
    size_t getPreferredColumn() const { return m_preferredColumn; }
    
    /// Set preferred column
    void setPreferredColumn(size_t column) { m_preferredColumn = column; }
    
    /// Update preferred column from current position
    void updatePreferredColumn() { m_preferredColumn = m_column; }
    
    // =========================================================================
    // Utility
    // =========================================================================
    
    /// Clamp position to buffer bounds
    void clampToBuffer(const TextBuffer& buffer);
    
    /// Check if position is at start of buffer
    bool isAtDocumentStart() const;
    
    /// Check if position is at end of buffer
    bool isAtDocumentEnd(const TextBuffer& buffer) const;

private:
    // =========================================================================
    // Member Variables
    // =========================================================================
    
    size_t m_line;                  // Current line (0-based)
    size_t m_column;                // Current column (0-based)
    size_t m_preferredColumn;       // Preferred column for vertical movement
    
    // Selection state
    bool m_hasSelection;            // Is text selected?
    size_t m_selectStartLine;       // Selection start line
    size_t m_selectStartColumn;     // Selection start column
    size_t m_selectEndLine;         // Selection end line
    size_t m_selectEndColumn;       // Selection end column
    
    // Visual state
    bool m_visible;                 // Cursor visibility (for blinking)
    double m_blinkTime;             // Blink timer (seconds)
    double m_blinkRate;             // Blink rate (seconds per blink)
    
    // Mouse selection state
    bool m_mouseSelecting;          // Currently selecting with mouse?
    
    // =========================================================================
    // Internal Helpers
    // =========================================================================
    
    /// Get length of line
    size_t getLineLength(const TextBuffer& buffer, size_t line) const;
    
    /// Check if character is word boundary
    bool isWordBoundary(char32_t ch) const;
    
    /// Find word start position
    Position findWordStart(const TextBuffer& buffer, Position pos) const;
    
    /// Find word end position (includes skipping trailing whitespace for cursor movement)
    Position findWordEnd(const TextBuffer& buffer, Position pos) const;
    
    /// Find word end position for selection (stops at word boundary, excludes spaces)
    Position findWordEndForSelection(const TextBuffer& buffer, Position pos) const;
};

} // namespace SuperTerminal

#endif // CURSOR_H