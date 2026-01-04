//
//  ScriptDatabase.mm
//  SuperTerminal Framework - Script Database Implementation
//
//  SQLite-backed storage for user scripts
//  Scripts are identified by (name, language) pair
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "ScriptDatabase.h"
#include <sqlite3.h>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <cctype>
#import <Foundation/Foundation.h>

namespace SuperTerminal {

// =============================================================================
// Language String Conversion
// =============================================================================

const char* scriptLanguageToString(ScriptLanguage lang) {
    switch (lang) {
        case ScriptLanguage::LUA:         return "lua";
        case ScriptLanguage::JAVASCRIPT:  return "javascript";
        case ScriptLanguage::BASIC:       return "basic";
        case ScriptLanguage::SCHEME:      return "scheme";
        case ScriptLanguage::ABC:         return "abc";
        case ScriptLanguage::VOICESCRIPT: return "voicescript";
        default:                          return "unknown";
    }
}

ScriptLanguage stringToScriptLanguage(const char* str) {
    if (!str) return ScriptLanguage::LUA;

    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    if (s == "lua")         return ScriptLanguage::LUA;
    if (s == "javascript")  return ScriptLanguage::JAVASCRIPT;
    if (s == "js")          return ScriptLanguage::JAVASCRIPT;
    if (s == "basic")       return ScriptLanguage::BASIC;
    if (s == "scheme")      return ScriptLanguage::SCHEME;
    if (s == "abc")         return ScriptLanguage::ABC;
    if (s == "voicescript") return ScriptLanguage::VOICESCRIPT;
    if (s == "vscript")     return ScriptLanguage::VOICESCRIPT;

    return ScriptLanguage::LUA; // Default
}

// =============================================================================
// ScriptDatabase Implementation
// =============================================================================

ScriptDatabase::ScriptDatabase()
    : m_db(nullptr)
    , m_readOnly(false)
{
}

ScriptDatabase::ScriptDatabase(const std::string& dbPath)
    : m_db(nullptr)
    , m_readOnly(false)
{
    open(dbPath);
}

ScriptDatabase::~ScriptDatabase() {
    close();
}

// Move constructor
ScriptDatabase::ScriptDatabase(ScriptDatabase&& other) noexcept
    : m_db(other.m_db)
    , m_dbPath(std::move(other.m_dbPath))
    , m_lastError(std::move(other.m_lastError))
    , m_readOnly(other.m_readOnly)
{
    other.m_db = nullptr;
}

// Move assignment
ScriptDatabase& ScriptDatabase::operator=(ScriptDatabase&& other) noexcept {
    if (this != &other) {
        close();
        m_db = other.m_db;
        m_dbPath = std::move(other.m_dbPath);
        m_lastError = std::move(other.m_lastError);
        m_readOnly = other.m_readOnly;
        other.m_db = nullptr;
    }
    return *this;
}

// =============================================================================
// Database Lifecycle
// =============================================================================

bool ScriptDatabase::open(const std::string& dbPath, bool readOnly) {
    if (m_db) {
        close();
    }

    m_dbPath = dbPath;
    m_readOnly = readOnly;

    int flags = readOnly ? SQLITE_OPEN_READONLY : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);

    int result = sqlite3_open_v2(dbPath.c_str(), &m_db, flags, nullptr);

    if (result != SQLITE_OK) {
        setError(std::string("Failed to open database: ") +
                (m_db ? sqlite3_errmsg(m_db) : "unknown error"));
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        return false;
    }

    // Enable foreign keys
    execute("PRAGMA foreign_keys = ON;");

    // If not read-only and database is new, create schema
    if (!readOnly) {
        // Check if scripts table exists
        sqlite3_stmt* stmt = prepare(
            "SELECT name FROM sqlite_master WHERE type='table' AND name='scripts';"
        );

        bool tableExists = false;
        if (stmt) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                tableExists = true;
            }
            finalize(stmt);
        }

        if (!tableExists) {
            if (!createSchema()) {
                close();
                return false;
            }
        }
    }

    return true;
}

void ScriptDatabase::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
    m_dbPath.clear();
}

bool ScriptDatabase::createSchema() {
    if (!m_db || m_readOnly) {
        setError("Cannot create schema: database not open or read-only");
        return false;
    }

    const char* schema = R"SQL(
        CREATE TABLE IF NOT EXISTS scripts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            language TEXT NOT NULL,
            content TEXT NOT NULL,
            description TEXT DEFAULT '',
            created_at INTEGER NOT NULL,
            modified_at INTEGER NOT NULL,
            UNIQUE(name, language)
        );

        CREATE INDEX IF NOT EXISTS idx_scripts_name ON scripts(name);
        CREATE INDEX IF NOT EXISTS idx_scripts_language ON scripts(language);
        CREATE INDEX IF NOT EXISTS idx_scripts_modified ON scripts(modified_at DESC);
    )SQL";

    return execute(schema);
}

// =============================================================================
// Script Operations
// =============================================================================

bool ScriptDatabase::saveScript(const std::string& name,
                                ScriptLanguage language,
                                const std::string& content,
                                const std::string& description)
{
    NSLog(@"[ScriptDatabase] === saveScript called ===");
    NSLog(@"[ScriptDatabase] name: '%s'", name.c_str());
    NSLog(@"[ScriptDatabase] language: '%s'", scriptLanguageToString(language));
    NSLog(@"[ScriptDatabase] content length: %zu bytes", content.length());

    if (!m_db || m_readOnly) {
        setError("Cannot save: database not open or read-only");
        NSLog(@"[ScriptDatabase] ERROR: Database not open or read-only");
        return false;
    }

    if (!isValidScriptName(name)) {
        setError("Invalid script name: must contain only letters, numbers, underscore, hyphen, and dot");
        NSLog(@"[ScriptDatabase] ERROR: Invalid script name: '%s'", name.c_str());
        return false;
    }

    NSLog(@"[ScriptDatabase] Script name is valid, proceeding with save");
    int64_t timestamp = getCurrentTimestamp();
    const char* langStr = scriptLanguageToString(language);

    // Use INSERT OR REPLACE to handle both new and existing scripts
    const char* sql = R"SQL(
        INSERT INTO scripts (name, language, content, description, created_at, modified_at)
        VALUES (?, ?, ?, ?, ?, ?)
        ON CONFLICT(name, language) DO UPDATE SET
            content = excluded.content,
            description = excluded.description,
            modified_at = excluded.modified_at;
    )SQL";

    sqlite3_stmt* stmt = prepare(sql);
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, langStr, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, timestamp);
    sqlite3_bind_int64(stmt, 6, timestamp);

    int result = sqlite3_step(stmt);
    finalize(stmt);

    if (result != SQLITE_DONE) {
        setError(std::string("Failed to save script: ") + sqlite3_errmsg(m_db));
        NSLog(@"[ScriptDatabase] ERROR: SQLite error: %s", sqlite3_errmsg(m_db));
        return false;
    }

    NSLog(@"[ScriptDatabase] ✓ Script saved successfully to database: '%s'", name.c_str());
    return true;
}

bool ScriptDatabase::loadScript(const std::string& name,
                               ScriptLanguage language,
                               std::string& outContent)
{
    if (!m_db) {
        setError("Cannot load: database not open");
        return false;
    }

    const char* langStr = scriptLanguageToString(language);

    const char* sql = "SELECT content FROM scripts WHERE name = ? AND language = ?;";

    sqlite3_stmt* stmt = prepare(sql);
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, langStr, -1, SQLITE_STATIC);

    int result = sqlite3_step(stmt);

    if (result == SQLITE_ROW) {
        const char* content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (content) {
            outContent = content;
        } else {
            outContent.clear();
        }
        finalize(stmt);
        return true;
    } else {
        finalize(stmt);
        setError("Script not found: " + name);
        return false;
    }
}

bool ScriptDatabase::deleteScript(const std::string& name, ScriptLanguage language) {
    if (!m_db || m_readOnly) {
        setError("Cannot delete: database not open or read-only");
        return false;
    }

    const char* langStr = scriptLanguageToString(language);

    const char* sql = "DELETE FROM scripts WHERE name = ? AND language = ?;";

    sqlite3_stmt* stmt = prepare(sql);
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, langStr, -1, SQLITE_STATIC);

    int result = sqlite3_step(stmt);
    finalize(stmt);

    if (result != SQLITE_DONE) {
        setError(std::string("Failed to delete script: ") + sqlite3_errmsg(m_db));
        return false;
    }

    if (sqlite3_changes(m_db) == 0) {
        setError("Script not found: " + name);
        return false;
    }

    return true;
}

bool ScriptDatabase::renameScript(const std::string& oldName,
                                  const std::string& newName,
                                  ScriptLanguage language)
{
    if (!m_db || m_readOnly) {
        setError("Cannot rename: database not open or read-only");
        return false;
    }

    if (!isValidScriptName(newName)) {
        setError("Invalid script name: must contain only letters, numbers, underscore, hyphen, and dot");
        return false;
    }

    // Check if new name already exists
    if (scriptExists(newName, language)) {
        setError("Script with new name already exists: " + newName);
        return false;
    }

    const char* langStr = scriptLanguageToString(language);

    const char* sql = "UPDATE scripts SET name = ?, modified_at = ? WHERE name = ? AND language = ?;";

    sqlite3_stmt* stmt = prepare(sql);
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, newName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, getCurrentTimestamp());
    sqlite3_bind_text(stmt, 3, oldName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, langStr, -1, SQLITE_STATIC);

    int result = sqlite3_step(stmt);
    finalize(stmt);

    if (result != SQLITE_DONE) {
        setError(std::string("Failed to rename script: ") + sqlite3_errmsg(m_db));
        return false;
    }

    if (sqlite3_changes(m_db) == 0) {
        setError("Script not found: " + oldName);
        return false;
    }

    return true;
}

bool ScriptDatabase::scriptExists(const std::string& name, ScriptLanguage language) {
    if (!m_db) return false;

    const char* langStr = scriptLanguageToString(language);

    const char* sql = "SELECT 1 FROM scripts WHERE name = ? AND language = ? LIMIT 1;";

    sqlite3_stmt* stmt = prepare(sql);
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, langStr, -1, SQLITE_STATIC);

    int result = sqlite3_step(stmt);
    finalize(stmt);

    return (result == SQLITE_ROW);
}

// =============================================================================
// Script Metadata
// =============================================================================

bool ScriptDatabase::getMetadata(const std::string& name,
                                ScriptLanguage language,
                                ScriptMetadata& outMetadata)
{
    if (!m_db) {
        setError("Cannot get metadata: database not open");
        return false;
    }

    const char* langStr = scriptLanguageToString(language);

    const char* sql = R"SQL(
        SELECT id, name, language, description, created_at, modified_at, LENGTH(content)
        FROM scripts WHERE name = ? AND language = ?;
    )SQL";

    sqlite3_stmt* stmt = prepare(sql);
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, langStr, -1, SQLITE_STATIC);

    int result = sqlite3_step(stmt);

    if (result == SQLITE_ROW) {
        outMetadata = readMetadata(stmt);
        finalize(stmt);
        return true;
    } else {
        finalize(stmt);
        setError("Script not found: " + name);
        return false;
    }
}

bool ScriptDatabase::updateDescription(const std::string& name,
                                       ScriptLanguage language,
                                       const std::string& description)
{
    if (!m_db || m_readOnly) {
        setError("Cannot update: database not open or read-only");
        return false;
    }

    const char* langStr = scriptLanguageToString(language);

    const char* sql = "UPDATE scripts SET description = ?, modified_at = ? WHERE name = ? AND language = ?;";

    sqlite3_stmt* stmt = prepare(sql);
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, getCurrentTimestamp());
    sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, langStr, -1, SQLITE_STATIC);

    int result = sqlite3_step(stmt);
    finalize(stmt);

    if (result != SQLITE_DONE) {
        setError(std::string("Failed to update description: ") + sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

// =============================================================================
// Listing and Searching
// =============================================================================

std::vector<ScriptMetadata> ScriptDatabase::listScripts(ScriptLanguage language,
                                                        bool allLanguages)
{
    std::vector<ScriptMetadata> results;

    if (!m_db) {
        setError("Cannot list: database not open");
        return results;
    }

    std::string sql = R"SQL(
        SELECT id, name, language, description, created_at, modified_at, LENGTH(content)
        FROM scripts
    )SQL";

    if (!allLanguages) {
        sql += " WHERE language = ?";
    }

    sql += " ORDER BY modified_at DESC;";

    sqlite3_stmt* stmt = prepare(sql.c_str());
    if (!stmt) return results;

    if (!allLanguages) {
        const char* langStr = scriptLanguageToString(language);
        sqlite3_bind_text(stmt, 1, langStr, -1, SQLITE_STATIC);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(readMetadata(stmt));
    }

    finalize(stmt);
    return results;
}

std::vector<ScriptMetadata> ScriptDatabase::searchScripts(const std::string& pattern,
                                                          ScriptLanguage language,
                                                          bool allLanguages)
{
    std::vector<ScriptMetadata> results;

    if (!m_db) {
        setError("Cannot search: database not open");
        return results;
    }

    std::string sql = R"SQL(
        SELECT id, name, language, description, created_at, modified_at, LENGTH(content)
        FROM scripts WHERE name LIKE ?
    )SQL";

    if (!allLanguages) {
        sql += " AND language = ?";
    }

    sql += " ORDER BY modified_at DESC;";

    sqlite3_stmt* stmt = prepare(sql.c_str());
    if (!stmt) return results;

    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);

    if (!allLanguages) {
        const char* langStr = scriptLanguageToString(language);
        sqlite3_bind_text(stmt, 2, langStr, -1, SQLITE_STATIC);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(readMetadata(stmt));
    }

    finalize(stmt);
    return results;
}

size_t ScriptDatabase::getScriptCount(ScriptLanguage language, bool allLanguages) {
    if (!m_db) return 0;

    std::string sql = "SELECT COUNT(*) FROM scripts";

    if (!allLanguages) {
        sql += " WHERE language = ?";
    }

    sql += ";";

    sqlite3_stmt* stmt = prepare(sql.c_str());
    if (!stmt) return 0;

    if (!allLanguages) {
        const char* langStr = scriptLanguageToString(language);
        sqlite3_bind_text(stmt, 1, langStr, -1, SQLITE_STATIC);
    }

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    finalize(stmt);
    return count;
}

// =============================================================================
// Utility
// =============================================================================

int64_t ScriptDatabase::getCurrentTimestamp() {
    return static_cast<int64_t>(std::time(nullptr));
}

bool ScriptDatabase::isValidScriptName(const std::string& name) {
    if (name.empty() || name.length() > 255) {
        return false;
    }

    for (char c : name) {
        if (!std::isalnum(c) && c != '_' && c != '-' && c != '.') {
            return false;
        }
    }

    return true;
}

std::string ScriptDatabase::generateUniqueName(const std::string& baseName,
                                               ScriptLanguage language)
{
    if (!scriptExists(baseName, language)) {
        return baseName;
    }

    // Try appending numbers
    for (int i = 2; i <= 999; ++i) {
        std::ostringstream oss;
        oss << baseName << "_" << i;
        std::string candidate = oss.str();

        if (!scriptExists(candidate, language)) {
            return candidate;
        }
    }

    // Fallback: append timestamp
    std::ostringstream oss;
    oss << baseName << "_" << getCurrentTimestamp();
    return oss.str();
}

// =============================================================================
// Internal Helpers
// =============================================================================

void ScriptDatabase::setError(const std::string& message) {
    m_lastError = message;
}

bool ScriptDatabase::execute(const char* sql) {
    if (!m_db) {
        setError("Database not open");
        return false;
    }

    char* errMsg = nullptr;
    int result = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);

    if (result != SQLITE_OK) {
        std::string error = errMsg ? errMsg : "unknown error";
        setError("SQL execution failed: " + error);
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }

    return true;
}

sqlite3_stmt* ScriptDatabase::prepare(const char* sql) {
    if (!m_db) {
        setError("Database not open");
        return nullptr;
    }

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);

    if (result != SQLITE_OK) {
        setError(std::string("Failed to prepare statement: ") + sqlite3_errmsg(m_db));
        return nullptr;
    }

    return stmt;
}

void ScriptDatabase::finalize(sqlite3_stmt* stmt) {
    if (stmt) {
        sqlite3_finalize(stmt);
    }
}

ScriptMetadata ScriptDatabase::readMetadata(sqlite3_stmt* stmt) {
    ScriptMetadata meta;

    meta.id = sqlite3_column_int64(stmt, 0);

    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    if (name) meta.name = name;

    const char* lang = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    if (lang) meta.language = stringToScriptLanguage(lang);

    const char* desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    if (desc) meta.description = desc;

    meta.createdAt = sqlite3_column_int64(stmt, 4);
    meta.modifiedAt = sqlite3_column_int64(stmt, 5);
    meta.contentLength = static_cast<size_t>(sqlite3_column_int64(stmt, 6));

    return meta;
}

bool ScriptDatabase::beginTransaction() {
    return execute("BEGIN TRANSACTION;");
}

bool ScriptDatabase::commitTransaction() {
    return execute("COMMIT;");
}

bool ScriptDatabase::rollbackTransaction() {
    return execute("ROLLBACK;");
}

// =============================================================================
// Line-Based Document Access Implementation
// =============================================================================

// Helper: Split content into lines
static std::vector<std::string> splitIntoLines(const std::string& content) {
    std::vector<std::string> lines;
    std::string currentLine;

    for (size_t i = 0; i < content.length(); ++i) {
        char c = content[i];

        if (c == '\n') {
            lines.push_back(currentLine);
            currentLine.clear();
        } else if (c == '\r') {
            lines.push_back(currentLine);
            currentLine.clear();
            // Handle \r\n
            if (i + 1 < content.length() && content[i + 1] == '\n') {
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

// Helper: Join lines into content
static std::string joinLines(const std::vector<std::string>& lines) {
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

size_t ScriptDatabase::getLineCount(const std::string& name, ScriptLanguage language) {
    std::string content;
    if (!loadScript(name, language, content)) {
        return 0;
    }

    std::vector<std::string> lines = splitIntoLines(content);
    return lines.size();
}

bool ScriptDatabase::getLine(const std::string& name,
                              ScriptLanguage language,
                              size_t lineNum,
                              std::string& outLine) {
    std::string content;
    if (!loadScript(name, language, content)) {
        return false;
    }

    std::vector<std::string> lines = splitIntoLines(content);

    if (lineNum >= lines.size()) {
        setError("Line number out of range");
        return false;
    }

    outLine = lines[lineNum];
    return true;
}

bool ScriptDatabase::getLines(const std::string& name,
                               ScriptLanguage language,
                               size_t startLine,
                               size_t endLine,
                               std::vector<std::string>& outLines) {
    std::string content;
    if (!loadScript(name, language, content)) {
        return false;
    }

    std::vector<std::string> lines = splitIntoLines(content);

    if (startLine >= lines.size() || endLine > lines.size() || startLine > endLine) {
        setError("Invalid line range");
        return false;
    }

    outLines.clear();
    outLines.reserve(endLine - startLine);

    for (size_t i = startLine; i < endLine; ++i) {
        outLines.push_back(lines[i]);
    }

    return true;
}

bool ScriptDatabase::setLine(const std::string& name,
                              ScriptLanguage language,
                              size_t lineNum,
                              const std::string& lineContent) {
    std::string content;
    if (!loadScript(name, language, content)) {
        return false;
    }

    std::vector<std::string> lines = splitIntoLines(content);

    if (lineNum >= lines.size()) {
        setError("Line number out of range");
        return false;
    }

    lines[lineNum] = lineContent;

    std::string newContent = joinLines(lines);
    return saveScript(name, language, newContent, "");
}

bool ScriptDatabase::insertLine(const std::string& name,
                                 ScriptLanguage language,
                                 size_t lineNum,
                                 const std::string& lineContent) {
    std::string content;
    if (!loadScript(name, language, content)) {
        return false;
    }

    std::vector<std::string> lines = splitIntoLines(content);

    if (lineNum > lines.size()) {
        setError("Line number out of range");
        return false;
    }

    lines.insert(lines.begin() + lineNum, lineContent);

    std::string newContent = joinLines(lines);
    return saveScript(name, language, newContent, "");
}

bool ScriptDatabase::deleteLine(const std::string& name,
                                 ScriptLanguage language,
                                 size_t lineNum) {
    std::string content;
    if (!loadScript(name, language, content)) {
        return false;
    }

    std::vector<std::string> lines = splitIntoLines(content);

    if (lineNum >= lines.size()) {
        setError("Line number out of range");
        return false;
    }

    lines.erase(lines.begin() + lineNum);

    std::string newContent = joinLines(lines);
    return saveScript(name, language, newContent, "");
}

} // namespace SuperTerminal
