//
//  Cursor.cpp
//  SuperTerminal Framework - Text Cursor Implementation
//
//  Cursor position, selection, and movement for text editor
//  Supports keyboard and mouse input
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "Cursor.h"
#include "TextBuffer.h"
#include <algorithm>
#include <cctype>

namespace SuperTerminal {

// =============================================================================
// Cursor Implementation
// =============================================================================

Cursor::Cursor()
    : m_line(0)
    , m_column(0)
    , m_preferredColumn(0)
    , m_hasSelection(false)
    , m_selectStartLine(0)
    , m_selectStartColumn(0)
    , m_selectEndLine(0)
    , m_selectEndColumn(0)
    , m_visible(true)
    , m_blinkTime(0.0)
    , m_blinkRate(0.5)
    , m_mouseSelecting(false)
{
}

Cursor::~Cursor() {
    // Nothing to clean up
}

// =============================================================================
// Position
// =============================================================================

void Cursor::setPosition(size_t line, size_t column, const TextBuffer& buffer) {
    m_line = line;
    m_column = column;
    
    clampToBuffer(buffer);
    updatePreferredColumn();
    resetBlink();
}

void Cursor::setPositionUnchecked(size_t line, size_t column) {
    m_line = line;
    m_column = column;
    m_preferredColumn = column;
    resetBlink();
}

// =============================================================================
// Movement (Keyboard)
// =============================================================================

void Cursor::moveUp(const TextBuffer& buffer) {
    if (m_line > 0) {
        --m_line;
        
        // Try to maintain preferred column
        size_t lineLen = getLineLength(buffer, m_line);
        m_column = std::min(m_preferredColumn, lineLen);
    }
    
    resetBlink();
}

void Cursor::moveDown(const TextBuffer& buffer) {
    if (m_line < buffer.getLineCount() - 1) {
        ++m_line;
        
        // Try to maintain preferred column
        size_t lineLen = getLineLength(buffer, m_line);
        m_column = std::min(m_preferredColumn, lineLen);
    }
    
    resetBlink();
}

void Cursor::moveLeft(const TextBuffer& buffer) {
    if (m_column > 0) {
        --m_column;
    } else if (m_line > 0) {
        // Move to end of previous line
        --m_line;
        m_column = getLineLength(buffer, m_line);
    }
    
    updatePreferredColumn();
    resetBlink();
}

void Cursor::moveRight(const TextBuffer& buffer) {
    size_t lineLen = getLineLength(buffer, m_line);
    
    if (m_column < lineLen) {
        ++m_column;
    } else if (m_line < buffer.getLineCount() - 1) {
        // Move to start of next line
        ++m_line;
        m_column = 0;
    }
    
    updatePreferredColumn();
    resetBlink();
}

void Cursor::moveToLineStart() {
    m_column = 0;
    updatePreferredColumn();
    resetBlink();
}

void Cursor::moveToLineEnd(const TextBuffer& buffer) {
    m_column = getLineLength(buffer, m_line);
    updatePreferredColumn();
    resetBlink();
}

void Cursor::moveToDocumentStart() {
    m_line = 0;
    m_column = 0;
    updatePreferredColumn();
    resetBlink();
}

void Cursor::moveToDocumentEnd(const TextBuffer& buffer) {
    if (buffer.getLineCount() > 0) {
        m_line = buffer.getLineCount() - 1;
        m_column = getLineLength(buffer, m_line);
    } else {
        m_line = 0;
        m_column = 0;
    }
    
    updatePreferredColumn();
    resetBlink();
}

void Cursor::moveWordLeft(const TextBuffer& buffer) {
    Position pos = findWordStart(buffer, Position(m_line, m_column));
    m_line = pos.line;
    m_column = pos.column;
    
    updatePreferredColumn();
    resetBlink();
}

void Cursor::moveWordRight(const TextBuffer& buffer) {
    Position pos = findWordEnd(buffer, Position(m_line, m_column));
    m_line = pos.line;
    m_column = pos.column;
    
    updatePreferredColumn();
    resetBlink();
}

void Cursor::movePageUp(const TextBuffer& buffer, int pageLines) {
    if (m_line > static_cast<size_t>(pageLines)) {
        m_line -= pageLines;
    } else {
        m_line = 0;
    }
    
    size_t lineLen = getLineLength(buffer, m_line);
    m_column = std::min(m_preferredColumn, lineLen);
    
    resetBlink();
}

void Cursor::movePageDown(const TextBuffer& buffer, int pageLines) {
    m_line += pageLines;
    
    if (m_line >= buffer.getLineCount()) {
        m_line = buffer.getLineCount() > 0 ? buffer.getLineCount() - 1 : 0;
    }
    
    size_t lineLen = getLineLength(buffer, m_line);
    m_column = std::min(m_preferredColumn, lineLen);
    
    resetBlink();
}

// =============================================================================
// Mouse Support
// =============================================================================

void Cursor::setPositionFromMouse(int gridX, int gridY, const TextBuffer& buffer) {
    // Convert grid coordinates to line/column
    m_line = static_cast<size_t>(std::max(0, gridY));
    m_column = static_cast<size_t>(std::max(0, gridX));
    
    clampToBuffer(buffer);
    updatePreferredColumn();
    resetBlink();
}

void Cursor::startMouseSelection(int gridX, int gridY, const TextBuffer& buffer) {
    setPositionFromMouse(gridX, gridY, buffer);
    
    m_hasSelection = true;
    m_selectStartLine = m_line;
    m_selectStartColumn = m_column;
    m_selectEndLine = m_line;
    m_selectEndColumn = m_column;
    m_mouseSelecting = true;
}

void Cursor::extendMouseSelection(int gridX, int gridY, const TextBuffer& buffer) {
    if (!m_mouseSelecting) {
        return;
    }
    
    setPositionFromMouse(gridX, gridY, buffer);
    
    m_selectEndLine = m_line;
    m_selectEndColumn = m_column;
    
    // If start == end, no selection
    if (m_selectStartLine == m_selectEndLine && 
        m_selectStartColumn == m_selectEndColumn) {
        m_hasSelection = false;
    } else {
        m_hasSelection = true;
    }
}

void Cursor::endMouseSelection() {
    m_mouseSelecting = false;
}

// =============================================================================
// Selection
// =============================================================================

void Cursor::startSelection() {
    m_hasSelection = true;
    m_selectStartLine = m_line;
    m_selectStartColumn = m_column;
    m_selectEndLine = m_line;
    m_selectEndColumn = m_column;
}

void Cursor::extendSelection(size_t newLine, size_t newColumn) {
    if (!m_hasSelection) {
        startSelection();
    }
    
    m_selectEndLine = newLine;
    m_selectEndColumn = newColumn;
}

void Cursor::clearSelection() {
    m_hasSelection = false;
    m_selectStartLine = 0;
    m_selectStartColumn = 0;
    m_selectEndLine = 0;
    m_selectEndColumn = 0;
}

std::pair<Position, Position> Cursor::getSelection() const {
    if (!m_hasSelection) {
        return {Position(m_line, m_column), Position(m_line, m_column)};
    }
    
    Position start(m_selectStartLine, m_selectStartColumn);
    Position end(m_selectEndLine, m_selectEndColumn);
    
    // Normalize: ensure start < end
    if (end < start) {
        std::swap(start, end);
    }
    
    return {start, end};
}

void Cursor::selectAll(const TextBuffer& buffer) {
    if (buffer.getLineCount() == 0) {
        m_hasSelection = false;
        return;
    }
    
    m_hasSelection = true;
    m_selectStartLine = 0;
    m_selectStartColumn = 0;
    m_selectEndLine = buffer.getLineCount() - 1;
    m_selectEndColumn = getLineLength(buffer, m_selectEndLine);
    
    // Move cursor to end
    m_line = m_selectEndLine;
    m_column = m_selectEndColumn;
}

void Cursor::selectLine(const TextBuffer& buffer) {
    m_hasSelection = true;
    m_selectStartLine = m_line;
    m_selectStartColumn = 0;
    m_selectEndLine = m_line;
    m_selectEndColumn = getLineLength(buffer, m_line);
}

void Cursor::selectWord(const TextBuffer& buffer) {
    Position start = findWordStart(buffer, Position(m_line, m_column));
    Position end = findWordEndForSelection(buffer, Position(m_line, m_column));
    
    m_hasSelection = true;
    m_selectStartLine = start.line;
    m_selectStartColumn = start.column;
    m_selectEndLine = end.line;
    m_selectEndColumn = end.column;
}

// =============================================================================
// Visual Properties
// =============================================================================

void Cursor::updateBlink(double deltaTime) {
    m_blinkTime += deltaTime;
    
    if (m_blinkTime >= m_blinkRate) {
        m_visible = !m_visible;
        m_blinkTime = 0.0;
    }
}

void Cursor::resetBlink() {
    m_visible = true;
    m_blinkTime = 0.0;
}

// =============================================================================
// Utility
// =============================================================================

void Cursor::clampToBuffer(const TextBuffer& buffer) {
    // Clamp line
    if (m_line >= buffer.getLineCount()) {
        m_line = buffer.getLineCount() > 0 ? buffer.getLineCount() - 1 : 0;
    }
    
    // Clamp column to line length
    size_t lineLen = getLineLength(buffer, m_line);
    if (m_column > lineLen) {
        m_column = lineLen;
    }
}

bool Cursor::isAtDocumentStart() const {
    return m_line == 0 && m_column == 0;
}

bool Cursor::isAtDocumentEnd(const TextBuffer& buffer) const {
    if (buffer.getLineCount() == 0) {
        return true;
    }
    
    size_t lastLine = buffer.getLineCount() - 1;
    size_t lastColumn = getLineLength(buffer, lastLine);
    
    return m_line == lastLine && m_column == lastColumn;
}

// =============================================================================
// Internal Helpers
// =============================================================================

size_t Cursor::getLineLength(const TextBuffer& buffer, size_t line) const {
    if (line >= buffer.getLineCount()) {
        return 0;
    }
    
    return buffer.getLine(line).length();
}

bool Cursor::isWordBoundary(char32_t ch) const {
    // Word boundaries: whitespace and punctuation
    if (ch <= 32) return true;  // Whitespace
    if (ch >= 127) return false; // Non-ASCII (treat as word chars)
    
    // ASCII punctuation
    return std::ispunct(static_cast<int>(ch)) != 0;
}

Position Cursor::findWordStart(const TextBuffer& buffer, Position pos) const {
    if (pos.line >= buffer.getLineCount()) {
        return pos;
    }
    
    std::string line = buffer.getLine(pos.line);
    
    // If at start of line, go to previous line
    if (pos.column == 0) {
        if (pos.line > 0) {
            pos.line--;
            pos.column = getLineLength(buffer, pos.line);
            return findWordStart(buffer, pos);
        }
        return pos;
    }
    
    // Move back to word start
    while (pos.column > 0) {
        char ch = line[pos.column - 1];
        
        if (isWordBoundary(ch)) {
            break;
        }
        
        pos.column--;
    }
    
    return pos;
}

Position Cursor::findWordEnd(const TextBuffer& buffer, Position pos) const {
    if (pos.line >= buffer.getLineCount()) {
        return pos;
    }
    
    std::string line = buffer.getLine(pos.line);
    
    // Skip current word
    while (pos.column < line.length()) {
        char ch = line[pos.column];
        
        if (isWordBoundary(ch)) {
            break;
        }
        
        pos.column++;
    }
    
    // Skip whitespace
    while (pos.column < line.length()) {
        char ch = line[pos.column];
        
        if (!std::isspace(ch)) {
            break;
        }
        
        pos.column++;
    }
    
    // If at end of line, go to next line
    if (pos.column >= line.length()) {
        if (pos.line < buffer.getLineCount() - 1) {
            pos.line++;
            pos.column = 0;
        }
    }
    
    return pos;
}

Position Cursor::findWordEndForSelection(const TextBuffer& buffer, Position pos) const {
    if (pos.line >= buffer.getLineCount()) {
        return pos;
    }
    
    std::string line = buffer.getLine(pos.line);
    
    // Skip current word (stop at word boundary, don't skip spaces)
    while (pos.column < line.length()) {
        char ch = line[pos.column];
        
        if (isWordBoundary(ch)) {
            break;
        }
        
        pos.column++;
    }
    
    return pos;
}

} // namespace SuperTerminal