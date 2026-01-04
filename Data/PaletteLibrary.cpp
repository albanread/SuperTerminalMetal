//
// PaletteLibrary.cpp
// SuperTerminal Framework - Standard Palette Library
//
// Implementation of standard palette loader
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "PaletteLibrary.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace SuperTerminal {

// =============================================================================
// Internal Data Structure
// =============================================================================

struct StandardPaletteLibrary::LibraryData {
    // Palette storage (32 palettes × 16 colors)
    PaletteColor palettes[STANDARD_PALETTE_COUNT][STANDARD_PALETTE_COLORS];
    
    // Metadata storage
    StandardPaletteInfo info[STANDARD_PALETTE_COUNT];
    std::string names[STANDARD_PALETTE_COUNT];
    std::string descriptions[STANDARD_PALETTE_COUNT];
    std::string categories[STANDARD_PALETTE_COUNT];
    
    // State
    bool initialized;
    std::string lastError;
    
    LibraryData() : initialized(false) {
        // Initialize metadata pointers
        for (int i = 0; i < STANDARD_PALETTE_COUNT; i++) {
            info[i].id = i;
            info[i].name = "";
            info[i].description = "";
            info[i].category = "";
        }
    }
};

// Static data
StandardPaletteLibrary::LibraryData* StandardPaletteLibrary::s_data = nullptr;

// =============================================================================
// Initialization
// =============================================================================

bool StandardPaletteLibrary::initialize(const std::string& path) {
    // Auto-detect format by extension
    if (path.length() >= 5 && path.substr(path.length() - 5) == ".json") {
        return initializeFromJSON(path);
    } else if (path.length() >= 4 && path.substr(path.length() - 4) == ".pal") {
        return initializeFromBinary(path);
    }
    
    // Try JSON first, then binary
    if (initializeFromJSON(path)) {
        return true;
    }
    
    return initializeFromBinary(path);
}

bool StandardPaletteLibrary::initializeFromJSON(const std::string& jsonPath) {
    // Create data structure if needed
    if (!s_data) {
        s_data = new LibraryData();
    }
    
    // Read file
    std::ifstream file(jsonPath, std::ios::binary);
    if (!file.is_open()) {
        setError("Failed to open file: " + jsonPath);
        return false;
    }
    
    // Read entire file
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    // Parse JSON
    if (!parseJSON(content)) {
        return false;
    }
    
    s_data->initialized = true;
    return true;
}

bool StandardPaletteLibrary::initializeFromBinary(const std::string& palPath) {
    // Create data structure if needed
    if (!s_data) {
        s_data = new LibraryData();
    }
    
    // Read file
    std::ifstream file(palPath, std::ios::binary);
    if (!file.is_open()) {
        setError("Failed to open file: " + palPath);
        return false;
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Read data
    uint8_t* data = new uint8_t[size];
    file.read(reinterpret_cast<char*>(data), size);
    file.close();
    
    // Parse binary
    bool success = parseBinary(data, size);
    delete[] data;
    
    if (!success) {
        return false;
    }
    
    s_data->initialized = true;
    return true;
}

bool StandardPaletteLibrary::isInitialized() {
    return s_data && s_data->initialized;
}

void StandardPaletteLibrary::shutdown() {
    if (s_data) {
        delete s_data;
        s_data = nullptr;
    }
}

// =============================================================================
// Palette Access
// =============================================================================

const PaletteColor* StandardPaletteLibrary::getPalette(uint8_t paletteID) {
    if (!isInitialized() || paletteID >= STANDARD_PALETTE_COUNT) {
        return nullptr;
    }
    
    // Debug: Log first 4 colors of the palette
    const PaletteColor* palette = s_data->palettes[paletteID];
    printf("[getPalette] Palette %d first 4 colors:\n", paletteID);
    for (int i = 0; i < 4; i++) {
        printf("  [%d] R=%d G=%d B=%d A=%d\n", i, palette[i].r, palette[i].g, palette[i].b, palette[i].a);
    }
    
    return s_data->palettes[paletteID];
}

const char* StandardPaletteLibrary::getPaletteName(uint8_t paletteID) {
    if (!isInitialized() || paletteID >= STANDARD_PALETTE_COUNT) {
        return nullptr;
    }
    
    return s_data->names[paletteID].c_str();
}

const char* StandardPaletteLibrary::getPaletteDescription(uint8_t paletteID) {
    if (!isInitialized() || paletteID >= STANDARD_PALETTE_COUNT) {
        return nullptr;
    }
    
    return s_data->descriptions[paletteID].c_str();
}

const char* StandardPaletteLibrary::getPaletteCategory(uint8_t paletteID) {
    if (!isInitialized() || paletteID >= STANDARD_PALETTE_COUNT) {
        return nullptr;
    }
    
    return s_data->categories[paletteID].c_str();
}

const StandardPaletteInfo* StandardPaletteLibrary::getPaletteInfo(uint8_t paletteID) {
    if (!isInitialized() || paletteID >= STANDARD_PALETTE_COUNT) {
        return nullptr;
    }
    
    return &s_data->info[paletteID];
}

// =============================================================================
// Validation
// =============================================================================

bool StandardPaletteLibrary::isValidPaletteID(uint8_t paletteID) {
    return paletteID < STANDARD_PALETTE_COUNT;
}

bool StandardPaletteLibrary::isStandardPaletteMode(uint8_t paletteMode) {
    return paletteMode != PALETTE_MODE_CUSTOM;
}

// =============================================================================
// Palette Operations
// =============================================================================

bool StandardPaletteLibrary::copyPalette(uint8_t paletteID, PaletteColor* outColors) {
    const PaletteColor* palette = getPalette(paletteID);
    if (!palette) {
        return false;
    }
    
    std::memcpy(outColors, palette, sizeof(PaletteColor) * STANDARD_PALETTE_COLORS);
    return true;
}

bool StandardPaletteLibrary::copyPaletteRGBA(uint8_t paletteID, uint8_t* outRGBA) {
    const PaletteColor* palette = getPalette(paletteID);
    if (!palette) {
        return false;
    }
    
    for (int i = 0; i < STANDARD_PALETTE_COLORS; i++) {
        outRGBA[i * 4 + 0] = palette[i].r;
        outRGBA[i * 4 + 1] = palette[i].g;
        outRGBA[i * 4 + 2] = palette[i].b;
        outRGBA[i * 4 + 3] = palette[i].a;
    }
    
    return true;
}

uint8_t StandardPaletteLibrary::findClosestPalette(const PaletteColor* customPalette,
                                                   int32_t* outDistance) {
    if (!isInitialized()) {
        if (outDistance) *outDistance = -1;
        return PALETTE_MODE_CUSTOM;
    }
    
    // CRITICAL: All standard palettes have black at index 2
    // Check if the custom palette also has black (or very close to black) at index 2
    const PaletteColor& index2Color = customPalette[2];
    const int32_t BLACK_THRESHOLD = 30 * 30 * 3; // Allow very dark colors (r,g,b each < ~30)
    int32_t blackDistance = index2Color.r * index2Color.r + 
                           index2Color.g * index2Color.g + 
                           index2Color.b * index2Color.b;
    
    if (blackDistance > BLACK_THRESHOLD) {
        // Index 2 is not black - this cannot match any standard palette
        if (outDistance) *outDistance = INT32_MAX;
        return PALETTE_MODE_CUSTOM;
    }
    
    int32_t bestDistance = INT32_MAX;
    uint8_t bestPaletteID = PALETTE_MODE_CUSTOM;
    
    // Compare against all standard palettes
    for (uint8_t pid = 0; pid < STANDARD_PALETTE_COUNT; pid++) {
        const PaletteColor* standardPal = s_data->palettes[pid];
        
        // Calculate total color distance
        // Weight index 2 (black) MUCH more heavily since ALL standard palettes have black there
        int32_t totalDistance = 0;
        for (int i = 0; i < STANDARD_PALETTE_COLORS; i++) {
            int32_t dist = colorDistance(customPalette[i], standardPal[i]);
            
            // Index 2 MUST match - weight it 100x more
            if (i == 2) {
                dist *= 100;
            }
            
            totalDistance += dist;
        }
        
        if (totalDistance < bestDistance) {
            bestDistance = totalDistance;
            bestPaletteID = pid;
        }
    }
    
    if (outDistance) {
        *outDistance = bestDistance;
    }
    
    // If distance is very large, consider it not a match
    // Note: threshold is higher now because we weight index 2 by 100x
    const int32_t MATCH_THRESHOLD = 50000;
    if (bestDistance > MATCH_THRESHOLD) {
        return PALETTE_MODE_CUSTOM;
    }
    
    return bestPaletteID;
}

// =============================================================================
// Enumeration
// =============================================================================

void StandardPaletteLibrary::enumeratePalettes(void (*callback)(uint8_t id, const StandardPaletteInfo* info)) {
    if (!isInitialized() || !callback) {
        return;
    }
    
    for (uint8_t i = 0; i < STANDARD_PALETTE_COUNT; i++) {
        callback(i, &s_data->info[i]);
    }
}

int32_t StandardPaletteLibrary::getPalettesByCategory(const char* category, uint8_t* outIDs) {
    if (!isInitialized() || !category || !outIDs) {
        return 0;
    }
    
    int32_t count = 0;
    for (uint8_t i = 0; i < STANDARD_PALETTE_COUNT; i++) {
        if (s_data->categories[i] == category) {
            outIDs[count++] = i;
        }
    }
    
    return count;
}

// =============================================================================
// Error Handling
// =============================================================================

const std::string& StandardPaletteLibrary::getLastError() {
    static std::string emptyError;
    if (!s_data) {
        return emptyError;
    }
    return s_data->lastError;
}

void StandardPaletteLibrary::clearError() {
    if (s_data) {
        s_data->lastError.clear();
    }
}

void StandardPaletteLibrary::setError(const std::string& error) {
    if (!s_data) {
        s_data = new LibraryData();
    }
    s_data->lastError = error;
}

// =============================================================================
// JSON Parsing (Simple hand-rolled parser for minimal dependencies)
// =============================================================================

// Simple JSON value extractor
static std::string extractStringValue(const std::string& json, const std::string& key, size_t start) {
    size_t keyPos = json.find("\"" + key + "\"", start);
    if (keyPos == std::string::npos) return "";

    size_t colonPos = json.find(":", keyPos);
    if (colonPos == std::string::npos) return "";

    size_t quoteStart = json.find("\"", colonPos);
    if (quoteStart == std::string::npos) return "";

    size_t quoteEnd = json.find("\"", quoteStart + 1);
    if (quoteEnd == std::string::npos) return "";

    return json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
}

static int extractIntValue(const std::string& json, const std::string& key, size_t start) {
    size_t keyPos = json.find("\"" + key + "\"", start);
    if (keyPos == std::string::npos) return 0;

    size_t colonPos = json.find(":", keyPos);
    if (colonPos == std::string::npos) return 0;

    size_t numStart = colonPos + 1;
    while (numStart < json.size() && (json[numStart] == ' ' || json[numStart] == '\t' || json[numStart] == '\n')) {
        numStart++;
    }

    std::string numStr;
    while (numStart < json.size() && (isdigit(json[numStart]) || json[numStart] == '-')) {
        numStr += json[numStart++];
    }

    return numStr.empty() ? 0 : std::stoi(numStr);
}

static size_t findKey(const std::string& json, const std::string& key, size_t start) {
    return json.find("\"" + key + "\"", start);
}

bool StandardPaletteLibrary::parseJSON(const std::string& json) {
    // Find "palettes" array
    size_t palettesPos = json.find("\"palettes\"");
    if (palettesPos == std::string::npos) {
        setError("JSON: 'palettes' key not found");
        return false;
    }

    size_t arrayStart = json.find("[", palettesPos);
    if (arrayStart == std::string::npos) {
        setError("JSON: palettes array not found");
        return false;
    }

    // Parse each palette
    size_t pos = arrayStart + 1;
    int paletteIndex = 0;

    while (paletteIndex < STANDARD_PALETTE_COUNT) {
        size_t objStart = json.find("{", pos);
        if (objStart == std::string::npos) break;

        // Find the matching closing brace for this palette object
        int braceDepth = 1;
        size_t objEnd = objStart + 1;
        while (objEnd < json.size() && braceDepth > 0) {
            if (json[objEnd] == '{') braceDepth++;
            else if (json[objEnd] == '}') braceDepth--;
            objEnd++;
        }

        if (braceDepth != 0) {
            setError("JSON: Unmatched braces in palette object");
            return false;
        }

        // Extract palette fields
        int id = extractIntValue(json, "id", objStart);
        std::string name = extractStringValue(json, "name", objStart);
        std::string description = extractStringValue(json, "description", objStart);
        std::string category = extractStringValue(json, "category", objStart);

        if (id < 0 || id >= STANDARD_PALETTE_COUNT) {
            setError("JSON: Invalid palette ID: " + std::to_string(id));
            return false;
        }

        // Store metadata in persistent storage
        s_data->names[id] = name;
        s_data->descriptions[id] = description;
        s_data->categories[id] = category;
        s_data->info[id].id = id;
        s_data->info[id].name = s_data->names[id].c_str();
        s_data->info[id].description = s_data->descriptions[id].c_str();
        s_data->info[id].category = s_data->categories[id].c_str();

        // Find colors array
        size_t colorsPos = json.find("\"colors\"", objStart);
        if (colorsPos == std::string::npos || colorsPos > objEnd) {
            setError("JSON: 'colors' not found for palette " + std::to_string(id));
            return false;
        }

        size_t colorsArrayStart = json.find("[", colorsPos);
        if (colorsArrayStart == std::string::npos || colorsArrayStart > objEnd) {
            setError("JSON: colors array not found for palette " + std::to_string(id));
            return false;
        }

        // Parse colors
        size_t colorPos = colorsArrayStart + 1;
        int colorIndex = 0;

        while (colorIndex < STANDARD_PALETTE_COLORS) {
            size_t colorObjStart = json.find("{", colorPos);
            if (colorObjStart == std::string::npos || colorObjStart > objEnd) break;

            // Find matching closing brace for color object
            int colorBraceDepth = 1;
            size_t colorObjEnd = colorObjStart + 1;
            while (colorObjEnd < json.size() && colorBraceDepth > 0) {
                if (json[colorObjEnd] == '{') colorBraceDepth++;
                else if (json[colorObjEnd] == '}') colorBraceDepth--;
                colorObjEnd++;
            }

            int r = extractIntValue(json, "r", colorObjStart);
            int g = extractIntValue(json, "g", colorObjStart);
            int b = extractIntValue(json, "b", colorObjStart);
            int a = extractIntValue(json, "a", colorObjStart);

            s_data->palettes[id][colorIndex].r = static_cast<uint8_t>(r);
            s_data->palettes[id][colorIndex].g = static_cast<uint8_t>(g);
            s_data->palettes[id][colorIndex].b = static_cast<uint8_t>(b);
            s_data->palettes[id][colorIndex].a = (a == 0) ? 255 : static_cast<uint8_t>(a);

            colorPos = colorObjEnd;
            colorIndex++;
        }

        if (colorIndex != STANDARD_PALETTE_COLORS) {
            setError("JSON: Expected 16 colors for palette " + std::to_string(id) + ", got " + std::to_string(colorIndex));
            return false;
        }

        pos = objEnd;
        paletteIndex++;
    }

    if (paletteIndex != STANDARD_PALETTE_COUNT) {
        setError("JSON: Expected 32 palettes, parsed " + std::to_string(paletteIndex));
        return false;
    }

    return true;
}

// =============================================================================
// Binary Parsing
// =============================================================================

bool StandardPaletteLibrary::parseBinary(const uint8_t* binaryData, size_t size) {
    // Binary format:
    // Header (16 bytes):
    //   Magic: "STPL" (4 bytes)
    //   Version: uint16 (2 bytes)
    //   Count: uint16 (2 bytes)
    //   Reserved: 8 bytes
    // Palette Data (2048 bytes):
    //   32 palettes × 16 colors × 4 bytes (RGBA)
    
    const size_t HEADER_SIZE = 16;
    const size_t EXPECTED_SIZE = HEADER_SIZE + (STANDARD_PALETTE_COUNT * STANDARD_PALETTE_COLORS * 4);
    
    if (size < EXPECTED_SIZE) {
        setError("Binary file too small: expected " + std::to_string(EXPECTED_SIZE) + " bytes");
        return false;
    }
    
    // Check magic
    if (binaryData[0] != 'S' || binaryData[1] != 'T' || 
        binaryData[2] != 'P' || binaryData[3] != 'L') {
        setError("Invalid binary format: bad magic number");
        return false;
    }
    
    // Read version (little-endian)
    uint16_t version = binaryData[4] | (binaryData[5] << 8);
    if (version != 1) {
        setError("Unsupported binary version: " + std::to_string(version));
        return false;
    }
    
    // Read count
    uint16_t count = binaryData[6] | (binaryData[7] << 8);
    if (count != STANDARD_PALETTE_COUNT) {
        setError("Invalid palette count: " + std::to_string(count));
        return false;
    }
    
    // Read palette data
    const uint8_t* paletteData = binaryData + HEADER_SIZE;
    for (int pid = 0; pid < STANDARD_PALETTE_COUNT; pid++) {
        for (int cid = 0; cid < STANDARD_PALETTE_COLORS; cid++) {
            size_t offset = (pid * STANDARD_PALETTE_COLORS + cid) * 4;
            s_data->palettes[pid][cid].r = paletteData[offset + 0];
            s_data->palettes[pid][cid].g = paletteData[offset + 1];
            s_data->palettes[pid][cid].b = paletteData[offset + 2];
            s_data->palettes[pid][cid].a = paletteData[offset + 3];
        }
        
        // Set default names for binary format (no metadata)
        s_data->names[pid] = "Palette " + std::to_string(pid);
        s_data->descriptions[pid] = "Standard palette " + std::to_string(pid);
        s_data->categories[pid] = "unknown";
        s_data->info[pid].name = s_data->names[pid].c_str();
        s_data->info[pid].description = s_data->descriptions[pid].c_str();
        s_data->info[pid].category = s_data->categories[pid].c_str();
    }
    
    return true;
}

// =============================================================================
// Helper Functions
// =============================================================================

int32_t StandardPaletteLibrary::colorDistance(const PaletteColor& c1, const PaletteColor& c2) {
    // Simple Euclidean distance in RGB space
    int32_t dr = static_cast<int32_t>(c1.r) - static_cast<int32_t>(c2.r);
    int32_t dg = static_cast<int32_t>(c1.g) - static_cast<int32_t>(c2.g);
    int32_t db = static_cast<int32_t>(c1.b) - static_cast<int32_t>(c2.b);
    
    return dr * dr + dg * dg + db * db;
}

} // namespace SuperTerminal