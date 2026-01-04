//
//  EditorRenderer.cpp
//  SuperTerminal Framework - Editor Renderer Implementation
//
//  Render text buffer to TextGrid with syntax highlighting and cursor
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "EditorRenderer.h"
#include "TextBuffer.h"
#include "Cursor.h"
#include "Display/TextGrid.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace SuperTerminal {

// =============================================================================
// Color Constants
// =============================================================================

// Default colors
static const uint32_t COLOR_DEFAULT_TEXT       = 0xE0E0E0FF;  // Light gray text
static const uint32_t COLOR_DEFAULT_BG         = 0x1E1E1EFF;  // Dark gray background
static const uint32_t COLOR_CURSOR             = 0xFFD700FF;  // Glowing yellow
static const uint32_t COLOR_SELECTION          = 0x4040A0FF;  // Blue selection
static const uint32_t COLOR_LINE_NUMBER        = 0x808080FF;  // Gray line numbers
static const uint32_t COLOR_LINE_NUMBER_BG     = 0x2A2A2AFF;  // Slightly lighter than bg
static const uint32_t COLOR_CURRENT_LINE       = 0x2A2A2AFF;  // Subtle highlight
static const uint32_t COLOR_STATUS_BAR_TEXT    = 0xFFFFFFFF;  // White
static const uint32_t COLOR_STATUS_BAR_BG      = 0x005577FF;  // Blue-gray

// =============================================================================
// EditorRenderer Implementation
// =============================================================================

EditorRenderer::EditorRenderer(std::shared_ptr<TextGrid> textGrid)
    : m_textGrid(textGrid)
    , m_viewportX(0)
    , m_viewportY(0)  // Start at row 0 - no status bar needed
    , m_viewportWidth(80)
    , m_viewportHeight(25)  // Use full height
    , m_showLineNumbers(true)
    , m_lineNumberColor(COLOR_LINE_NUMBER)
    , m_lineNumberBgColor(COLOR_LINE_NUMBER_BG)
    , m_cursorVisible(true)
    , m_cursorColor(COLOR_CURSOR)
    , m_cursorStyle(CursorStyle::BLOCK)
    , m_selectionColor(COLOR_SELECTION)
    , m_defaultTextColor(COLOR_DEFAULT_TEXT)
    , m_defaultBgColor(COLOR_DEFAULT_BG)
    , m_currentLineColor(COLOR_CURRENT_LINE)
    , m_highlightCurrentLine(true)
    , m_statusBarTextColor(COLOR_STATUS_BAR_TEXT)
    , m_statusBarBgColor(COLOR_STATUS_BAR_BG)
    , m_syntaxHighlighter(nullptr)
{
    if (m_textGrid) {
        m_viewportWidth = m_textGrid->getWidth();
        m_viewportHeight = m_textGrid->getHeight();  // Use full height - status bar is native Cocoa NSView
    }
}

EditorRenderer::~EditorRenderer() {
    // Nothing to clean up
}

// =============================================================================
// Main Rendering
// =============================================================================

void EditorRenderer::render(const TextBuffer& buffer, const Cursor& cursor, int scrollLine) {
    if (!m_textGrid) {
        return;
    }

    // Render order:
    // 1. Text content (with syntax highlighting)
    // 2. Line numbers
    // 3. Selection
    // 4. Cursor (on top of everything)
    
    renderText(buffer, cursor, scrollLine);
    
    if (m_showLineNumbers) {
        renderLineNumbers(scrollLine, static_cast<int>(buffer.getLineCount()));
    }
    
    renderSelection(cursor, scrollLine);
    
    // Use cursor's own visibility state for blinking
    if (cursor.isVisible()) {
        // Get character under cursor for rendering
        char32_t charUnderCursor = buffer.getChar(cursor.getLine(), cursor.getColumn());
        if (charUnderCursor == 0) {
            charUnderCursor = U' ';
        }
        renderCursor(cursor.getLine(), cursor.getColumn(), scrollLine, charUnderCursor);
    }
}

void EditorRenderer::clear() {
    if (!m_textGrid) {
        return;
    }
    
    // Clear viewport area
    m_textGrid->clearRegion(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight);
}

void EditorRenderer::setTextGrid(std::shared_ptr<TextGrid> textGrid) {
    if (!textGrid) {
        return;
    }
    
    m_textGrid = textGrid;
    
    // Update viewport to match new grid size if needed
    int gridWidth = m_textGrid->getWidth();
    int gridHeight = m_textGrid->getHeight();
    
    // Adjust viewport if it's larger than the new grid
    if (m_viewportX + m_viewportWidth > gridWidth) {
        m_viewportWidth = gridWidth - m_viewportX;
    }
    if (m_viewportY + m_viewportHeight > gridHeight) {
        m_viewportHeight = gridHeight - m_viewportY;
    }
    
    // If viewport was set to full grid, update it to new full grid
    if (m_viewportWidth <= 0 || m_viewportHeight <= 0) {
        setViewport(0, 0, gridWidth, gridHeight);
    }
}

// =============================================================================
// Viewport Configuration
// =============================================================================

void EditorRenderer::setViewport(int x, int y, int width, int height) {
    m_viewportX = x;
    m_viewportY = y;
    m_viewportWidth = width;
    m_viewportHeight = height;
}

// =============================================================================
// Line Numbers
// =============================================================================

int EditorRenderer::getLineNumberGutterWidth() const {
    if (!m_showLineNumbers) {
        return 0;
    }
    
    // Calculate based on max line number we might show
    // For now, assume up to 9999 lines (4 digits + 1 space + 1 separator)
    return 6;  // " 1234 │"
}

// =============================================================================
// Status Bars
// =============================================================================

void EditorRenderer::renderTopStatusBar(const std::string& filename,
                                       int line, int column, int totalLines,
                                       bool modified, const std::string& language)
{
    if (!m_textGrid) {
        return;
    }
    
    int y = 0;  // Top row
    int width = m_textGrid->getWidth();
    
    // Build status string
    std::ostringstream status;
    status << (modified ? "[●] " : "[ ] ");
    status << filename;
    
    if (!language.empty()) {
        status << " · " << language;
    }
    
    status << " · Line " << line << "/" << totalLines;
    status << " · Col " << column;
    
    if (modified) {
        status << " · Modified";
    }
    
    std::string statusStr = status.str();
    
    // Clear status bar row
    m_textGrid->fillRegion(0, y, width, 1, U' ', m_statusBarTextColor, m_statusBarBgColor);
    
    // Render status text
    m_textGrid->putString(0, y, statusStr.c_str(), m_statusBarTextColor, m_statusBarBgColor);
}

void EditorRenderer::renderBottomStatusBar(const std::string& commands) {
    if (!m_textGrid) {
        return;
    }
    
    int y = m_textGrid->getHeight() - 1;  // Bottom row
    int width = m_textGrid->getWidth();
    
    // Clear status bar row
    m_textGrid->fillRegion(0, y, width, 1, U' ', m_statusBarTextColor, m_statusBarBgColor);
    
    // Render command hints
    if (!commands.empty()) {
        m_textGrid->putString(0, y, commands.c_str(), m_statusBarTextColor, m_statusBarBgColor);
    }
}

// =============================================================================
// Scrolling
// =============================================================================

int EditorRenderer::calculateScrollPosition(const TextBuffer& buffer,
                                           const Cursor& cursor,
                                           int currentScroll) const
{
    int cursorLine = static_cast<int>(cursor.getLine());
    int visibleLines = m_viewportHeight;
    
    // Keep cursor within visible area with some margin
    int scrollMargin = 3;  // Lines from top/bottom before scrolling
    
    // Scroll down if cursor is below visible area
    if (cursorLine >= currentScroll + visibleLines - scrollMargin) {
        currentScroll = cursorLine - visibleLines + scrollMargin + 1;
    }
    
    // Scroll up if cursor is above visible area
    if (cursorLine < currentScroll + scrollMargin) {
        currentScroll = cursorLine - scrollMargin;
    }
    
    // Clamp scroll position
    int maxScroll = static_cast<int>(buffer.getLineCount()) - visibleLines;
    if (currentScroll > maxScroll) {
        currentScroll = std::max(0, maxScroll);
    }
    
    if (currentScroll < 0) {
        currentScroll = 0;
    }
    
    return currentScroll;
}

// =============================================================================
// Utility
// =============================================================================

bool EditorRenderer::screenToBuffer(int screenX, int screenY, int scrollLine,
                                    size_t& outLine, size_t& outColumn) const
{
    // Account for line number gutter
    int gutterWidth = getLineNumberGutterWidth();
    
    if (screenX < gutterWidth) {
        return false;  // Click in line number area
    }
    
    // Convert to buffer coordinates
    outLine = static_cast<size_t>(scrollLine + screenY);
    outColumn = static_cast<size_t>(screenX - gutterWidth);
    
    return true;
}

bool EditorRenderer::bufferToScreen(size_t line, size_t column, int scrollLine,
                                    int& outScreenX, int& outScreenY) const
{
    // Check if line is visible
    int screenY = static_cast<int>(line) - scrollLine;
    
    if (screenY < 0 || screenY >= m_viewportHeight) {
        return false;  // Not visible
    }
    
    // Account for line number gutter
    int gutterWidth = getLineNumberGutterWidth();
    
    outScreenX = gutterWidth + static_cast<int>(column);
    outScreenY = screenY;
    
    return true;
}

// =============================================================================
// Internal Rendering
// =============================================================================

void EditorRenderer::renderText(const TextBuffer& buffer, const Cursor& cursor, int scrollLine) {
    if (!m_textGrid) {
        return;
    }
    
    int gutterWidth = getLineNumberGutterWidth();
    int textAreaWidth = m_viewportWidth - gutterWidth;
    
    // Render visible lines
    for (int screenY = 0; screenY < m_viewportHeight; ++screenY) {
        size_t lineNum = static_cast<size_t>(scrollLine + screenY);
        
        if (lineNum >= buffer.getLineCount()) {
            // Past end of buffer - clear line
            m_textGrid->fillRegion(m_viewportX + gutterWidth, m_viewportY + screenY,
                                  textAreaWidth, 1,
                                  U' ', m_defaultTextColor, m_defaultBgColor);
            continue;
        }
        
        std::string lineText = buffer.getLine(lineNum);
        renderLine(lineNum, lineText, screenY, scrollLine, cursor);
    }
}

void EditorRenderer::renderLine(size_t lineNum, const std::string& lineText,
                                int screenY, int scrollLine,
                                const Cursor& cursor)
{
    if (!m_textGrid) {
        return;
    }
    
    int gutterWidth = getLineNumberGutterWidth();
    int textAreaWidth = m_viewportWidth - gutterWidth;
    
    // Determine if this is the current line (for highlighting)
    bool isCurrentLine = (lineNum == cursor.getLine()) && m_highlightCurrentLine;
    
    // Get syntax highlighting colors for this line
    std::vector<uint32_t> syntaxColors;
    if (m_syntaxHighlighter) {
        syntaxColors = m_syntaxHighlighter(lineText, lineNum);
    }
    
    // Render each character
    for (int col = 0; col < textAreaWidth; ++col) {
        int screenX = m_viewportX + gutterWidth + col;
        int screenYAbs = m_viewportY + screenY;
        
        char32_t ch = U' ';
        uint32_t fgColor = m_defaultTextColor;
        uint32_t bgColor = m_defaultBgColor;
        
        // Get character from line
        if (col < static_cast<int>(lineText.length())) {
            ch = static_cast<char32_t>(static_cast<unsigned char>(lineText[col]));
            
            // Apply syntax highlighting
            if (!syntaxColors.empty() && col < static_cast<int>(syntaxColors.size())) {
                fgColor = syntaxColors[col];
            }
        }
        
        // Current line highlight
        if (isCurrentLine) {
            bgColor = m_currentLineColor;
        }
        
        // Selection will be rendered separately on top
        
        m_textGrid->putChar(screenX, screenYAbs, ch, fgColor, bgColor);
    }
}

void EditorRenderer::renderLineNumbers(int scrollLine, int lineCount) {
    if (!m_textGrid || !m_showLineNumbers) {
        return;
    }
    
    int gutterWidth = getLineNumberGutterWidth();
    
    for (int screenY = 0; screenY < m_viewportHeight; ++screenY) {
        int lineNum = scrollLine + screenY + 1;  // 1-based for display
        
        if (lineNum > lineCount) {
            // Past end of buffer - show tilde or blank
            m_textGrid->fillRegion(m_viewportX, m_viewportY + screenY,
                                  gutterWidth, 1,
                                  U' ', m_lineNumberColor, m_lineNumberBgColor);
            m_textGrid->putChar(m_viewportX, m_viewportY + screenY,
                              U'~', m_lineNumberColor, m_lineNumberBgColor);
            continue;
        }
        
        // Format line number
        std::string lineNumStr = formatLineNumber(lineNum, gutterWidth - 2);
        
        // Clear gutter area
        m_textGrid->fillRegion(m_viewportX, m_viewportY + screenY,
                              gutterWidth, 1,
                              U' ', m_lineNumberColor, m_lineNumberBgColor);
        
        // Render line number
        m_textGrid->putString(m_viewportX, m_viewportY + screenY,
                             lineNumStr.c_str(), m_lineNumberColor, m_lineNumberBgColor);
        
        // Render separator
        m_textGrid->putChar(m_viewportX + gutterWidth - 1, m_viewportY + screenY,
                           U'│', m_lineNumberColor, m_lineNumberBgColor);
    }
}

void EditorRenderer::renderCursor(size_t line, size_t column, int scrollLine, char32_t charUnderCursor) {
    if (!m_textGrid) {
        return;
    }
    
    // Convert to screen coordinates
    int screenX, screenY;
    if (!bufferToScreen(line, column, scrollLine, screenX, screenY)) {
        return;  // Cursor not visible
    }
    
    int absX = m_viewportX + screenX;
    int absY = m_viewportY + screenY;
    
    // Yellow glowing cursor
    switch (m_cursorStyle) {
        case CursorStyle::BLOCK:
            // Solid yellow block with inverted text
            m_textGrid->putChar(absX, absY, charUnderCursor,
                              0x000000FF,      // Black text
                              m_cursorColor);  // Yellow background
            break;
            
        case CursorStyle::UNDERLINE:
            // Underline at bottom of cell
            m_textGrid->putChar(absX, absY, U'_', m_cursorColor, m_defaultBgColor);
            break;
            
        case CursorStyle::VERTICAL_BAR:
            // Vertical bar at start of cell
            m_textGrid->putChar(absX, absY, U'│', m_cursorColor, m_defaultBgColor);
            break;
    }
}

void EditorRenderer::renderSelection(const Cursor& cursor, int scrollLine) {
    if (!m_textGrid || !cursor.hasSelection()) {
        return;
    }
    
    auto [start, end] = cursor.getSelection();
    
    // Render selection by modifying background color
    for (size_t line = start.line; line <= end.line; ++line) {
        int screenX, screenY;
        if (!bufferToScreen(line, 0, scrollLine, screenX, screenY)) {
            continue;  // Line not visible
        }
        
        size_t startCol = (line == start.line) ? start.column : 0;
        size_t endCol = (line == end.line) ? end.column : 9999;  // Large number
        
        int gutterWidth = getLineNumberGutterWidth();
        
        for (size_t col = startCol; col < endCol; ++col) {
            int colScreenX = gutterWidth + static_cast<int>(col);
            
            if (colScreenX >= m_viewportWidth) {
                break;  // Past viewport
            }
            
            int absX = m_viewportX + colScreenX;
            int absY = m_viewportY + screenY;
            
            // Get current cell
            auto cell = m_textGrid->getCell(absX, absY);
            
            // Modify background to selection color
            m_textGrid->putChar(absX, absY, cell.character, cell.foreground, m_selectionColor);
        }
    }
}

bool EditorRenderer::isInSelection(size_t line, size_t column, const Cursor& cursor) const {
    if (!cursor.hasSelection()) {
        return false;
    }
    
    auto [start, end] = cursor.getSelection();
    
    // Check if position is within selection
    if (line < start.line || line > end.line) {
        return false;
    }
    
    if (line == start.line && column < start.column) {
        return false;
    }
    
    if (line == end.line && column >= end.column) {
        return false;
    }
    
    return true;
}

uint32_t EditorRenderer::getCharacterColor(const std::string& line, size_t lineNum,
                                          size_t column, bool inSelection) const
{
    // If syntax highlighter is set, use it
    if (m_syntaxHighlighter) {
        std::vector<uint32_t> colors = m_syntaxHighlighter(line, lineNum);
        if (!colors.empty() && column < colors.size()) {
            return colors[column];
        }
    }
    
    return m_defaultTextColor;
}

uint32_t EditorRenderer::getBackgroundColor(size_t line, size_t column,
                                           const Cursor& cursor, bool inSelection) const
{
    if (inSelection) {
        return m_selectionColor;
    }
    
    if (m_highlightCurrentLine && line == cursor.getLine()) {
        return m_currentLineColor;
    }
    
    return m_defaultBgColor;
}

int EditorRenderer::calculateLineNumberWidth(int lineCount) const {
    if (lineCount <= 0) {
        return 4;  // Minimum width
    }
    
    // Calculate digits needed
    int digits = 1;
    int temp = lineCount;
    while (temp >= 10) {
        ++digits;
        temp /= 10;
    }
    
    // Add space for padding and separator
    return digits + 2;  // +1 for space, +1 for separator
}

std::string EditorRenderer::formatLineNumber(int lineNum, int width) const {
    std::ostringstream oss;
    oss << std::setw(width) << std::right << lineNum << " ";
    return oss.str();
}

} // namespace SuperTerminal