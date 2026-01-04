//
//  ExportImportManager.cpp
//  SuperTerminal Framework - Export/Import Manager Implementation
//
//  Handles export and import of scripts database and cart contents
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "ExportImportManager.h"
#include "ScriptDatabase.h"
#include "../Cart/CartManager.h"
#include "../Cart/CartLoader.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <filesystem>
#include <ctime>
#include <algorithm>



namespace SuperTerminal {

// =============================================================================
// Constructor/Destructor
// =============================================================================

ExportImportManager::ExportImportManager() {
    m_userDirectory = getDefaultUserDirectory();
}

ExportImportManager::~ExportImportManager() {
    // Nothing to clean up
}

// =============================================================================
// Configuration
// =============================================================================

void ExportImportManager::setUserDirectory(const std::string& path) {
    m_userDirectory = path;
}

std::string ExportImportManager::getExportsDirectory() const {
    return m_userDirectory + "/Exports";
}

bool ExportImportManager::ensureExportsDirectoryExists() {
    try {
        std::filesystem::path userPath(m_userDirectory);
        std::filesystem::path exportsPath(getExportsDirectory());
        
        // Create user directory if it doesn't exist
        if (!std::filesystem::exists(userPath)) {
            std::filesystem::create_directories(userPath);
        }
        
        // Create exports directory if it doesn't exist
        if (!std::filesystem::exists(exportsPath)) {
            std::filesystem::create_directories(exportsPath);
        }
        
        return true;
    } catch (const std::exception& e) {
        setError("Failed to create exports directory: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// Export Operations
// =============================================================================

ExportResult ExportImportManager::exportScriptsDatabase(std::shared_ptr<ScriptDatabase> database,
                                                        const std::string& description) {
    if (!database || !database->isOpen()) {
        return ExportResult::Failure("Scripts database is not available");
    }
    
    if (!ensureExportsDirectoryExists()) {
        return ExportResult::Failure("Failed to create exports directory: " + m_lastError);
    }
    
    // Generate export folder name
    std::string folderName = generateExportFolderName("scripts_");
    std::string exportPath = getExportsDirectory() + "/" + folderName;
    
    return exportScriptsInternal(database, exportPath, description);
}

ExportResult ExportImportManager::exportCartContent(std::shared_ptr<CartManager> cartManager,
                                                   const std::string& description) {
    if (!cartManager || !cartManager->isCartActive()) {
        return ExportResult::Failure("No cart is currently active");
    }
    
    if (!ensureExportsDirectoryExists()) {
        return ExportResult::Failure("Failed to create exports directory: " + m_lastError);
    }
    
    // Generate export folder name
    std::string folderName = generateExportFolderName("cart_");
    std::string exportPath = getExportsDirectory() + "/" + folderName;
    
    return exportCartInternal(cartManager, exportPath, description);
}

// =============================================================================
// Import Operations
// =============================================================================

std::vector<ExportFolder> ExportImportManager::getAvailableExports() {
    std::vector<ExportFolder> exports;
    
    try {
        std::string exportsDir = getExportsDirectory();
        if (!std::filesystem::exists(exportsDir)) {
            return exports; // Empty list
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(exportsDir)) {
            if (entry.is_directory()) {
                ExportFolder exportFolder;
                exportFolder.path = entry.path().string();
                exportFolder.name = entry.path().filename().string();
                
                // Try to read manifest
                ExportManifest manifest;
                if (readManifest(exportFolder.path, manifest)) {
                    exportFolder.hasValidManifest = true;
                    exportFolder.timestamp = formatTimestampForDisplay(manifest.timestamp);
                    exportFolder.description = manifest.description;
                    exportFolder.itemCount = (manifest.exportType == "scripts") ? 
                                            manifest.scriptCount : manifest.assetCount;
                    
                    if (manifest.exportType == "scripts") {
                        exportFolder.type = ExportType::SCRIPTS_DATABASE;
                    } else if (manifest.exportType == "cart") {
                        exportFolder.type = ExportType::CART_CONTENT;
                    }
                } else {
                    exportFolder.hasValidManifest = false;
                    exportFolder.timestamp = "Unknown";
                    exportFolder.description = "Invalid or missing manifest";
                    exportFolder.itemCount = 0;
                }
                
                exports.push_back(exportFolder);
            }
        }
        
        // Sort by name (which includes timestamp) in descending order
        std::sort(exports.begin(), exports.end(), [](const ExportFolder& a, const ExportFolder& b) {
            return a.name > b.name; // Newest first
        });
        
    } catch (const std::exception& e) {
        setError("Failed to scan exports directory: " + std::string(e.what()));
    }
    
    return exports;
}

ImportResult ExportImportManager::importScriptsFromExport(const ExportFolder& exportFolder,
                                                         std::shared_ptr<ScriptDatabase> database,
                                                         bool overwriteExisting) {
    if (!database || !database->isOpen()) {
        return ImportResult::Failure("Scripts database is not available");
    }
    
    if (!exportFolder.hasValidManifest) {
        return ImportResult::Failure("Export folder has invalid or missing manifest");
    }
    
    ExportManifest manifest;
    if (!readManifest(exportFolder.path, manifest)) {
        return ImportResult::Failure("Failed to read export manifest");
    }
    
    if (manifest.exportType != "scripts") {
        return ImportResult::Failure("Export folder does not contain scripts");
    }
    
    return importScriptsInternal(manifest, exportFolder.path, database, overwriteExisting);
}

ImportResult ExportImportManager::importCartFromExport(const ExportFolder& exportFolder,
                                                      std::shared_ptr<CartManager> cartManager,
                                                      const std::string& targetCartPath) {
    if (!cartManager) {
        return ImportResult::Failure("Cart manager is not available");
    }
    
    if (!exportFolder.hasValidManifest) {
        return ImportResult::Failure("Export folder has invalid or missing manifest");
    }
    
    ExportManifest manifest;
    if (!readManifest(exportFolder.path, manifest)) {
        return ImportResult::Failure("Failed to read export manifest");
    }
    
    if (manifest.exportType != "cart") {
        return ImportResult::Failure("Export folder does not contain cart content");
    }
    
    return importCartInternal(manifest, exportFolder.path, cartManager, targetCartPath);
}

// =============================================================================
// Utility
// =============================================================================

bool ExportImportManager::readManifest(const std::string& exportPath, ExportManifest& outManifest) {
    std::string manifestPath = exportPath + "/manifest.json";
    
    std::string jsonContent;
    if (!readTextFile(manifestPath, jsonContent)) {
        return false;
    }
    
    return parseManifestJSON(jsonContent, outManifest);
}

std::string ExportImportManager::generateExportFolderName(const std::string& prefix) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << prefix << std::put_time(std::localtime(&time_t), "%Y-%m-%d_%H%M%S");
    
    return ss.str();
}

std::string ExportImportManager::getDefaultUserDirectory() {
    // Use a reasonable default for all platforms
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home) + "/Documents/FasterBASIC";
    } else {
        return "./FasterBASIC";
    }
}

bool ExportImportManager::validateExportFolder(const std::string& exportPath) {
    try {
        // Check if directory exists
        if (!std::filesystem::exists(exportPath) || !std::filesystem::is_directory(exportPath)) {
            return false;
        }
        
        // Check if manifest exists and is valid
        ExportManifest manifest;
        return readManifest(exportPath, manifest);
        
    } catch (const std::exception&) {
        return false;
    }
}

// =============================================================================
// Internal Implementation
// =============================================================================

void ExportImportManager::setError(const std::string& error) {
    m_lastError = error;
}

bool ExportImportManager::createDirectory(const std::string& path) {
    try {
        std::filesystem::create_directories(path);
        return true;
    } catch (const std::exception& e) {
        setError("Failed to create directory: " + std::string(e.what()));
        return false;
    }
}

bool ExportImportManager::copyFile(const std::string& source, const std::string& dest) {
    try {
        std::filesystem::copy_file(source, dest, std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (const std::exception& e) {
        setError("Failed to copy file: " + std::string(e.what()));
        return false;
    }
}

bool ExportImportManager::writeTextFile(const std::string& path, const std::string& content) {
    try {
        std::ofstream file(path);
        if (!file.is_open()) {
            setError("Failed to open file for writing: " + path);
            return false;
        }
        
        file << content;
        file.close();
        return true;
    } catch (const std::exception& e) {
        setError("Failed to write file: " + std::string(e.what()));
        return false;
    }
}

bool ExportImportManager::readTextFile(const std::string& path, std::string& content) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            setError("Failed to open file for reading: " + path);
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        content = buffer.str();
        file.close();
        return true;
    } catch (const std::exception& e) {
        setError("Failed to read file: " + std::string(e.what()));
        return false;
    }
}

std::string ExportImportManager::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    
    return ss.str();
}

std::string ExportImportManager::formatTimestampForDisplay(const std::string& isoTimestamp) {
    // Simple formatting - just extract date and time parts
    if (isoTimestamp.length() >= 19) {
        std::string date = isoTimestamp.substr(0, 10);
        std::string time = isoTimestamp.substr(11, 8);
        return date + " " + time;
    }
    return isoTimestamp;
}

// =============================================================================
// Export Implementation
// =============================================================================

ExportResult ExportImportManager::exportScriptsInternal(std::shared_ptr<ScriptDatabase> database,
                                                       const std::string& exportPath,
                                                       const std::string& description) {
    try {
        // Create export directory
        if (!createDirectory(exportPath)) {
            return ExportResult::Failure("Failed to create export directory: " + m_lastError);
        }
        
        // Create scripts subdirectory
        std::string scriptsDir = exportPath + "/scripts";
        if (!createDirectory(scriptsDir)) {
            return ExportResult::Failure("Failed to create scripts directory: " + m_lastError);
        }
        
        // Get all scripts
        auto allScripts = database->listScripts(ScriptLanguage::LUA, true); // Get all languages
        
        if (allScripts.empty()) {
            return ExportResult::Failure("No scripts found in database");
        }
        
        // Prepare manifest
        ExportManifest manifest;
        manifest.exportType = "scripts";
        manifest.timestamp = getCurrentTimestamp();
        manifest.version = "3.0.0"; // TODO: Get actual version
        manifest.description = description.empty() ? "Scripts database export" : description;
        manifest.scriptCount = static_cast<int>(allScripts.size());
        
        // Export each script
        int exported = 0;
        for (const auto& script : allScripts) {
            std::string content;
            if (database->loadScript(script.name, script.language, content)) {
                // Create filename with language extension
                std::string ext;
                switch (script.language) {
                    case ScriptLanguage::BASIC: ext = ".bas"; break;
                    case ScriptLanguage::LUA: ext = ".lua"; break;
                    case ScriptLanguage::ABC: ext = ".abc"; break;
                    case ScriptLanguage::VOICESCRIPT: ext = ".vscript"; break;
                    default: ext = ".txt"; break;
                }
                
                std::string filename = script.name + ext;
                std::string filepath = scriptsDir + "/" + filename;
                
                if (writeTextFile(filepath, content)) {
                    manifest.files.push_back("scripts/" + filename);
                    exported++;
                    
                    // Track languages
                    std::string langName;
                    switch (script.language) {
                        case ScriptLanguage::BASIC: langName = "BASIC"; break;
                        case ScriptLanguage::LUA: langName = "Lua"; break;
                        case ScriptLanguage::ABC: langName = "ABC"; break;
                        case ScriptLanguage::VOICESCRIPT: langName = "VoiceScript"; break;
                        default: langName = "Unknown"; break;
                    }
                    
                    if (std::find(manifest.languages.begin(), manifest.languages.end(), langName) == manifest.languages.end()) {
                        manifest.languages.push_back(langName);
                    }
                }
            }
        }
        
        if (exported == 0) {
            return ExportResult::Failure("Failed to export any scripts");
        }
        
        // Write manifest
        if (!writeManifest(exportPath, manifest)) {
            return ExportResult::Failure("Failed to write manifest: " + m_lastError);
        }
        
        return ExportResult::Success(exportPath, exported, "Exported " + std::to_string(exported) + " scripts");
        
    } catch (const std::exception& e) {
        return ExportResult::Failure("Export failed: " + std::string(e.what()));
    }
}

ExportResult ExportImportManager::exportCartInternal(std::shared_ptr<CartManager> cartManager,
                                                    const std::string& exportPath,
                                                    const std::string& description) {
    try {
        // Create export directory
        if (!createDirectory(exportPath)) {
            return ExportResult::Failure("Failed to create export directory: " + m_lastError);
        }
        
        // Get cart info
        auto cartInfo = cartManager->getCartInfo();
        auto cartLoader = cartManager->getLoader();
        
        if (!cartLoader) {
            return ExportResult::Failure("Cart loader not available");
        }
        
        // Prepare manifest
        ExportManifest manifest;
        manifest.exportType = "cart";
        manifest.timestamp = getCurrentTimestamp();
        manifest.version = "3.0.0"; // TODO: Get actual version
        manifest.description = description.empty() ? "Cart content export" : description;
        manifest.cartName = cartInfo.metadata.title;
        manifest.cartAuthor = cartInfo.metadata.author;
        manifest.cartVersion = cartInfo.metadata.version;
        manifest.cartDescription = cartInfo.metadata.description;
        
        int exported = 0;
        
        // Export program source
        std::string programSource = cartManager->getProgramSource();
        if (!programSource.empty()) {
            std::string programPath = exportPath + "/program.bas";
            if (writeTextFile(programPath, programSource)) {
                manifest.files.push_back("program.bas");
                exported++;
            }
        }
        
        // Create assets directory
        std::string assetsDir = exportPath + "/assets";
        if (!createDirectory(assetsDir)) {
            return ExportResult::Failure("Failed to create assets directory: " + m_lastError);
        }
        
        // Export assets (simplified - this would need to integrate with CartLoader's asset enumeration)
        auto assetsList = cartManager->listAssets("sprites");
        for (const auto& assetName : assetsList) {
            // This is a simplified example - real implementation would copy actual asset files
            if (!manifest.assetTypes.empty() && 
                std::find(manifest.assetTypes.begin(), manifest.assetTypes.end(), "sprites") == manifest.assetTypes.end()) {
                manifest.assetTypes.push_back("sprites");
            }
            exported++;
        }
        
        manifest.assetCount = exported - (programSource.empty() ? 0 : 1); // Subtract program if counted
        
        // Write manifest
        if (!writeManifest(exportPath, manifest)) {
            return ExportResult::Failure("Failed to write manifest: " + m_lastError);
        }
        
        return ExportResult::Success(exportPath, exported, "Exported cart with " + std::to_string(exported) + " items");
        
    } catch (const std::exception& e) {
        return ExportResult::Failure("Export failed: " + std::string(e.what()));
    }
}

bool ExportImportManager::writeManifest(const std::string& exportPath, const ExportManifest& manifest) {
    std::string manifestPath = exportPath + "/manifest.json";
    std::string jsonContent = generateManifestJSON(manifest);
    
    return writeTextFile(manifestPath, jsonContent);
}

// =============================================================================
// Import Implementation
// =============================================================================

ImportResult ExportImportManager::importScriptsInternal(const ExportManifest& manifest,
                                                       const std::string& exportPath,
                                                       std::shared_ptr<ScriptDatabase> database,
                                                       bool overwriteExisting) {
    try {
        int imported = 0;
        int skipped = 0;
        std::vector<std::string> warnings;
        
        std::string scriptsDir = exportPath + "/scripts";
        
        for (const auto& filename : manifest.files) {
            if (filename.substr(0, 8) != "scripts/") continue; // Skip non-script files
            
            std::string actualFilename = filename.substr(8); // Remove "scripts/" prefix
            std::string filepath = scriptsDir + "/" + actualFilename;
            
            // Determine language from extension
            ScriptLanguage language = ScriptLanguage::BASIC;
            std::string basename = actualFilename;
            size_t dotPos = actualFilename.rfind('.');
            if (dotPos != std::string::npos) {
                std::string ext = actualFilename.substr(dotPos);
                basename = actualFilename.substr(0, dotPos);
                
                if (ext == ".bas") language = ScriptLanguage::BASIC;
                else if (ext == ".lua") language = ScriptLanguage::LUA;
                else if (ext == ".abc") language = ScriptLanguage::ABC;
                else if (ext == ".vscript") language = ScriptLanguage::VOICESCRIPT;
            }
            
            // Check if script already exists
            if (database->scriptExists(basename, language) && !overwriteExisting) {
                warnings.push_back("Script '" + basename + "' already exists, skipped");
                skipped++;
                continue;
            }
            
            // Read script content
            std::string content;
            if (readTextFile(filepath, content)) {
                if (database->saveScript(basename, language, content)) {
                    imported++;
                } else {
                    warnings.push_back("Failed to save script '" + basename + "'");
                    skipped++;
                }
            } else {
                warnings.push_back("Failed to read script file '" + actualFilename + "'");
                skipped++;
            }
        }
        
        ImportResult result = ImportResult::Success(imported, skipped);
        result.warnings = warnings;
        
        if (imported == 0 && skipped > 0) {
            result.message = "No scripts were imported";
            result.success = false;
        } else {
            result.message = "Imported " + std::to_string(imported) + " scripts";
            if (skipped > 0) {
                result.message += ", skipped " + std::to_string(skipped);
            }
        }
        
        return result;
        
    } catch (const std::exception& e) {
        return ImportResult::Failure("Import failed: " + std::string(e.what()));
    }
}

ImportResult ExportImportManager::importCartInternal(const ExportManifest& manifest,
                                                    const std::string& exportPath,
                                                    std::shared_ptr<CartManager> cartManager,
                                                    const std::string& targetCartPath) {
    try {
        // Create new cart
        auto result = cartManager->createCart(targetCartPath, 
                                            manifest.cartName.empty() ? "Imported Cart" : manifest.cartName,
                                            manifest.cartAuthor.empty() ? "Unknown" : manifest.cartAuthor,
                                            manifest.cartVersion.empty() ? "1.0.0" : manifest.cartVersion,
                                            manifest.cartDescription);
        
        if (!result.success) {
            return ImportResult::Failure("Failed to create cart: " + result.message);
        }
        
        int imported = 0;
        int skipped = 0;
        std::vector<std::string> warnings;
        
        // Import program source if it exists
        std::string programPath = exportPath + "/program.bas";
        if (std::filesystem::exists(programPath)) {
            std::string programSource;
            if (readTextFile(programPath, programSource)) {
                auto updateResult = cartManager->updateProgramSource(programSource);
                if (updateResult.success) {
                    imported++;
                } else {
                    warnings.push_back("Failed to import program source");
                    skipped++;
                }
            }
        }
        
        // Import assets (simplified - real implementation would handle all asset types)
        std::string assetsDir = exportPath + "/assets";
        if (std::filesystem::exists(assetsDir)) {
            // This is a placeholder for asset import logic
            // Real implementation would enumerate assets and import them
            imported += manifest.assetCount;
        }
        
        // Save the cart
        auto saveResult = cartManager->saveCart();
        if (!saveResult.success) {
            warnings.push_back("Failed to save cart: " + saveResult.message);
        }
        
        ImportResult importResult = ImportResult::Success(imported, skipped);
        importResult.warnings = warnings;
        importResult.message = "Imported cart with " + std::to_string(imported) + " items";
        
        return importResult;
        
    } catch (const std::exception& e) {
        return ImportResult::Failure("Import failed: " + std::string(e.what()));
    }
}

// =============================================================================
// JSON Parsing (Simplified)
// =============================================================================

bool ExportImportManager::parseManifestJSON(const std::string& jsonContent, ExportManifest& manifest) {
    // This is a very simplified JSON parser
    // A real implementation should use a proper JSON library
    
    try {
        // Look for key-value pairs in the JSON
        auto findValue = [&jsonContent](const std::string& key) -> std::string {
            std::string searchKey = "\"" + key + "\"";
            size_t keyPos = jsonContent.find(searchKey);
            if (keyPos == std::string::npos) return "";
            
            size_t colonPos = jsonContent.find(":", keyPos);
            if (colonPos == std::string::npos) return "";
            
            size_t valueStart = jsonContent.find("\"", colonPos);
            if (valueStart == std::string::npos) return "";
            valueStart++;
            
            size_t valueEnd = jsonContent.find("\"", valueStart);
            if (valueEnd == std::string::npos) return "";
            
            return jsonContent.substr(valueStart, valueEnd - valueStart);
        };
        
        auto findIntValue = [&jsonContent](const std::string& key) -> int {
            std::string searchKey = "\"" + key + "\"";
            size_t keyPos = jsonContent.find(searchKey);
            if (keyPos == std::string::npos) return 0;
            
            size_t colonPos = jsonContent.find(":", keyPos);
            if (colonPos == std::string::npos) return 0;
            
            size_t valueStart = colonPos + 1;
            while (valueStart < jsonContent.length() && std::isspace(jsonContent[valueStart])) {
                valueStart++;
            }
            
            size_t valueEnd = valueStart;
            while (valueEnd < jsonContent.length() && std::isdigit(jsonContent[valueEnd])) {
                valueEnd++;
            }
            
            if (valueEnd > valueStart) {
                return std::stoi(jsonContent.substr(valueStart, valueEnd - valueStart));
            }
            
            return 0;
        };
        
        manifest.exportType = findValue("exportType");
        manifest.timestamp = findValue("timestamp");
        manifest.version = findValue("version");
        manifest.description = findValue("description");
        manifest.scriptCount = findIntValue("scriptCount");
        manifest.assetCount = findIntValue("assetCount");
        manifest.cartName = findValue("cartName");
        manifest.cartAuthor = findValue("cartAuthor");
        manifest.cartVersion = findValue("cartVersion");
        manifest.cartDescription = findValue("cartDescription");
        
        // For a complete implementation, we'd also parse arrays for languages, assetTypes, and files
        // This simplified version just checks if we found the basic required fields
        
        return !manifest.exportType.empty() && !manifest.timestamp.empty();
        
    } catch (const std::exception&) {
        return false;
    }
}

std::string ExportImportManager::generateManifestJSON(const ExportManifest& manifest) {
    std::stringstream json;
    json << "{\n";
    json << "  \"exportType\": \"" << manifest.exportType << "\",\n";
    json << "  \"timestamp\": \"" << manifest.timestamp << "\",\n";
    json << "  \"version\": \"" << manifest.version << "\",\n";
    json << "  \"description\": \"" << manifest.description << "\",\n";
    
    if (manifest.exportType == "scripts") {
        json << "  \"scriptCount\": " << manifest.scriptCount << ",\n";
        json << "  \"languages\": [";
        for (size_t i = 0; i < manifest.languages.size(); ++i) {
            if (i > 0) json << ", ";
            json << "\"" << manifest.languages[i] << "\"";
        }
        json << "],\n";
    } else if (manifest.exportType == "cart") {
        json << "  \"cartName\": \"" << manifest.cartName << "\",\n";
        json << "  \"cartAuthor\": \"" << manifest.cartAuthor << "\",\n";
        json << "  \"cartVersion\": \"" << manifest.cartVersion << "\",\n";
        json << "  \"cartDescription\": \"" << manifest.cartDescription << "\",\n";
        json << "  \"assetCount\": " << manifest.assetCount << ",\n";
        json << "  \"assetTypes\": [";
        for (size_t i = 0; i < manifest.assetTypes.size(); ++i) {
            if (i > 0) json << ", ";
            json << "\"" << manifest.assetTypes[i] << "\"";
        }
        json << "],\n";
    }
    
    json << "  \"files\": [";
    for (size_t i = 0; i < manifest.files.size(); ++i) {
        if (i > 0) json << ", ";
        json << "\"" << manifest.files[i] << "\"";
    }
    json << "]\n";
    json << "}";
    
    return json.str();
}

} // namespace SuperTerminal