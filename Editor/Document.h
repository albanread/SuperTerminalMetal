//
//  Document.h
//  SuperTerminal Framework - Document Class
//
//  A live representation of a script managed by ScriptDatabase
//  The Document is the single source of truth for editing operations
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "ScriptDatabase.h"

namespace SuperTerminal {

// Forward declarations
class ScriptDatabase;

// =============================================================================
// Document - Live representation of a script
// =============================================================================
//
// Design:
//   ScriptDatabase (persistent storage)
//         ↓
//   Document (live instance in memory)
//         ↓
//   TextEditor (view/controller for editing)
//
// The Document:
// - Loads from ScriptDatabase on open
// - Keeps content in memory (vector of lines)
// - Tracks modifications (dirty flag)
// - Saves back to ScriptDatabase
// - Notifies observers of changes
//
// The TextEditor:
// - Displays 25 lines at a time from Document
// - Sends edit commands to Document
// - Renders from Document lines (no separate buffer)
//

class Document {
public:
    // Observer callback for document changes
    // Called when document content changes
    using ChangeCallback = std::function<void()>;

    // Constructor/Destructor
    Document();
    ~Document();

    // No copy (represents unique document)
    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    // =========================================================================
    // Document Lifecycle
    // =========================================================================

    /// Open a document from the database
    /// @param database Database to load from
    /// @param name Script name
    /// @param language Script language
    /// @return true on success
    bool open(std::shared_ptr<ScriptDatabase> database,
              const std::string& name,
              ScriptLanguage language);

    /// Create a new empty document
    /// @param database Database for future saves
    /// @param name Script name
    /// @param language Script language
    void createNew(std::shared_ptr<ScriptDatabase> database,
                   const std::string& name,
                   ScriptLanguage language);

    /// Save document back to database
    /// @return true on success
    bool save();

    /// Save document to a different name
    /// @param newName New script name
    /// @return true on success
    bool saveAs(const std::string& newName);

    /// Close document (prompts to save if dirty)
    void close();

    /// Check if document is open
    bool isOpen() const { return m_isOpen; }

    /// Get document name
    std::string getName() const { return m_name; }

    /// Get document language
    ScriptLanguage getLanguage() const { return m_language; }

    // =========================================================================
    // Content Access (Read-Only)
    // =========================================================================

    /// Get total line count
    size_t getLineCount() const { return m_lines.size(); }

    /// Get a specific line
    /// @param lineNum Line number (0-based)
    /// @return Line content (without newline), or empty if out of range
    std::string getLine(size_t lineNum) const;

    /// Get a range of lines
    /// @param startLine Starting line (0-based, inclusive)
    /// @param endLine Ending line (0-based, exclusive)
    /// @return Vector of lines
    std::vector<std::string> getLines(size_t startLine, size_t endLine) const;

    /// Get entire document as text
    /// @return Full text with newlines
    std::string getText() const;

    /// Check if document is empty
    bool isEmpty() const { return m_lines.empty() || (m_lines.size() == 1 && m_lines[0].empty()); }

    // =========================================================================
    // Content Modification
    // =========================================================================

    /// Replace a line
    /// @param lineNum Line number (0-based)
    /// @param content New line content (without newline)
    /// @return true on success
    bool setLine(size_t lineNum, const std::string& content);

    /// Insert a new line
    /// @param lineNum Position to insert (0-based)
    /// @param content Line content (without newline)
    /// @return true on success
    bool insertLine(size_t lineNum, const std::string& content);

    /// Delete a line
    /// @param lineNum Line number (0-based)
    /// @return true on success
    bool deleteLine(size_t lineNum);

    /// Insert text at a position
    /// @param lineNum Line number (0-based)
    /// @param column Column position (0-based)
    /// @param text Text to insert (may contain newlines)
    /// @return true on success
    bool insertText(size_t lineNum, size_t column, const std::string& text);

    /// Delete text range
    /// @param startLine Starting line (0-based)
    /// @param startColumn Starting column (0-based)
    /// @param endLine Ending line (0-based)
    /// @param endColumn Ending column (0-based)
    /// @return true on success
    bool deleteRange(size_t startLine, size_t startColumn,
                     size_t endLine, size_t endColumn);

    /// Replace entire document content
    /// @param text New content
    void setText(const std::string& text);

    /// Clear document
    void clear();

    // =========================================================================
    // Dirty State Tracking
    // =========================================================================

    /// Check if document has unsaved changes
    bool isDirty() const { return m_isDirty; }

    /// Mark document as clean (after save)
    void markClean() { m_isDirty = false; }

    /// Mark document as dirty (after edit)
    void markDirty() { m_isDirty = true; notifyChange(); }

    // =========================================================================
    // Change Notifications
    // =========================================================================

    /// Register a change callback
    /// @param callback Function to call when document changes
    void setChangeCallback(ChangeCallback callback) { m_changeCallback = callback; }

    /// Clear change callback
    void clearChangeCallback() { m_changeCallback = nullptr; }

    // =========================================================================
    // Utility
    // =========================================================================

    /// Get last error message
    std::string getLastError() const { return m_lastError; }

private:
    // =========================================================================
    // Internal Helpers
    // =========================================================================

    /// Load content from database
    bool loadFromDatabase();

    /// Set error message
    void setError(const std::string& message) { m_lastError = message; }

    /// Notify observers of change
    void notifyChange() {
        if (m_changeCallback) {
            m_changeCallback();
        }
    }

    /// Split text into lines
    static std::vector<std::string> splitLines(const std::string& text);

    /// Join lines into text
    static std::string joinLines(const std::vector<std::string>& lines);

    // =========================================================================
    // Member Variables
    // =========================================================================

    // Document identity
    std::string m_name;                             // Script name
    ScriptLanguage m_language;                      // Programming language
    bool m_isOpen;                                  // Is document open?

    // Content storage
    std::vector<std::string> m_lines;               // Document lines (THE SINGLE SOURCE)
    bool m_isDirty;                                 // Has unsaved changes?

    // Database connection
    std::shared_ptr<ScriptDatabase> m_database;     // Database for persistence

    // Observers
    ChangeCallback m_changeCallback;                // Called on content change

    // Error tracking
    std::string m_lastError;                        // Last error message
};

} // namespace SuperTerminal

#endif // DOCUMENT_H