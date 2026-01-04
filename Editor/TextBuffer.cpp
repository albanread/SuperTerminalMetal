//
//  TextBuffer.cpp
//  SuperTerminal Framework - Text Buffer Implementation
//
//  Multi-line text storage with undo/redo support
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "TextBuffer.h"
#include <algorithm>
#include <sstream>
#include <codecvt>
#include <locale>

namespace SuperTerminal {

// =============================================================================
// TextBuffer Implementation
// =============================================================================

TextBuffer::TextBuffer()
    : m_maxUndoSize(1000)
    , m_dirty(false)
{
    m_lines.push_back("");  // Always have at least one line
}

TextBuffer::~TextBuffer() {
    // Nothing to clean up
}

// Copy constructor
TextBuffer::TextBuffer(const TextBuffer& other)
    : m_lines(other.m_lines)
    , m_maxUndoSize(other.m_maxUndoSize)
    , m_dirty(other.m_dirty)
{
    // Don't copy undo/redo stacks (intentional)
}

// Copy assignment
TextBuffer& TextBuffer::operator=(const TextBuffer& other) {
    if (this != &other) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lines = other.m_lines;
        m_maxUndoSize = other.m_maxUndoSize;
        m_dirty = other.m_dirty;
        // Don't copy undo/redo stacks
    }
    return *this;
}

// Move constructor
TextBuffer::TextBuffer(TextBuffer&& other) noexcept
    : m_lines(std::move(other.m_lines))
    , m_undoStack(std::move(other.m_undoStack))
    , m_redoStack(std::move(other.m_redoStack))
    , m_maxUndoSize(other.m_maxUndoSize)
    , m_dirty(other.m_dirty)
{
    ensureNonEmpty();
}

// Move assignment
TextBuffer& TextBuffer::operator=(TextBuffer&& other) noexcept {
    if (this != &other) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lines = std::move(other.m_lines);
        m_undoStack = std::move(other.m_undoStack);
        m_redoStack = std::move(other.m_redoStack);
        m_maxUndoSize = other.m_maxUndoSize;
        m_dirty = other.m_dirty;
        ensureNonEmpty();
    }
    return *this;
}

// =============================================================================
// Content Management
// =============================================================================

void TextBuffer::setText(const std::string& text) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_lines = splitLines(text);
    ensureNonEmpty();
    m_dirty = true;
}

std::string TextBuffer::getText() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_lines.empty()) {
        return "";
    }
    
    std::ostringstream oss;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        oss << m_lines[i];
        if (i < m_lines.size() - 1) {
            oss << '\n';
        }
    }
    
    return oss.str();
}

void TextBuffer::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_lines.clear();
    m_lines.push_back("");
    m_dirty = true;
}

bool TextBuffer::isEmpty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    return m_lines.size() == 1 && m_lines[0].empty();
}

// =============================================================================
// Line Operations
// =============================================================================

size_t TextBuffer::getLineCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lines.size();
}

std::string TextBuffer::getLine(size_t lineNum) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (lineNum >= m_lines.size()) {
        return "";
    }
    
    return m_lines[lineNum];
}

bool TextBuffer::setLine(size_t lineNum, const std::string& text) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (lineNum >= m_lines.size()) {
        return false;
    }
    
    m_lines[lineNum] = text;
    m_dirty = true;
    return true;
}

void TextBuffer::insertLine(size_t lineNum, const std::string& text) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (lineNum > m_lines.size()) {
        lineNum = m_lines.size();
    }
    
    m_lines.insert(m_lines.begin() + lineNum, text);
    m_dirty = true;
}

bool TextBuffer::deleteLine(size_t lineNum) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (lineNum >= m_lines.size()) {
        return false;
    }
    
    // Never delete the last line (keep at least one empty line)
    if (m_lines.size() == 1) {
        m_lines[0].clear();
    } else {
        m_lines.erase(m_lines.begin() + lineNum);
    }
    
    m_dirty = true;
    return true;
}

bool TextBuffer::splitLine(size_t lineNum, size_t column) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (lineNum >= m_lines.size()) {
        return false;
    }
    
    std::string& line = m_lines[lineNum];
    
    // Clamp column to line length
    if (column > line.length()) {
        column = line.length();
    }
    
    // Split the line
    std::string firstPart = line.substr(0, column);
    std::string secondPart = line.substr(column);
    
    m_lines[lineNum] = firstPart;
    m_lines.insert(m_lines.begin() + lineNum + 1, secondPart);
    
    m_dirty = true;
    return true;
}

bool TextBuffer::joinLine(size_t lineNum) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (lineNum >= m_lines.size() - 1) {
        return false;  // Can't join last line
    }
    
    // Append next line to current line
    m_lines[lineNum] += m_lines[lineNum + 1];
    m_lines.erase(m_lines.begin() + lineNum + 1);
    
    m_dirty = true;
    return true;
}

// =============================================================================
// Character Operations
// =============================================================================

bool TextBuffer::insertChar(size_t lineNum, size_t column, char32_t ch) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (lineNum >= m_lines.size()) {
        return false;
    }
    
    std::string& line = m_lines[lineNum];
    
    // Convert UTF-32 char to UTF-8
    std::string utf8 = utf32ToUtf8(ch);
    
    // Clamp column to line length
    if (column > line.length()) {
        column = line.length();
    }
    
    line.insert(column, utf8);
    m_dirty = true;
    return true;
}

bool TextBuffer::deleteChar(size_t lineNum, size_t column) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (lineNum >= m_lines.size()) {
        return false;
    }
    
    std::string& line = m_lines[lineNum];
    
    if (column >= line.length()) {
        return false;
    }
    
    // Find UTF-8 character boundary
    size_t charStart = column;
    size_t charLen = 1;
    
    // Handle UTF-8 multi-byte characters
    unsigned char c = static_cast<unsigned char>(line[charStart]);
    if ((c & 0x80) == 0) {
        charLen = 1;  // ASCII
    } else if ((c & 0xE0) == 0xC0) {
        charLen = 2;  // 2-byte UTF-8
    } else if ((c & 0xF0) == 0xE0) {
        charLen = 3;  // 3-byte UTF-8
    } else if ((c & 0xF8) == 0xF0) {
        charLen = 4;  // 4-byte UTF-8
    }
    
    line.erase(charStart, charLen);
    m_dirty = true;
    return true;
}

bool TextBuffer::insertText(size_t lineNum, size_t column, const std::string& text) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (lineNum >= m_lines.size()) {
        return false;
    }
    
    // If text contains newlines, handle multi-line insertion
    if (text.find('\n') != std::string::npos || text.find('\r') != std::string::npos) {
        std::vector<std::string> textLines = splitLines(text);
        
        if (textLines.empty()) {
            return true;
        }
        
        std::string& line = m_lines[lineNum];
        
        // Clamp column
        if (column > line.length()) {
            column = line.length();
        }
        
        // Split current line at insertion point
        std::string beforeInsert = line.substr(0, column);
        std::string afterInsert = line.substr(column);
        
        // First line: append to current line
        m_lines[lineNum] = beforeInsert + textLines[0];
        
        // Insert middle lines
        for (size_t i = 1; i < textLines.size(); ++i) {
            m_lines.insert(m_lines.begin() + lineNum + i, textLines[i]);
        }
        
        // Last line: append remainder
        m_lines[lineNum + textLines.size() - 1] += afterInsert;
        
    } else {
        // Single line insertion
        std::string& line = m_lines[lineNum];
        
        if (column > line.length()) {
            column = line.length();
        }
        
        line.insert(column, text);
    }
    
    m_dirty = true;
    return true;
}

std::string TextBuffer::deleteRange(size_t startLine, size_t startColumn,
                                   size_t endLine, size_t endColumn)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Normalize range (ensure start <= end)
    if (startLine > endLine || (startLine == endLine && startColumn > endColumn)) {
        std::swap(startLine, endLine);
        std::swap(startColumn, endColumn);
    }
    
    if (startLine >= m_lines.size() || endLine >= m_lines.size()) {
        return "";
    }
    
    std::ostringstream deleted;
    
    if (startLine == endLine) {
        // Single line deletion
        std::string& line = m_lines[startLine];
        
        if (startColumn > line.length()) startColumn = line.length();
        if (endColumn > line.length()) endColumn = line.length();
        
        if (startColumn < endColumn) {
            deleted << line.substr(startColumn, endColumn - startColumn);
            line.erase(startColumn, endColumn - startColumn);
        }
        
    } else {
        // Multi-line deletion
        
        // Delete from start line
        std::string& startLineStr = m_lines[startLine];
        if (startColumn < startLineStr.length()) {
            deleted << startLineStr.substr(startColumn);
            startLineStr.erase(startColumn);
        }
        deleted << '\n';
        
        // Delete middle lines
        for (size_t i = startLine + 1; i < endLine; ++i) {
            deleted << m_lines[i] << '\n';
        }
        
        // Delete from end line
        std::string& endLineStr = m_lines[endLine];
        if (endColumn > endLineStr.length()) {
            endColumn = endLineStr.length();
        }
        
        deleted << endLineStr.substr(0, endColumn);
        std::string remainder = endLineStr.substr(endColumn);
        
        // Join start and end lines
        m_lines[startLine] += remainder;
        
        // Erase deleted lines
        m_lines.erase(m_lines.begin() + startLine + 1,
                     m_lines.begin() + endLine + 1);
    }
    
    ensureNonEmpty();
    m_dirty = true;
    return deleted.str();
}

char32_t TextBuffer::getChar(size_t lineNum, size_t column) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (lineNum >= m_lines.size()) {
        return 0;
    }
    
    const std::string& line = m_lines[lineNum];
    
    if (column >= line.length()) {
        return 0;
    }
    
    // Convert UTF-8 to UTF-32
    std::vector<char32_t> chars = utf8ToUtf32(line);
    
    // Column is byte offset, need to find character offset
    // For simplicity, just return the byte at that position as char32_t
    // (This is a simplified implementation; full UTF-8 navigation would be more complex)
    
    unsigned char c = static_cast<unsigned char>(line[column]);
    
    if ((c & 0x80) == 0) {
        return static_cast<char32_t>(c);
    }
    
    // For multi-byte UTF-8, we'd need to decode properly
    // Simplified: just return the first byte
    return static_cast<char32_t>(c);
}

// =============================================================================
// Undo/Redo
// =============================================================================

void TextBuffer::pushUndoState() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    TextBufferState state(m_lines, 0, 0);  // Cursor position handled externally
    m_undoStack.push_back(state);
    
    // Clear redo stack when new edit is made
    m_redoStack.clear();
    
    trimUndoStack();
}

bool TextBuffer::undo() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_undoStack.empty()) {
        return false;
    }
    
    // Save current state to redo stack
    TextBufferState currentState(m_lines, 0, 0);
    m_redoStack.push_back(currentState);
    
    // Restore previous state
    TextBufferState state = m_undoStack.back();
    m_undoStack.pop_back();
    
    m_lines = state.lines;
    ensureNonEmpty();
    
    return true;
}

bool TextBuffer::redo() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_redoStack.empty()) {
        return false;
    }
    
    // Save current state to undo stack
    TextBufferState currentState(m_lines, 0, 0);
    m_undoStack.push_back(currentState);
    
    // Restore redo state
    TextBufferState state = m_redoStack.back();
    m_redoStack.pop_back();
    
    m_lines = state.lines;
    ensureNonEmpty();
    trimUndoStack();
    
    return true;
}

bool TextBuffer::canUndo() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_undoStack.empty();
}

bool TextBuffer::canRedo() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_redoStack.empty();
}

void TextBuffer::clearUndoHistory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_undoStack.clear();
    m_redoStack.clear();
}

void TextBuffer::setMaxUndoSize(size_t maxSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxUndoSize = maxSize;
    trimUndoStack();
}

size_t TextBuffer::getUndoStackSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_undoStack.size();
}

// =============================================================================
// Utility
// =============================================================================

size_t TextBuffer::getCharacterCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t count = 0;
    for (const auto& line : m_lines) {
        count += line.length();
    }
    
    // Add newline characters (except for last line)
    if (!m_lines.empty()) {
        count += m_lines.size() - 1;
    }
    
    return count;
}

size_t TextBuffer::getByteSize() const {
    return getCharacterCount();  // In UTF-8, byte size ≈ character count
}

bool TextBuffer::isValidPosition(size_t lineNum, size_t column) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (lineNum >= m_lines.size()) {
        return false;
    }
    
    return column <= m_lines[lineNum].length();
}

void TextBuffer::clampPosition(size_t& lineNum, size_t& column) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (lineNum >= m_lines.size()) {
        lineNum = m_lines.size() > 0 ? m_lines.size() - 1 : 0;
    }
    
    if (lineNum < m_lines.size()) {
        size_t lineLen = m_lines[lineNum].length();
        if (column > lineLen) {
            column = lineLen;
        }
    } else {
        column = 0;
    }
}

// =============================================================================
// UTF-8 Conversion
// =============================================================================

std::vector<char32_t> TextBuffer::utf8ToUtf32(const std::string& utf8) {
    std::vector<char32_t> result;
    
    size_t i = 0;
    while (i < utf8.length()) {
        char32_t codepoint = 0;
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        
        if ((c & 0x80) == 0) {
            // 1-byte ASCII
            codepoint = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte UTF-8
            if (i + 1 < utf8.length()) {
                codepoint = ((c & 0x1F) << 6) |
                           (utf8[i+1] & 0x3F);
                i += 2;
            } else {
                break;
            }
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte UTF-8
            if (i + 2 < utf8.length()) {
                codepoint = ((c & 0x0F) << 12) |
                           ((utf8[i+1] & 0x3F) << 6) |
                           (utf8[i+2] & 0x3F);
                i += 3;
            } else {
                break;
            }
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte UTF-8
            if (i + 3 < utf8.length()) {
                codepoint = ((c & 0x07) << 18) |
                           ((utf8[i+1] & 0x3F) << 12) |
                           ((utf8[i+2] & 0x3F) << 6) |
                           (utf8[i+3] & 0x3F);
                i += 4;
            } else {
                break;
            }
        } else {
            // Invalid UTF-8, skip byte
            i += 1;
            continue;
        }
        
        result.push_back(codepoint);
    }
    
    return result;
}

std::string TextBuffer::utf32ToUtf8(char32_t codepoint) {
    std::string result;
    
    if (codepoint < 0x80) {
        // 1-byte ASCII
        result += static_cast<char>(codepoint);
    } else if (codepoint < 0x800) {
        // 2-byte UTF-8
        result += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
        result += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
        // 3-byte UTF-8
        result += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
        result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x110000) {
        // 4-byte UTF-8
        result += static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07));
        result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    
    return result;
}

std::vector<std::string> TextBuffer::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string currentLine;
    
    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];
        
        if (c == '\n') {
            lines.push_back(currentLine);
            currentLine.clear();
        } else if (c == '\r') {
            // Handle \r\n or standalone \r
            lines.push_back(currentLine);
            currentLine.clear();
            
            if (i + 1 < text.length() && text[i + 1] == '\n') {
                ++i;  // Skip the \n in \r\n
            }
        } else {
            currentLine += c;
        }
    }
    
    // Add last line (even if empty)
    lines.push_back(currentLine);
    
    return lines;
}

TextBuffer::LineEnding TextBuffer::detectLineEnding(const std::string& text) {
    // Count different line ending types
    size_t crlf = 0, lf = 0, cr = 0;
    
    for (size_t i = 0; i < text.length(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.length() && text[i + 1] == '\n') {
                ++crlf;
                ++i;  // Skip the \n
            } else {
                ++cr;
            }
        } else if (text[i] == '\n') {
            ++lf;
        }
    }
    
    // Return most common
    if (crlf > lf && crlf > cr) return LineEnding::CRLF;
    if (cr > lf && cr > crlf) return LineEnding::CR;
    return LineEnding::LF;  // Default
}

// =============================================================================
// Internal Helpers
// =============================================================================

void TextBuffer::ensureNonEmpty() {
    if (m_lines.empty()) {
        m_lines.push_back("");
    }
}

TextBufferState TextBuffer::captureState(size_t cursorLine, size_t cursorColumn) const {
    return TextBufferState(m_lines, cursorLine, cursorColumn);
}

void TextBuffer::restoreState(const TextBufferState& state) {
    m_lines = state.lines;
    ensureNonEmpty();
}

void TextBuffer::trimUndoStack() {
    if (m_maxUndoSize > 0) {
        while (m_undoStack.size() > m_maxUndoSize) {
            m_undoStack.pop_front();
        }
    }
}

} // namespace SuperTerminal