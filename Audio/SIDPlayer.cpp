//
//  SIDPlayer.cpp
//  SuperTerminal Framework
//
//  SID Music Player - Commodore 64 SID chip emulation wrapper
//  Implementation of libsidplayfp wrapper
//
//  Copyright © 2024 Alban Read. All rights reserved.
//  With thanks to libsidplayer see licenses.

#include "SIDPlayer.h"
#include <sidplayfp/sidplayfp.h>
#include <sidplayfp/SidTune.h>
#include <sidplayfp/SidInfo.h>
#include <sidplayfp/SidConfig.h>
#include <sidplayfp/SidTuneInfo.h>
#include <sidplayfp/builders/residfp.h>
#include <cstring>
#include <algorithm>

namespace SuperTerminal {

// ============================================================================
// MARK: - Implementation Structure
// ============================================================================

struct SIDPlayer::Impl {
    // libsidplayfp objects
    std::unique_ptr<sidplayfp> player;
    std::unique_ptr<SidTune> tune;
    std::unique_ptr<ReSIDfpBuilder> builder;

    // State
    SIDPlaybackState state;
    int sampleRate;
    int maxSids;  // Maximum number of SID chips to emulate (1-3)
    bool initialized;
    bool loaded;

    // Configuration
    SIDQuality quality;
    SIDChipModel chipModel;
    float volume;
    bool stereo;
    float speed;

    // Current tune info
    int currentSubtune;

    // Error tracking
    std::string lastError;

    // Temporary buffer for int16 conversion
    std::vector<int16_t> tempBuffer;

    // Playback end detection
    int consecutiveZeroSamples;
    static constexpr int ZERO_SAMPLE_THRESHOLD = 5; // Stop after 5 consecutive zero-sample returns

    Impl()
        : player(nullptr)
        , tune(nullptr)
        , builder(nullptr)
        , state(SIDPlaybackState::Stopped)
        , sampleRate(48000)
        , maxSids(3)  // Support up to 3 SID chips
        , initialized(false)
        , loaded(false)
        , quality(SIDQuality::Good)
        , chipModel(SIDChipModel::Auto)
        , volume(1.0f)
        , stereo(false)
        , speed(1.0f)
        , currentSubtune(1)
        , consecutiveZeroSamples(0)
    {}

    ~Impl() {
        cleanup();
    }

    void cleanup() {
        tune.reset();
        builder.reset();
        player.reset();
        loaded = false;
        state = SIDPlaybackState::Stopped;
    }

    bool configurePlayer() {
        if (!player || !builder) {
            lastError = "Player or builder not initialized";
            return false;
        }

        SidConfig config;
        config.frequency = sampleRate;
        config.playback = stereo ? SidConfig::STEREO : SidConfig::MONO;
        config.defaultC64Model = SidConfig::PAL;
        config.forceC64Model = false; // Let tune choose PAL/NTSC
        config.defaultSidModel = SidConfig::MOS6581; // Old SID chip (most common)
        config.forceSidModel = false; // Use tune's preferred SID model
        config.powerOnDelay = SidConfig::DEFAULT_POWER_ON_DELAY;

        // Use INTERPOLATE for now as RESAMPLE_INTERPOLATE may cause distortion
        config.samplingMethod = SidConfig::INTERPOLATE;

        // Set SID emulation
        config.sidEmulation = builder.get();

        // Configure the player
        if (!player->config(config)) {
            lastError = player->error();
            return false;
        }

        return true;
    }

    bool loadTune() {
        if (!player || !tune) {
            lastError = "Player or tune not initialized";
            return false;
        }

        // Select the current subtune
        tune->selectSong(currentSubtune - 1); // libsidplayfp uses 0-based

        // Load into player
        if (!player->load(tune.get())) {
            lastError = player->error();
            return false;
        }

        loaded = true;
        return true;
    }
};

// ============================================================================
// MARK: - Constructor / Destructor
// ============================================================================

SIDPlayer::SIDPlayer()
    : m_impl(std::make_unique<Impl>())
{
}

SIDPlayer::~SIDPlayer() {
    shutdown();
}

// ============================================================================
// MARK: - Initialization
// ============================================================================

bool SIDPlayer::initialize(int sampleRate) {
    if (m_impl->initialized) {
        shutdown();
    }

    m_impl->sampleRate = sampleRate;

    try {
        m_impl->player = std::make_unique<sidplayfp>();
        m_impl->builder = std::make_unique<ReSIDfpBuilder>("ReSIDfp");

        // Create up to 3 SID chips for richer sound (tune will use what it needs)
        int chips = m_impl->builder->create(m_impl->maxSids);

        if (chips == 0) {
            m_impl->lastError = "Failed to create SID emulation";
            m_impl->cleanup();
            return false;
        }

        m_impl->builder->filter(true);

        if (!m_impl->configurePlayer()) {
            m_impl->cleanup();
            return false;
        }

        m_impl->initialized = true;
        return true;

    } catch (const std::exception& e) {
        m_impl->lastError = std::string("Initialization exception: ") + e.what();
        m_impl->cleanup();
        return false;
    }
}

void SIDPlayer::shutdown() {
    if (m_impl->initialized) {
        stop();
        m_impl->cleanup();
        m_impl->initialized = false;
    }
}

bool SIDPlayer::isInitialized() const {
    return m_impl->initialized;
}

// ============================================================================
// MARK: - Loading
// ============================================================================

bool SIDPlayer::loadFromFile(const std::string& path) {
    if (!m_impl->initialized) {
        m_impl->lastError = "Player not initialized";
        return false;
    }

    try {
        // Create and load tune
        auto tune = std::make_unique<SidTune>(path.c_str());

        if (!tune->getStatus()) {
            m_impl->lastError = tune->statusString();
            return false;
        }

        // Store tune
        m_impl->tune = std::move(tune);

        // Get info
        const SidTuneInfo* info = m_impl->tune->getInfo();
        m_impl->currentSubtune = info->startSong();

        // Load into player
        if (!m_impl->loadTune()) {
            m_impl->tune.reset();
            return false;
        }

        m_impl->state = SIDPlaybackState::Stopped;
        return true;

    } catch (const std::exception& e) {
        m_impl->lastError = std::string("Load exception: ") + e.what();
        return false;
    }
}

bool SIDPlayer::loadFromMemory(const uint8_t* data, size_t size) {
    if (!m_impl->initialized) {
        m_impl->lastError = "Player not initialized";
        return false;
    }

    if (!data || size == 0) {
        m_impl->lastError = "Invalid data or size";
        return false;
    }

    try {
        // Create and load tune from memory
        auto tune = std::make_unique<SidTune>(data, size);

        if (!tune->getStatus()) {
            m_impl->lastError = tune->statusString();
            return false;
        }

        // Store tune
        m_impl->tune = std::move(tune);

        // Get info
        const SidTuneInfo* info = m_impl->tune->getInfo();
        m_impl->currentSubtune = info->startSong();

        // Load into player
        if (!m_impl->loadTune()) {
            m_impl->tune.reset();
            return false;
        }

        m_impl->state = SIDPlaybackState::Stopped;
        return true;

    } catch (const std::exception& e) {
        m_impl->lastError = std::string("Load exception: ") + e.what();
        return false;
    }
}

void SIDPlayer::unload() {
    stop();
    m_impl->tune.reset();
    m_impl->loaded = false;
}

bool SIDPlayer::isLoaded() const {
    return m_impl->loaded;
}

// ============================================================================
// MARK: - Playback Control
// ============================================================================

void SIDPlayer::play() {
    if (!m_impl->loaded || !m_impl->player) {
        return;
    }

    if (m_impl->state == SIDPlaybackState::Paused) {
        // Just resume
        m_impl->state = SIDPlaybackState::Playing;
    } else {
        // Start fresh
        m_impl->player->stop();
        m_impl->loadTune(); // Reload to reset state
        m_impl->state = SIDPlaybackState::Playing;
        m_impl->consecutiveZeroSamples = 0; // Reset end detection
    }
}

void SIDPlayer::stop() {
    if (m_impl->player) {
        m_impl->player->stop();
    }
    m_impl->state = SIDPlaybackState::Stopped;
    m_impl->consecutiveZeroSamples = 0; // Reset end detection
}

void SIDPlayer::pause() {
    if (m_impl->state == SIDPlaybackState::Playing) {
        m_impl->state = SIDPlaybackState::Paused;
        m_impl->consecutiveZeroSamples = 0; // Reset end detection
    }
}

void SIDPlayer::resume() {
    if (m_impl->state == SIDPlaybackState::Paused) {
        m_impl->state = SIDPlaybackState::Playing;
        m_impl->consecutiveZeroSamples = 0; // Reset end detection
    }
}

SIDPlaybackState SIDPlayer::getState() const {
    return m_impl->state;
}

bool SIDPlayer::isPlaying() const {
    return m_impl->state == SIDPlaybackState::Playing;
}

bool SIDPlayer::isPaused() const {
    return m_impl->state == SIDPlaybackState::Paused;
}

// ============================================================================
// MARK: - Subtune Selection
// ============================================================================

bool SIDPlayer::setSubtune(int subtune) {
    if (!m_impl->tune) {
        return false;
    }

    const SidTuneInfo* info = m_impl->tune->getInfo();
    if (subtune < 1 || subtune > static_cast<int>(info->songs())) {
        return false;
    }

    m_impl->currentSubtune = subtune;

    // Reload tune with new subtune
    if (m_impl->loaded) {
        bool wasPlaying = isPlaying();
        stop();
        m_impl->loadTune();
        if (wasPlaying) {
            play();
        }
    }

    return true;
}

int SIDPlayer::getCurrentSubtune() const {
    return m_impl->currentSubtune;
}

int SIDPlayer::getSubtuneCount() const {
    if (!m_impl->tune) {
        return 0;
    }

    const SidTuneInfo* info = m_impl->tune->getInfo();
    return static_cast<int>(info->songs());
}

int SIDPlayer::getStartSubtune() const {
    if (!m_impl->tune) {
        return 0;
    }

    const SidTuneInfo* info = m_impl->tune->getInfo();
    return static_cast<int>(info->startSong());
}

// ============================================================================
// MARK: - Audio Generation
// ============================================================================

size_t SIDPlayer::generateSamples(float* buffer, size_t frameCount) {
    if (!m_impl->player || !m_impl->loaded || m_impl->state != SIDPlaybackState::Playing) {
        // Fill with silence
        std::memset(buffer, 0, frameCount * 2 * sizeof(float));
        return frameCount;
    }

    // Ensure temp buffer is large enough
    size_t sampleCount = frameCount * 2; // stereo
    if (m_impl->tempBuffer.size() < sampleCount) {
        m_impl->tempBuffer.resize(sampleCount);
    }

    // Generate samples from libsidplayfp (int16)
    uint_least32_t generated = m_impl->player->play(m_impl->tempBuffer.data(), frameCount);

    // Detect end of playback - if we get 0 samples multiple times, the tune has ended
    if (generated == 0) {
        m_impl->consecutiveZeroSamples++;
        if (m_impl->consecutiveZeroSamples >= m_impl->ZERO_SAMPLE_THRESHOLD) {
            // Tune has ended - automatically stop
            m_impl->state = SIDPlaybackState::Stopped;
            std::memset(buffer, 0, frameCount * 2 * sizeof(float));
            return 0;
        }
    } else {
        // Reset counter if we got samples
        m_impl->consecutiveZeroSamples = 0;
    }

    // Convert int16 to float (-1.0 to 1.0) and apply volume
    for (size_t i = 0; i < generated * 2; i++) {
        buffer[i] = (m_impl->tempBuffer[i] / 32768.0f) * m_impl->volume;
    }

    // Fill remainder with silence if needed
    if (generated < frameCount) {
        std::memset(buffer + generated * 2, 0, (frameCount - generated) * 2 * sizeof(float));
    }

    return generated;
}

size_t SIDPlayer::generateSamplesInt16(int16_t* buffer, size_t frameCount) {
    if (!m_impl->player || !m_impl->loaded || m_impl->state != SIDPlaybackState::Playing) {
        // Fill with silence
        std::memset(buffer, 0, frameCount * 2 * sizeof(int16_t));
        return frameCount;
    }

    // Generate samples directly
    uint_least32_t generated = m_impl->player->play(buffer, frameCount);

    // Detect end of playback - if we get 0 samples multiple times, the tune has ended
    if (generated == 0) {
        m_impl->consecutiveZeroSamples++;
        if (m_impl->consecutiveZeroSamples >= m_impl->ZERO_SAMPLE_THRESHOLD) {
            // Tune has ended - automatically stop
            m_impl->state = SIDPlaybackState::Stopped;
            std::memset(buffer, 0, frameCount * 2 * sizeof(int16_t));
            return 0;
        }
    } else {
        // Reset counter if we got samples
        m_impl->consecutiveZeroSamples = 0;
    }

    // Apply volume if not 1.0
    if (m_impl->volume != 1.0f) {
        for (size_t i = 0; i < generated * 2; i++) {
            buffer[i] = static_cast<int16_t>(buffer[i] * m_impl->volume);
        }
    }

    // Fill remainder with silence if needed
    if (generated < frameCount) {
        std::memset(buffer + generated * 2, 0, (frameCount - generated) * 2 * sizeof(int16_t));
    }

    return generated;
}

// ============================================================================
// MARK: - Metadata
// ============================================================================

SIDInfo SIDPlayer::getInfo() const {
    SIDInfo result;

    if (!m_impl->tune) {
        return result;
    }

    const SidTuneInfo* info = m_impl->tune->getInfo();
    if (!info) {
        return result;
    }

    // Get strings (handle NULL pointers)
    if (info->numberOfInfoStrings() > 0 && info->infoString(0)) {
        result.title = info->infoString(0);
    }
    if (info->numberOfInfoStrings() > 1 && info->infoString(1)) {
        result.author = info->infoString(1);
    }
    if (info->numberOfInfoStrings() > 2 && info->infoString(2)) {
        result.copyright = info->infoString(2);
    }

    // Format string
    if (info->formatString()) {
        result.formatString = info->formatString();
    }

    // Numeric info
    result.subtunes = static_cast<int>(info->songs());
    result.startSubtune = static_cast<int>(info->startSong());
    result.currentSubtune = m_impl->currentSubtune;
    result.loadAddress = info->loadAddr();
    result.initAddress = info->initAddr();
    result.playAddress = info->playAddr();
    result.sidChipCount = info->sidChips();
    result.sidModel = info->sidModel(0); // Primary SID model
    result.isRSID = (info->compatibility() == SidTuneInfo::COMPATIBILITY_R64);

    return result;
}

std::string SIDPlayer::getTitle() const {
    if (!m_impl->tune) {
        return "";
    }

    const SidTuneInfo* info = m_impl->tune->getInfo();
    if (info && info->numberOfInfoStrings() > 0 && info->infoString(0)) {
        return info->infoString(0);
    }

    return "";
}

std::string SIDPlayer::getAuthor() const {
    if (!m_impl->tune) {
        return "";
    }

    const SidTuneInfo* info = m_impl->tune->getInfo();
    if (info && info->numberOfInfoStrings() > 1 && info->infoString(1)) {
        return info->infoString(1);
    }

    return "";
}

std::string SIDPlayer::getCopyright() const {
    if (!m_impl->tune) {
        return "";
    }

    const SidTuneInfo* info = m_impl->tune->getInfo();
    if (info && info->numberOfInfoStrings() > 2 && info->infoString(2)) {
        return info->infoString(2);
    }

    return "";
}

std::string SIDPlayer::getFormat() const {
    if (!m_impl->tune) {
        return "";
    }

    const SidTuneInfo* info = m_impl->tune->getInfo();
    if (info && info->formatString()) {
        return info->formatString();
    }

    return "";
}

int SIDPlayer::getSIDModel() const {
    if (!m_impl->tune) {
        return 0;
    }

    const SidTuneInfo* info = m_impl->tune->getInfo();
    if (info) {
        return info->sidModel(0);
    }

    return 0;
}

int SIDPlayer::getSIDChipCount() const {
    if (!m_impl->tune) {
        return 1;
    }

    const SidTuneInfo* info = m_impl->tune->getInfo();
    if (info) {
        return info->sidChips();
    }

    return 1;
}

// ============================================================================
// MARK: - Configuration
// ============================================================================

void SIDPlayer::setQuality(SIDQuality quality) {
    m_impl->quality = quality;
    if (m_impl->initialized) {
        m_impl->configurePlayer();
    }
}

SIDQuality SIDPlayer::getQuality() const {
    return m_impl->quality;
}

void SIDPlayer::setChipModel(SIDChipModel model) {
    m_impl->chipModel = model;

    if (m_impl->builder) {
        // Configure chip model in builder
        switch (model) {
            case SIDChipModel::MOS6581:
                m_impl->builder->filter6581Curve(0.5); // Default 6581 filter
                break;
            case SIDChipModel::MOS8580:
                m_impl->builder->filter8580Curve(12500); // Default 8580 filter
                break;
            case SIDChipModel::Auto:
                // Use tune's specified model
                break;
        }
    }
}

SIDChipModel SIDPlayer::getChipModel() const {
    return m_impl->chipModel;
}

void SIDPlayer::setVolume(float volume) {
    m_impl->volume = std::max(0.0f, std::min(1.0f, volume));
}

float SIDPlayer::getVolume() const {
    return m_impl->volume;
}

void SIDPlayer::setStereo(bool enable) {
    m_impl->stereo = enable;
    if (m_impl->initialized) {
        m_impl->configurePlayer();
    }
}

bool SIDPlayer::isStereo() const {
    return m_impl->stereo;
}

void SIDPlayer::setSpeed(float speed) {
    m_impl->speed = std::max(0.1f, std::min(4.0f, speed));
    // Note: libsidplayfp doesn't have direct speed control,
    // would need to implement via sample rate manipulation
}

float SIDPlayer::getSpeed() const {
    return m_impl->speed;
}

void SIDPlayer::setMaxSids(int maxSids) {
    // Clamp to valid range (1-3 SID chips)
    m_impl->maxSids = std::max(1, std::min(3, maxSids));
    // Note: Changing maxSids requires re-initialization
    // Must call shutdown() and initialize() again for this to take effect
}

int SIDPlayer::getMaxSids() const {
    return m_impl->maxSids;
}

// ============================================================================
// MARK: - Diagnostics
// ============================================================================

int SIDPlayer::getSampleRate() const {
    return m_impl->sampleRate;
}

std::string SIDPlayer::getError() const {
    return m_impl->lastError;
}

void SIDPlayer::reset() {
    stop();
    unload();
    if (m_impl->player) {
        m_impl->player->stop();
    }
}

} // namespace SuperTerminal
