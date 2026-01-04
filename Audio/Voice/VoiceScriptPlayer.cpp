//
//  VoiceScriptPlayer.cpp
//  SuperTerminal Framework - Voice Script Player
//
//  Implementation of background thread player for voice scripts
//
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "VoiceScriptPlayer.h"
#include "VoiceController.h"
#include <chrono>
#include <thread>

namespace SuperTerminal {

// =============================================================================
// VoiceScriptPlayer Implementation
// =============================================================================

VoiceScriptPlayer::VoiceScriptPlayer(VoiceController* voiceController)
    : m_voiceController(voiceController)
    , m_currentScript(nullptr)
    , m_playing(false)
    , m_initialized(false)
    , m_shutdown(false)
{
    if (m_voiceController) {
        m_interpreter = std::make_unique<VoiceScriptInterpreter>(m_voiceController);
    }
}

VoiceScriptPlayer::~VoiceScriptPlayer() {
    shutdown();
}

bool VoiceScriptPlayer::initialize() {
    if (m_initialized) {
        return true;
    }

    if (!m_voiceController || !m_interpreter) {
        return false;
    }

    // Start background thread
    m_shutdown = false;
    m_thread = std::thread(&VoiceScriptPlayer::threadFunc, this);

    m_initialized = true;
    return true;
}

void VoiceScriptPlayer::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Signal shutdown
    m_shutdown = true;
    m_cv.notify_all();

    // Wait for thread to finish
    if (m_thread.joinable()) {
        m_thread.join();
    }

    // Stop playback
    stop();

    m_initialized = false;
}

// =============================================================================
// Script Management
// =============================================================================

bool VoiceScriptPlayer::defineScript(const std::string& name, const std::string& source, std::string& error) {
    error.clear();

    // Compile script
    auto bytecode = m_compiler.compile(source, name, error);
    if (!bytecode) {
        return false;
    }

    // Store in library
    std::lock_guard<std::mutex> lock(m_scriptsMutex);
    m_scripts[name] = std::move(bytecode);

    return true;
}

bool VoiceScriptPlayer::removeScript(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_scriptsMutex);

    // Don't remove if currently playing
    if (m_playing && m_currentScript && m_currentScript->name == name) {
        return false;
    }

    auto it = m_scripts.find(name);
    if (it == m_scripts.end()) {
        return false;
    }

    m_scripts.erase(it);
    return true;
}

bool VoiceScriptPlayer::scriptExists(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_scriptsMutex);
    return m_scripts.find(name) != m_scripts.end();
}

std::vector<std::string> VoiceScriptPlayer::getScriptNames() const {
    std::lock_guard<std::mutex> lock(m_scriptsMutex);
    std::vector<std::string> names;
    names.reserve(m_scripts.size());
    for (const auto& pair : m_scripts) {
        names.push_back(pair.first);
    }
    return names;
}

size_t VoiceScriptPlayer::getScriptCount() const {
    std::lock_guard<std::mutex> lock(m_scriptsMutex);
    return m_scripts.size();
}

const VoiceScriptBytecode* VoiceScriptPlayer::getScript(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_scriptsMutex);
    auto it = m_scripts.find(name);
    if (it != m_scripts.end()) {
        return it->second.get();
    }
    return nullptr;
}

void VoiceScriptPlayer::clearAllScripts() {
    std::lock_guard<std::mutex> lock(m_scriptsMutex);
    
    // Stop if playing
    if (m_playing) {
        stop();
    }
    
    m_scripts.clear();
}

// =============================================================================
// Playback Control
// =============================================================================

bool VoiceScriptPlayer::play(const std::string& name, float bpm) {
    if (!m_initialized || !m_interpreter) {
        return false;
    }

    // Find script
    const VoiceScriptBytecode* script = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_scriptsMutex);
        auto it = m_scripts.find(name);
        if (it == m_scripts.end()) {
            return false;
        }
        script = it->second.get();
    }

    // Stop current playback
    if (m_playing) {
        stop();
    }

    // Start new script
    {
        std::lock_guard<std::mutex> lock(m_playbackMutex);
        m_currentScript = script;
        m_interpreter->start(script, bpm);
        m_playing = true;
    }

    // Wake up thread
    m_cv.notify_all();

    return true;
}

void VoiceScriptPlayer::stop() {
    if (!m_playing) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_playbackMutex);
    if (m_interpreter) {
        m_interpreter->stop();
    }
    m_playing = false;
    m_currentScript = nullptr;
}

bool VoiceScriptPlayer::isPlaying() const {
    return m_playing;
}

std::string VoiceScriptPlayer::getCurrentScript() const {
    std::lock_guard<std::mutex> lock(m_playbackMutex);
    if (m_currentScript) {
        return m_currentScript->name;
    }
    return "";
}

void VoiceScriptPlayer::setTempo(float bpm) {
    std::lock_guard<std::mutex> lock(m_playbackMutex);
    if (m_interpreter) {
        m_interpreter->setTempo(bpm);
    }
}

float VoiceScriptPlayer::getTempo() const {
    std::lock_guard<std::mutex> lock(m_playbackMutex);
    if (m_interpreter) {
        return m_interpreter->getTempo();
    }
    return 120.0f;
}

// =============================================================================
// Background Thread
// =============================================================================

void VoiceScriptPlayer::threadFunc() {
    using namespace std::chrono;
    
    auto lastTime = high_resolution_clock::now();

    while (!m_shutdown) {
        // Calculate delta time
        auto currentTime = high_resolution_clock::now();
        auto deltaTime = duration_cast<duration<float>>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Update interpreter if playing
        if (m_playing) {
            bool stillRunning = false;
            {
                std::lock_guard<std::mutex> lock(m_playbackMutex);
                if (m_interpreter && m_interpreter->isRunning()) {
                    stillRunning = m_interpreter->update(deltaTime);
                }
            }

            if (!stillRunning) {
                // Script finished
                std::lock_guard<std::mutex> lock(m_playbackMutex);
                m_playing = false;
                m_currentScript = nullptr;
            }
        }

        // Sleep for update interval
        std::this_thread::sleep_for(milliseconds(UPDATE_INTERVAL_MS));
    }
}

} // namespace SuperTerminal