//
//  TextBuffer.h
//  SuperTerminal Framework - Text Buffer
//
//  Multi-line text storage with undo/redo support
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef TEXT_BUFFER_H
#define TEXT_BUFFER_H

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <cstdint>

namespace SuperTerminal {

// =============================================================================
// TextBufferState - Snapshot for undo/redo
// =============================================================================

struct TextBufferState {
    std::vector<std::string> lines;
    size_t cursorLine;
    size_t cursorColumn;
    
    TextBufferState() : cursorLine(0), cursorColumn(0) {}
    
    TextBufferState(const std::vector<std::string>& l, size_t ln, size_t col)
        : lines(l), cursorLine(ln), cursorColumn(col) {}
};

// =============================================================================
// TextBuffer - Line-based text storage
// =============================================================================

class TextBuffer {
public:
    // Constructor
    TextBuffer();
    
    // Destructor
    ~TextBuffer();
    
    // Copy is allowed (for undo/redo)
    TextBuffer(const TextBuffer& other);
    TextBuffer& operator=(const TextBuffer& other);
    
    // Move is allowed
    TextBuffer(TextBuffer&& other) noexcept;
    TextBuffer& operator=(TextBuffer&& other) noexcept;
    
    // =========================================================================
    // Content Management
    // =========================================================================
    
    /// Set entire buffer content from string (splits on newlines)
    /// @param text Text content (may contain \n, \r\n, or \r)
    void setText(const std::string& text);
    
    /// Get entire buffer content as string (joined with \n)
    /// @return Text content with newlines
    std::string getText() const;
    
    /// Clear buffer (empty document)
    void clear();
    
    /// Check if buffer is empty
    bool isEmpty() const;
    
    // =========================================================================
    // Line Operations
    // =========================================================================
    
    /// Get number of lines
    /// @return Line count (always at least 1)
    size_t getLineCount() const;
    
    /// Get line content
    /// @param lineNum Line number (0-based)
    /// @return Line content (empty string if out of bounds)
    std::string getLine(size_t lineNum) const;
    
    /// Set line content
    /// @param lineNum Line number (0-based)
    /// @param text New line content
    /// @return true if successful
    bool setLine(size_t lineNum, const std::string& text);
    
    /// Insert new line at position
    /// @param lineNum Line number (0-based, inserts before this line)
    /// @param text Line content
    void insertLine(size_t lineNum, const std::string& text);
    
    /// Delete line at position
    /// @param lineNum Line number (0-based)
    /// @return true if successful
    bool deleteLine(size_t lineNum);
    
    /// Split line at column (creates new line)
    /// @param lineNum Line number (0-based)
    /// @param column Column position (0-based)
    /// @return true if successful
    bool splitLine(size_t lineNum, size_t column);
    
    /// Join line with next line
    /// @param lineNum Line number (0-based)
    /// @return true if successful
    bool joinLine(size_t lineNum);
    
    // =========================================================================
    // Character Operations
    // =========================================================================
    
    /// Insert character at position
    /// @param lineNum Line number (0-based)
    /// @param column Column position (0-based)
    /// @param ch Character to insert (UTF-32)
    /// @return true if successful
    bool insertChar(size_t lineNum, size_t column, char32_t ch);
    
    /// Delete character at position
    /// @param lineNum Line number (0-based)
    /// @param column Column position (0-based)
    /// @return true if successful (false if out of bounds)
    bool deleteChar(size_t lineNum, size_t column);
    
    /// Insert text at position
    /// @param lineNum Line number (0-based)
    /// @param column Column position (0-based)
    /// @param text Text to insert (UTF-8, may contain newlines)
    /// @return true if successful
    bool insertText(size_t lineNum, size_t column, const std::string& text);
    
    /// Delete range of text
    /// @param startLine Start line (0-based)
    /// @param startColumn Start column (0-based)
    /// @param endLine End line (0-based)
    /// @param endColumn End column (0-based)
    /// @return Deleted text
    std::string deleteRange(size_t startLine, size_t startColumn,
                           size_t endLine, size_t endColumn);
    
    /// Get character at position
    /// @param lineNum Line number (0-based)
    /// @param column Column position (0-based)
    /// @return Character (0 if out of bounds)
    char32_t getChar(size_t lineNum, size_t column) const;
    
    // =========================================================================
    // Undo/Redo
    // =========================================================================
    
    /// Push current state onto undo stack
    void pushUndoState();
    
    /// Undo last change
    /// @return true if undo was performed
    bool undo();
    
    /// Redo last undone change
    /// @return true if redo was performed
    bool redo();
    
    /// Check if undo is available
    bool canUndo() const;
    
    /// Check if redo is available
    bool canRedo() const;
    
    /// Clear undo/redo history
    void clearUndoHistory();
    
    /// Set maximum undo stack size
    /// @param maxSize Maximum number of undo states (0 = unlimited)
    void setMaxUndoSize(size_t maxSize);
    
    /// Get current undo stack size
    size_t getUndoStackSize() const;
    
    // =========================================================================
    // Dirty State
    // =========================================================================
    
    /// Check if buffer has been modified
    bool isDirty() const { return m_dirty; }
    
    /// Mark buffer as clean (e.g., after save)
    void markClean() { m_dirty = false; }
    
    /// Mark buffer as dirty (e.g., after edit)
    void markDirty() { m_dirty = true; }
    
    // =========================================================================
    // Utility
    // =========================================================================
    
    /// Get total character count
    size_t getCharacterCount() const;
    
    /// Get byte size of content
    size_t getByteSize() const;
    
    /// Validate line and column position
    /// @param lineNum Line number (0-based)
    /// @param column Column position (0-based)
    /// @return true if position is valid
    bool isValidPosition(size_t lineNum, size_t column) const;
    
    /// Clamp position to valid range
    /// @param lineNum Input/output line number
    /// @param column Input/output column position
    void clampPosition(size_t& lineNum, size_t& column) const;
    
    /// Convert UTF-8 string to UTF-32 codepoints
    static std::vector<char32_t> utf8ToUtf32(const std::string& utf8);
    
    /// Convert UTF-32 codepoint to UTF-8 string
    static std::string utf32ToUtf8(char32_t codepoint);
    
    /// Split string by newlines
    static std::vector<std::string> splitLines(const std::string& text);
    
    /// Get line ending style from text
    enum class LineEnding { LF, CRLF, CR };
    static LineEnding detectLineEnding(const std::string& text);

private:
    // =========================================================================
    // Member Variables
    // =========================================================================
    
    std::vector<std::string> m_lines;           // Line storage (UTF-8)
    std::deque<TextBufferState> m_undoStack;    // Undo history
    std::deque<TextBufferState> m_redoStack;    // Redo history
    size_t m_maxUndoSize;                       // Max undo states (0 = unlimited)
    bool m_dirty;                               // Modification flag
    mutable std::mutex m_mutex;                 // Thread safety
    
    // =========================================================================
    // Internal Helpers
    // =========================================================================
    
    /// Ensure buffer has at least one line
    void ensureNonEmpty();
    
    /// Save current state (for undo/redo)
    TextBufferState captureState(size_t cursorLine, size_t cursorColumn) const;
    
    /// Restore state (for undo/redo)
    void restoreState(const TextBufferState& state);
    
    /// Trim undo stack if needed
    void trimUndoStack();
};

} // namespace SuperTerminal

#endif // TEXT_BUFFER_H