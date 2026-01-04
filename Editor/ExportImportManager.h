//
//  ExportImportManager.h
//  SuperTerminal Framework - Export/Import Manager
//
//  Handles export and import of scripts database and cart contents
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef EXPORT_IMPORT_MANAGER_H
#define EXPORT_IMPORT_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace SuperTerminal {

// Forward declarations
class ScriptDatabase;
class CartManager;

// =============================================================================
// Export/Import Types
// =============================================================================

enum class ExportType {
    SCRIPTS_DATABASE,   // Export all scripts from database
    CART_CONTENT        // Export cart content and assets
};

struct ExportManifest {
    std::string exportType;         // "scripts" or "cart"
    std::string timestamp;          // ISO 8601 timestamp
    std::string version;            // FasterBASIC version
    std::string description;        // User-provided description
    
    // For scripts export
    int scriptCount = 0;
    std::vector<std::string> languages;
    
    // For cart export
    std::string cartName;
    std::string cartAuthor;
    std::string cartVersion;
    std::string cartDescription;
    std::vector<std::string> assetTypes;
    int assetCount = 0;
    
    // Files included
    std::vector<std::string> files;
};

struct ExportResult {
    bool success = false;
    std::string message;
    std::string exportPath;        // Path to created export folder
    std::string manifestPath;     // Path to manifest.json
    int itemsExported = 0;
    
    ExportResult() = default;
    
    static ExportResult Success(const std::string& path, int items, const std::string& msg = "Export completed successfully") {
        ExportResult result;
        result.success = true;
        result.exportPath = path;
        result.itemsExported = items;
        result.message = msg;
        return result;
    }
    
    static ExportResult Failure(const std::string& msg) {
        ExportResult result;
        result.success = false;
        result.message = msg;
        return result;
    }
};

struct ImportResult {
    bool success = false;
    std::string message;
    int itemsImported = 0;
    int itemsSkipped = 0;
    std::vector<std::string> warnings;
    
    ImportResult() = default;
    
    static ImportResult Success(int imported, int skipped, const std::string& msg = "Import completed successfully") {
        ImportResult result;
        result.success = true;
        result.itemsImported = imported;
        result.itemsSkipped = skipped;
        result.message = msg;
        return result;
    }
    
    static ImportResult Failure(const std::string& msg) {
        ImportResult result;
        result.success = false;
        result.message = msg;
        return result;
    }
};

struct ExportFolder {
    std::string name;              // Folder name (e.g., "2024-12-23_143021")
    std::string path;              // Full path to export folder
    std::string timestamp;         // Human-readable timestamp
    ExportType type;               // What was exported
    std::string description;       // Description from manifest
    int itemCount = 0;             // Number of items exported
    bool hasValidManifest = false; // Whether manifest.json is valid
};

// =============================================================================
// ExportImportManager - Main class for export/import operations
// =============================================================================

class ExportImportManager {
public:
    // Constructor
    ExportImportManager();
    ~ExportImportManager();
    
    // No copy
    ExportImportManager(const ExportImportManager&) = delete;
    ExportImportManager& operator=(const ExportImportManager&) = delete;
    
    // =========================================================================
    // Configuration
    // =========================================================================
    
    /// Set FasterBASIC user directory (defaults to ~/Documents/FasterBASIC)
    /// @param path Path to user directory
    void setUserDirectory(const std::string& path);
    
    /// Get FasterBASIC user directory
    /// @return Path to user directory
    std::string getUserDirectory() const { return m_userDirectory; }
    
    /// Get exports directory (user_dir/Exports)
    /// @return Path to exports directory
    std::string getExportsDirectory() const;
    
    /// Ensure exports directory exists
    /// @return true on success
    bool ensureExportsDirectoryExists();
    
    // =========================================================================
    // Export Operations
    // =========================================================================
    
    /// Export all scripts from database
    /// @param database Script database to export from
    /// @param description Optional description for the export
    /// @return Export result
    ExportResult exportScriptsDatabase(std::shared_ptr<ScriptDatabase> database,
                                      const std::string& description = "");
    
    /// Export cart content and assets
    /// @param cartManager Cart manager with active cart
    /// @param description Optional description for the export
    /// @return Export result
    ExportResult exportCartContent(std::shared_ptr<CartManager> cartManager,
                                  const std::string& description = "");
    
    // =========================================================================
    // Import Operations
    // =========================================================================
    
    /// Get list of available export folders
    /// @return Vector of export folders found
    std::vector<ExportFolder> getAvailableExports();
    
    /// Import scripts from export folder
    /// @param exportFolder Export folder to import from
    /// @param database Target script database
    /// @param overwriteExisting Whether to overwrite existing scripts
    /// @return Import result
    ImportResult importScriptsFromExport(const ExportFolder& exportFolder,
                                        std::shared_ptr<ScriptDatabase> database,
                                        bool overwriteExisting = false);
    
    /// Import cart from export folder
    /// @param exportFolder Export folder to import from
    /// @param cartManager Target cart manager
    /// @param targetCartPath Path for new cart file
    /// @return Import result
    ImportResult importCartFromExport(const ExportFolder& exportFolder,
                                     std::shared_ptr<CartManager> cartManager,
                                     const std::string& targetCartPath);
    
    // =========================================================================
    // Utility
    // =========================================================================
    
    /// Read manifest from export folder
    /// @param exportPath Path to export folder
    /// @param outManifest Output: parsed manifest
    /// @return true on success
    bool readManifest(const std::string& exportPath, ExportManifest& outManifest);
    
    /// Generate unique export folder name with timestamp
    /// @param prefix Optional prefix (default: exports use date/time)
    /// @return Unique folder name
    std::string generateExportFolderName(const std::string& prefix = "");
    
    /// Get default user directory (~Documents/FasterBASIC)
    /// @return Default user directory path
    static std::string getDefaultUserDirectory();
    
    /// Validate export folder structure
    /// @param exportPath Path to export folder
    /// @return true if folder has valid structure and manifest
    bool validateExportFolder(const std::string& exportPath);
    
    /// Get last error message
    /// @return Error message
    std::string getLastError() const { return m_lastError; }
    
private:
    // =========================================================================
    // Internal Implementation
    // =========================================================================
    
    // User directory configuration
    std::string m_userDirectory;
    std::string m_lastError;
    
    // Helper methods
    void setError(const std::string& error);
    bool createDirectory(const std::string& path);
    bool copyFile(const std::string& source, const std::string& dest);
    bool writeTextFile(const std::string& path, const std::string& content);
    bool readTextFile(const std::string& path, std::string& content);
    std::string getCurrentTimestamp();
    std::string formatTimestampForDisplay(const std::string& isoTimestamp);
    
    // Export helpers
    ExportResult exportScriptsInternal(std::shared_ptr<ScriptDatabase> database,
                                      const std::string& exportPath,
                                      const std::string& description);
    ExportResult exportCartInternal(std::shared_ptr<CartManager> cartManager,
                                   const std::string& exportPath,
                                   const std::string& description);
    bool writeManifest(const std::string& exportPath, const ExportManifest& manifest);
    
    // Import helpers
    ImportResult importScriptsInternal(const ExportManifest& manifest,
                                      const std::string& exportPath,
                                      std::shared_ptr<ScriptDatabase> database,
                                      bool overwriteExisting);
    ImportResult importCartInternal(const ExportManifest& manifest,
                                   const std::string& exportPath,
                                   std::shared_ptr<CartManager> cartManager,
                                   const std::string& targetCartPath);
    
    // Parsing helpers
    bool parseManifestJSON(const std::string& jsonContent, ExportManifest& manifest);
    std::string generateManifestJSON(const ExportManifest& manifest);
};

} // namespace SuperTerminal

#endif // EXPORT_IMPORT_MANAGER_H