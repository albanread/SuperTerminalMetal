//
//  SIDBank.cpp
//  SuperTerminal Framework
//
//  SID Bank - ID-based storage and management for SID music files
//  Implementation of SIDBank class
//
//  Copyright © 2024-2026 Alban Read. All rights reserved.
//

#include "SIDBank.h"
#include "SIDPlayer.h"
#include <fstream>
#include <algorithm>

namespace SuperTerminal {

// ============================================================================
// MARK: - Constructor / Destructor
// ============================================================================

SIDBank::SIDBank()
    : m_nextId(1)
{
}

SIDBank::~SIDBank() {
    freeAll();
}

// ============================================================================
// MARK: - Loading
// ============================================================================

uint32_t SIDBank::loadFromFile(const std::string& path) {
    // Read file into memory
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return 0;
    }

    size_t size = file.tellg();
    if (size == 0) {
        return 0;
    }

    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        return 0;
    }

    // Load from memory
    return loadFromMemory(data.data(), size);
}

uint32_t SIDBank::loadFromMemory(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return 0;
    }

    // Validate SID format
    std::vector<uint8_t> fileData(data, data + size);
    if (!validateSID(fileData)) {
        return 0;
    }

    // Create SIDData entry
    auto sidData = std::make_unique<SIDData>();
    sidData->fileData = fileData;

    // Parse metadata
    if (!parseMetadata(fileData, *sidData)) {
        return 0;
    }

    // Assign ID and store
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t id = m_nextId++;
    m_sids[id] = std::move(sidData);

    return id;
}

// ============================================================================
// MARK: - Retrieval
// ============================================================================

const SIDData* SIDBank::get(uint32_t id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sids.find(id);
    if (it == m_sids.end()) {
        return nullptr;
    }

    return it->second.get();
}

bool SIDBank::exists(uint32_t id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sids.find(id) != m_sids.end();
}

// ============================================================================
// MARK: - Metadata Queries
// ============================================================================

std::string SIDBank::getTitle(uint32_t id) const {
    const SIDData* data = get(id);
    return data ? data->title : "";
}

std::string SIDBank::getAuthor(uint32_t id) const {
    const SIDData* data = get(id);
    return data ? data->author : "";
}

std::string SIDBank::getCopyright(uint32_t id) const {
    const SIDData* data = get(id);
    return data ? data->copyright : "";
}

std::string SIDBank::getFormat(uint32_t id) const {
    const SIDData* data = get(id);
    return data ? data->format : "";
}

int SIDBank::getSubtuneCount(uint32_t id) const {
    const SIDData* data = get(id);
    return data ? data->subtunes : 0;
}

int SIDBank::getStartSubtune(uint32_t id) const {
    const SIDData* data = get(id);
    return data ? data->startSubtune : 0;
}

int SIDBank::getCurrentSubtune(uint32_t id) const {
    const SIDData* data = get(id);
    return data ? data->currentSubtune : 0;
}

bool SIDBank::setCurrentSubtune(uint32_t id, int subtune) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sids.find(id);
    if (it == m_sids.end()) {
        return false;
    }

    if (subtune < 1 || subtune > it->second->subtunes) {
        return false;
    }

    it->second->currentSubtune = subtune;
    return true;
}

int SIDBank::getSIDChipCount(uint32_t id) const {
    const SIDData* data = get(id);
    return data ? data->sidChipCount : 0;
}

int SIDBank::getSIDModel(uint32_t id) const {
    const SIDData* data = get(id);
    return data ? data->sidModel : -1;
}

bool SIDBank::isRSID(uint32_t id) const {
    const SIDData* data = get(id);
    return data ? data->isRSID : false;
}

// ============================================================================
// MARK: - Management
// ============================================================================

bool SIDBank::free(uint32_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sids.find(id);
    if (it == m_sids.end()) {
        return false;
    }

    m_sids.erase(it);
    return true;
}

void SIDBank::freeAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sids.clear();
}

size_t SIDBank::getCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sids.size();
}

size_t SIDBank::getMemoryUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t total = 0;
    for (const auto& pair : m_sids) {
        const SIDData* data = pair.second.get();
        if (data) {
            // File data
            total += data->fileData.size();

            // Strings
            total += data->title.capacity();
            total += data->author.capacity();
            total += data->copyright.capacity();
            total += data->format.capacity();

            // Struct overhead
            total += sizeof(SIDData);
        }
    }

    return total;
}

std::vector<uint32_t> SIDBank::getAllIDs() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<uint32_t> ids;
    ids.reserve(m_sids.size());

    for (const auto& pair : m_sids) {
        ids.push_back(pair.first);
    }

    return ids;
}

// ============================================================================
// MARK: - Private Helpers
// ============================================================================

bool SIDBank::validateSID(const std::vector<uint8_t>& data) const {
    if (data.size() < 0x7C) {
        return false; // Too small to be valid SID file
    }

    // Check magic bytes (PSID or RSID)
    if ((data[0] == 'P' || data[0] == 'R') &&
        data[1] == 'S' &&
        data[2] == 'I' &&
        data[3] == 'D') {
        return true;
    }

    return false;
}

bool SIDBank::parseMetadata(const std::vector<uint8_t>& data, SIDData& sidData) {
    if (!validateSID(data)) {
        return false;
    }

    // Use SIDPlayer to parse metadata
    SIDPlayer parser;
    if (!parser.initialize()) {
        return false;
    }

    if (!parser.loadFromMemory(data.data(), data.size())) {
        return false;
    }

    // Extract metadata
    SIDInfo info = parser.getInfo();

    sidData.title = info.title;
    sidData.author = info.author;
    sidData.copyright = info.copyright;
    sidData.format = info.formatString;
    sidData.subtunes = info.subtunes;
    sidData.startSubtune = info.startSubtune;
    sidData.currentSubtune = info.startSubtune; // Initialize to start subtune
    sidData.sidChipCount = info.sidChipCount;
    sidData.sidModel = info.sidModel;
    sidData.isRSID = info.isRSID;

    return true;
}

} // namespace SuperTerminal
