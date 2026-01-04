//
//  EditorRenderer.h
//  SuperTerminal Framework - Editor Renderer
//
//  Render text buffer to TextGrid with syntax highlighting and cursor
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef EDITOR_RENDERER_H
#define EDITOR_RENDERER_H

#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <cstdint>

namespace SuperTerminal {

// Forward declarations
class TextGrid;
class TextBuffer;
class Cursor;

// =============================================================================
// EditorRenderer - Render text editor to TextGrid
// =============================================================================

class EditorRenderer {
public:
    // Constructor
    EditorRenderer(std::shared_ptr<TextGrid> textGrid);
    
    // Destructor
    ~EditorRenderer();
    
    // =========================================================================
    // Main Rendering
    // =========================================================================
    
    /// Render the editor (text, cursor, selection, line numbers)
    /// @param buffer Text buffer to render
    /// @param cursor Cursor state
    /// @param scrollLine Top line visible in viewport
    void render(const TextBuffer& buffer, const Cursor& cursor, int scrollLine);
    
    /// Clear the editor area
    void clear();
    
    /// Update TextGrid reference (for window resize)
    /// @param textGrid New text grid to render to
    void setTextGrid(std::shared_ptr<TextGrid> textGrid);
    
    // =========================================================================
    // Viewport Configuration
    // =========================================================================
    
    /// Set viewport region (editor area in text grid)
    /// @param x Starting column
    /// @param y Starting row
    /// @param width Width in characters
    /// @param height Height in characters
    void setViewport(int x, int y, int width, int height);
    
    /// Get viewport width
    int getViewportWidth() const { return m_viewportWidth; }
    
    /// Get viewport height
    int getViewportHeight() const { return m_viewportHeight; }
    
    /// Get viewport X offset
    int getViewportX() const { return m_viewportX; }
    
    /// Get viewport Y offset
    int getViewportY() const { return m_viewportY; }
    
    // =========================================================================
    // Line Numbers
    // =========================================================================
    
    /// Enable/disable line numbers
    void setLineNumbersVisible(bool visible) { m_showLineNumbers = visible; }
    
    /// Check if line numbers are visible
    bool areLineNumbersVisible() const { return m_showLineNumbers; }
    
    /// Get line number gutter width (in characters)
    int getLineNumberGutterWidth() const;
    
    /// Set line number color
    void setLineNumberColor(uint32_t color) { m_lineNumberColor = color; }
    
    /// Set line number background color
    void setLineNumberBackgroundColor(uint32_t color) { m_lineNumberBgColor = color; }
    
    // =========================================================================
    // Cursor Rendering
    // =========================================================================
    
    /// Set cursor visibility (for blinking)
    void setCursorVisible(bool visible) { m_cursorVisible = visible; }
    
    /// Check if cursor is visible
    bool isCursorVisible() const { return m_cursorVisible; }
    
    /// Set cursor color (yellow by default)
    void setCursorColor(uint32_t color) { m_cursorColor = color; }
    
    /// Get cursor color
    uint32_t getCursorColor() const { return m_cursorColor; }
    
    /// Set cursor style
    enum class CursorStyle {
        BLOCK,          // Solid block (default)
        UNDERLINE,      // Underline
        VERTICAL_BAR    // Vertical bar (|)
    };
    
    void setCursorStyle(CursorStyle style) { m_cursorStyle = style; }
    
    /// Get cursor style
    CursorStyle getCursorStyle() const { return m_cursorStyle; }
    
    // =========================================================================
    // Selection Rendering
    // =========================================================================
    
    /// Set selection background color
    void setSelectionColor(uint32_t color) { m_selectionColor = color; }
    
    /// Get selection color
    uint32_t getSelectionColor() const { return m_selectionColor; }
    
    // =========================================================================
    // Syntax Highlighting
    // =========================================================================
    
    /// Syntax highlighter callback: takes line text, returns color per character
    /// Return vector should be same length as line, or empty for default colors
    using SyntaxHighlighter = std::function<std::vector<uint32_t>(const std::string& line, size_t lineNumber)>;
    
    /// Set syntax highlighter callback
    void setSyntaxHighlighter(SyntaxHighlighter highlighter) {
        m_syntaxHighlighter = highlighter;
    }
    
    /// Clear syntax highlighter
    void clearSyntaxHighlighter() {
        m_syntaxHighlighter = nullptr;
    }
    
    /// Check if syntax highlighter is set
    bool hasSyntaxHighlighter() const {
        return m_syntaxHighlighter != nullptr;
    }
    
    // =========================================================================
    // Colors
    // =========================================================================
    
    /// Set default text color
    void setDefaultTextColor(uint32_t color) { m_defaultTextColor = color; }
    
    /// Get default text color
    uint32_t getDefaultTextColor() const { return m_defaultTextColor; }
    
    /// Set default background color
    void setDefaultBackgroundColor(uint32_t color) { m_defaultBgColor = color; }
    
    /// Get default background color
    uint32_t getDefaultBackgroundColor() const { return m_defaultBgColor; }
    
    /// Set current line highlight color
    void setCurrentLineColor(uint32_t color) { m_currentLineColor = color; }
    
    /// Enable/disable current line highlighting
    void setCurrentLineHighlight(bool enable) { m_highlightCurrentLine = enable; }
    
    // =========================================================================
    // Status Bars
    // =========================================================================
    
    /// Render top status bar
    /// @param filename Script name
    /// @param line Current line number (1-based for display)
    /// @param column Current column number (1-based for display)
    /// @param totalLines Total line count
    /// @param modified Dirty flag
    /// @param language Language name (e.g., "Lua", "JavaScript")
    void renderTopStatusBar(const std::string& filename,
                           int line, int column, int totalLines,
                           bool modified, const std::string& language);
    
    /// Render bottom status bar (command hints)
    /// @param commands Command hint string (e.g., "^R Run  ^S Save")
    void renderBottomStatusBar(const std::string& commands);
    
    /// Set status bar colors
    void setStatusBarColors(uint32_t textColor, uint32_t bgColor) {
        m_statusBarTextColor = textColor;
        m_statusBarBgColor = bgColor;
    }
    
    // =========================================================================
    // Scrolling
    // =========================================================================
    
    /// Calculate optimal scroll position to keep cursor visible
    /// @param buffer Text buffer
    /// @param cursor Cursor position
    /// @param currentScroll Current scroll line
    /// @return New scroll line
    int calculateScrollPosition(const TextBuffer& buffer,
                               const Cursor& cursor,
                               int currentScroll) const;
    
    // =========================================================================
    // Utility
    // =========================================================================
    
    /// Convert screen coordinates to buffer coordinates
    /// @param screenX Screen X (in viewport)
    /// @param screenY Screen Y (in viewport)
    /// @param scrollLine Current scroll position
    /// @param outLine Output: buffer line number
    /// @param outColumn Output: buffer column
    /// @return true if coordinates are valid
    bool screenToBuffer(int screenX, int screenY, int scrollLine,
                       size_t& outLine, size_t& outColumn) const;
    
    /// Convert buffer coordinates to screen coordinates
    /// @param line Buffer line number
    /// @param column Buffer column
    /// @param scrollLine Current scroll position
    /// @param outScreenX Output: screen X
    /// @param outScreenY Output: screen Y
    /// @return true if position is visible on screen
    bool bufferToScreen(size_t line, size_t column, int scrollLine,
                       int& outScreenX, int& outScreenY) const;

private:
    // =========================================================================
    // Member Variables
    // =========================================================================
    
    std::shared_ptr<TextGrid> m_textGrid;   // Target text grid
    
    // Viewport
    int m_viewportX;                        // Viewport X offset
    int m_viewportY;                        // Viewport Y offset
    int m_viewportWidth;                    // Viewport width
    int m_viewportHeight;                   // Viewport height
    
    // Line numbers
    bool m_showLineNumbers;                 // Show line number gutter
    uint32_t m_lineNumberColor;             // Line number text color
    uint32_t m_lineNumberBgColor;           // Line number background color
    
    // Cursor
    bool m_cursorVisible;                   // Cursor visibility (for blinking)
    uint32_t m_cursorColor;                 // Cursor color (yellow)
    CursorStyle m_cursorStyle;              // Cursor style (block/underline/bar)
    
    // Selection
    uint32_t m_selectionColor;              // Selection background color
    
    // Colors
    uint32_t m_defaultTextColor;            // Default text color
    uint32_t m_defaultBgColor;              // Default background color
    uint32_t m_currentLineColor;            // Current line highlight color
    bool m_highlightCurrentLine;            // Highlight current line
    
    // Status bar
    uint32_t m_statusBarTextColor;          // Status bar text color
    uint32_t m_statusBarBgColor;            // Status bar background color
    
    // Syntax highlighting
    SyntaxHighlighter m_syntaxHighlighter;  // Syntax highlighter callback
    
    // =========================================================================
    // Internal Rendering
    // =========================================================================
    
    /// Render text content
    void renderText(const TextBuffer& buffer, const Cursor& cursor, int scrollLine);
    
    /// Render line numbers
    void renderLineNumbers(int scrollLine, int lineCount);
    
    /// Render cursor at position
    void renderCursor(size_t line, size_t column, int scrollLine, char32_t charUnderCursor);
    
    /// Render selection
    void renderSelection(const Cursor& cursor, int scrollLine);
    
    /// Render single line of text
    void renderLine(size_t lineNum, const std::string& lineText,
                   int screenY, int scrollLine,
                   const Cursor& cursor);
    
    /// Check if position is in selection
    bool isInSelection(size_t line, size_t column, const Cursor& cursor) const;
    
    /// Get color for character (with syntax highlighting)
    uint32_t getCharacterColor(const std::string& line, size_t lineNum,
                              size_t column, bool inSelection) const;
    
    /// Get background color for character
    uint32_t getBackgroundColor(size_t line, size_t column,
                               const Cursor& cursor, bool inSelection) const;
    
    /// Calculate line number width needed for given line count
    int calculateLineNumberWidth(int lineCount) const;
    
    /// Format line number with padding
    std::string formatLineNumber(int lineNum, int width) const;
};

} // namespace SuperTerminal

#endif // EDITOR_RENDERER_H