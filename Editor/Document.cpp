//
//  Document.cpp
//  SuperTerminal Framework - Document Implementation
//
//  A live representation of a script managed by ScriptDatabase
//  The Document is the single source of truth for editing operations
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "Document.h"
#include <sstream>
#include <algorithm>

namespace SuperTerminal {

// =============================================================================
// Helper Functions
// =============================================================================

std::vector<std::string> Document::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string currentLine;

    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];

        if (c == '\n') {
            lines.push_back(currentLine);
            currentLine.clear();
        } else if (c == '\r') {
            lines.push_back(currentLine);
            currentLine.clear();
            // Handle \r\n
            if (i + 1 < text.length() && text[i + 1] == '\n') {
                ++i;
            }
        } else {
            currentLine += c;
        }
    }

    // Add last line (even if empty)
    lines.push_back(currentLine);

    return lines;
}

std::string Document::joinLines(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return "";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < lines.size(); ++i) {
        oss << lines[i];
        if (i < lines.size() - 1) {
            oss << '\n';
        }
    }

    return oss.str();
}

// =============================================================================
// Document Implementation
// =============================================================================

Document::Document()
    : m_language(ScriptLanguage::BASIC)
    , m_isOpen(false)
    , m_isDirty(false)
{
    // Start with one empty line
    m_lines.push_back("");
}

Document::~Document() {
    close();
}

// =============================================================================
// Document Lifecycle
// =============================================================================

bool Document::open(std::shared_ptr<ScriptDatabase> database,
                    const std::string& name,
                    ScriptLanguage language) {
    if (!database) {
        setError("No database provided");
        return false;
    }

    if (name.empty()) {
        setError("No script name provided");
        return false;
    }

    // Close any existing document
    if (m_isOpen) {
        close();
    }

    // Set document identity
    m_database = database;
    m_name = name;
    m_language = language;

    // Load content from database
    if (!loadFromDatabase()) {
        m_isOpen = false;
        return false;
    }

    m_isOpen = true;
    m_isDirty = false;
    notifyChange();

    return true;
}

void Document::createNew(std::shared_ptr<ScriptDatabase> database,
                         const std::string& name,
                         ScriptLanguage language) {
    // Close any existing document
    if (m_isOpen) {
        close();
    }

    printf("[Document] === createNew() called ===\n");
    printf("[Document]   Name: '%s'\n", name.c_str());
    printf("[Document]   Language: %d\n", (int)language);

    m_database = database;
    m_name = name;
    m_language = language;

    // Start with one empty line
    m_lines.clear();
    m_lines.push_back("");

    m_isOpen = true;
    m_isDirty = true;  // New document is dirty until saved
    
    printf("[Document] ✓ Document created - name='%s', language=%d, isOpen=%d, isDirty=%d\n",
          m_name.c_str(), (int)m_language, m_isOpen, m_isDirty);
    
    notifyChange();
}

bool Document::save() {
    if (!m_isOpen) {
        setError("Document not open");
        return false;
    }

    if (!m_database) {
        setError("No database connection");
        return false;
    }

    // Join lines into text
    std::string content = joinLines(m_lines);

    // DEBUG: Log what we're saving
    printf("[Document] === save() called ===\n");
    printf("[Document]   Name: '%s'\n", m_name.c_str());
    printf("[Document]   Language: %d\n", (int)m_language);
    printf("[Document]   Content length: %zu\n", content.length());

    // Save to database
    if (!m_database->saveScript(m_name, m_language, content, "")) {
        setError("Failed to save to database: " + m_database->getLastError());
        printf("[Document] ERROR: Save failed: %s\n", m_database->getLastError().c_str());
        return false;
    }

    printf("[Document] ✓ Save successful\n");
    m_isDirty = false;
    notifyChange();

    return true;
}

bool Document::saveAs(const std::string& newName) {
    if (!m_isOpen) {
        setError("Document not open");
        return false;
    }

    if (newName.empty()) {
        setError("No script name provided");
        return false;
    }

    printf("[Document] === saveAs() called ===\n");
    printf("[Document]   Old name: '%s'\n", m_name.c_str());
    printf("[Document]   New name: '%s'\n", newName.c_str());
    printf("[Document]   Language: %d (should be preserved)\n", (int)m_language);

    // Update name
    m_name = newName;

    // Save with new name
    return save();
}

void Document::close() {
    if (m_isOpen) {
        // TODO: Prompt to save if dirty
        m_isOpen = false;
        m_isDirty = false;
        m_lines.clear();
        m_lines.push_back("");
        m_database.reset();
        notifyChange();
    }
}

bool Document::loadFromDatabase() {
    if (!m_database) {
        setError("No database connection");
        return false;
    }

    std::string content;
    if (!m_database->loadScript(m_name, m_language, content)) {
        setError("Failed to load from database: " + m_database->getLastError());
        return false;
    }

    // Split into lines
    m_lines = splitLines(content);

    // Ensure at least one line
    if (m_lines.empty()) {
        m_lines.push_back("");
    }

    return true;
}

// =============================================================================
// Content Access (Read-Only)
// =============================================================================

std::string Document::getLine(size_t lineNum) const {
    if (lineNum >= m_lines.size()) {
        return "";
    }
    return m_lines[lineNum];
}

std::vector<std::string> Document::getLines(size_t startLine, size_t endLine) const {
    std::vector<std::string> result;

    if (startLine >= m_lines.size()) {
        return result;
    }

    size_t end = std::min(endLine, m_lines.size());

    for (size_t i = startLine; i < end; ++i) {
        result.push_back(m_lines[i]);
    }

    return result;
}

std::string Document::getText() const {
    return joinLines(m_lines);
}

// =============================================================================
// Content Modification
// =============================================================================

bool Document::setLine(size_t lineNum, const std::string& content) {
    if (lineNum >= m_lines.size()) {
        setError("Line number out of range");
        return false;
    }

    m_lines[lineNum] = content;
    markDirty();

    return true;
}

bool Document::insertLine(size_t lineNum, const std::string& content) {
    if (lineNum > m_lines.size()) {
        setError("Line number out of range");
        return false;
    }

    m_lines.insert(m_lines.begin() + lineNum, content);
    markDirty();

    return true;
}

bool Document::deleteLine(size_t lineNum) {
    if (lineNum >= m_lines.size()) {
        setError("Line number out of range");
        return false;
    }

    m_lines.erase(m_lines.begin() + lineNum);

    // Ensure at least one line
    if (m_lines.empty()) {
        m_lines.push_back("");
    }

    markDirty();

    return true;
}

bool Document::insertText(size_t lineNum, size_t column, const std::string& text) {
    if (lineNum >= m_lines.size()) {
        setError("Line number out of range");
        return false;
    }

    std::string& line = m_lines[lineNum];

    // Clamp column to line length
    if (column > line.length()) {
        column = line.length();
    }

    // Check if text contains newlines
    if (text.find('\n') != std::string::npos || text.find('\r') != std::string::npos) {
        // Multi-line insertion - split and insert
        std::vector<std::string> textLines = splitLines(text);

        if (textLines.empty()) {
            return true;
        }

        // Split current line at insertion point
        std::string before = line.substr(0, column);
        std::string after = line.substr(column);

        // First line: append to current line
        line = before + textLines[0];

        // Insert middle lines
        for (size_t i = 1; i < textLines.size() - 1; ++i) {
            m_lines.insert(m_lines.begin() + lineNum + i, textLines[i]);
        }

        // Last line: prepend to after text
        if (textLines.size() > 1) {
            m_lines.insert(m_lines.begin() + lineNum + textLines.size() - 1,
                          textLines[textLines.size() - 1] + after);
        } else {
            // Single line with newline at end
            line += after;
        }
    } else {
        // Single-line insertion
        line.insert(column, text);
    }

    markDirty();

    return true;
}

bool Document::deleteRange(size_t startLine, size_t startColumn,
                           size_t endLine, size_t endColumn) {
    if (startLine >= m_lines.size() || endLine >= m_lines.size()) {
        setError("Line number out of range");
        return false;
    }

    if (startLine == endLine) {
        // Single line deletion
        std::string& line = m_lines[startLine];
        if (startColumn > line.length()) {
            startColumn = line.length();
        }
        if (endColumn > line.length()) {
            endColumn = line.length();
        }

        if (startColumn < endColumn) {
            line.erase(startColumn, endColumn - startColumn);
        }
    } else {
        // Multi-line deletion
        std::string& firstLine = m_lines[startLine];
        std::string& lastLine = m_lines[endLine];

        // Keep text before start and after end
        if (startColumn > firstLine.length()) {
            startColumn = firstLine.length();
        }
        if (endColumn > lastLine.length()) {
            endColumn = lastLine.length();
        }

        std::string before = firstLine.substr(0, startColumn);
        std::string after = lastLine.substr(endColumn);

        // Delete lines in between
        m_lines.erase(m_lines.begin() + startLine, m_lines.begin() + endLine + 1);

        // Insert combined line
        m_lines.insert(m_lines.begin() + startLine, before + after);
    }

    markDirty();

    return true;
}

void Document::setText(const std::string& text) {
    m_lines = splitLines(text);

    // Ensure at least one line
    if (m_lines.empty()) {
        m_lines.push_back("");
    }

    markDirty();
}

void Document::clear() {
    m_lines.clear();
    m_lines.push_back("");
    markDirty();
}

} // namespace SuperTerminal