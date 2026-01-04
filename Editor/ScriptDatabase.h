//
//  ScriptDatabase.h
//  SuperTerminal Framework - Script Database System
//
//  SQLite-backed storage for user scripts
//  Scripts are identified by (name, language) pair
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef SCRIPT_DATABASE_H
#define SCRIPT_DATABASE_H

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

// Forward declare SQLite types
struct sqlite3;
struct sqlite3_stmt;

namespace SuperTerminal {

// =============================================================================
// ScriptLanguage - Supported programming languages
// =============================================================================

enum class ScriptLanguage {
    LUA,
    JAVASCRIPT,
    BASIC,
    SCHEME,
    ABC,
    VOICESCRIPT
};

// Convert language to/from string
const char* scriptLanguageToString(ScriptLanguage lang);
ScriptLanguage stringToScriptLanguage(const char* str);

// =============================================================================
// ScriptMetadata - Information about a stored script
// =============================================================================

struct ScriptMetadata {
    int64_t id;                     // Database primary key
    std::string name;               // Script name (unique per language)
    ScriptLanguage language;        // Programming language
    int64_t createdAt;             // Unix timestamp (seconds)
    int64_t modifiedAt;            // Unix timestamp (seconds)
    std::string description;        // Optional description
    size_t contentLength;           // Content size in bytes
    
    ScriptMetadata()
        : id(0)
        , language(ScriptLanguage::LUA)
        , createdAt(0)
        , modifiedAt(0)
        , contentLength(0)
    {}
};

// =============================================================================
// ScriptDatabase - Script storage and retrieval
// =============================================================================

class ScriptDatabase {
public:
    // Constructor/Destructor
    ScriptDatabase();
    explicit ScriptDatabase(const std::string& dbPath);
    ~ScriptDatabase();
    
    // No copy (SQLite handles can't be copied)
    ScriptDatabase(const ScriptDatabase&) = delete;
    ScriptDatabase& operator=(const ScriptDatabase&) = delete;
    
    // Move is OK
    ScriptDatabase(ScriptDatabase&& other) noexcept;
    ScriptDatabase& operator=(ScriptDatabase&& other) noexcept;
    
    // =========================================================================
    // Database Lifecycle
    // =========================================================================
    
    /// Open database at path
    /// @param dbPath Path to SQLite database file
    /// @param readOnly Open in read-only mode
    /// @return true on success
    bool open(const std::string& dbPath, bool readOnly = false);
    
    /// Close database
    void close();
    
    /// Check if database is open
    bool isOpen() const { return m_db != nullptr; }
    
    /// Create database schema (call after open on new database)
    /// @return true on success
    bool createSchema();
    
    /// Get database path
    std::string getPath() const { return m_dbPath; }
    
    // =========================================================================
    // Script Operations
    // =========================================================================
    
    /// Save script to database (insert or update)
    /// @param name Script name (unique per language)
    /// @param language Programming language
    /// @param content Script source code
    /// @param description Optional description
    /// @return true on success
    bool saveScript(const std::string& name,
                   ScriptLanguage language,
                   const std::string& content,
                   const std::string& description = "");
    
    /// Load script content from database
    /// @param name Script name
    /// @param language Programming language
    /// @param outContent Output: script source code
    /// @return true on success
    bool loadScript(const std::string& name,
                   ScriptLanguage language,
                   std::string& outContent);
    
    /// Delete script from database
    /// @param name Script name
    /// @param language Programming language
    /// @return true on success
    bool deleteScript(const std::string& name, ScriptLanguage language);
    
    /// Rename script (within same language)
    /// @param oldName Current script name
    /// @param newName New script name
    /// @param language Programming language
    /// @return true on success
    bool renameScript(const std::string& oldName,
                     const std::string& newName,
                     ScriptLanguage language);
    
    /// Check if script exists
    /// @param name Script name
    /// @param language Programming language
    /// @return true if script exists
    bool scriptExists(const std::string& name, ScriptLanguage language);
    
    // =========================================================================
    // Script Metadata
    // =========================================================================
    
    /// Get script metadata
    /// @param name Script name
    /// @param language Programming language
    /// @param outMetadata Output: script metadata
    /// @return true on success
    bool getMetadata(const std::string& name,
                    ScriptLanguage language,
                    ScriptMetadata& outMetadata);
    
    /// Update script description
    /// @param name Script name
    /// @param language Programming language
    /// @param description New description
    /// @return true on success
    bool updateDescription(const std::string& name,
                          ScriptLanguage language,
                          const std::string& description);
    
    // =========================================================================
    // Listing and Searching
    // =========================================================================
    
    /// List all scripts (optionally filtered by language)
    /// @param language Language filter (use LUA and check all if needed)
    /// @param allLanguages If true, ignore language filter
    /// @return Vector of script metadata
    std::vector<ScriptMetadata> listScripts(ScriptLanguage language = ScriptLanguage::LUA,
                                           bool allLanguages = false);
    
    /// Search scripts by name pattern (SQL LIKE)
    /// @param pattern SQL LIKE pattern (e.g., "%game%")
    /// @param language Language filter
    /// @param allLanguages If true, search all languages
    /// @return Vector of matching script metadata
    std::vector<ScriptMetadata> searchScripts(const std::string& pattern,
                                              ScriptLanguage language = ScriptLanguage::LUA,
                                              bool allLanguages = false);
    
    /// Get total script count
    /// @param language Language filter
    /// @param allLanguages If true, count all languages
    /// @return Number of scripts
    size_t getScriptCount(ScriptLanguage language = ScriptLanguage::LUA,
                         bool allLanguages = false);
    
    // =========================================================================
    // Utility
    // =========================================================================
    
    /// Get last error message
    std::string getLastError() const { return m_lastError; }
    
    /// Get current Unix timestamp
    static int64_t getCurrentTimestamp();
    
    /// Validate script name (alphanumeric, underscore, hyphen, dot only)
    static bool isValidScriptName(const std::string& name);
    
    /// Generate unique name if name already exists
    std::string generateUniqueName(const std::string& baseName,
                                   ScriptLanguage language);

    // =========================================================================
    // Line-Based Document Access (for Editor)
    // =========================================================================
    
    /// Get total line count for a script
    /// @param name Script name
    /// @param language Programming language
    /// @return Number of lines (0 if script doesn't exist)
    size_t getLineCount(const std::string& name, ScriptLanguage language);
    
    /// Get a specific line from a script
    /// @param name Script name
    /// @param language Programming language
    /// @param lineNum Line number (0-based)
    /// @param outLine Output: line content (without newline)
    /// @return true on success
    bool getLine(const std::string& name,
                 ScriptLanguage language,
                 size_t lineNum,
                 std::string& outLine);
    
    /// Get a range of lines from a script
    /// @param name Script name
    /// @param language Programming language
    /// @param startLine Starting line number (0-based, inclusive)
    /// @param endLine Ending line number (0-based, exclusive)
    /// @param outLines Output: vector of line contents (without newlines)
    /// @return true on success
    bool getLines(const std::string& name,
                  ScriptLanguage language,
                  size_t startLine,
                  size_t endLine,
                  std::vector<std::string>& outLines);
    
    /// Replace a line in a script
    /// @param name Script name
    /// @param language Programming language
    /// @param lineNum Line number (0-based)
    /// @param lineContent New line content (without newline)
    /// @return true on success
    bool setLine(const std::string& name,
                 ScriptLanguage language,
                 size_t lineNum,
                 const std::string& lineContent);
    
    /// Insert a line into a script
    /// @param name Script name
    /// @param language Programming language
    /// @param lineNum Line number (0-based) - line will be inserted before this position
    /// @param lineContent Line content (without newline)
    /// @return true on success
    bool insertLine(const std::string& name,
                    ScriptLanguage language,
                    size_t lineNum,
                    const std::string& lineContent);
    
    /// Delete a line from a script
    /// @param name Script name
    /// @param language Programming language
    /// @param lineNum Line number (0-based)
    /// @return true on success
    bool deleteLine(const std::string& name,
                    ScriptLanguage language,
                    size_t lineNum);
    
    /// Get the currently active document name (for editor)
    std::string getActiveDocument() const { return m_activeDocumentName; }
    
    /// Get the currently active document language
    ScriptLanguage getActiveLanguage() const { return m_activeDocumentLanguage; }
    
    /// Set the active document (for editor focus)
    void setActiveDocument(const std::string& name, ScriptLanguage language) {
        m_activeDocumentName = name;
        m_activeDocumentLanguage = language;
    }
    
    /// Clear active document
    void clearActiveDocument() {
        m_activeDocumentName.clear();
        m_activeDocumentLanguage = ScriptLanguage::BASIC;
    }

private:
    // =========================================================================
    // Internal Helpers
    // =========================================================================
    
    /// Set last error message
    void setError(const std::string& message);
    
    /// Execute SQL without results
    bool execute(const char* sql);
    
    /// Prepare SQL statement
    sqlite3_stmt* prepare(const char* sql);
    
    /// Finalize statement
    void finalize(sqlite3_stmt* stmt);
    
    /// Read metadata from prepared statement
    ScriptMetadata readMetadata(sqlite3_stmt* stmt);
    
    /// Begin transaction
    bool beginTransaction();
    
    /// Commit transaction
    bool commitTransaction();
    
    /// Rollback transaction
    bool rollbackTransaction();
    
    // =========================================================================
    // Member Variables
    // =========================================================================
    
    sqlite3* m_db;                  // SQLite database handle
    std::string m_dbPath;           // Database file path
    std::string m_lastError;        // Last error message
    bool m_readOnly;                // Read-only mode flag
    
    // Active document tracking (for editor)
    std::string m_activeDocumentName;
    ScriptLanguage m_activeDocumentLanguage;
};

} // namespace SuperTerminal

#endif // SCRIPT_DATABASE_H