//
//  SIDPlayer.h
//  SuperTerminal Framework
//
//  SID Music Player - Commodore 64 SID chip emulation wrapper
//  Wraps libsidplayfp for playback of .sid music files
//
//  Copyright © 2024-2026 Alban Read. All rights reserved.
//  With thanks to Sidplayerfp see licenses

#ifndef SUPERTERMINAL_SID_PLAYER_H
#define SUPERTERMINAL_SID_PLAYER_H

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace SuperTerminal {

/// Information about a loaded SID tune
struct SIDInfo {
    std::string title;           // Song title (from T: field)
    std::string author;          // Author/composer name (from A: field)
    std::string copyright;       // Copyright/release info (from C: field)
    std::string formatString;    // Format description (PSID/RSID)
    int subtunes;                // Total number of subtunes
    int startSubtune;            // Default starting subtune
    int currentSubtune;          // Currently selected subtune
    int loadAddress;             // Load address of music data
    int initAddress;             // Init routine address
    int playAddress;             // Play routine address
    int sidChipCount;            // Number of SID chips (1-3)
    int sidModel;                // SID chip model (0=6581, 1=8580)
    bool isRSID;                 // True if RSID format (hardware accurate)

    SIDInfo()
        : subtunes(0)
        , startSubtune(0)
        , currentSubtune(0)
        , loadAddress(0)
        , initAddress(0)
        , playAddress(0)
        , sidChipCount(1)
        , sidModel(0)
        , isRSID(false)
    {}
};

/// Quality levels for SID emulation
enum class SIDQuality {
    Fast = 0,       // Fastest, lower quality
    Medium = 1,     // Balanced
    Good = 2,       // Higher quality
    Best = 3        // Highest quality, more CPU
};

/// SID chip models
enum class SIDChipModel {
    MOS6581 = 0,    // Original C64 SID chip
    MOS8580 = 1,    // Later C64C SID chip
    Auto = 2        // Auto-detect from tune
};

/// Playback state
enum class SIDPlaybackState {
    Stopped,
    Playing,
    Paused
};

/// SIDPlayer - Wrapper around libsidplayfp for C64 SID music playback
class SIDPlayer {
public:
    SIDPlayer();
    ~SIDPlayer();

    // Non-copyable
    SIDPlayer(const SIDPlayer&) = delete;
    SIDPlayer& operator=(const SIDPlayer&) = delete;

    /// Initialize the SID player with specified sample rate
    /// @param sampleRate Audio sample rate (typically 48000)
    /// @return True if initialization successful
    bool initialize(int sampleRate = 48000);

    /// Shutdown the SID player
    void shutdown();

    /// Check if initialized
    /// @return True if player is initialized
    bool isInitialized() const;

    // ========== Loading ==========

    /// Load a SID file from disk
    /// @param path Full path to .sid file
    /// @return True if loaded successfully
    bool loadFromFile(const std::string& path);

    /// Load a SID file from memory buffer
    /// @param data Pointer to SID file data
    /// @param size Size of data in bytes
    /// @return True if loaded successfully
    bool loadFromMemory(const uint8_t* data, size_t size);

    /// Unload current tune
    void unload();

    /// Check if a tune is loaded
    /// @return True if tune is loaded
    bool isLoaded() const;

    // ========== Playback Control ==========

    /// Start playing the current tune
    void play();

    /// Stop playback
    void stop();

    /// Pause playback (can be resumed)
    void pause();

    /// Resume paused playback
    void resume();

    /// Get current playback state
    /// @return Current state (Stopped, Playing, Paused)
    SIDPlaybackState getState() const;

    /// Check if currently playing
    /// @return True if playing
    bool isPlaying() const;

    /// Check if paused
    /// @return True if paused
    bool isPaused() const;

    // ========== Subtune Selection ==========

    /// Select a subtune to play
    /// @param subtune Subtune number (1-based)
    /// @return True if subtune valid and selected
    bool setSubtune(int subtune);

    /// Get currently selected subtune
    /// @return Current subtune number (1-based)
    int getCurrentSubtune() const;

    /// Get total number of subtunes
    /// @return Number of subtunes available
    int getSubtuneCount() const;

    /// Get default starting subtune
    /// @return Default subtune number (1-based)
    int getStartSubtune() const;

    // ========== Audio Generation ==========

    /// Generate audio samples (stereo interleaved)
    /// @param buffer Output buffer for samples (stereo interleaved floats)
    /// @param frameCount Number of stereo frames to generate
    /// @return Number of frames actually generated
    size_t generateSamples(float* buffer, size_t frameCount);

    /// Generate audio samples (16-bit integer format)
    /// @param buffer Output buffer for samples (stereo interleaved int16)
    /// @param frameCount Number of stereo frames to generate
    /// @return Number of frames actually generated
    size_t generateSamplesInt16(int16_t* buffer, size_t frameCount);

    // ========== Metadata ==========

    /// Get complete information about current tune
    /// @return SIDInfo struct with all metadata
    SIDInfo getInfo() const;

    /// Get song title
    /// @return Title string (or empty if none)
    std::string getTitle() const;

    /// Get author/composer name
    /// @return Author string (or empty if none)
    std::string getAuthor() const;

    /// Get copyright/release info
    /// @return Copyright string (or empty if none)
    std::string getCopyright() const;

    /// Get format description
    /// @return Format string (e.g., "PlaySID v2NG, C64 compatible")
    std::string getFormat() const;

    /// Get SID chip model used
    /// @return Chip model (0=6581, 1=8580)
    int getSIDModel() const;

    /// Get number of SID chips required
    /// @return Number of chips (1-3)
    int getSIDChipCount() const;

    // ========== Configuration ==========

    /// Set emulation quality level
    /// @param quality Quality level (Fast, Medium, Good, Best)
    void setQuality(SIDQuality quality);

    /// Get current quality level
    /// @return Current quality setting
    SIDQuality getQuality() const;

    /// Set SID chip model to emulate
    /// @param model Chip model (6581, 8580, or Auto)
    void setChipModel(SIDChipModel model);

    /// Get current chip model setting
    /// @return Current chip model
    SIDChipModel getChipModel() const;

    /// Set master volume (affects output)
    /// @param volume Volume level (0.0 = silent, 1.0 = full)
    void setVolume(float volume);

    /// Get current volume
    /// @return Volume level (0.0 - 1.0)
    float getVolume() const;

    /// Enable/disable stereo SID (for multi-SID tunes)
    /// @param enable True to enable stereo
    void setStereo(bool enable);

    /// Check if stereo is enabled
    /// @return True if stereo enabled
    bool isStereo() const;

    /// Set playback speed (affects tempo)
    /// @param speed Speed multiplier (1.0 = normal, 2.0 = double speed, etc.)
    void setSpeed(float speed);

    /// Get current playback speed
    /// @return Speed multiplier
    float getSpeed() const;

    /// Set maximum number of SID chips to emulate
    /// @param maxSids Number of chips (1-3, default 3)
    void setMaxSids(int maxSids);

    /// Get maximum number of SID chips
    /// @return Maximum SID chips (1-3)
    int getMaxSids() const;

    // ========== Diagnostics ==========

    /// Get sample rate
    /// @return Current sample rate in Hz
    int getSampleRate() const;

    /// Get error message from last failed operation
    /// @return Error message string
    std::string getError() const;

    /// Reset player to initial state (stop, clear tune)
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_SID_PLAYER_H
