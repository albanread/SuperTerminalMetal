//
//  SIDBank.h
//  SuperTerminal Framework
//
//  SID Bank - ID-based storage and management for SID music files
//  Follows the Sound Bank / Music Bank pattern
//
//  Copyright © 2024-2026 Alban Read. All rights reserved.
//

#ifndef SUPERTERMINAL_SID_BANK_H
#define SUPERTERMINAL_SID_BANK_H

#include <cstdint>
#include <string>
#include <memory>
#include <mutex>
#include <map>
#include <vector>

namespace SuperTerminal {

/// Storage for a loaded SID file
struct SIDData {
    std::vector<uint8_t> fileData;  // Raw SID file data
    std::string title;              // Song title
    std::string author;             // Author/composer
    std::string copyright;          // Copyright/release info
    std::string format;             // Format description
    int subtunes;                   // Number of subtunes
    int startSubtune;               // Default subtune
    int currentSubtune;             // Currently selected subtune
    int sidChipCount;               // Number of SID chips
    int sidModel;                   // SID chip model (0=6581, 1=8580)
    bool isRSID;                    // True if RSID format

    SIDData()
        : subtunes(0)
        , startSubtune(0)
        , currentSubtune(0)
        , sidChipCount(1)
        , sidModel(0)
        , isRSID(false)
    {}
};

/// SIDBank - Thread-safe storage for SID music files
///
/// Provides ID-based management following the Sound Bank / Music Bank pattern.
/// Each SID file is assigned a unique ID (starting at 1) and can be retrieved,
/// queried, and deleted using this ID.
///
/// Thread-safe: All operations are protected by mutex locks.
///
/// Example usage:
/// ```cpp
/// SIDBank bank;
/// uint32_t id = bank.loadFromFile("music/commando.sid");
/// if (id > 0) {
///     std::string title = bank.getTitle(id);
///     int subtunes = bank.getSubtuneCount(id);
///     const SIDData* data = bank.get(id);
///     // ... use data ...
///     bank.free(id);
/// }
/// ```
class SIDBank {
public:
    SIDBank();
    ~SIDBank();

    // Non-copyable
    SIDBank(const SIDBank&) = delete;
    SIDBank& operator=(const SIDBank&) = delete;

    // ========== Loading ==========

    /// Load a SID file from disk
    /// @param path Full path to .sid file
    /// @return SID ID (> 0) on success, 0 on failure
    uint32_t loadFromFile(const std::string& path);

    /// Load a SID file from memory buffer
    /// @param data Pointer to SID file data
    /// @param size Size of data in bytes
    /// @return SID ID (> 0) on success, 0 on failure
    uint32_t loadFromMemory(const uint8_t* data, size_t size);

    // ========== Retrieval ==========

    /// Get SID data by ID
    /// @param id SID ID
    /// @return Pointer to SIDData, or nullptr if not found
    const SIDData* get(uint32_t id) const;

    /// Check if SID exists
    /// @param id SID ID
    /// @return True if SID exists
    bool exists(uint32_t id) const;

    // ========== Metadata Queries ==========

    /// Get song title
    /// @param id SID ID
    /// @return Title string (empty if not found)
    std::string getTitle(uint32_t id) const;

    /// Get author/composer name
    /// @param id SID ID
    /// @return Author string (empty if not found)
    std::string getAuthor(uint32_t id) const;

    /// Get copyright/release info
    /// @param id SID ID
    /// @return Copyright string (empty if not found)
    std::string getCopyright(uint32_t id) const;

    /// Get format description
    /// @param id SID ID
    /// @return Format string (empty if not found)
    std::string getFormat(uint32_t id) const;

    /// Get number of subtunes
    /// @param id SID ID
    /// @return Number of subtunes (0 if not found)
    int getSubtuneCount(uint32_t id) const;

    /// Get default starting subtune
    /// @param id SID ID
    /// @return Starting subtune number (0 if not found)
    int getStartSubtune(uint32_t id) const;

    /// Get currently selected subtune
    /// @param id SID ID
    /// @return Current subtune number (0 if not found)
    int getCurrentSubtune(uint32_t id) const;

    /// Set current subtune (for tracking purposes)
    /// @param id SID ID
    /// @param subtune Subtune number
    /// @return True if successful
    bool setCurrentSubtune(uint32_t id, int subtune);

    /// Get number of SID chips required
    /// @param id SID ID
    /// @return Number of chips (0 if not found)
    int getSIDChipCount(uint32_t id) const;

    /// Get SID chip model
    /// @param id SID ID
    /// @return Chip model (0=6581, 1=8580, -1 if not found)
    int getSIDModel(uint32_t id) const;

    /// Check if RSID format
    /// @param id SID ID
    /// @return True if RSID, false otherwise
    bool isRSID(uint32_t id) const;

    // ========== Management ==========

    /// Free (delete) a SID from the bank
    /// @param id SID ID
    /// @return True if SID was found and deleted
    bool free(uint32_t id);

    /// Free all SIDs in the bank
    void freeAll();

    /// Get number of SIDs in the bank
    /// @return Number of loaded SIDs
    size_t getCount() const;

    /// Get total memory usage
    /// @return Total bytes used by all SIDs
    size_t getMemoryUsage() const;

    /// Get list of all SID IDs
    /// @return Vector of all valid SID IDs
    std::vector<uint32_t> getAllIDs() const;

private:
    /// Parse metadata from SID file data
    /// @param data SID file data
    /// @param sidData Output SIDData structure
    /// @return True if metadata parsed successfully
    bool parseMetadata(const std::vector<uint8_t>& data, SIDData& sidData);

    /// Validate SID file format
    /// @param data SID file data
    /// @return True if valid SID file
    bool validateSID(const std::vector<uint8_t>& data) const;

    mutable std::mutex m_mutex;
    std::map<uint32_t, std::unique_ptr<SIDData>> m_sids;
    uint32_t m_nextId;
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_SID_BANK_H
