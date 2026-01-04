//
//  VoiceScriptPlayer.h
//  SuperTerminal Framework - Voice Script Player
//
//  Background thread player for voice scripts
//  Manages library of compiled scripts and executes them non-blocking
//
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef SUPERTERMINAL_VOICE_SCRIPT_PLAYER_H
#define SUPERTERMINAL_VOICE_SCRIPT_PLAYER_H

#include "VoiceScript.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace SuperTerminal {

// Forward declarations
class VoiceController;

/// Voice Script Player - manages and executes voice scripts in background
class VoiceScriptPlayer {
public:
    /// Constructor
    /// @param voiceController Voice controller to use for playback
    VoiceScriptPlayer(VoiceController* voiceController);
    
    /// Destructor
    ~VoiceScriptPlayer();
    
    // Disable copy
    VoiceScriptPlayer(const VoiceScriptPlayer&) = delete;
    VoiceScriptPlayer& operator=(const VoiceScriptPlayer&) = delete;
    
    /// Initialize the player (starts background thread)
    /// @return true on success
    bool initialize();
    
    /// Shutdown the player (stops background thread)
    void shutdown();
    
    /// Check if player is initialized
    /// @return true if initialized
    bool isInitialized() const { return m_initialized; }
    
    // =========================================================================
    // Script Management
    // =========================================================================
    
    /// Define a new voice script (compile and store)
    /// @param name Script name (unique identifier)
    /// @param source Script source code
    /// @param error Output error message if compilation fails
    /// @return true on success, false on error
    bool defineScript(const std::string& name, const std::string& source, std::string& error);
    
    /// Remove a script from the library
    /// @param name Script name
    /// @return true if script was found and removed
    bool removeScript(const std::string& name);
    
    /// Check if a script exists
    /// @param name Script name
    /// @return true if script exists
    bool scriptExists(const std::string& name) const;
    
    /// Get list of all script names
    /// @return Vector of script names
    std::vector<std::string> getScriptNames() const;
    
    /// Get number of scripts in library
    /// @return Script count
    size_t getScriptCount() const;
    
    /// Get a script by name (for offline rendering)
    /// @param name Script name
    /// @return Pointer to bytecode or nullptr if not found
    const VoiceScriptBytecode* getScript(const std::string& name) const;
    
    /// Clear all scripts
    void clearAllScripts();
    
    // =========================================================================
    // Playback Control
    // =========================================================================
    
    /// Play a script (non-blocking)
    /// @param name Script name
    /// @param bpm Tempo in beats per minute (default: 120)
    /// @return true if script was found and started
    bool play(const std::string& name, float bpm = 120.0f);
    
    /// Stop current script playback
    void stop();
    
    /// Check if a script is currently playing
    /// @return true if playing
    bool isPlaying() const;
    
    /// Get name of currently playing script
    /// @return Script name or empty string if not playing
    std::string getCurrentScript() const;
    
    /// Set tempo for current playback
    /// @param bpm Beats per minute
    void setTempo(float bpm);
    
    /// Get current tempo
    /// @return Beats per minute
    float getTempo() const;
    
private:
    // Thread function
    void threadFunc();
    
    // Voice controller
    VoiceController* m_voiceController;
    
    // Script library
    std::unordered_map<std::string, std::unique_ptr<VoiceScriptBytecode>> m_scripts;
    mutable std::mutex m_scriptsMutex;
    
    // Playback state
    std::unique_ptr<VoiceScriptInterpreter> m_interpreter;
    const VoiceScriptBytecode* m_currentScript;
    std::atomic<bool> m_playing;
    mutable std::mutex m_playbackMutex;
    
    // Thread management
    std::thread m_thread;
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_shutdown;
    std::condition_variable m_cv;
    std::mutex m_cvMutex;
    
    // Timing (use high-resolution timer for beat-accurate playback)
    static constexpr int UPDATE_INTERVAL_MS = 5; // Update every 5ms for smooth playback
    
    // Compiler (used for defineScript)
    VoiceScriptCompiler m_compiler;
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_VOICE_SCRIPT_PLAYER_H