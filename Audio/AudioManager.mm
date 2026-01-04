//
// AudioManager.mm
// SuperTerminal v2
//
// Audio management system implementation.
// Wraps SynthEngine for unified audio API.
//

#include "AudioManager.h"
#include "SynthEngine.h"
#include "SoundBank.h"
#include "MusicBank.h"
#include "SIDBank.h"
#include "SIDPlayer.h"
#include "MidiEngine.h"
#include "ABCParser.h"
#include "CoreAudioEngine.h"
#include "Voice/VoiceController.h"
#include "Voice/VoiceScriptPlayer.h"
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#include <unordered_map>
#include <fstream>

namespace SuperTerminal {

// =============================================================================
// Internal Implementation
// =============================================================================

struct AudioManager::Impl {
    std::unique_ptr<::SynthEngine> synthEngine;
    std::unique_ptr<SoundBank> soundBank;
    std::unique_ptr<MusicBank> musicBank;
    std::unique_ptr<SIDBank> sidBank;
    std::unique_ptr<SIDPlayer> sidPlayer;
    std::unique_ptr<MidiEngine> midiEngine;
    std::unique_ptr<ABCParser> abcParser;
    std::unique_ptr<CoreAudioEngine> coreAudioEngine;
    std::unique_ptr<VoiceController> voiceController;
    std::unique_ptr<VoiceScriptPlayer> voiceScriptPlayer;

    // Audio session and playback
    AVAudioEngine* audioEngine;
    AVAudioPlayerNode* playerNode;
    AVAudioFormat* audioFormat;
    AVAudioSourceNode* sidSourceNode API_AVAILABLE(macos(10.15));
    AVAudioSourceNode* voiceSourceNode API_AVAILABLE(macos(10.15));
    std::vector<float> sidInterleavedBuffer; // Pre-allocated buffer for SID audio conversion

    // State
    std::atomic<bool> initialized{false};
    std::atomic<float> soundVolume{1.0f};
    std::atomic<float> musicVolume{1.0f};
    MusicState musicState{MusicState::Stopped};

    // Thread safety
    mutable std::mutex mutex;
    mutable std::mutex musicMutex;

    // Sound effect ID mapping
    std::unordered_map<uint32_t, AVAudioPCMBuffer*> loadedSounds;
    uint32_t nextSoundId{1};

    // Music playback
    int currentMusicSequenceId{-1};
    std::shared_ptr<ABCTune> currentTune;

    // SID playback
    uint32_t currentSidId{0};
    float sidVolume{1.0f};

    // VOICES timeline recording system
    bool voicesRecording{false};
    std::atomic<bool> voicesPlaying{false};  // True when VOICES_END_PLAY buffer is playing (atomic for thread-safe access)
    float voicesBeatCursor{0.0f};
    float voicesTempo{120.0f}; // BPM (beats per minute)
    std::vector<AudioManager::VoiceCommand> voicesTimeline;

    Impl()
        : audioEngine(nil)
        , playerNode(nil)
        , audioFormat(nil)
        , sidSourceNode(nil)
        , voiceSourceNode(nil)
    {
        synthEngine = std::make_unique<::SynthEngine>();
        soundBank = std::make_unique<SoundBank>();
        musicBank = std::make_unique<MusicBank>();
        sidBank = std::make_unique<SIDBank>();
        sidPlayer = std::make_unique<SIDPlayer>();
        coreAudioEngine = std::make_unique<CoreAudioEngine>();
        midiEngine = std::make_unique<MidiEngine>();
        abcParser = std::make_unique<ABCParser>();
        voiceController = std::make_unique<VoiceController>(8, 48000.0f);
        voiceScriptPlayer = std::make_unique<VoiceScriptPlayer>(voiceController.get());
    }

    void ensureVoiceAudioNodeCreated();

    ~Impl() {
        if (audioEngine) {
            [audioEngine stop];
            audioEngine = nil;
        }
        playerNode = nil;
        audioFormat = nil;
        sidSourceNode = nil;
        voiceSourceNode = nil;

        // Clean up loaded sounds
        for (auto& pair : loadedSounds) {
            if (pair.second) {
                // ARC will handle cleanup
            }
        }
        loadedSounds.clear();
    }
};

// =============================================================================
// Constructor / Destructor
// =============================================================================

AudioManager::AudioManager()
    : m_impl(std::make_unique<Impl>())
{
}

AudioManager::~AudioManager() {
    shutdown();
}

AudioManager::AudioManager(AudioManager&&) noexcept = default;
AudioManager& AudioManager::operator=(AudioManager&&) noexcept = default;

// =============================================================================
// Initialization
// =============================================================================

bool AudioManager::initialize() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    if (m_impl->initialized) {
        return true;
    }

    @autoreleasepool {
        NSError* error = nil;

        // Note: AVAudioSession is iOS-only, not needed on macOS

        // Create audio engine
        m_impl->audioEngine = [[AVAudioEngine alloc] init];

        // Create player node
        m_impl->playerNode = [[AVAudioPlayerNode alloc] init];
        [m_impl->audioEngine attachNode:m_impl->playerNode];

        // Set up audio format (44.1kHz, stereo, float)
        m_impl->audioFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                               sampleRate:44100.0
                                                                 channels:2
                                                              interleaved:NO];

        // Connect player node to main mixer
        @try {
            [m_impl->audioEngine connect:m_impl->playerNode
                                       to:m_impl->audioEngine.mainMixerNode
                                   format:m_impl->audioFormat];

            // Start the audio engine
            [m_impl->audioEngine prepare];
            [m_impl->audioEngine startAndReturnError:&error];
            if (error) {
                NSLog(@"AudioManager: Failed to start audio engine: %@", error);
                return false;
            }
        }
        @catch (NSException *exception) {
            NSLog(@"AudioManager: Exception during audio engine initialization: %@", exception.reason);
            NSLog(@"AudioManager: Exception name: %@", exception.name);
            return false;
        }
        @catch (...) {
            NSLog(@"AudioManager: Unknown C++ exception during audio engine initialization");
            return false;
        }

        // Initialize synth engine
        if (!m_impl->synthEngine->initialize()) {
            NSLog(@"AudioManager: Failed to initialize synth engine");
            return false;
        }

        // Initialize CoreAudioEngine
        if (!m_impl->coreAudioEngine->initialize()) {
            NSLog(@"AudioManager: Failed to initialize CoreAudio engine");
            return false;
        }

        // Link synth engine to CoreAudio
        m_impl->coreAudioEngine->setSynthEngine(m_impl->synthEngine.get());

        // Initialize MIDI engine
        if (!m_impl->midiEngine->initialize(m_impl->coreAudioEngine.get())) {
            NSLog(@"AudioManager: Failed to initialize MIDI engine");
            return false;
        }

        // SID player will be initialized lazily on first use
        NSLog(@"AudioManager: SID player will initialize on first use (lazy initialization)");

        // DEFER SID audio node setup - will be created when first SID is loaded
        // This avoids hanging during initialization
        NSLog(@"AudioManager: SID audio node will be created on first SID load (deferred setup)");

        // Pre-allocate SID interleaved buffer (typical audio callback size is 512-4096 frames)
        // This avoids memory allocation in the real-time audio thread
        m_impl->sidInterleavedBuffer.resize(8192 * 2); // 8192 frames * 2 channels (stereo)
        NSLog(@"AudioManager: Pre-allocated SID interleaved buffer (%zu samples)", m_impl->sidInterleavedBuffer.size());

        // Initialize Voice Controller audio node (deferred to avoid blocking)
        // Will be created on first voice command if needed
        NSLog(@"AudioManager: Voice Controller will be initialized on first use (deferred setup)");

        // Initialize VoiceScriptPlayer
        if (m_impl->voiceScriptPlayer) {
            if (!m_impl->voiceScriptPlayer->initialize()) {
                NSLog(@"AudioManager: Warning - VoiceScriptPlayer failed to initialize");
            } else {
                NSLog(@"AudioManager: VoiceScriptPlayer initialized successfully");
            }
        }

        m_impl->initialized = true;
        NSLog(@"AudioManager: Initialized successfully");
        return true;
    } // @autoreleasepool
}

void AudioManager::shutdown() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    if (!m_impl->initialized) {
        return;
    }

    stopAll();

    if (m_impl->audioEngine) {
        // Detach SID source node if present
        if (@available(macOS 10.15, *)) {
            if (m_impl->sidSourceNode) {
                [m_impl->audioEngine detachNode:m_impl->sidSourceNode];
                m_impl->sidSourceNode = nil;
            }
            if (m_impl->voiceSourceNode) {
                [m_impl->audioEngine detachNode:m_impl->voiceSourceNode];
                m_impl->voiceSourceNode = nil;
            }
        }
        [m_impl->audioEngine stop];
    }

    m_impl->synthEngine->shutdown();
    m_impl->midiEngine->shutdown();
    m_impl->coreAudioEngine->shutdown();

    // Shutdown SID player
    if (m_impl->sidPlayer) {
        m_impl->sidPlayer->shutdown();
    }

    m_impl->initialized = false;
}

bool AudioManager::isInitialized() const {
    return m_impl->initialized;
}

// =============================================================================
// Sound Effects
// =============================================================================

void AudioManager::playSoundEffect(SoundEffect effect, float volume) {
    if (!m_impl->initialized) {
        return;
    }

    @autoreleasepool {
        // Generate sound effect using SynthEngine
        std::unique_ptr<SynthAudioBuffer> buffer;

        switch (effect) {
            case SoundEffect::Beep:
                buffer = m_impl->synthEngine->generateBeep();
                break;
            case SoundEffect::Bang:
                buffer = m_impl->synthEngine->generateBang();
                break;
            case SoundEffect::Explode:
                buffer = m_impl->synthEngine->generateExplode();
                break;
            case SoundEffect::BigExplosion:
                buffer = m_impl->synthEngine->generateBigExplosion();
                break;
            case SoundEffect::SmallExplosion:
                buffer = m_impl->synthEngine->generateSmallExplosion();
                break;
            case SoundEffect::DistantExplosion:
                buffer = m_impl->synthEngine->generateDistantExplosion();
                break;
            case SoundEffect::MetalExplosion:
                buffer = m_impl->synthEngine->generateMetalExplosion();
                break;
            case SoundEffect::Zap:
                buffer = m_impl->synthEngine->generateZap();
                break;
            case SoundEffect::Coin:
                buffer = m_impl->synthEngine->generateCoin();
                break;
            case SoundEffect::Jump:
                buffer = m_impl->synthEngine->generateJump();
                break;
            case SoundEffect::PowerUp:
                buffer = m_impl->synthEngine->generatePowerUp();
                break;
            case SoundEffect::Hurt:
                buffer = m_impl->synthEngine->generateHurt();
                break;
            case SoundEffect::Shoot:
                buffer = m_impl->synthEngine->generateShoot();
                break;
            case SoundEffect::Click:
                buffer = m_impl->synthEngine->generateClick();
                break;
            case SoundEffect::SweepUp:
                buffer = m_impl->synthEngine->generateSweepUp();
                break;
            case SoundEffect::SweepDown:
                buffer = m_impl->synthEngine->generateSweepDown();
                break;
            case SoundEffect::RandomBeep:
                buffer = m_impl->synthEngine->generateRandomBeep();
                break;
            case SoundEffect::Pickup:
                buffer = m_impl->synthEngine->generatePickup();
                break;
            case SoundEffect::Blip:
                buffer = m_impl->synthEngine->generateBlip();
                break;
            default:
                return;
        }

        if (!buffer || buffer->samples.empty()) {
            return;
        }

        // Convert to AVAudioPCMBuffer
        AVAudioFrameCount frameCount = (AVAudioFrameCount)buffer->getFrameCount();
        AVAudioPCMBuffer* pcmBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:m_impl->audioFormat
                                                                    frameCapacity:frameCount];
        pcmBuffer.frameLength = frameCount;

        float* leftChannel = pcmBuffer.floatChannelData[0];
        float* rightChannel = pcmBuffer.floatChannelData[1];

        // Apply volume and copy samples
        float finalVolume = volume * m_impl->soundVolume;
        size_t sampleIndex = 0;
        for (AVAudioFrameCount i = 0; i < frameCount; i++) {
            if (buffer->channels == 1) {
                // Mono to stereo
                float sample = buffer->samples[sampleIndex++] * finalVolume;
                leftChannel[i] = sample;
                rightChannel[i] = sample;
            } else {
                // Stereo
                leftChannel[i] = buffer->samples[sampleIndex++] * finalVolume;
                rightChannel[i] = buffer->samples[sampleIndex++] * finalVolume;
            }
        }

        // Schedule buffer for playback
        if (![m_impl->playerNode isPlaying]) {
            [m_impl->playerNode play];
        }

        [m_impl->playerNode scheduleBuffer:pcmBuffer
                                    atTime:nil
                                   options:AVAudioPlayerNodeBufferInterrupts
                         completionHandler:nil];
    }
}

void AudioManager::playNote(int midiNote, float duration, float volume) {
    if (!m_impl->initialized) {
        return;
    }

    float frequency = noteToFrequency(midiNote);
    playFrequency(frequency, duration, volume);
}

void AudioManager::playFrequency(float frequency, float duration, float volume) {
    if (!m_impl->initialized || duration <= 0.0f) {
        return;
    }

    @autoreleasepool {
        // Generate a simple sine wave tone
        ::SynthSoundEffect effect;
        effect.duration = duration;
        effect.synthesisType = ::SynthesisType::SUBTRACTIVE;

        // Add sine wave oscillator
        ::Oscillator osc;
        osc.waveform = ::WaveformType::SINE;
        osc.frequency = frequency;
        osc.amplitude = 1.0f;
        effect.oscillators.push_back(osc);

        // Simple envelope
        effect.envelope.attackTime = 0.01f;
        effect.envelope.decayTime = 0.05f;
        effect.envelope.sustainLevel = 0.7f;
        effect.envelope.releaseTime = 0.1f;

        auto buffer = m_impl->synthEngine->generateSound(effect);

        if (!buffer || buffer->samples.empty()) {
            return;
        }

        // Convert to AVAudioPCMBuffer
        AVAudioFrameCount frameCount = (AVAudioFrameCount)buffer->getFrameCount();
        AVAudioPCMBuffer* pcmBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:m_impl->audioFormat
                                                                    frameCapacity:frameCount];
        pcmBuffer.frameLength = frameCount;

        float* leftChannel = pcmBuffer.floatChannelData[0];
        float* rightChannel = pcmBuffer.floatChannelData[1];

        // Apply volume
        float finalVolume = volume * m_impl->soundVolume;
        size_t sampleIndex = 0;
        for (AVAudioFrameCount i = 0; i < frameCount; i++) {
            if (buffer->channels == 1) {
                float sample = buffer->samples[sampleIndex++] * finalVolume;
                leftChannel[i] = sample;
                rightChannel[i] = sample;
            } else {
                leftChannel[i] = buffer->samples[sampleIndex++] * finalVolume;
                rightChannel[i] = buffer->samples[sampleIndex++] * finalVolume;
            }
        }

        // Play
        if (![m_impl->playerNode isPlaying]) {
            [m_impl->playerNode play];
        }

        [m_impl->playerNode scheduleBuffer:pcmBuffer
                                    atTime:nil
                                   options:AVAudioPlayerNodeBufferInterrupts
                         completionHandler:nil];
    }
}

void AudioManager::setSoundVolume(float volume) {
    m_impl->soundVolume = std::max(0.0f, std::min(1.0f, volume));
}

float AudioManager::getSoundVolume() const {
    return m_impl->soundVolume;
}

// =============================================================================
// Sound Bank API (ID-based Sound Management)
// =============================================================================

uint32_t AudioManager::soundCreateBeep(float frequency, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    // Generate sound using SynthEngine
    auto buffer = m_impl->synthEngine->generateBeep(frequency, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate beep sound");
        return 0;
    }

    // Register in sound bank
    uint32_t soundId = m_impl->soundBank->registerSound(std::move(buffer));

    return soundId;
}

// =============================================================================
// Predefined Sound Effects - Phase 2
// =============================================================================

uint32_t AudioManager::soundCreateZap(float frequency, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generateZap(frequency, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate zap sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateExplode(float size, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generateExplode(size, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate explode sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateCoin(float pitch, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generateCoin(pitch, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate coin sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateJump(float power, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generateJump(power, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate jump sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateShoot(float power, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generateShoot(power, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate shoot sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateClick(float sharpness, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generateClick(sharpness, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate click sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateBlip(float pitch, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generateBlip(pitch, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate blip sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreatePickup(float brightness, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generatePickup(brightness, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate pickup sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreatePowerup(float intensity, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generatePowerUp(intensity, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate powerup sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateHurt(float severity, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generateHurt(severity, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate hurt sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateSweepUp(float startFreq, float endFreq, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generateSweepUp(startFreq, endFreq, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate sweep up sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateSweepDown(float startFreq, float endFreq, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generateSweepDown(startFreq, endFreq, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate sweep down sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateBigExplosion(float size, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generateBigExplosion(size, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate big explosion sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateSmallExplosion(float intensity, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generateSmallExplosion(intensity, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate small explosion sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateDistantExplosion(float distance, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generateDistantExplosion(distance, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate distant explosion sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateMetalExplosion(float shrapnel, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generateMetalExplosion(shrapnel, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate metal explosion sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateBang(float intensity, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generateBang(intensity, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate bang sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateRandomBeep(uint32_t seed, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    auto buffer = m_impl->synthEngine->generateRandomBeep(seed, duration);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate random beep sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

// =============================================================================
// Phase 3: Custom Synthesis
// =============================================================================

uint32_t AudioManager::soundCreateTone(float frequency, float duration, int waveform) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    // Map waveform integer to WaveformType enum
    WaveformType waveType;
    switch (waveform) {
        case 0: waveType = WaveformType::SINE; break;
        case 1: waveType = WaveformType::SQUARE; break;
        case 2: waveType = WaveformType::SAWTOOTH; break;
        case 3: waveType = WaveformType::TRIANGLE; break;
        case 4: waveType = WaveformType::NOISE; break;
        case 5: waveType = WaveformType::PULSE; break;
        default: waveType = WaveformType::SINE; break;
    }

    // Create oscillator configuration
    Oscillator osc;
    osc.waveform = waveType;
    osc.frequency = frequency;
    osc.amplitude = 0.5f;
    osc.phase = 0.0f;
    osc.pulseWidth = 0.5f;

    // Create simple envelope
    EnvelopeADSR envelope;
    envelope.attackTime = 0.01f;
    envelope.decayTime = 0.05f;
    envelope.sustainLevel = 0.8f;
    envelope.releaseTime = 0.1f;

    // Generate the tone
    auto buffer = m_impl->synthEngine->synthesizeOscillator(osc, duration, &envelope);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate tone");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateNote(int note, float duration, int waveform,
                                        float attack, float decay, float sustainLevel, float release) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    // Convert MIDI note to frequency
    float frequency = noteToFrequency(note);

    // Map waveform integer to WaveformType enum
    WaveformType waveType;
    switch (waveform) {
        case 0: waveType = WaveformType::SINE; break;
        case 1: waveType = WaveformType::SQUARE; break;
        case 2: waveType = WaveformType::SAWTOOTH; break;
        case 3: waveType = WaveformType::TRIANGLE; break;
        case 4: waveType = WaveformType::NOISE; break;
        case 5: waveType = WaveformType::PULSE; break;
        default: waveType = WaveformType::SINE; break;
    }

    // Create oscillator configuration
    Oscillator osc;
    osc.waveform = waveType;
    osc.frequency = frequency;
    osc.amplitude = 0.5f;
    osc.phase = 0.0f;
    osc.pulseWidth = 0.5f;

    // Create ADSR envelope with user-specified parameters
    EnvelopeADSR envelope;
    envelope.attackTime = std::max(0.0f, attack);
    envelope.decayTime = std::max(0.0f, decay);
    envelope.sustainLevel = std::clamp(sustainLevel, 0.0f, 1.0f);
    envelope.releaseTime = std::max(0.0f, release);

    // Generate the note
    auto buffer = m_impl->synthEngine->synthesizeOscillator(osc, duration, &envelope);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate note");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateNoise(int noiseType, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    // Create noise oscillator
    Oscillator osc;
    osc.waveform = WaveformType::NOISE;
    osc.frequency = 0.0f; // Not used for noise
    osc.amplitude = 0.4f; // Slightly quieter for noise
    osc.phase = 0.0f;

    // Simple envelope for noise
    EnvelopeADSR envelope;
    envelope.attackTime = 0.01f;
    envelope.decayTime = 0.05f;
    envelope.sustainLevel = 0.8f;
    envelope.releaseTime = 0.1f;

    // Generate noise
    auto buffer = m_impl->synthEngine->synthesizeOscillator(osc, duration, &envelope);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate noise");
        return 0;
    }

    // Apply noise coloring based on type
    // 0 = WHITE (no filtering needed)
    // 1 = PINK (apply 1/f filter)
    // 2 = BROWN/RED (apply 1/f^2 filter)
    if (noiseType == 1 || noiseType == 2) {
        // Simple filtering for pink/brown noise
        float prev1 = 0.0f;
        float prev2 = 0.0f;
        float filterCoef = (noiseType == 1) ? 0.3f : 0.6f;

        for (size_t i = 0; i < buffer->samples.size(); i++) {
            float input = buffer->samples[i];
            float output = input + filterCoef * prev1;
            if (noiseType == 2) {
                output += filterCoef * prev2;
                prev2 = prev1;
            }
            prev1 = output;
            buffer->samples[i] = output * 0.5f; // Normalize
        }
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

void AudioManager::soundPlay(uint32_t soundId, float volume, float pan) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot play sound - not initialized");
        return;
    }

    if (soundId == 0) {
        NSLog(@"AudioManager: Invalid sound ID");
        return;
    }

    // Get sound from bank
    const SynthAudioBuffer* buffer = m_impl->soundBank->getSound(soundId);
    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Sound ID %u not found in bank", soundId);
        return;
    }

    @autoreleasepool {
        // Convert to AVAudioPCMBuffer
        AVAudioFrameCount frameCount = (AVAudioFrameCount)buffer->getFrameCount();
        AVAudioPCMBuffer* pcmBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:m_impl->audioFormat
                                                                    frameCapacity:frameCount];
        pcmBuffer.frameLength = frameCount;

        float* leftChannel = pcmBuffer.floatChannelData[0];
        float* rightChannel = pcmBuffer.floatChannelData[1];

        // Apply volume and pan
        float finalVolume = volume * m_impl->soundVolume;

        // Pan: -1.0 = left, 0.0 = center, 1.0 = right
        float leftGain = 1.0f;
        float rightGain = 1.0f;
        if (pan < 0.0f) {
            // Pan left
            rightGain = 1.0f + pan; // pan is negative, so this reduces right gain
        } else if (pan > 0.0f) {
            // Pan right
            leftGain = 1.0f - pan;
        }

        leftGain *= finalVolume;
        rightGain *= finalVolume;

        // Copy samples with volume and pan applied
        size_t sampleIndex = 0;
        for (AVAudioFrameCount i = 0; i < frameCount; i++) {
            if (buffer->channels == 1) {
                // Mono to stereo with pan
                float sample = buffer->samples[sampleIndex++];
                leftChannel[i] = sample * leftGain;
                rightChannel[i] = sample * rightGain;
            } else {
                // Stereo with pan
                leftChannel[i] = buffer->samples[sampleIndex++] * leftGain;
                rightChannel[i] = buffer->samples[sampleIndex++] * rightGain;
            }
        }

        // Schedule buffer for playback
        if (![m_impl->playerNode isPlaying]) {
            [m_impl->playerNode play];
        }

        [m_impl->playerNode scheduleBuffer:pcmBuffer
                                    atTime:nil
                                   options:0
                         completionHandler:nil];
    }
}

bool AudioManager::soundFree(uint32_t soundId) {
    if (!m_impl->soundBank) {
        return false;
    }

    return m_impl->soundBank->freeSound(soundId);
}

void AudioManager::soundFreeAll() {
    if (m_impl->soundBank) {
        m_impl->soundBank->freeAll();
    }
}

bool AudioManager::soundExists(uint32_t soundId) const {
    if (!m_impl->soundBank) {
        return false;
    }

    return m_impl->soundBank->hasSound(soundId);
}

size_t AudioManager::soundGetCount() const {
    if (!m_impl->soundBank) {
        return 0;
    }

    return m_impl->soundBank->getSoundCount();
}

size_t AudioManager::soundGetMemoryUsage() const {
    if (!m_impl->soundBank) {
        return 0;
    }

    return m_impl->soundBank->getMemoryUsage();
}

// =============================================================================
// Music Playback (ABC Notation)
// =============================================================================

void AudioManager::playMusic(const std::string& abcNotation, float volume) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot play music - not initialized");
        return;
    }

    std::lock_guard<std::mutex> lock(m_impl->musicMutex);

    // Stop any currently playing music
    if (m_impl->currentMusicSequenceId >= 0) {
        m_impl->midiEngine->stopSequence(m_impl->currentMusicSequenceId);
        m_impl->midiEngine->deleteSequence(m_impl->currentMusicSequenceId);
        m_impl->currentMusicSequenceId = -1;
    }

    // Parse ABC notation
    ABCParseResult result = m_impl->abcParser->parse(abcNotation);
    if (!result.success) {
        NSLog(@"AudioManager: Failed to parse ABC notation: %s", result.errorMessage.c_str());
        return;
    }

    m_impl->currentTune = result.tune;

    // Create MIDI sequence from ABC tune
    int sequenceId = m_impl->midiEngine->createSequence(result.tune->title, result.tune->tempo);
    if (sequenceId < 0) {
        NSLog(@"AudioManager: Failed to create MIDI sequence");
        return;
    }

    MidiSequence* sequence = m_impl->midiEngine->getSequence(sequenceId);
    if (!sequence) {
        NSLog(@"AudioManager: Failed to get MIDI sequence");
        return;
    }

    // Convert ABC voices to MIDI tracks
    for (const auto& abcVoice : result.tune->voices) {
        int trackIdx = sequence->addTrack(abcVoice.name, abcVoice.midiChannel);
        MidiTrack* track = sequence->getTrack(trackIdx);
        if (!track) continue;

        // Set instrument
        track->addProgramChange(abcVoice.midiInstrument, 0.0);

        // Add notes
        for (const auto& abcNote : abcVoice.notes) {
            if (!abcNote.isRest) {
                int midiNote = abcNote.toMIDINote();
                int velocity = 80; // Default velocity
                double startTime = abcNote.startTime;
                double duration = abcNote.duration;

                track->addNote(midiNote, velocity, startTime, duration);
            }
        }

        // Transpose if needed
        if (abcVoice.transpose != 0) {
            track->transposeTrack(abcVoice.transpose);
        }
    }

    // Play the sequence
    float finalVolume = volume * m_impl->musicVolume;
    if (m_impl->midiEngine->playSequence(sequenceId, finalVolume, false)) {
        m_impl->currentMusicSequenceId = sequenceId;
        m_impl->musicState = MusicState::Playing;
        NSLog(@"AudioManager: Playing music: %s (tempo: %d BPM, duration: %.2fs)",
              result.tune->title.c_str(), result.tune->tempo, result.tune->totalDuration);
    } else {
        NSLog(@"AudioManager: Failed to start music playback");
        m_impl->midiEngine->deleteSequence(sequenceId);
    }
}

void AudioManager::stopMusic() {
    std::lock_guard<std::mutex> lock(m_impl->musicMutex);

    if (m_impl->currentMusicSequenceId >= 0) {
        m_impl->midiEngine->stopSequence(m_impl->currentMusicSequenceId);
        m_impl->midiEngine->deleteSequence(m_impl->currentMusicSequenceId);
        m_impl->currentMusicSequenceId = -1;
    }

    m_impl->musicState = MusicState::Stopped;
    m_impl->currentTune.reset();
}

void AudioManager::pauseMusic() {
    std::lock_guard<std::mutex> lock(m_impl->musicMutex);

    if (m_impl->musicState == MusicState::Playing && m_impl->currentMusicSequenceId >= 0) {
        m_impl->midiEngine->pauseSequence(m_impl->currentMusicSequenceId);
        m_impl->musicState = MusicState::Paused;
    }
}

void AudioManager::resumeMusic() {
    std::lock_guard<std::mutex> lock(m_impl->musicMutex);

    if (m_impl->musicState == MusicState::Paused && m_impl->currentMusicSequenceId >= 0) {
        m_impl->midiEngine->resumeSequence(m_impl->currentMusicSequenceId);
        m_impl->musicState = MusicState::Playing;
    }
}

bool AudioManager::isMusicPlaying() const {
    return m_impl->musicState == MusicState::Playing;
}

MusicState AudioManager::getMusicState() const {
    return m_impl->musicState;
}

void AudioManager::setMusicVolume(float volume) {
    m_impl->musicVolume = std::max(0.0f, std::min(1.0f, volume));

    // Update current playing music volume if any
    std::lock_guard<std::mutex> lock(m_impl->musicMutex);
    if (m_impl->currentMusicSequenceId >= 0) {
        m_impl->midiEngine->setSequenceVolume(m_impl->currentMusicSequenceId, m_impl->musicVolume);
    }
}

float AudioManager::getMusicVolume() const {
    return m_impl->musicVolume;
}

bool AudioManager::abcRenderToWAV(const std::string& abcNotation, const std::string& outputPath,
                                   float durationSeconds, int sampleRate) {
    if (!m_impl->initialized || !m_impl->abcParser || !m_impl->midiEngine) {
        NSLog(@"AudioManager: Cannot render - ABC/MIDI system not initialized");
        return false;
    }

    // Parse ABC notation
    ABCParseResult result = m_impl->abcParser->parse(abcNotation);
    if (!result.success) {
        NSLog(@"AudioManager: Failed to parse ABC notation: %s", result.errorMessage.c_str());
        return false;
    }

    // Calculate duration if not specified
    float duration = durationSeconds;
    if (duration <= 0.0f) {
        duration = static_cast<float>(result.tune->totalDuration);
        if (duration <= 0.0f) {
            duration = 60.0f; // Default to 1 minute if can't determine
        }
        // Add 2 second buffer
        duration += 2.0f;
    }

    // Clamp duration to reasonable limits (max 10 minutes)
    duration = std::min(duration, 600.0f);

    NSLog(@"AudioManager: Rendering ABC music to WAV (%.1f seconds at %d Hz)", duration, sampleRate);
    NSLog(@"  Title: %s", result.tune->title.c_str());
    NSLog(@"  Tempo: %d BPM", result.tune->tempo);
    NSLog(@"  Voices: %zu", result.tune->voices.size());

    // Create MIDI sequence from ABC tune
    int sequenceId = m_impl->midiEngine->createSequence(result.tune->title, result.tune->tempo);
    if (sequenceId < 0) {
        NSLog(@"AudioManager: Failed to create MIDI sequence");
        return false;
    }

    MidiSequence* sequence = m_impl->midiEngine->getSequence(sequenceId);
    if (!sequence) {
        NSLog(@"AudioManager: Failed to get MIDI sequence");
        return false;
    }

    // Convert ABC voices to MIDI tracks
    for (const auto& abcVoice : result.tune->voices) {
        int trackIdx = sequence->addTrack(abcVoice.name, abcVoice.midiChannel);
        MidiTrack* track = sequence->getTrack(trackIdx);
        if (!track) continue;

        // Set instrument
        track->addProgramChange(abcVoice.midiInstrument, 0.0);

        // Add notes
        for (const auto& abcNote : abcVoice.notes) {
            if (!abcNote.isRest) {
                int midiNote = abcNote.toMIDINote();
                int velocity = 80; // Default velocity
                double startTime = abcNote.startTime;
                double noteDuration = abcNote.duration;

                track->addNote(midiNote, velocity, startTime, noteDuration);
            }
        }

        // Transpose if needed
        if (abcVoice.transpose != 0) {
            track->transposeTrack(abcVoice.transpose);
        }
    }

    // Start playback (for offline rendering we need the sequence active)
    if (!m_impl->midiEngine->playSequence(sequenceId, 1.0f, false)) {
        NSLog(@"AudioManager: Failed to start sequence playback");
        m_impl->midiEngine->deleteSequence(sequenceId);
        return false;
    }

    // Calculate total samples needed
    const int channels = 2; // Stereo
    size_t totalFrames = static_cast<size_t>(sampleRate * duration);
    size_t totalSamples = totalFrames * channels;
    std::vector<float> audioBuffer(totalSamples, 0.0f);

    NSLog(@"AudioManager: Generating %zu frames (%.1f seconds)", totalFrames, duration);

    // Render audio offline by requesting samples from CoreAudioEngine
    // Note: This is a simplified approach - in reality we'd need to directly
    // capture the MIDI synthesis output. For now, we'll report not fully implemented
    // but structure is in place for when CoreAudioEngine exposes offline rendering.

    // Stop and clean up sequence
    m_impl->midiEngine->stopSequence(sequenceId);
    m_impl->midiEngine->deleteSequence(sequenceId);

    // For now, fill with silence and write the file structure
    // (This needs CoreAudioEngine/MidiEngine to expose offline rendering API)
    NSLog(@"AudioManager: Note - ABC offline rendering requires CoreAudioEngine capture API");
    NSLog(@"AudioManager: Writing WAV file structure (audio will be silent until capture implemented)");

    // Write WAV file
    FILE* file = fopen(outputPath.c_str(), "wb");
    if (!file) {
        NSLog(@"AudioManager: Failed to create WAV file: %s", outputPath.c_str());
        return false;
    }

    // Write WAV header
    struct WAVHeader {
        char riff[4] = {'R', 'I', 'F', 'F'};
        uint32_t fileSize;
        char wave[4] = {'W', 'A', 'V', 'E'};
        char fmt[4] = {'f', 'm', 't', ' '};
        uint32_t fmtSize = 16;
        uint16_t audioFormat = 1; // PCM
        uint16_t numChannels = 2; // Stereo
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample = 16;
        char data[4] = {'d', 'a', 't', 'a'};
        uint32_t dataSize;
    };

    // Convert float samples to int16
    std::vector<int16_t> int16Buffer(totalSamples);
    for (size_t i = 0; i < totalSamples; i++) {
        float sample = std::max(-1.0f, std::min(1.0f, audioBuffer[i]));
        int16Buffer[i] = static_cast<int16_t>(sample * 32767.0f);
    }

    WAVHeader header;
    header.sampleRate = sampleRate;
    header.blockAlign = channels * (header.bitsPerSample / 8);
    header.byteRate = sampleRate * header.blockAlign;
    header.dataSize = static_cast<uint32_t>(int16Buffer.size() * sizeof(int16_t));
    header.fileSize = 36 + header.dataSize;

    fwrite(&header, sizeof(WAVHeader), 1, file);
    fwrite(int16Buffer.data(), sizeof(int16_t), int16Buffer.size(), file);
    fclose(file);

    NSLog(@"AudioManager: WAV file written: %s", outputPath.c_str());
    return true;
}

// =============================================================================
// Music Bank API
// =============================================================================

float AudioManager::noteToFrequency(int midiNote) {
    // Standard MIDI note to frequency conversion
    // A4 (MIDI note 69) = 440 Hz
    return 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
}

int AudioManager::frequencyToNote(float frequency) {
    // Convert frequency to nearest MIDI note
    if (frequency <= 0.0f) return 0;
    return static_cast<int>(std::round(69.0f + 12.0f * std::log2(frequency / 440.0f)));
}

void AudioManager::stopAll() {
    if (m_impl->playerNode && [m_impl->playerNode isPlaying]) {
        [m_impl->playerNode stop];
    }

    // Clear voices playing flag if it was set
    if (m_impl->voicesPlaying.load()) {
        NSLog(@"VOICES PLAYBACK: Clearing voicesPlaying flag due to stopAll()");
        m_impl->voicesPlaying.store(false);
    }

    stopMusic();
    sidStop();
}

// =============================================================================
// Voice Controller (Persistent Voice Synthesis)
// =============================================================================

// Helper to lazily initialize voice audio node on first use
void AudioManager::Impl::ensureVoiceAudioNodeCreated() {
    if (voiceSourceNode || !audioEngine) {
        return; // Already created or no engine
    }

    if (@available(macOS 10.15, *)) {
        @try {
            NSLog(@"AudioManager: Creating Voice Controller audio source node (lazy init)...");

            AVAudioFormat* voiceFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                                           sampleRate:48000.0
                                                                             channels:2
                                                                          interleaved:NO];

            // Capture this pointer for the render block
            Impl* capturedImpl = this;

            voiceSourceNode = [[AVAudioSourceNode alloc] initWithFormat:voiceFormat
                renderBlock:^OSStatus(BOOL *isSilence, const AudioTimeStamp *timestamp,
                                     AVAudioFrameCount frameCount, AudioBufferList *outputData) {

                if (!capturedImpl || !capturedImpl->voiceController) {
                    *isSilence = YES;
                    return noErr;
                }

                // Get the output buffer (non-interleaved)
                float* leftChannel = (float*)outputData->mBuffers[0].mData;
                float* rightChannel = (float*)outputData->mBuffers[1].mData;

                // Defensive checks: validate buffer pointers
                if (!leftChannel || !rightChannel) {
                    NSLog(@"AudioManager: ERROR - NULL output buffer in voice callback");
                    *isSilence = YES;
                    return noErr;
                }

                // Defensive check: validate frame count
                if (frameCount <= 0 || frameCount > 8192) {
                    NSLog(@"AudioManager: ERROR - Invalid frameCount=%u in voice callback", (unsigned)frameCount);
                    *isSilence = YES;
                    return noErr;
                }

                // Defensive check: validate voice controller
                if (!capturedImpl->voiceController) {
                    NSLog(@"AudioManager: ERROR - Voice controller is NULL");
                    *isSilence = YES;
                    return noErr;
                }

                // Generate audio from voice controller (stereo interleaved)
                std::vector<float> buffer(frameCount * 2);
                capturedImpl->voiceController->generateAudio(buffer.data(), frameCount);

                // De-interleave into left/right channels
                for (AVAudioFrameCount i = 0; i < frameCount; i++) {
                    leftChannel[i] = buffer[i * 2];
                    rightChannel[i] = buffer[i * 2 + 1];
                }

                *isSilence = NO;
                return noErr;
            }];

            [audioEngine attachNode:voiceSourceNode];
            [audioEngine connect:voiceSourceNode
                              to:audioEngine.mainMixerNode
                          format:voiceFormat];

            NSLog(@"AudioManager: Voice Controller audio node ready");
        } @catch (NSException *exception) {
            NSLog(@"AudioManager: Failed to create voice audio node: %@", exception);
            voiceSourceNode = nil;
        }
    }
}

void SuperTerminal::AudioManager::voiceSetWaveform(int voiceNum, int waveform) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    m_impl->ensureVoiceAudioNodeCreated();

    VoiceWaveform wf = VoiceWaveform::Sine;
    switch (waveform) {
        case 0: wf = VoiceWaveform::Silence; break;
        case 1: wf = VoiceWaveform::Sine; break;
        case 2: wf = VoiceWaveform::Square; break;
        case 3: wf = VoiceWaveform::Sawtooth; break;
        case 4: wf = VoiceWaveform::Triangle; break;
        case 5: wf = VoiceWaveform::Noise; break;
        case 6: wf = VoiceWaveform::Pulse; break;
        default:
            NSLog(@"AudioManager: Invalid waveform type: %d", waveform);
            return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_WAVEFORM, waveform);

    m_impl->voiceController->setWaveform(voiceNum, wf);
}

void SuperTerminal::AudioManager::voiceSetFrequency(int voiceNum, float frequencyHz) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }
    m_impl->ensureVoiceAudioNodeCreated();

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_FREQUENCY, frequencyHz);

    m_impl->voiceController->setFrequency(voiceNum, frequencyHz);
}

void SuperTerminal::AudioManager::voiceSetNote(int voiceNum, int midiNote) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }
    m_impl->ensureVoiceAudioNodeCreated();

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_NOTE, midiNote);

    m_impl->voiceController->setNote(voiceNum, midiNote);
}

void SuperTerminal::AudioManager::voiceSetNoteName(int voiceNum, const std::string& noteName) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }
    m_impl->ensureVoiceAudioNodeCreated();
    m_impl->voiceController->setNoteName(voiceNum, noteName);
}

void SuperTerminal::AudioManager::voiceSetEnvelope(int voiceNum, float attackMs, float decayMs, float sustainLevel, float releaseMs) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_ENVELOPE, attackMs, decayMs, sustainLevel, releaseMs);

    m_impl->voiceController->setEnvelope(voiceNum, attackMs, decayMs, sustainLevel, releaseMs);
}

void SuperTerminal::AudioManager::voiceSetGate(int voiceNum, bool gateOn) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    m_impl->ensureVoiceAudioNodeCreated();

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_GATE, gateOn ? 1 : 0);

    m_impl->voiceController->setGate(voiceNum, gateOn);
}

void SuperTerminal::AudioManager::voiceSetVolume(int voiceNum, float volume) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_VOLUME, volume);

    m_impl->voiceController->setVolume(voiceNum, volume);
}

void SuperTerminal::AudioManager::voiceSetPulseWidth(int voiceNum, float pulseWidth) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_PULSE_WIDTH, pulseWidth);

    m_impl->voiceController->setPulseWidth(voiceNum, pulseWidth);
}

void SuperTerminal::AudioManager::voiceSetFilterRouting(int voiceNum, bool enabled) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_FILTER_ROUTING, enabled ? 1 : 0);

    m_impl->voiceController->setFilterRouting(voiceNum, enabled);
}

void SuperTerminal::AudioManager::voiceSetFilterType(int filterType) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    VoiceFilterType ft = VoiceFilterType::LowPass;
    switch (filterType) {
        case 0: ft = VoiceFilterType::None; break;
        case 1: ft = VoiceFilterType::LowPass; break;
        case 2: ft = VoiceFilterType::HighPass; break;
        case 3: ft = VoiceFilterType::BandPass; break;
        default:
            NSLog(@"AudioManager: Invalid filter type: %d", filterType);
            return;
    }

    // Record to timeline if recording (using voice 0 as filter is global)
    recordVoiceCommand(0, VoiceCommandType::SET_FILTER_TYPE, filterType);

    m_impl->voiceController->setFilterType(ft);
}

void SuperTerminal::AudioManager::voiceSetFilterCutoff(float cutoffHz) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording (using voice 0 as filter is global)
    recordVoiceCommand(0, VoiceCommandType::SET_FILTER_CUTOFF, cutoffHz);

    m_impl->voiceController->setFilterCutoff(cutoffHz);
}

void SuperTerminal::AudioManager::voiceSetFilterResonance(float resonance) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording (using voice 0 as filter is global)
    recordVoiceCommand(0, VoiceCommandType::SET_FILTER_RESONANCE, resonance);

    m_impl->voiceController->setFilterResonance(resonance);
}

void SuperTerminal::AudioManager::voiceSetFilterEnabled(bool enabled) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording (using voice 0 as filter is global)
    recordVoiceCommand(0, VoiceCommandType::SET_FILTER_ENABLED, enabled ? 1 : 0);

    m_impl->voiceController->setFilterEnabled(enabled);
}

void SuperTerminal::AudioManager::voiceSetMasterVolume(float volume) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }
    m_impl->voiceController->setMasterVolume(volume);
}

float AudioManager::voiceGetMasterVolume() const {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return 0.0f;
    }
    return m_impl->voiceController->getMasterVolume();
}

void SuperTerminal::AudioManager::voiceResetAll() {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }
    m_impl->voiceController->resetAllVoices();
}

int AudioManager::voiceGetActiveCount() const {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return 0;
    }
    return m_impl->voiceController->getActiveVoiceCount();
}

bool AudioManager::voicesArePlaying() const {
    if (!m_impl->initialized) {
        return false;
    }

    // Check both our flag and if the player node is actually playing
    // If the flag is set but the player isn't playing, clear the flag
    bool flagValue = m_impl->voicesPlaying.load();
    NSLog(@"VOICES_ARE_PLAYING: Checking flag = %d", flagValue);
    if (flagValue) {
        // Double-check the player node is actually playing AND the engine is running
        bool playerIsPlaying = m_impl->playerNode && [m_impl->playerNode isPlaying];
        bool engineIsRunning = m_impl->audioEngine && [m_impl->audioEngine isRunning];

        if (playerIsPlaying && engineIsRunning) {
            return true;
        } else {
            // Player stopped or engine not running but flag still set - clear it
            m_impl->voicesPlaying.store(false);
            return false;
        }
    }

    return false;
}

// =============================================================================
// Voice Controller Extended API
// =============================================================================

void SuperTerminal::AudioManager::voiceSetPan(int voiceNum, float pan) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_PAN, pan);

    m_impl->voiceController->setPan(voiceNum, pan);
}

void SuperTerminal::AudioManager::voiceSetRingMod(int voiceNum, int sourceVoice) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_RING_MOD, sourceVoice);

    m_impl->voiceController->setRingMod(voiceNum, sourceVoice);
}

void SuperTerminal::AudioManager::voiceSetSync(int voiceNum, int sourceVoice) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_SYNC, sourceVoice);

    m_impl->voiceController->setSync(voiceNum, sourceVoice);
}

void SuperTerminal::AudioManager::voiceSetPortamento(int voiceNum, float time) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_PORTAMENTO, time);

    m_impl->voiceController->setPortamento(voiceNum, time);
}

void SuperTerminal::AudioManager::voiceSetDetune(int voiceNum, float cents) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_DETUNE, cents);

    m_impl->voiceController->setDetune(voiceNum, cents);
}

void SuperTerminal::AudioManager::voiceSetDelayEnable(int voiceNum, bool enabled) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_DELAY_ENABLE, enabled ? 1 : 0);

    m_impl->voiceController->setDelayEnabled(voiceNum, enabled);
}

void SuperTerminal::AudioManager::voiceSetDelayTime(int voiceNum, float time) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_DELAY_TIME, time);

    m_impl->voiceController->setDelayTime(voiceNum, time);
}

void SuperTerminal::AudioManager::voiceSetDelayFeedback(int voiceNum, float feedback) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_DELAY_FEEDBACK, feedback);

    m_impl->voiceController->setDelayFeedback(voiceNum, feedback);
}

void SuperTerminal::AudioManager::voiceSetDelayMix(int voiceNum, float mix) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_DELAY_MIX, mix);

    m_impl->voiceController->setDelayMix(voiceNum, mix);
}

void SuperTerminal::AudioManager::lfoSetWaveform(int lfoNum, int waveform) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording (using voice 0 as LFO is global)
    recordVoiceCommand(lfoNum, VoiceCommandType::LFO_SET_WAVEFORM, waveform);

    m_impl->voiceController->setLFOWaveform(lfoNum, static_cast<LFOWaveform>(waveform));
}

void SuperTerminal::AudioManager::lfoSetRate(int lfoNum, float rateHz) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(lfoNum, VoiceCommandType::LFO_SET_RATE, rateHz);

    m_impl->voiceController->setLFORate(lfoNum, rateHz);
}

void SuperTerminal::AudioManager::lfoReset(int lfoNum) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(lfoNum, VoiceCommandType::LFO_RESET, 0);

    m_impl->voiceController->resetLFO(lfoNum);
}

void SuperTerminal::AudioManager::lfoToPitch(int voiceNum, int lfoNum, float depthCents) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::LFO_TO_PITCH, lfoNum, depthCents);

    m_impl->voiceController->setLFOToPitch(voiceNum, lfoNum, depthCents);
}

void SuperTerminal::AudioManager::lfoToVolume(int voiceNum, int lfoNum, float depth) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::LFO_TO_VOLUME, lfoNum, depth);

    m_impl->voiceController->setLFOToVolume(voiceNum, lfoNum, depth);
}

void SuperTerminal::AudioManager::lfoToFilter(int voiceNum, int lfoNum, float depthHz) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::LFO_TO_FILTER, lfoNum, depthHz);

    m_impl->voiceController->setLFOToFilter(voiceNum, lfoNum, depthHz);
}

void SuperTerminal::AudioManager::lfoToPulseWidth(int voiceNum, int lfoNum, float depth) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::LFO_TO_PULSEWIDTH, lfoNum, depth);

    m_impl->voiceController->setLFOToPulseWidth(voiceNum, lfoNum, depth);
}

void SuperTerminal::AudioManager::voiceSetPhysicalModel(int voiceNum, int modelType) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_PHYSICAL_MODEL, modelType);

    m_impl->voiceController->setPhysicalModel(voiceNum, static_cast<PhysicalModelType>(modelType));
}

void SuperTerminal::AudioManager::voiceSetPhysicalDamping(int voiceNum, float damping) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_PHYSICAL_DAMPING, damping);

    m_impl->voiceController->setPhysicalDamping(voiceNum, damping);
}

void SuperTerminal::AudioManager::voiceSetPhysicalBrightness(int voiceNum, float brightness) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_PHYSICAL_BRIGHTNESS, brightness);

    m_impl->voiceController->setPhysicalBrightness(voiceNum, brightness);
}

void SuperTerminal::AudioManager::voiceSetPhysicalExcitation(int voiceNum, float excitation) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_PHYSICAL_EXCITATION, excitation);

    m_impl->voiceController->setPhysicalExcitation(voiceNum, excitation);
}

void SuperTerminal::AudioManager::voiceSetPhysicalResonance(int voiceNum, float resonance) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_PHYSICAL_RESONANCE, resonance);

    m_impl->voiceController->setPhysicalResonance(voiceNum, resonance);
}

void SuperTerminal::AudioManager::voiceSetPhysicalTension(int voiceNum, float tension) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_PHYSICAL_TENSION, tension);

    m_impl->voiceController->setPhysicalTension(voiceNum, tension);
}

void SuperTerminal::AudioManager::voiceSetPhysicalPressure(int voiceNum, float pressure) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::SET_PHYSICAL_PRESSURE, pressure);

    m_impl->voiceController->setPhysicalPressure(voiceNum, pressure);
}

void SuperTerminal::AudioManager::voicePhysicalTrigger(int voiceNum) {
    if (!m_impl->initialized || !m_impl->voiceController) {
        return;
    }

    // Record to timeline if recording
    recordVoiceCommand(voiceNum, VoiceCommandType::PHYSICAL_TRIGGER, 0);

    m_impl->voiceController->triggerPhysical(voiceNum);
}

// =============================================================================
// Voice Script System
// =============================================================================

bool AudioManager::voiceScriptDefine(const std::string& name, const std::string& source, std::string& error) {
    if (!m_impl->initialized || !m_impl->voiceScriptPlayer) {
        error = "AudioManager not initialized";
        return false;
    }
    return m_impl->voiceScriptPlayer->defineScript(name, source, error);
}

bool AudioManager::voiceScriptPlay(const std::string& name, float bpm) {
    if (!m_impl->initialized || !m_impl->voiceScriptPlayer) {
        return false;
    }
    m_impl->ensureVoiceAudioNodeCreated();
    return m_impl->voiceScriptPlayer->play(name, bpm);
}

void SuperTerminal::AudioManager::voiceScriptStop() {
    if (!m_impl->initialized || !m_impl->voiceScriptPlayer) {
        return;
    }
    m_impl->voiceScriptPlayer->stop();
}

bool AudioManager::voiceScriptIsPlaying() const {
    if (!m_impl->initialized || !m_impl->voiceScriptPlayer) {
        return false;
    }
    return m_impl->voiceScriptPlayer->isPlaying();
}

bool AudioManager::voiceScriptRemove(const std::string& name) {
    if (!m_impl->initialized || !m_impl->voiceScriptPlayer) {
        return false;
    }
    return m_impl->voiceScriptPlayer->removeScript(name);
}

void SuperTerminal::AudioManager::voiceScriptClear() {
    if (!m_impl->initialized || !m_impl->voiceScriptPlayer) {
        return;
    }
    m_impl->voiceScriptPlayer->clearAllScripts();
}

bool AudioManager::voiceScriptExists(const std::string& name) const {
    if (!m_impl->initialized || !m_impl->voiceScriptPlayer) {
        return false;
    }
    return m_impl->voiceScriptPlayer->scriptExists(name);
}

std::vector<std::string> AudioManager::voiceScriptList() const {
    if (!m_impl->initialized || !m_impl->voiceScriptPlayer) {
        return {};
    }
    return m_impl->voiceScriptPlayer->getScriptNames();
}

bool AudioManager::voiceScriptLoad(const std::string& path, const std::string& name) {
    if (!m_impl->initialized || !m_impl->voiceScriptPlayer) {
        return false;
    }

    // Read file
    std::ifstream file(path);
    if (!file.is_open()) {
        NSLog(@"AudioManager: Failed to open VoiceScript file: %s", path.c_str());
        return false;
    }

    std::string source((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();

    // Determine script name
    std::string scriptName = name;
    if (scriptName.empty()) {
        // Extract filename without extension
        size_t lastSlash = path.find_last_of("/\\");
        size_t start = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
        size_t lastDot = path.find_last_of('.');
        size_t end = (lastDot == std::string::npos) ? path.length() : lastDot;
        scriptName = path.substr(start, end - start);
    }

    // Compile and store
    std::string error;
    if (!m_impl->voiceScriptPlayer->defineScript(scriptName, source, error)) {
        NSLog(@"AudioManager: Failed to compile VoiceScript '%s': %s",
              scriptName.c_str(), error.c_str());
        return false;
    }

    NSLog(@"AudioManager: Loaded VoiceScript '%s' from %s",
          scriptName.c_str(), path.c_str());
    return true;
}

bool AudioManager::voiceScriptRenderToWAV(const std::string& scriptName,
                                           const std::string& outputPath,
                                           float durationSeconds,
                                           int sampleRate,
                                           float bpm,
                                           bool fastRender) {
    if (!m_impl->initialized || !m_impl->voiceController || !m_impl->voiceScriptPlayer) {
        NSLog(@"AudioManager: Cannot render - VoiceScript system not initialized");
        return false;
    }

    // Get the script
    auto bytecode = m_impl->voiceScriptPlayer->getScript(scriptName);
    if (!bytecode) {
        NSLog(@"AudioManager: Script '%s' not found", scriptName.c_str());
        return false;
    }

    // Calculate duration if not specified
    float duration = durationSeconds;
    if (duration <= 0.0f) {
        // Use estimated beats from bytecode * 60 / bpm + 2 second buffer
        duration = (bytecode->estimatedBeats * 60.0f / bpm) + 2.0f;
    }

    // Clamp duration to reasonable limits (max 5 minutes)
    duration = std::min(duration, 300.0f);

    NSLog(@"AudioManager: Rendering VoiceScript '%s' to WAV (%.1f seconds at %d Hz) [%s mode]",
          scriptName.c_str(), duration, sampleRate, fastRender ? "FAST" : "normal");

    // Create temporary voice controller for offline rendering
    VoiceController tempController(8, static_cast<float>(sampleRate));
    VoiceScriptInterpreter interpreter(&tempController);

    // Start the script
    interpreter.start(bytecode, bpm);

    // Calculate total samples needed
    int totalSamples = static_cast<int>(duration * sampleRate);
    std::vector<float> audioBuffer(totalSamples * 2); // Stereo

    // Render audio offline
    int samplesRendered = 0;

    if (fastRender) {
        // Fast mode: Update interpreter at 1kHz instead of 48kHz (48x faster!)
        const int SAMPLES_PER_UPDATE = 48;  // Update every 1ms at 48kHz
        const float UPDATE_DELTA = SAMPLES_PER_UPDATE / static_cast<float>(sampleRate);
        std::vector<float> batchBuffer(SAMPLES_PER_UPDATE * 2); // Stereo batch buffer

        while (samplesRendered < totalSamples && interpreter.isRunning()) {
            // Update interpreter once per batch
            interpreter.update(UPDATE_DELTA);

            // Generate a batch of samples
            int samplesToGenerate = std::min(SAMPLES_PER_UPDATE, totalSamples - samplesRendered);
            tempController.generateAudio(batchBuffer.data(), samplesToGenerate);

            // Copy batch to output buffer
            memcpy(&audioBuffer[samplesRendered * 2], batchBuffer.data(), samplesToGenerate * 2 * sizeof(float));
            samplesRendered += samplesToGenerate;
        }
    } else {
        // Normal mode: Sample-accurate updates (slower but precise)
        float deltaTime = 1.0f / static_cast<float>(sampleRate);

        while (samplesRendered < totalSamples && interpreter.isRunning()) {
            // Update interpreter
            interpreter.update(deltaTime);

            // Generate one sample
            float tempSample[2];
            tempController.generateAudio(tempSample, 1);

            audioBuffer[samplesRendered * 2 + 0] = tempSample[0];
            audioBuffer[samplesRendered * 2 + 1] = tempSample[1];
            samplesRendered++;
        }
    }

    NSLog(@"AudioManager: Rendered %d samples (%.2f seconds)",
          samplesRendered, samplesRendered / static_cast<float>(sampleRate));

    // Write WAV file
    FILE* file = fopen(outputPath.c_str(), "wb");
    if (!file) {
        NSLog(@"AudioManager: Failed to create WAV file: %s", outputPath.c_str());
        return false;
    }

    // Write WAV header
    struct WAVHeader {
        char riff[4] = {'R', 'I', 'F', 'F'};
        uint32_t fileSize;
        char wave[4] = {'W', 'A', 'V', 'E'};
        char fmt[4] = {'f', 'm', 't', ' '};
        uint32_t fmtSize = 16;
        uint16_t audioFormat = 1; // PCM
        uint16_t numChannels = 2; // Stereo
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample = 16;
        char data[4] = {'d', 'a', 't', 'a'};
        uint32_t dataSize;
    };

    WAVHeader header;
    header.sampleRate = sampleRate;
    header.byteRate = sampleRate * 2 * 2; // stereo * 16-bit
    header.blockAlign = 4; // 2 channels * 2 bytes
    header.dataSize = samplesRendered * 2 * 2; // stereo * 16-bit
    header.fileSize = 36 + header.dataSize;

    fwrite(&header, sizeof(WAVHeader), 1, file);

    // Convert float samples to 16-bit PCM and write
    std::vector<int16_t> pcmData(samplesRendered * 2);
    for (int i = 0; i < samplesRendered * 2; i++) {
        float sample = audioBuffer[i];
        // Clamp and convert to 16-bit
        sample = std::max(-1.0f, std::min(1.0f, sample));
        pcmData[i] = static_cast<int16_t>(sample * 32767.0f);
    }

    fwrite(pcmData.data(), sizeof(int16_t), samplesRendered * 2, file);
    fclose(file);

    NSLog(@"AudioManager: Successfully wrote WAV file: %s", outputPath.c_str());
    return true;
}

uint32_t SuperTerminal::AudioManager::voiceScriptSaveToBank(const std::string& scriptName,
                                               float durationSeconds,
                                               int sampleRate,
                                               float bpm,
                                               bool fastRender) {
    if (!m_impl->initialized || !m_impl->voiceController || !m_impl->voiceScriptPlayer) {
        NSLog(@"AudioManager: Cannot render - VoiceScript system not initialized");
        return 0;
    }

    // Get the script
    auto bytecode = m_impl->voiceScriptPlayer->getScript(scriptName);
    if (!bytecode) {
        NSLog(@"AudioManager: Script '%s' not found", scriptName.c_str());
        return 0;
    }

    // Calculate duration if not specified
    float duration = durationSeconds;
    if (duration <= 0.0f) {
        // Use estimated beats from bytecode * 60 / bpm + 2 second buffer
        duration = (bytecode->estimatedBeats * 60.0f / bpm) + 2.0f;
    }

    // Clamp duration to reasonable limits (max 5 minutes)
    duration = std::min(duration, 300.0f);

    NSLog(@"AudioManager: Rendering VoiceScript '%s' for sound bank (%.1f seconds at %d Hz) [%s mode]",
          scriptName.c_str(), duration, sampleRate, fastRender ? "FAST" : "normal");

    // IMPORTANT: This renders SYNCHRONOUSLY in the calling thread, not in the background.
    // We create a temporary voice controller and interpreter specifically for offline
    // rendering. This blocks until complete and returns the sound ID.
    // This is intentional - the caller needs the ID immediately for storage/playback.

    // Create temporary voice controller for offline rendering
    VoiceController tempController(8, static_cast<float>(sampleRate));
    VoiceScriptInterpreter interpreter(&tempController);

    // Start the script
    interpreter.start(bytecode, bpm);

    // Calculate total samples needed
    int totalSamples = static_cast<int>(duration * sampleRate);
    std::vector<float> audioBuffer(totalSamples * 2); // Stereo

    // Render audio offline
    int samplesRendered = 0;

    if (fastRender) {
        // Fast mode: Update interpreter at 1kHz instead of 48kHz (48x faster!)
        const int SAMPLES_PER_UPDATE = 48;  // Update every 1ms at 48kHz
        const float UPDATE_DELTA = SAMPLES_PER_UPDATE / static_cast<float>(sampleRate);
        std::vector<float> batchBuffer(SAMPLES_PER_UPDATE * 2); // Stereo batch buffer

        while (samplesRendered < totalSamples && interpreter.isRunning()) {
            // Update interpreter once per batch
            interpreter.update(UPDATE_DELTA);

            // Generate a batch of samples
            int samplesToGenerate = std::min(SAMPLES_PER_UPDATE, totalSamples - samplesRendered);
            tempController.generateAudio(batchBuffer.data(), samplesToGenerate);

            // Copy batch to output buffer
            memcpy(&audioBuffer[samplesRendered * 2], batchBuffer.data(), samplesToGenerate * 2 * sizeof(float));
            samplesRendered += samplesToGenerate;
        }
    } else {
        // Normal mode: Sample-accurate updates (slower but precise)
        float deltaTime = 1.0f / static_cast<float>(sampleRate);

        while (samplesRendered < totalSamples && interpreter.isRunning()) {
            // Update interpreter
            interpreter.update(deltaTime);

            // Generate one sample
            float tempSample[2];
            tempController.generateAudio(tempSample, 1);

            audioBuffer[samplesRendered * 2 + 0] = tempSample[0];
            audioBuffer[samplesRendered * 2 + 1] = tempSample[1];
            samplesRendered++;
        }
    }

    NSLog(@"AudioManager: Rendered %d samples (%.2f seconds)",
          samplesRendered, samplesRendered / static_cast<float>(sampleRate));

    // Convert to SynthAudioBuffer
    auto buffer = std::make_unique<SynthAudioBuffer>();
    buffer->sampleRate = sampleRate;
    buffer->channels = 2;
    buffer->samples.resize(samplesRendered * 2);

    // Copy samples
    for (int i = 0; i < samplesRendered * 2; i++) {
        buffer->samples[i] = audioBuffer[i];
    }

    // Register in sound bank
    if (!m_impl->soundBank) {
        NSLog(@"AudioManager: SoundBank not available");
        return 0;
    }

    uint32_t soundId = m_impl->soundBank->registerSound(std::move(buffer));

    if (soundId > 0) {
        NSLog(@"AudioManager: VoiceScript '%s' saved to sound bank with ID %u",
              scriptName.c_str(), soundId);
    } else {
        NSLog(@"AudioManager: Failed to register sound in sound bank");
    }

    return soundId;
}

// =============================================================================
// Phase 4: Advanced Synthesis
// =============================================================================

uint32_t AudioManager::soundCreateFM(float carrierFreq, float modulatorFreq, float modIndex, float duration) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    // Create FM parameters
    FMParams fmParams;
    fmParams.carrierFreq = carrierFreq;
    fmParams.modulatorFreq = modulatorFreq;
    fmParams.modIndex = modIndex;
    fmParams.modulatorRatio = modulatorFreq / carrierFreq;
    fmParams.carrierWave = WaveformType::SINE;
    fmParams.modulatorWave = WaveformType::SINE;
    fmParams.feedback = 0.0f;

    // Simple envelope for FM
    EnvelopeADSR envelope;
    envelope.attackTime = 0.01f;
    envelope.decayTime = 0.1f;
    envelope.sustainLevel = 0.7f;
    envelope.releaseTime = 0.2f;

    // Generate FM sound
    auto buffer = m_impl->synthEngine->synthesizeFM(fmParams, duration, &envelope);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate FM sound");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateFilteredTone(float frequency, float duration, int waveform,
                                                int filterType, float cutoff, float resonance) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    // Map waveform integer to WaveformType enum
    WaveformType waveType;
    switch (waveform) {
        case 0: waveType = WaveformType::SINE; break;
        case 1: waveType = WaveformType::SQUARE; break;
        case 2: waveType = WaveformType::SAWTOOTH; break;
        case 3: waveType = WaveformType::TRIANGLE; break;
        case 4: waveType = WaveformType::NOISE; break;
        case 5: waveType = WaveformType::PULSE; break;
        default: waveType = WaveformType::SINE; break;
    }

    // Create oscillator configuration
    Oscillator osc;
    osc.waveform = waveType;
    osc.frequency = frequency;
    osc.amplitude = 0.5f;
    osc.phase = 0.0f;
    osc.pulseWidth = 0.5f;

    // Create envelope
    EnvelopeADSR envelope;
    envelope.attackTime = 0.01f;
    envelope.decayTime = 0.05f;
    envelope.sustainLevel = 0.8f;
    envelope.releaseTime = 0.1f;

    // Create filter parameters
    FilterParams filter;
    switch (filterType) {
        case 0: filter.type = FilterType::NONE; break;
        case 1: filter.type = FilterType::LOW_PASS; break;
        case 2: filter.type = FilterType::HIGH_PASS; break;
        case 3: filter.type = FilterType::BAND_PASS; break;
        default: filter.type = FilterType::NONE; break;
    }
    filter.cutoffFreq = cutoff;
    filter.resonance = std::clamp(resonance, 0.0f, 1.0f);
    filter.enabled = (filterType != 0);
    filter.mix = 1.0f;

    // Generate tone with filter
    auto buffer = m_impl->synthEngine->synthesizeOscillator(osc, duration, &envelope, &filter);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate filtered tone");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateFilteredNote(int note, float duration, int waveform,
                                                float attack, float decay, float sustainLevel, float release,
                                                int filterType, float cutoff, float resonance) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    // Convert MIDI note to frequency
    float frequency = noteToFrequency(note);

    // Map waveform integer to WaveformType enum
    WaveformType waveType;
    switch (waveform) {
        case 0: waveType = WaveformType::SINE; break;
        case 1: waveType = WaveformType::SQUARE; break;
        case 2: waveType = WaveformType::SAWTOOTH; break;
        case 3: waveType = WaveformType::TRIANGLE; break;
        case 4: waveType = WaveformType::NOISE; break;
        case 5: waveType = WaveformType::PULSE; break;
        default: waveType = WaveformType::SINE; break;
    }

    // Create oscillator configuration
    Oscillator osc;
    osc.waveform = waveType;
    osc.frequency = frequency;
    osc.amplitude = 0.5f;
    osc.phase = 0.0f;
    osc.pulseWidth = 0.5f;

    // Create ADSR envelope with user-specified parameters
    EnvelopeADSR envelope;
    envelope.attackTime = std::max(0.0f, attack);
    envelope.decayTime = std::max(0.0f, decay);
    envelope.sustainLevel = std::clamp(sustainLevel, 0.0f, 1.0f);
    envelope.releaseTime = std::max(0.0f, release);

    // Create filter parameters
    FilterParams filter;
    switch (filterType) {
        case 0: filter.type = FilterType::NONE; break;
        case 1: filter.type = FilterType::LOW_PASS; break;
        case 2: filter.type = FilterType::HIGH_PASS; break;
        case 3: filter.type = FilterType::BAND_PASS; break;
        default: filter.type = FilterType::NONE; break;
    }
    filter.cutoffFreq = cutoff;
    filter.resonance = std::clamp(resonance, 0.0f, 1.0f);
    filter.enabled = (filterType != 0);
    filter.mix = 1.0f;

    // Generate note with filter
    auto buffer = m_impl->synthEngine->synthesizeOscillator(osc, duration, &envelope, &filter);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate filtered note");
        return 0;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

// =============================================================================
// Phase 5: Effects Chain
// =============================================================================

uint32_t AudioManager::soundCreateWithReverb(float frequency, float duration, int waveform,
                                              float roomSize, float damping, float wet) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    // Map waveform integer to WaveformType enum
    WaveformType waveType;
    switch (waveform) {
        case 0: waveType = WaveformType::SINE; break;
        case 1: waveType = WaveformType::SQUARE; break;
        case 2: waveType = WaveformType::SAWTOOTH; break;
        case 3: waveType = WaveformType::TRIANGLE; break;
        case 4: waveType = WaveformType::NOISE; break;
        case 5: waveType = WaveformType::PULSE; break;
        default: waveType = WaveformType::SINE; break;
    }

    // Create oscillator configuration
    Oscillator osc;
    osc.waveform = waveType;
    osc.frequency = frequency;
    osc.amplitude = 0.5f;
    osc.phase = 0.0f;
    osc.pulseWidth = 0.5f;

    // Create envelope
    EnvelopeADSR envelope;
    envelope.attackTime = 0.01f;
    envelope.decayTime = 0.05f;
    envelope.sustainLevel = 0.8f;
    envelope.releaseTime = 0.1f;

    // Generate base sound
    auto buffer = m_impl->synthEngine->synthesizeOscillator(osc, duration, &envelope);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate base sound for reverb");
        return 0;
    }

    // Apply reverb effect
    // Simple reverb simulation using multiple delays
    size_t sampleCount = buffer->samples.size();
    std::vector<float> reverbBuffer(sampleCount, 0.0f);

    // Clamp parameters
    roomSize = std::clamp(roomSize, 0.0f, 1.0f);
    damping = std::clamp(damping, 0.0f, 1.0f);
    wet = std::clamp(wet, 0.0f, 1.0f);

    // Calculate delay times based on room size
    int delay1 = (int)(roomSize * 0.03f * buffer->sampleRate);
    int delay2 = (int)(roomSize * 0.04f * buffer->sampleRate);
    int delay3 = (int)(roomSize * 0.05f * buffer->sampleRate);

    float decay = 0.5f * (1.0f - damping);

    for (size_t i = 0; i < sampleCount; i++) {
        float dry = buffer->samples[i];
        float verb = 0.0f;

        // Add delayed signals
        if (i >= (size_t)delay1) verb += buffer->samples[i - delay1] * decay * 0.7f;
        if (i >= (size_t)delay2) verb += buffer->samples[i - delay2] * decay * 0.5f;
        if (i >= (size_t)delay3) verb += buffer->samples[i - delay3] * decay * 0.3f;

        reverbBuffer[i] = verb;
    }

    // Mix dry and wet signals
    for (size_t i = 0; i < sampleCount; i++) {
        buffer->samples[i] = buffer->samples[i] * (1.0f - wet) + reverbBuffer[i] * wet;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateWithDelay(float frequency, float duration, int waveform,
                                             float delayTime, float feedback, float mix) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    // Map waveform integer to WaveformType enum
    WaveformType waveType;
    switch (waveform) {
        case 0: waveType = WaveformType::SINE; break;
        case 1: waveType = WaveformType::SQUARE; break;
        case 2: waveType = WaveformType::SAWTOOTH; break;
        case 3: waveType = WaveformType::TRIANGLE; break;
        case 4: waveType = WaveformType::NOISE; break;
        case 5: waveType = WaveformType::PULSE; break;
        default: waveType = WaveformType::SINE; break;
    }

    // Create oscillator configuration
    Oscillator osc;
    osc.waveform = waveType;
    osc.frequency = frequency;
    osc.amplitude = 0.5f;
    osc.phase = 0.0f;
    osc.pulseWidth = 0.5f;

    // Create envelope
    EnvelopeADSR envelope;
    envelope.attackTime = 0.01f;
    envelope.decayTime = 0.05f;
    envelope.sustainLevel = 0.8f;
    envelope.releaseTime = 0.1f;

    // Generate base sound
    auto buffer = m_impl->synthEngine->synthesizeOscillator(osc, duration, &envelope);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate base sound for delay");
        return 0;
    }

    // Apply delay effect
    size_t sampleCount = buffer->samples.size();
    std::vector<float> delayBuffer(sampleCount, 0.0f);

    // Clamp parameters
    feedback = std::clamp(feedback, 0.0f, 0.95f); // Prevent runaway feedback
    mix = std::clamp(mix, 0.0f, 1.0f);
    delayTime = std::max(0.01f, delayTime); // Minimum 10ms delay

    // Calculate delay in samples
    int delaySamples = (int)(delayTime * buffer->sampleRate);
    if (delaySamples >= (int)sampleCount) delaySamples = sampleCount - 1;

    // Apply delay with feedback
    for (size_t i = 0; i < sampleCount; i++) {
        delayBuffer[i] = buffer->samples[i];

        if (i >= (size_t)delaySamples) {
            delayBuffer[i] += delayBuffer[i - delaySamples] * feedback;
        }
    }

    // Mix dry and wet signals
    for (size_t i = 0; i < sampleCount; i++) {
        buffer->samples[i] = buffer->samples[i] * (1.0f - mix) + delayBuffer[i] * mix;
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

uint32_t AudioManager::soundCreateWithDistortion(float frequency, float duration, int waveform,
                                                  float drive, float tone, float level) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot create sound - not initialized");
        return 0;
    }

    if (!m_impl->synthEngine) {
        NSLog(@"AudioManager: SynthEngine not available");
        return 0;
    }

    // Map waveform integer to WaveformType enum
    WaveformType waveType;
    switch (waveform) {
        case 0: waveType = WaveformType::SINE; break;
        case 1: waveType = WaveformType::SQUARE; break;
        case 2: waveType = WaveformType::SAWTOOTH; break;
        case 3: waveType = WaveformType::TRIANGLE; break;
        case 4: waveType = WaveformType::NOISE; break;
        case 5: waveType = WaveformType::PULSE; break;
        default: waveType = WaveformType::SINE; break;
    }

    // Create oscillator configuration
    Oscillator osc;
    osc.waveform = waveType;
    osc.frequency = frequency;
    osc.amplitude = 0.5f;
    osc.phase = 0.0f;
    osc.pulseWidth = 0.5f;

    // Create envelope
    EnvelopeADSR envelope;
    envelope.attackTime = 0.01f;
    envelope.decayTime = 0.05f;
    envelope.sustainLevel = 0.8f;
    envelope.releaseTime = 0.1f;

    // Generate base sound
    auto buffer = m_impl->synthEngine->synthesizeOscillator(osc, duration, &envelope);

    if (!buffer || buffer->samples.empty()) {
        NSLog(@"AudioManager: Failed to generate base sound for distortion");
        return 0;
    }

    // Apply distortion effect
    // Clamp parameters
    drive = std::clamp(drive, 0.0f, 10.0f);
    tone = std::clamp(tone, 0.0f, 1.0f);
    level = std::clamp(level, 0.0f, 1.0f);

    // Apply waveshaping distortion
    for (size_t i = 0; i < buffer->samples.size(); i++) {
        float sample = buffer->samples[i];

        // Pre-gain
        sample *= (1.0f + drive);

        // Soft clipping (tanh-like)
        if (sample > 1.0f) {
            sample = 1.0f - (1.0f / (1.0f + sample - 1.0f));
        } else if (sample < -1.0f) {
            sample = -1.0f + (1.0f / (1.0f - sample - 1.0f));
        } else {
            // Waveshaping in normal range
            sample = sample * (1.5f - 0.5f * sample * sample);
        }

        // Tone control (simple high-frequency rolloff)
        if (i > 0) {
            sample = sample * tone + buffer->samples[i-1] * (1.0f - tone) * 0.3f;
        }

        // Output level
        buffer->samples[i] = sample * level * 0.7f; // Scale down to prevent clipping
    }

    return m_impl->soundBank->registerSound(std::move(buffer));
}

// =============================================================================
// Music Bank API (ID-based Music Management)
// =============================================================================

uint32_t AudioManager::musicLoadString(const std::string& abcNotation) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot load music - not initialized");
        return 0;
    }

    if (!m_impl->musicBank) {
        NSLog(@"AudioManager: MusicBank not available");
        return 0;
    }

    return m_impl->musicBank->loadFromString(abcNotation);
}

uint32_t AudioManager::musicLoadFile(const std::string& filename) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot load music file - not initialized");
        return 0;
    }

    if (!m_impl->musicBank) {
        NSLog(@"AudioManager: MusicBank not available");
        return 0;
    }

    return m_impl->musicBank->loadFromFile(filename);
}

void AudioManager::musicPlay(uint32_t musicId, float volume) {
    if (!m_impl->initialized) {
        NSLog(@"AudioManager: Cannot play music - not initialized");
        return;
    }

    if (!m_impl->musicBank) {
        NSLog(@"AudioManager: MusicBank not available");
        return;
    }

    // Get ABC notation from music bank
    std::string abcNotation = m_impl->musicBank->getABCNotation(musicId);
    if (abcNotation.empty()) {
        NSLog(@"AudioManager: Music ID %u not found in bank", musicId);
        return;
    }

    // Play the music using existing playMusic method
    playMusic(abcNotation, volume);
}

bool AudioManager::musicExists(uint32_t musicId) const {
    if (!m_impl->musicBank) {
        return false;
    }

    return m_impl->musicBank->hasMusic(musicId);
}

std::string AudioManager::musicGetTitle(uint32_t musicId) const {
    if (!m_impl->musicBank) {
        return "";
    }

    return m_impl->musicBank->getTitle(musicId);
}

std::string AudioManager::musicGetComposer(uint32_t musicId) const {
    if (!m_impl->musicBank) {
        return "";
    }

    return m_impl->musicBank->getComposer(musicId);
}

std::string AudioManager::musicGetKey(uint32_t musicId) const {
    if (!m_impl->musicBank) {
        return "";
    }

    return m_impl->musicBank->getKey(musicId);
}

float AudioManager::musicGetTempo(uint32_t musicId) const {
    if (!m_impl->musicBank) {
        return 0.0f;
    }

    return m_impl->musicBank->getTempo(musicId);
}

bool AudioManager::musicFree(uint32_t musicId) {
    if (!m_impl->musicBank) {
        return false;
    }

    return m_impl->musicBank->freeMusic(musicId);
}

void AudioManager::musicFreeAll() {
    if (!m_impl->musicBank) {
        return;
    }

    m_impl->musicBank->freeAll();
}

size_t AudioManager::musicGetCount() const {
    if (!m_impl->musicBank) {
        return 0;
    }

    return m_impl->musicBank->getMusicCount();
}

size_t AudioManager::musicGetMemoryUsage() const {
    if (!m_impl->musicBank) {
        return 0;
    }

    return m_impl->musicBank->getMemoryUsage();
}

// =============================================================================
// SID Music (Commodore 64 SID Chip Emulation)
// =============================================================================

uint32_t AudioManager::sidLoadFile(const std::string& path) {
    NSLog(@"AudioManager::sidLoadFile called with path: %s", path.c_str());

    if (!m_impl->sidBank) {
        NSLog(@"AudioManager::sidLoadFile ERROR: sidBank is null");
        return 0;
    }

    NSLog(@"AudioManager::sidLoadFile calling sidBank->loadFromFile...");
    uint32_t result = m_impl->sidBank->loadFromFile(path);
    NSLog(@"AudioManager::sidLoadFile returned ID: %u", result);
    return result;
}

uint32_t AudioManager::sidLoadMemory(const uint8_t* data, size_t size) {
    if (!m_impl->sidBank) {
        return 0;
    }

    return m_impl->sidBank->loadFromMemory(data, size);
}

void AudioManager::sidPlay(uint32_t id, int subtune, float volume) {
    NSLog(@"AudioManager::sidPlay called with id=%u, subtune=%d, volume=%.2f", id, subtune, volume);

    if (!m_impl->sidBank || !m_impl->sidPlayer) {
        NSLog(@"AudioManager::sidPlay ERROR: sidBank or sidPlayer is null");
        return;
    }

    // Lazy initialization on first use
    if (!m_impl->sidPlayer->isInitialized()) {
        // Get the audio engine's actual sample rate first
        AVAudioFormat* engineFormat = [m_impl->audioEngine.outputNode outputFormatForBus:0];
        double engineSampleRate = engineFormat.sampleRate;
        NSLog(@"AudioManager: Lazy-initializing SID player at engine sample rate: %.0f Hz", engineSampleRate);

        if (!m_impl->sidPlayer->initialize((int)engineSampleRate)) {
            NSLog(@"AudioManager: Failed to initialize SID player: %s", m_impl->sidPlayer->getError().c_str());
            return;
        }
        // Use MONO mode (sounds better) and we'll duplicate to stereo in the callback
        m_impl->sidPlayer->setStereo(false);
        NSLog(@"AudioManager: SID player initialized successfully (mono mode)");
    }

    // Create audio node on first play if not already created
    if (!m_impl->sidSourceNode && m_impl->audioEngine) {
        NSLog(@"AudioManager: Creating SID audio source node...");
        if (@available(macOS 10.15, *)) {
            @try {
                // Stop the audio engine before modifying the graph
                BOOL wasRunning = m_impl->audioEngine.isRunning;
                if (wasRunning) {
                    NSLog(@"AudioManager: Stopping audio engine to attach SID node...");
                    [m_impl->audioEngine stop];
                }

                // Use the audio engine's native sample rate to avoid format mismatch
                AVAudioFormat* engineFormat = [m_impl->audioEngine.outputNode outputFormatForBus:0];
                double engineSampleRate = engineFormat.sampleRate;
                NSLog(@"AudioManager: Using engine sample rate: %.0f Hz", engineSampleRate);

                AVAudioFormat* sidFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                                             sampleRate:engineSampleRate
                                                                               channels:2
                                                                            interleaved:NO];

                __weak Impl* weakImpl = m_impl.get();

                m_impl->sidSourceNode = [[AVAudioSourceNode alloc] initWithFormat:sidFormat
                    renderBlock:^OSStatus(BOOL *isSilence, const AudioTimeStamp *timestamp,
                                         AVAudioFrameCount frameCount, AudioBufferList *outputData) {

                    Impl* strongImpl = weakImpl;
                        if (!strongImpl || !strongImpl->sidPlayer || !strongImpl->sidPlayer->isPlaying()) {
                            *isSilence = YES;
                            float* leftBuffer = (float*)outputData->mBuffers[0].mData;
                            float* rightBuffer = (float*)outputData->mBuffers[1].mData;
                            memset(leftBuffer, 0, frameCount * sizeof(float));
                            memset(rightBuffer, 0, frameCount * sizeof(float));
                            return noErr;
                        }

                    @try {
                        // Non-interleaved format: separate buffers for left and right channels
                        float* leftBuffer = (float*)outputData->mBuffers[0].mData;
                        float* rightBuffer = (float*)outputData->mBuffers[1].mData;

                        // Ensure pre-allocated buffer is large enough (avoid allocation in audio thread)
                        // MONO mode, so we only need frameCount samples
                        if (strongImpl->sidInterleavedBuffer.size() < frameCount) {
                            // This should only happen once at startup
                            strongImpl->sidInterleavedBuffer.resize(frameCount * 2); // Allocate extra to avoid future resizes
                        }

                        // Generate MONO samples from SID player
                        size_t generated = strongImpl->sidPlayer->generateSamples(strongImpl->sidInterleavedBuffer.data(), frameCount);

                        // Duplicate mono to both left and right channels
                        for (size_t i = 0; i < generated; i++) {
                            float sample = strongImpl->sidInterleavedBuffer[i];
                            leftBuffer[i] = sample;
                            rightBuffer[i] = sample;
                        }

                        // Fill remainder with silence if needed
                        if (generated < frameCount) {
                            memset(leftBuffer + generated, 0, (frameCount - generated) * sizeof(float));
                            memset(rightBuffer + generated, 0, (frameCount - generated) * sizeof(float));
                        }

                        *isSilence = (generated == 0);
                        return noErr;
                    } @catch (NSException *exception) {
                        NSLog(@"AudioManager: Exception in SID render callback: %@", exception);
                        *isSilence = YES;
                        float* leftBuffer = (float*)outputData->mBuffers[0].mData;
                        float* rightBuffer = (float*)outputData->mBuffers[1].mData;
                        memset(leftBuffer, 0, frameCount * sizeof(float));
                        memset(rightBuffer, 0, frameCount * sizeof(float));
                        return noErr;
                    }
                }];

                NSLog(@"AudioManager: SID audio source node allocated, attaching to engine...");
                [m_impl->audioEngine attachNode:m_impl->sidSourceNode];

                NSLog(@"AudioManager: Connecting SID node to mixer...");
                [m_impl->audioEngine connect:m_impl->sidSourceNode
                                           to:m_impl->audioEngine.mainMixerNode
                                       format:sidFormat];

                // Restart the audio engine if it was running
                if (wasRunning) {
                    NSLog(@"AudioManager: Restarting audio engine...");
                    NSError* error = nil;
                    @try {
                        if (![m_impl->audioEngine startAndReturnError:&error]) {
                            NSLog(@"AudioManager: Failed to restart audio engine: %@", error);
                        } else {
                            NSLog(@"AudioManager: Audio engine restarted successfully");
                        }
                    }
                    @catch (NSException *exception) {
                        NSLog(@"AudioManager: Exception restarting audio engine: %@", exception.reason);
                    }
                    @catch (...) {
                        NSLog(@"AudioManager: Unknown exception restarting audio engine");
                    }
                }

                NSLog(@"AudioManager: SID audio source node created and connected");
            } @catch (NSException *exception) {
                NSLog(@"AudioManager: EXCEPTION creating SID audio node: %@", exception);
                NSLog(@"AudioManager: Exception reason: %@", exception.reason);
                m_impl->sidSourceNode = nil;

                // Try to restart the engine even if we failed
                if (m_impl->audioEngine && !m_impl->audioEngine.isRunning) {
                    NSError* error = nil;
                    @try {
                        [m_impl->audioEngine startAndReturnError:&error];
                    }
                    @catch (NSException *exception) {
                        NSLog(@"AudioManager: Exception starting audio engine: %@", exception.reason);
                    }
                    @catch (...) {
                        NSLog(@"AudioManager: Unknown exception starting audio engine");
                    }
                }
                return;
            }
        } else {
            NSLog(@"AudioManager: AVAudioSourceNode not available (requires macOS 10.15+)");
            return;
        }
    }

    // Get SID data from bank
    const SIDData* sidData = m_impl->sidBank->get(id);
    if (!sidData) {
        NSLog(@"AudioManager: SID ID %u not found", id);
        return;
    }

    // Stop any currently playing SID
    if (m_impl->sidPlayer->isPlaying()) {
        m_impl->sidPlayer->stop();
    }

    // Load the SID into the player
    if (!m_impl->sidPlayer->loadFromMemory(sidData->fileData.data(), sidData->fileData.size())) {
        NSLog(@"AudioManager: Failed to load SID into player: %s", m_impl->sidPlayer->getError().c_str());
        return;
    }

    // Set subtune: 0 means use default, otherwise use specified subtune
    int targetSubtune = subtune;
    if (targetSubtune == 0) {
        targetSubtune = sidData->startSubtune;
    }
    if (targetSubtune > 0) {
        m_impl->sidPlayer->setSubtune(targetSubtune);
        m_impl->sidBank->setCurrentSubtune(id, targetSubtune);
    }

    // Set volume
    m_impl->sidVolume = std::max(0.0f, std::min(1.0f, volume));
    m_impl->sidPlayer->setVolume(m_impl->sidVolume);

    // Store current SID ID
    m_impl->currentSidId = id;

    // Start playback
    m_impl->sidPlayer->play();

    NSLog(@"AudioManager: Playing SID %u (subtune %d): %s", id, targetSubtune, sidData->title.c_str());
}

void AudioManager::sidStop() {
    if (!m_impl->sidPlayer) {
        return;
    }

    m_impl->sidPlayer->stop();
    m_impl->currentSidId = 0;
}

void AudioManager::sidPause() {
    if (!m_impl->sidPlayer) {
        return;
    }

    m_impl->sidPlayer->pause();
}

void AudioManager::sidResume() {
    if (!m_impl->sidPlayer) {
        return;
    }

    m_impl->sidPlayer->resume();
}

bool AudioManager::sidIsPlaying() const {
    if (!m_impl->sidPlayer) {
        return false;
    }

    return m_impl->sidPlayer->isPlaying();
}

void AudioManager::sidSetVolume(float volume) {
    if (!m_impl->sidPlayer) {
        return;
    }

    m_impl->sidVolume = std::max(0.0f, std::min(1.0f, volume));
    m_impl->sidPlayer->setVolume(m_impl->sidVolume);
}

bool AudioManager::sidSetSubtune(uint32_t id, int subtune) {
    if (!m_impl->sidBank) {
        return false;
    }

    // Update the bank's tracking
    if (!m_impl->sidBank->setCurrentSubtune(id, subtune)) {
        return false;
    }

    // If this SID is currently playing, update the player
    if (m_impl->currentSidId == id && m_impl->sidPlayer) {
        return m_impl->sidPlayer->setSubtune(subtune);
    }

    return true;
}

int AudioManager::sidGetSubtune(uint32_t id) const {
    if (!m_impl->sidBank) {
        return 0;
    }

    return m_impl->sidBank->getCurrentSubtune(id);
}

int AudioManager::sidGetSubtuneCount(uint32_t id) const {
    if (!m_impl->sidBank) {
        return 0;
    }

    return m_impl->sidBank->getSubtuneCount(id);
}

int AudioManager::sidGetDefaultSubtune(uint32_t id) const {
    if (!m_impl->sidBank) {
        return 0;
    }

    const SIDData* sidData = m_impl->sidBank->get(id);
    if (!sidData) {
        return 0;
    }

    return sidData->startSubtune;
}

std::string AudioManager::sidGetTitle(uint32_t id) const {
    if (!m_impl->sidBank) {
        return "";
    }

    return m_impl->sidBank->getTitle(id);
}

std::string AudioManager::sidGetAuthor(uint32_t id) const {
    if (!m_impl->sidBank) {
        return "";
    }

    return m_impl->sidBank->getAuthor(id);
}

std::string AudioManager::sidGetCopyright(uint32_t id) const {
    if (!m_impl->sidBank) {
        return "";
    }

    return m_impl->sidBank->getCopyright(id);
}

bool AudioManager::sidExists(uint32_t id) const {
    if (!m_impl->sidBank) {
        return false;
    }

    return m_impl->sidBank->exists(id);
}

bool AudioManager::sidRenderToWAV(uint32_t sidId, const std::string& outputPath,
                                   float durationSeconds, int subtune) {
    if (!m_impl->initialized || !m_impl->sidBank) {
        NSLog(@"AudioManager: Cannot render - SID system not initialized");
        return false;
    }

    // Get SID data from bank
    const SIDData* sidData = m_impl->sidBank->get(sidId);
    if (!sidData) {
        NSLog(@"AudioManager: SID ID %u not found", sidId);
        return false;
    }

    NSLog(@"AudioManager: Rendering SID %u to WAV (%.1f seconds)", sidId, durationSeconds);

    // Create temporary SID player for offline rendering
    SIDPlayer tempPlayer;
    const int sampleRate = 48000;

    if (!tempPlayer.initialize(sampleRate)) {
        NSLog(@"AudioManager: Failed to initialize temp SID player: %s", tempPlayer.getError().c_str());
        return false;
    }

    // Enable stereo mode
    tempPlayer.setStereo(true);

    // Load SID data
    if (!tempPlayer.loadFromMemory(sidData->fileData.data(), sidData->fileData.size())) {
        NSLog(@"AudioManager: Failed to load SID data: %s", tempPlayer.getError().c_str());
        return false;
    }

    // Set subtune if specified
    if (subtune > 0) {
        if (!tempPlayer.setSubtune(subtune)) {
            NSLog(@"AudioManager: Failed to set subtune %d", subtune);
            return false;
        }
    }

    // Start playback
    tempPlayer.play();

    // Calculate total samples needed
    const int channels = 2; // Stereo
    size_t totalFrames = static_cast<size_t>(sampleRate * durationSeconds);
    size_t totalSamples = totalFrames * channels;
    std::vector<int16_t> audioBuffer(totalSamples);

    NSLog(@"AudioManager: Generating %zu frames (%.1f seconds)", totalFrames, durationSeconds);

    // Generate audio in chunks
    const size_t chunkFrames = 4800; // 0.1 seconds at 48kHz
    size_t framesGenerated = 0;

    while (framesGenerated < totalFrames) {
        size_t framesToGenerate = std::min(chunkFrames, totalFrames - framesGenerated);
        size_t offset = framesGenerated * channels;

        size_t generated = tempPlayer.generateSamplesInt16(audioBuffer.data() + offset, framesToGenerate);

        if (generated == 0) {
            NSLog(@"AudioManager: generateSamples returned 0 at frame %zu", framesGenerated);
            break;
        }

        framesGenerated += generated;
    }

    // Trim buffer to actual size
    audioBuffer.resize(framesGenerated * channels);

    NSLog(@"AudioManager: Rendered %zu frames (%.2f seconds)",
          framesGenerated, framesGenerated / static_cast<float>(sampleRate));

    // Write WAV file
    FILE* file = fopen(outputPath.c_str(), "wb");
    if (!file) {
        NSLog(@"AudioManager: Failed to create WAV file: %s", outputPath.c_str());
        return false;
    }

    // Write WAV header
    struct WAVHeader {
        char riff[4] = {'R', 'I', 'F', 'F'};
        uint32_t fileSize;
        char wave[4] = {'W', 'A', 'V', 'E'};
        char fmt[4] = {'f', 'm', 't', ' '};
        uint32_t fmtSize = 16;
        uint16_t audioFormat = 1; // PCM
        uint16_t numChannels = 2; // Stereo
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample = 16;
        char data[4] = {'d', 'a', 't', 'a'};
        uint32_t dataSize;
    };

    WAVHeader header;
    header.sampleRate = sampleRate;
    header.blockAlign = channels * (header.bitsPerSample / 8);
    header.byteRate = sampleRate * header.blockAlign;
    header.dataSize = static_cast<uint32_t>(audioBuffer.size() * sizeof(int16_t));
    header.fileSize = 36 + header.dataSize;

    fwrite(&header, sizeof(WAVHeader), 1, file);
    fwrite(audioBuffer.data(), sizeof(int16_t), audioBuffer.size(), file);
    fclose(file);

    NSLog(@"AudioManager: WAV file written successfully: %s", outputPath.c_str());
    return true;
}

uint32_t AudioManager::sidRenderToBank(uint32_t sidId, float durationSeconds,
                                        int subtune, int sampleRate, bool fastRender) {
    if (!m_impl->initialized || !m_impl->sidBank || !m_impl->soundBank) {
        NSLog(@"AudioManager: Cannot render - SID or sound system not initialized");
        return 0;
    }

    // Get SID data from bank
    const SIDData* sidData = m_impl->sidBank->get(sidId);
    if (!sidData) {
        NSLog(@"AudioManager: SID ID %u not found", sidId);
        return 0;
    }

    NSLog(@"AudioManager: Rendering SID %u to sound bank (%.1f seconds at %d Hz) [%s mode]",
          sidId, durationSeconds, sampleRate, fastRender ? "FAST" : "normal");

    // Create temporary SID player for offline rendering
    SIDPlayer tempPlayer;

    if (!tempPlayer.initialize(sampleRate)) {
        NSLog(@"AudioManager: Failed to initialize temp SID player: %s", tempPlayer.getError().c_str());
        return 0;
    }

    // Enable stereo mode
    tempPlayer.setStereo(true);

    // Load SID data
    if (!tempPlayer.loadFromMemory(sidData->fileData.data(), sidData->fileData.size())) {
        NSLog(@"AudioManager: Failed to load SID data: %s", tempPlayer.getError().c_str());
        return 0;
    }

    // Set subtune if specified
    if (subtune > 0) {
        if (!tempPlayer.setSubtune(subtune)) {
            NSLog(@"AudioManager: Failed to set subtune %d", subtune);
            return 0;
        }
    }

    // Start playback
    tempPlayer.play();

    // Calculate total samples needed
    const int channels = 2; // Stereo
    size_t totalFrames = static_cast<size_t>(sampleRate * durationSeconds);
    std::vector<int16_t> int16Buffer(totalFrames * channels);

    NSLog(@"AudioManager: Generating %zu frames (%.1f seconds)", totalFrames, durationSeconds);

    // Generate audio in chunks
    const size_t chunkFrames = 4800; // 0.1 seconds at 48kHz
    size_t framesGenerated = 0;

    while (framesGenerated < totalFrames) {
        size_t framesToGenerate = std::min(chunkFrames, totalFrames - framesGenerated);
        size_t offset = framesGenerated * channels;

        size_t generated = tempPlayer.generateSamplesInt16(int16Buffer.data() + offset, framesToGenerate);

        if (generated == 0) {
            break;
        }

        framesGenerated += generated;
    }

    NSLog(@"AudioManager: Rendered %zu frames (%.2f seconds)",
          framesGenerated, framesGenerated / static_cast<float>(sampleRate));

    // Convert int16 to float and create SynthAudioBuffer
    auto buffer = std::make_unique<SynthAudioBuffer>();
    buffer->sampleRate = sampleRate;
    buffer->channels = channels;
    buffer->samples.resize(framesGenerated * channels);

    // Convert int16 to float (-1.0 to 1.0)
    for (size_t i = 0; i < framesGenerated * channels; i++) {
        buffer->samples[i] = int16Buffer[i] / 32768.0f;
    }

    // Register in sound bank
    uint32_t soundId = m_impl->soundBank->registerSound(std::move(buffer));

    if (soundId > 0) {
        NSLog(@"AudioManager: SID %u saved to sound bank with ID %u", sidId, soundId);
    } else {
        NSLog(@"AudioManager: Failed to register sound in sound bank");
    }

    return soundId;
}

uint32_t AudioManager::abcRenderToBank(const std::string& abcNotation,
                                        float durationSeconds, int sampleRate, bool fastRender) {
    if (!m_impl->initialized || !m_impl->soundBank) {
        NSLog(@"AudioManager: Cannot render - sound system not initialized");
        return 0;
    }

    // TODO: Implement ABC offline rendering to memory buffer
    // For now, this is a placeholder that returns 0
    // Full implementation would require CoreAudioEngine to support offline rendering

    NSLog(@"AudioManager: abcRenderToBank not yet fully implemented");
    NSLog(@"AudioManager: ABC offline rendering requires CoreAudioEngine capture API");

    return 0;
}

bool AudioManager::sidFree(uint32_t id) {
    if (!m_impl->sidBank) {
        return false;
    }

    // Stop playing if this is the current SID
    if (m_impl->currentSidId == id && m_impl->sidPlayer) {
        m_impl->sidPlayer->stop();
        m_impl->currentSidId = 0;
    }

    return m_impl->sidBank->free(id);
}

void AudioManager::sidFreeAll() {
    if (!m_impl->sidBank) {
        return;
    }

    // Stop any playing SID
    if (m_impl->sidPlayer) {
        m_impl->sidPlayer->stop();
        m_impl->currentSidId = 0;
    }

    m_impl->sidBank->freeAll();
}

size_t AudioManager::sidGetCount() const {
    if (!m_impl->sidBank) {
        return 0;
    }

    return m_impl->sidBank->getCount();
}

size_t AudioManager::sidGetMemoryUsage() const {
    if (!m_impl->sidBank) {
        return 0;
    }

    return m_impl->sidBank->getMemoryUsage();
}

void AudioManager::sidSetQuality(int quality) {
    if (!m_impl->sidPlayer) {
        return;
    }

    // Map int to SIDQuality enum
    SIDQuality q = SIDQuality::Good; // default
    switch (quality) {
        case 0: q = SIDQuality::Fast; break;
        case 1: q = SIDQuality::Medium; break;
        case 2: q = SIDQuality::Good; break;
        case 3: q = SIDQuality::Best; break;
        default: q = SIDQuality::Good; break;
    }

    m_impl->sidPlayer->setQuality(q);
}

void AudioManager::sidSetChipModel(int model) {
    if (!m_impl->sidPlayer) {
        return;
    }

    // Map int to SIDChipModel enum
    SIDChipModel m = SIDChipModel::Auto; // default
    switch (model) {
        case 0: m = SIDChipModel::MOS6581; break;
        case 1: m = SIDChipModel::MOS8580; break;
        case 2: m = SIDChipModel::Auto; break;
        default: m = SIDChipModel::Auto; break;
    }

    m_impl->sidPlayer->setChipModel(m);
}

void AudioManager::sidSetSpeed(float speed) {
    // Note: Speed control is not yet implemented in SIDPlayer
    // This is a placeholder for future implementation
    (void)speed;
}

void AudioManager::sidSetMaxSids(int maxSids) {
    if (m_impl->sidPlayer) {
        m_impl->sidPlayer->setMaxSids(maxSids);
    }
}

int AudioManager::sidGetMaxSids() const {
    if (m_impl->sidPlayer) {
        return m_impl->sidPlayer->getMaxSids();
    }
    return 3; // Default
}

float AudioManager::sidGetTime() const {
    // Note: Playback time tracking is not yet implemented in SIDPlayer
    // This is a placeholder for future implementation
    return 0.0f;
}

} // namespace SuperTerminal

SuperTerminal::VoiceController* SuperTerminal::AudioManager::getVoiceController() const {
    if (!m_impl || !m_impl->voiceController) {
        return nullptr;
    }
    return m_impl->voiceController.get();
}

void SuperTerminal::AudioManager::voiceSetRenderMode(bool enable, const std::string& outputPath) {
    // TODO: Implement WAV rendering mode
    NSLog(@"voiceSetRenderMode not yet implemented");
}

uint32_t SuperTerminal::AudioManager::voiceRenderToSlot(int slotNum, float volume, float duration) {
    // TODO: Implement render to sound slot
    NSLog(@"voiceRenderToSlot not yet implemented");
    return 0;
}

// =============================================================================
// VOICES Timeline System Implementation
// =============================================================================

void SuperTerminal::AudioManager::recordVoiceCommand(int voice, VoiceCommandType type, int intValue) {
    if (!m_impl->voicesRecording) return;

    VoiceCommand cmd;
    cmd.beat = m_impl->voicesBeatCursor;
    cmd.type = type;
    cmd.voice = voice;
    cmd.param.intValue = intValue;
    m_impl->voicesTimeline.push_back(cmd);
}

void SuperTerminal::AudioManager::recordVoiceCommand(int voice, VoiceCommandType type, float floatValue) {
    if (!m_impl->voicesRecording) return;

    VoiceCommand cmd;
    cmd.beat = m_impl->voicesBeatCursor;
    cmd.type = type;
    cmd.voice = voice;
    cmd.param.floatValue = floatValue;
    m_impl->voicesTimeline.push_back(cmd);
}

void SuperTerminal::AudioManager::recordVoiceCommand(int voice, VoiceCommandType type, float a, float b, float c, float d) {
    if (!m_impl->voicesRecording) return;

    VoiceCommand cmd;
    cmd.beat = m_impl->voicesBeatCursor;
    cmd.type = type;
    cmd.voice = voice;
    cmd.param.envelope.a = a;
    cmd.param.envelope.b = b;
    cmd.param.envelope.c = c;
    cmd.param.envelope.d = d;
    m_impl->voicesTimeline.push_back(cmd);
}

void SuperTerminal::AudioManager::recordVoiceCommand(int voice, VoiceCommandType type, int lfo, float amount) {
    if (!m_impl->voicesRecording) return;

    VoiceCommand cmd;
    cmd.beat = m_impl->voicesBeatCursor;
    cmd.type = type;
    cmd.voice = voice;
    cmd.param.lfoRoute.lfo = lfo;
    cmd.param.lfoRoute.amount = amount;
    m_impl->voicesTimeline.push_back(cmd);
}

// Record voice commands at explicit beat positions (for _AT commands)
void SuperTerminal::AudioManager::recordVoiceCommandAtBeat(int voice, float beat, VoiceCommandType type, int intValue) {
    NSLog(@"recordVoiceCommandAtBeat(int): voice=%d, beat=%.2f, type=%d, intValue=%d, recording=%d",
          voice, beat, (int)type, intValue, m_impl->voicesRecording);
    if (!m_impl->voicesRecording) return;

    VoiceCommand cmd;
    cmd.beat = beat;
    cmd.type = type;
    cmd.voice = voice;
    cmd.param.intValue = intValue;
    m_impl->voicesTimeline.push_back(cmd);
    NSLog(@"recordVoiceCommandAtBeat(int): Command recorded, timeline size=%lu", m_impl->voicesTimeline.size());
}

void SuperTerminal::AudioManager::recordVoiceCommandAtBeat(int voice, float beat, VoiceCommandType type, float floatValue) {
    NSLog(@"recordVoiceCommandAtBeat(float): voice=%d, beat=%.2f, type=%d, floatValue=%.2f, recording=%d",
          voice, beat, (int)type, floatValue, m_impl->voicesRecording);
    if (!m_impl->voicesRecording) return;

    VoiceCommand cmd;
    cmd.beat = beat;
    cmd.type = type;
    cmd.voice = voice;
    cmd.param.floatValue = floatValue;
    m_impl->voicesTimeline.push_back(cmd);
    NSLog(@"recordVoiceCommandAtBeat(float): Command recorded, timeline size=%lu", m_impl->voicesTimeline.size());
}

void SuperTerminal::AudioManager::recordVoiceCommandAtBeat(int voice, float beat, VoiceCommandType type, float a, float b, float c, float d) {
    NSLog(@"recordVoiceCommandAtBeat(4-float): voice=%d, beat=%.2f, type=%d, a=%.2f, b=%.2f, c=%.2f, d=%.2f, recording=%d",
          voice, beat, (int)type, a, b, c, d, m_impl->voicesRecording);
    if (!m_impl->voicesRecording) return;

    VoiceCommand cmd;
    cmd.beat = beat;
    cmd.type = type;
    cmd.voice = voice;
    cmd.param.envelope.a = a;
    cmd.param.envelope.b = b;
    cmd.param.envelope.c = c;
    cmd.param.envelope.d = d;
    m_impl->voicesTimeline.push_back(cmd);
    NSLog(@"recordVoiceCommandAtBeat(4-float): Command recorded, timeline size=%lu", m_impl->voicesTimeline.size());
}

void SuperTerminal::AudioManager::recordVoiceCommandAtBeat(int voice, float beat, VoiceCommandType type, int lfo, float amount) {
    if (!m_impl->voicesRecording) return;

    VoiceCommand cmd;
    cmd.beat = beat;
    cmd.type = type;
    cmd.voice = voice;
    cmd.param.lfoRoute.lfo = lfo;
    cmd.param.lfoRoute.amount = amount;
    m_impl->voicesTimeline.push_back(cmd);
}

void SuperTerminal::AudioManager::voicesStartRecording() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->voicesRecording = true;
    m_impl->voicesBeatCursor = 0.0f;
    m_impl->voicesTempo = 120.0f; // Reset to default 120 BPM
    m_impl->voicesTimeline.clear();
    NSLog(@"VOICES: Started recording timeline");
}

void SuperTerminal::AudioManager::voicesAdvanceBeatCursor(float beats) {
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (m_impl->voicesRecording) {
            m_impl->voicesBeatCursor += beats;
            NSLog(@"VOICES: Advanced beat cursor by %.2f to %.2f", beats, m_impl->voicesBeatCursor);
            return; // Exit early, no real-time wait needed
        }
    }

    // When not recording to timeline, perform real-time wait
    // Convert beats to seconds based on current tempo (default 120 BPM)
    float bpm;
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        bpm = m_impl->voicesTempo > 0 ? m_impl->voicesTempo : 120.0f;
    }

    float secondsPerBeat = 60.0f / bpm;
    float waitSeconds = beats * secondsPerBeat;

    NSLog(@"VOICE_WAIT: Real-time wait for %.2f beats (%.3f seconds at %.1f BPM)",
          beats, waitSeconds, bpm);

    std::this_thread::sleep_for(std::chrono::duration<float>(waitSeconds));
}

void SuperTerminal::AudioManager::voicesSetTempo(float bpm) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (m_impl->voicesRecording) {
        // Record tempo change to timeline
        VoiceCommand cmd;
        cmd.beat = m_impl->voicesBeatCursor;
        cmd.type = VoiceCommandType::SET_TEMPO;
        cmd.voice = 0; // Not voice-specific
        cmd.param.floatValue = bpm;
        m_impl->voicesTimeline.push_back(cmd);

        // Also update current tempo for subsequent commands
        m_impl->voicesTempo = bpm;

        NSLog(@"VOICES: Set tempo to %.1f BPM at beat %.2f", bpm, m_impl->voicesBeatCursor);
    }
}

void SuperTerminal::AudioManager::voicesEndAndSaveToSlot(int slot, float volume) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (!m_impl->voicesRecording) {
        NSLog(@"VOICES: Not recording, ignoring END_SLOT");
        return;
    }

    // Render timeline to audio buffer
    std::vector<float> audioBuffer;
    if (!renderVoicesTimeline(audioBuffer)) {
        NSLog(@"VOICES: Failed to render timeline");
        m_impl->voicesRecording = false;
        m_impl->voicesTimeline.clear();
        return;
    }

    // Convert to SynthAudioBuffer
    int frameCount = audioBuffer.size() / 2; // Stereo
    auto buffer = std::make_unique<SynthAudioBuffer>();
    buffer->sampleRate = 48000;
    buffer->channels = 2;
    buffer->samples.resize(audioBuffer.size());

    // Copy samples
    for (size_t i = 0; i < audioBuffer.size(); i++) {
        buffer->samples[i] = audioBuffer[i];
    }

    // Register in sound bank
    if (!m_impl->soundBank) {
        NSLog(@"VOICES: SoundBank not available");
        m_impl->voicesRecording = false;
        m_impl->voicesTimeline.clear();
        return;
    }

    uint32_t soundId = m_impl->soundBank->registerSound(std::move(buffer));

    if (soundId > 0) {
        NSLog(@"VOICES: Rendered %.2f beats and saved to sound bank with ID %u",
              m_impl->voicesBeatCursor, soundId);
    } else {
        NSLog(@"VOICES: Failed to register sound in sound bank");
    }

    m_impl->voicesRecording = false;
    m_impl->voicesTimeline.clear();
}

uint32_t SuperTerminal::AudioManager::voicesEndAndReturnSlot(float volume) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (!m_impl->voicesRecording) {
        NSLog(@"VOICES: Not recording, ignoring voices_next_slot");
        return 0;
    }

    // Render timeline to audio buffer
    std::vector<float> audioBuffer;
    if (!renderVoicesTimeline(audioBuffer)) {
        NSLog(@"VOICES: Failed to render timeline");
        m_impl->voicesRecording = false;
        m_impl->voicesTimeline.clear();
        return 0;
    }

    // Convert to SynthAudioBuffer
    int frameCount = audioBuffer.size() / 2; // Stereo
    auto buffer = std::make_unique<SynthAudioBuffer>();
    buffer->sampleRate = 48000;
    buffer->channels = 2;
    buffer->samples.resize(audioBuffer.size());

    // Copy samples
    for (size_t i = 0; i < audioBuffer.size(); i++) {
        buffer->samples[i] = audioBuffer[i];
    }

    // Register in sound bank
    if (!m_impl->soundBank) {
        NSLog(@"VOICES: SoundBank not available");
        m_impl->voicesRecording = false;
        m_impl->voicesTimeline.clear();
        return 0;
    }

    uint32_t soundId = m_impl->soundBank->registerSound(std::move(buffer));

    if (soundId > 0) {
        NSLog(@"VOICES: Rendered %.2f beats and saved to sound bank with ID %u (via voices_next_slot)",
              m_impl->voicesBeatCursor, soundId);
    } else {
        NSLog(@"VOICES: Failed to register sound in sound bank");
    }

    m_impl->voicesRecording = false;
    m_impl->voicesTimeline.clear();

    return soundId;
}

void SuperTerminal::AudioManager::voicesEndAndPlay() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (!m_impl->voicesRecording) {
        NSLog(@"VOICES: Not recording, ignoring END_PLAY");
        return;
    }

    // Render timeline to audio buffer
    std::vector<float> audioBuffer;
    if (!renderVoicesTimeline(audioBuffer)) {
        NSLog(@"VOICES: Failed to render timeline");
        m_impl->voicesRecording = false;
        m_impl->voicesTimeline.clear();
        return;
    }

    // Create PCM buffer and schedule for playback
    int frameCount = audioBuffer.size() / 2; // Stereo
    float durationSeconds = frameCount / 48000.0f;

    NSLog(@"VOICES PLAYBACK: Creating PCM buffer with %d frames (%.2f seconds @ 44.1kHz)",
          frameCount, durationSeconds);

    AVAudioPCMBuffer* pcmBuffer = [[AVAudioPCMBuffer alloc]
                                   initWithPCMFormat:m_impl->audioFormat
                                   frameCapacity:frameCount];
    pcmBuffer.frameLength = frameCount;

    float* leftChannel = pcmBuffer.floatChannelData[0];
    float* rightChannel = pcmBuffer.floatChannelData[1];

    // Copy and calculate audio statistics
    float maxSample = 0.0f;
    float totalEnergy = 0.0f;
    for (int i = 0; i < frameCount; i++) {
        leftChannel[i] = audioBuffer[i * 2];
        rightChannel[i] = audioBuffer[i * 2 + 1];
        float sample = fabsf(audioBuffer[i * 2]);
        maxSample = fmaxf(maxSample, sample);
        totalEnergy += sample * sample;
    }
    float rms = sqrtf(totalEnergy / frameCount);

    NSLog(@"VOICES PLAYBACK: Audio stats - max: %.4f, RMS: %.4f", maxSample, rms);
    NSLog(@"VOICES PLAYBACK: PCM buffer format - sampleRate: %.0f, channels: %d, frameLength: %u",
          pcmBuffer.format.sampleRate, pcmBuffer.format.channelCount, (unsigned int)pcmBuffer.frameLength);

    // Record start time for actual playback duration measurement
    NSDate* playbackStartTime = [NSDate date];
    NSLog(@"VOICES PLAYBACK: Scheduling buffer at wall-clock time: %@", playbackStartTime);

    // Mark voices as playing
    m_impl->voicesPlaying.store(true);

    // Capture raw pointer to voice controller to reset voices after playback
    VoiceController* voiceController = m_impl->voiceController.get();
    // Capture atomic flag pointer for clearing playing flag
    std::atomic<bool>* playingFlag = &(m_impl->voicesPlaying);

    [m_impl->playerNode scheduleBuffer:pcmBuffer
                                atTime:nil
                               options:AVAudioPlayerNodeBufferInterrupts
                     completionHandler:^{
                         NSDate* playbackEndTime = [NSDate date];
                         NSTimeInterval actualDuration = [playbackEndTime timeIntervalSinceDate:playbackStartTime];
                         NSLog(@"VOICES PLAYBACK: Buffer playback completed");
                         NSLog(@"VOICES PLAYBACK: Expected duration: %.2f seconds", durationSeconds);
                         NSLog(@"VOICES PLAYBACK: Actual wall-clock duration: %.2f seconds", actualDuration);
                         NSLog(@"VOICES PLAYBACK: Duration difference: %.2f seconds (%.1f%%)",
                               actualDuration - durationSeconds,
                               100.0 * (actualDuration - durationSeconds) / durationSeconds);

                         // Gate off all voices after playback completes
                         if (voiceController) {
                             NSLog(@"VOICES PLAYBACK: Resetting all voices (gating off)");
                             voiceController->resetAllVoices();
                         }

                         // Clear playing flag (atomic operation)
                         if (playingFlag) {
                             NSLog(@"VOICES PLAYBACK: About to clear voicesPlaying flag (was %d)", playingFlag->load());
                             playingFlag->store(false);
                             NSLog(@"VOICES PLAYBACK: Cleared voicesPlaying flag (now %d)", playingFlag->load());
                         }
                     }];

    if (![m_impl->playerNode isPlaying]) {
        NSLog(@"VOICES PLAYBACK: Starting player node at %@", [NSDate date]);
        [m_impl->playerNode play];
    } else {
        NSLog(@"VOICES PLAYBACK: Player node already playing");
    }

    NSLog(@"VOICES: Rendered %.2f beats (%.2f seconds) and scheduled for playback",
          m_impl->voicesBeatCursor, durationSeconds);

    m_impl->voicesRecording = false;
    m_impl->voicesTimeline.clear();
}

void SuperTerminal::AudioManager::voicesEndAndSaveToWAV(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (!m_impl->voicesRecording) {
        NSLog(@"VOICES: Not recording, ignoring END_SAVE");
        return;
    }

    // Render timeline to audio buffer
    std::vector<float> audioBuffer;
    if (!renderVoicesTimeline(audioBuffer)) {
        NSLog(@"VOICES: Failed to render timeline");
        m_impl->voicesRecording = false;
        m_impl->voicesTimeline.clear();
        return;
    }

    // Save to WAV file
    float renderSampleRate = m_impl->audioFormat.sampleRate;
    if (saveWAVFile(filename, audioBuffer, renderSampleRate)) {
        NSLog(@"VOICES: Rendered %.2f beats and saved to %s",
              m_impl->voicesBeatCursor, filename.c_str());
    } else {
        NSLog(@"VOICES: Failed to save WAV file: %s", filename.c_str());
    }

    m_impl->voicesRecording = false;
    m_impl->voicesTimeline.clear();
}

bool SuperTerminal::AudioManager::renderVoicesTimeline(std::vector<float>& outBuffer) {
    // Defensive check: validate impl
    if (!m_impl) {
        NSLog(@"VOICES: ERROR - m_impl is NULL");
        return false;
    }

    if (m_impl->voicesTimeline.empty()) {
        NSLog(@"VOICES: Timeline is empty, nothing to render");
        return false;
    }

    // Sort timeline by beat
    std::sort(m_impl->voicesTimeline.begin(), m_impl->voicesTimeline.end(),
              [](const VoiceCommand& a, const VoiceCommand& b) {
                  return a.beat < b.beat;
              });

    int sampleRate = (int)m_impl->audioFormat.sampleRate;

    // Build tempo map: find all tempo changes
    struct TempoChange {
        float beat;
        float bpm;
    };
    std::vector<TempoChange> tempoMap;
    tempoMap.push_back({0.0f, 120.0f}); // Default starting tempo

    for (const auto& cmd : m_impl->voicesTimeline) {
        if (cmd.type == VoiceCommandType::SET_TEMPO) {
            tempoMap.push_back({cmd.beat, cmd.param.floatValue});
        }
    }

    // Calculate total duration accounting for tempo changes
    // For _AT commands, find the maximum beat from all commands
    float maxBeat = m_impl->voicesBeatCursor;
    for (const auto& cmd : m_impl->voicesTimeline) {
        if (cmd.beat > maxBeat) {
            maxBeat = cmd.beat;
        }
    }

    NSLog(@"VOICES DEBUG: Beat cursor: %.2f, Max command beat: %.2f",
          m_impl->voicesBeatCursor, maxBeat);

    float totalSeconds = 0.0f;

    for (size_t i = 0; i < tempoMap.size(); i++) {
        float startBeat = tempoMap[i].beat;
        float endBeat = (i + 1 < tempoMap.size()) ? tempoMap[i + 1].beat : maxBeat;
        float bpm = tempoMap[i].bpm;
        float beatsPerSecond = bpm / 60.0f;
        float beatDuration = endBeat - startBeat;
        float segmentSeconds = beatDuration / beatsPerSecond;
        totalSeconds += segmentSeconds;

        NSLog(@"VOICES TEMPO: Beats %.2f-%.2f at %.1f BPM = %.3f sec",
              startBeat, endBeat, bpm, segmentSeconds);
    }

    int totalSamples = (int)(totalSeconds * sampleRate);

    NSLog(@"VOICES RENDER: Starting render of %.2f beats (%.2f seconds, %d samples @ %d Hz)",
          maxBeat, totalSeconds, totalSamples, sampleRate);
    NSLog(@"VOICES RENDER: Initial tempo: %.0f BPM", tempoMap[0].bpm);

    // Defensive check: validate totalSamples
    if (totalSamples <= 0 || totalSamples > 48000 * 300) { // Max 5 minutes at 48kHz
        NSLog(@"VOICES: ERROR - Invalid totalSamples=%d", totalSamples);
        return false;
    }

    // Allocate output buffer (stereo interleaved)
    outBuffer.resize(totalSamples * 2);

    // Create temporary VoiceController for rendering
    VoiceController tempController(8, sampleRate);

    // Pre-calculate exact sample positions for all commands accounting for tempo changes
    struct TimedCommand {
        int samplePosition;
        size_t commandIndex;
    };

    std::vector<TimedCommand> timedCommands;
    size_t currentTempoIdx = 0;
    float accumulatedSeconds = 0.0f;
    float lastBeat = 0.0f;

    for (size_t i = 0; i < m_impl->voicesTimeline.size(); i++) {
        float beat = m_impl->voicesTimeline[i].beat;

        // Advance through tempo changes up to this beat
        while (currentTempoIdx + 1 < tempoMap.size() &&
               tempoMap[currentTempoIdx + 1].beat <= beat) {
            // Add time for segment we just passed
            float segmentBeats = tempoMap[currentTempoIdx + 1].beat - lastBeat;
            float bpm = tempoMap[currentTempoIdx].bpm;
            accumulatedSeconds += segmentBeats / (bpm / 60.0f);
            lastBeat = tempoMap[currentTempoIdx + 1].beat;
            currentTempoIdx++;
        }

        // Calculate time from last tempo change to this beat
        float remainingBeats = beat - lastBeat;
        float bpm = tempoMap[currentTempoIdx].bpm;
        float commandTime = accumulatedSeconds + (remainingBeats / (bpm / 60.0f));
        int samplePos = (int)(commandTime * sampleRate);

        timedCommands.push_back({samplePos, i});

        // Debug: Log first 20 commands
        if (i < 20) {
            NSLog(@"VOICES DEBUG: Command %zu at beat %.2f -> sample %d (%.3f sec)",
                  i, beat, samplePos, samplePos / (float)sampleRate);
        }
    }
    NSLog(@"VOICES DEBUG: Total commands: %zu, will render %d samples (%.2f sec)",
          timedCommands.size(), totalSamples, totalSamples / (float)sampleRate);

    // Render audio in segments between commands for sample-accurate timing
    int samplesRendered = 0;
    size_t nextCommandIdx = 0;
    int loopIterations = 0;
    const int MAX_LOOP_ITERATIONS = totalSamples + 1000; // Safety limit

    while (samplesRendered < totalSamples) {
        // Defensive check: prevent infinite loops
        if (++loopIterations > MAX_LOOP_ITERATIONS) {
            NSLog(@"VOICES: ERROR - Rendering loop exceeded max iterations, aborting");
            return false;
        }
        // Find next command that should be executed
        int nextCommandSample = totalSamples; // Default to end if no more commands
        if (nextCommandIdx < timedCommands.size()) {
            nextCommandSample = timedCommands[nextCommandIdx].samplePosition;
        }

        // Render audio up to the next command (or to the end)
        int samplesToRender = std::min(nextCommandSample - samplesRendered, totalSamples - samplesRendered);

        if (samplesToRender > 0) {
            // Defensive check: validate buffer bounds
            if ((samplesRendered + samplesToRender) * 2 > (int)outBuffer.size()) {
                NSLog(@"VOICES: ERROR - Buffer overflow prevented (rendered=%d, toRender=%d, bufSize=%zu)",
                      samplesRendered, samplesToRender, outBuffer.size());
                return false;
            }

            // Break large renders into chunks (VoiceController max is 8192 samples)
            const int MAX_CHUNK_SIZE = 8192;
            int samplesRemaining = samplesToRender;
            int chunkOffset = 0;

            while (samplesRemaining > 0) {
                int chunkSize = std::min(samplesRemaining, MAX_CHUNK_SIZE);
                tempController.generateAudio(&outBuffer[(samplesRendered + chunkOffset) * 2], chunkSize);
                chunkOffset += chunkSize;
                samplesRemaining -= chunkSize;
            }

            samplesRendered += samplesToRender;
        }

        // Execute all commands at this exact sample position
        while (nextCommandIdx < timedCommands.size() &&
               timedCommands[nextCommandIdx].samplePosition == samplesRendered) {
            size_t cmdIdx = timedCommands[nextCommandIdx].commandIndex;

            // Defensive check: validate command index
            if (cmdIdx >= m_impl->voicesTimeline.size()) {
                NSLog(@"VOICES: ERROR - Command index %zu out of bounds (size=%zu)",
                      cmdIdx, m_impl->voicesTimeline.size());
                nextCommandIdx++;
                continue;
            }

            const VoiceCommand& cmd = m_impl->voicesTimeline[cmdIdx];

            // Debug: Log what we're executing
            if (nextCommandIdx < 20 || cmd.type == VoiceCommandType::SET_GATE) {
                NSLog(@"VOICES EXEC: At sample %d (%.3f sec) - executing command %zu (type=%d, voice=%d)",
                      samplesRendered, samplesRendered / (float)sampleRate,
                      cmdIdx, (int)cmd.type, cmd.voice);
            }

            executeVoiceCommand(tempController, cmd);
            nextCommandIdx++;
        }
    }

    NSLog(@"VOICES RENDER: Complete - generated %d samples (%.2f seconds @ %d Hz)",
          samplesRendered, samplesRendered / (float)sampleRate, sampleRate);
    NSLog(@"VOICES RENDER: Expected duration: %.2f sec, Actual duration: %.2f sec",
          totalSeconds, samplesRendered / (float)sampleRate);
    return true;
}

void SuperTerminal::AudioManager::executeVoiceCommand(VoiceController& vc, const VoiceCommand& cmd) {
    using CmdType = VoiceCommandType;

    switch (cmd.type) {
        case CmdType::SET_WAVEFORM:
            vc.setWaveform(cmd.voice, static_cast<VoiceWaveform>(cmd.param.intValue));
            break;
        case CmdType::SET_FREQUENCY:
            vc.setFrequency(cmd.voice, cmd.param.floatValue);
            break;
        case CmdType::SET_NOTE:
            vc.setNote(cmd.voice, cmd.param.intValue);
            break;
        case CmdType::SET_VOLUME:
            vc.setVolume(cmd.voice, cmd.param.floatValue);
            break;
        case CmdType::SET_ENVELOPE:
            vc.setEnvelope(cmd.voice, cmd.param.envelope.a, cmd.param.envelope.b,
                          cmd.param.envelope.c, cmd.param.envelope.d);
            break;
        case CmdType::SET_GATE:
            vc.setGate(cmd.voice, cmd.param.intValue != 0);
            break;
        case CmdType::SET_PAN:
            vc.setPan(cmd.voice, cmd.param.floatValue);
            break;
        case CmdType::SET_PHYSICAL_MODEL:
            vc.setPhysicalModel(cmd.voice, static_cast<PhysicalModelType>(cmd.param.intValue));
            break;
        case CmdType::SET_PHYSICAL_DAMPING:
            vc.setPhysicalDamping(cmd.voice, cmd.param.floatValue);
            break;
        case CmdType::SET_PHYSICAL_BRIGHTNESS:
            vc.setPhysicalBrightness(cmd.voice, cmd.param.floatValue);
            break;
        case CmdType::SET_PHYSICAL_EXCITATION:
            vc.setPhysicalExcitation(cmd.voice, cmd.param.floatValue);
            break;
        case CmdType::SET_PHYSICAL_RESONANCE:
            vc.setPhysicalResonance(cmd.voice, cmd.param.floatValue);
            break;
        case CmdType::SET_PULSE_WIDTH:
            vc.setPulseWidth(cmd.voice, cmd.param.floatValue);
            break;
        case CmdType::SET_FILTER_ROUTING:
            vc.setFilterRouting(cmd.voice, cmd.param.intValue != 0);
            break;
        case CmdType::SET_FILTER_TYPE:
            vc.setFilterType(static_cast<VoiceFilterType>(cmd.param.intValue));
            break;
        case CmdType::SET_FILTER_CUTOFF:
            vc.setFilterCutoff(cmd.param.floatValue);
            break;
        case CmdType::SET_FILTER_RESONANCE:
            vc.setFilterResonance(cmd.param.floatValue);
            break;
        case CmdType::SET_FILTER_ENABLED:
            vc.setFilterEnabled(cmd.param.intValue != 0);
            break;
        case CmdType::SET_DELAY_TIME:
            vc.setDelayTime(cmd.voice, cmd.param.floatValue);
            break;
        case CmdType::SET_DELAY_FEEDBACK:
            vc.setDelayFeedback(cmd.voice, cmd.param.floatValue);
            break;
        case CmdType::SET_DELAY_MIX:
            vc.setDelayMix(cmd.voice, cmd.param.floatValue);
            break;
        case CmdType::LFO_SET_WAVEFORM:
            vc.setLFOWaveform(cmd.voice, static_cast<LFOWaveform>(cmd.param.intValue));
            break;
        case CmdType::LFO_SET_RATE:
            vc.setLFORate(cmd.voice, cmd.param.floatValue);
            break;
        case CmdType::LFO_TO_PITCH:
            vc.setLFOToPitch(cmd.voice, cmd.param.lfoRoute.lfo, cmd.param.lfoRoute.amount);
            break;
        case CmdType::LFO_TO_VOLUME:
            vc.setLFOToVolume(cmd.voice, cmd.param.lfoRoute.lfo, cmd.param.lfoRoute.amount);
            break;
        case CmdType::LFO_TO_FILTER:
            vc.setLFOToFilter(cmd.voice, cmd.param.lfoRoute.lfo, cmd.param.lfoRoute.amount);
            break;
        case CmdType::LFO_TO_PULSEWIDTH:
            vc.setLFOToPulseWidth(cmd.voice, cmd.param.lfoRoute.lfo, cmd.param.lfoRoute.amount);
            break;
        case CmdType::SET_RING_MOD:
            vc.setRingMod(cmd.voice, cmd.param.intValue);
            break;
        case CmdType::SET_SYNC:
            vc.setSync(cmd.voice, cmd.param.intValue);
            break;
        case CmdType::SET_PORTAMENTO:
            vc.setPortamento(cmd.voice, cmd.param.floatValue);
            break;
        case CmdType::SET_DETUNE:
            vc.setDetune(cmd.voice, cmd.param.floatValue);
            break;
        case CmdType::SET_DELAY_ENABLE:
            vc.setDelayEnabled(cmd.voice, cmd.param.intValue != 0);
            break;
        case CmdType::SET_PHYSICAL_TENSION:
            vc.setPhysicalTension(cmd.voice, cmd.param.floatValue);
            break;
        case CmdType::SET_PHYSICAL_PRESSURE:
            vc.setPhysicalPressure(cmd.voice, cmd.param.floatValue);
            break;
        case CmdType::LFO_RESET:
            vc.resetLFO(cmd.voice);
            break;
        case CmdType::PHYSICAL_TRIGGER:
            vc.triggerPhysical(cmd.voice);
            break;
        case CmdType::SET_TEMPO:
            // Tempo changes are handled during sample position calculation
            // No action needed during rendering
            NSLog(@"VOICES EXEC: Tempo change to %.1f BPM at beat (already accounted for)", cmd.param.floatValue);
            break;
    }
}

bool SuperTerminal::AudioManager::saveWAVFile(const std::string& filename,
                                              const std::vector<float>& audioData,
                                              float sampleRate) {
    // WAV file header structure
    struct WAVHeader {
        char riff[4] = {'R', 'I', 'F', 'F'};
        uint32_t fileSize;
        char wave[4] = {'W', 'A', 'V', 'E'};
        char fmt[4] = {'f', 'm', 't', ' '};
        uint32_t fmtSize = 16;
        uint16_t audioFormat = 3; // IEEE float
        uint16_t numChannels = 2;
        uint32_t sampleRateVal;
        uint32_t byteRate;
        uint16_t blockAlign = 8; // 2 channels * 4 bytes
        uint16_t bitsPerSample = 32;
        char data[4] = {'d', 'a', 't', 'a'};
        uint32_t dataSize;
    };

    WAVHeader header;
    header.sampleRateVal = (uint32_t)sampleRate;
    header.byteRate = (uint32_t)sampleRate * 2 * 4; // sampleRate * channels * bytesPerSample
    header.dataSize = audioData.size() * sizeof(float);
    header.fileSize = 36 + header.dataSize;

    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) {
        NSLog(@"Failed to open file for writing: %s", filename.c_str());
        return false;
    }

    fwrite(&header, sizeof(WAVHeader), 1, file);
    fwrite(audioData.data(), sizeof(float), audioData.size(), file);
    fclose(file);

    return true;
}
