//
// AudioManager.h
// SuperTerminal v2
//
// Audio management system for sound effects and music playback.
// Wraps SynthEngine and MidiEngine for simple, unified audio API.
//

#ifndef SUPERTERMINAL_AUDIO_MANAGER_H
#define SUPERTERMINAL_AUDIO_MANAGER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

// Forward declarations (SynthEngine is in global namespace)
class SynthEngine;

namespace SuperTerminal {
    class MidiEngine;
    class VoiceController;
}

namespace SuperTerminal {

/// Sound effect types (predefined synthesized sounds)
enum class SoundEffect {
    Beep,
    Bang,
    Explode,
    BigExplosion,
    SmallExplosion,
    DistantExplosion,
    MetalExplosion,
    Zap,
    Coin,
    Jump,
    PowerUp,
    Hurt,
    Shoot,
    Click,
    SweepUp,
    SweepDown,
    RandomBeep,
    Pickup,
    Blip
};

/// Music playback state
enum class MusicState {
    Stopped,
    Playing,
    Paused
};

/// AudioManager: Unified audio system for sound effects and music
///
/// Responsibilities:
/// - Generate and play synthesized sound effects
/// - Parse and play ABC notation music
/// - Control music playback (play/pause/stop)
/// - Manage audio volume and state
///
/// Features:
/// - Built-in sound effect synthesis (no external files needed)
/// - ABC notation music playback with MIDI synthesis
/// - Thread-safe audio operations
/// - Volume control per sound and global
class AudioManager {
public:
    /// Constructor
    AudioManager();
    
    /// Destructor
    ~AudioManager();
    
    // Disable copy, allow move
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
    AudioManager(AudioManager&&) noexcept;
    AudioManager& operator=(AudioManager&&) noexcept;
    
    // =========================================================================
    // Initialization
    // =========================================================================
    
    /// Initialize the audio system
    /// @return true if initialization succeeded
    bool initialize();
    
    /// Shutdown the audio system
    void shutdown();
    
    /// Check if audio system is initialized
    /// @return true if initialized
    bool isInitialized() const;
    
    // =========================================================================
    // Voice Controller Access
    // =========================================================================
    
    /// Get direct access to VoiceController for voice synthesis
    /// @return Pointer to VoiceController (nullptr if not initialized)
    VoiceController* getVoiceController() const;
    
    // =========================================================================
    // Sound Effects (Synthesized)
    // =========================================================================
    
    /// Play a predefined sound effect
    /// @param effect Sound effect type
    /// @param volume Volume (0.0 = silent, 1.0 = full volume)
    void playSoundEffect(SoundEffect effect, float volume = 1.0f);
    
    /// Play a synthesized note
    /// @param midiNote MIDI note number (0-127, middle C = 60)
    /// @param duration Duration in seconds
    /// @param volume Volume (0.0-1.0)
    void playNote(int midiNote, float duration, float volume = 1.0f);
    
    /// Play a frequency directly
    /// @param frequency Frequency in Hz
    /// @param duration Duration in seconds
    /// @param volume Volume (0.0-1.0)
    void playFrequency(float frequency, float duration, float volume = 1.0f);
    
    /// Set global sound effects volume
    /// @param volume Volume (0.0-1.0)
    void setSoundVolume(float volume);
    
    /// Get global sound effects volume
    /// @return Volume (0.0-1.0)
    float getSoundVolume() const;
    
    // =========================================================================
    // Sound Bank API (ID-based Sound Management)
    // =========================================================================
    
    /// Create a beep sound and store in sound bank
    /// @param frequency Frequency in Hz
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateBeep(float frequency, float duration);
    
    // -------------------------------------------------------------------------
    // Predefined Sound Effects - Phase 2
    // -------------------------------------------------------------------------
    
    /// Create a laser zap sound
    /// @param frequency Frequency in Hz
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateZap(float frequency, float duration);
    
    /// Create an explosion sound
    /// @param size Explosion size (0.5-2.0)
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateExplode(float size, float duration);
    
    /// Create a coin pickup sound
    /// @param pitch Pitch multiplier (0.5-2.0)
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateCoin(float pitch, float duration);
    
    /// Create a jump sound
    /// @param power Jump power (0.5-2.0)
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateJump(float power, float duration);
    
    /// Create a shooting sound
    /// @param power Shot power (0.5-2.0)
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateShoot(float power, float duration);
    
    /// Create a UI click sound
    /// @param sharpness Click sharpness (0.5-2.0)
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateClick(float sharpness, float duration);
    
    /// Create a blip sound
    /// @param pitch Pitch multiplier (0.5-2.0)
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateBlip(float pitch, float duration);
    
    /// Create an item pickup sound
    /// @param brightness Brightness (0.5-2.0)
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreatePickup(float brightness, float duration);
    
    /// Create a power-up sound
    /// @param intensity Power-up intensity (0.5-2.0)
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreatePowerup(float intensity, float duration);
    
    /// Create a hurt/damage sound
    /// @param severity Damage severity (0.5-2.0)
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateHurt(float severity, float duration);
    
    /// Create a rising frequency sweep sound
    /// @param startFreq Starting frequency in Hz
    /// @param endFreq Ending frequency in Hz
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateSweepUp(float startFreq, float endFreq, float duration);
    
    /// Create a falling frequency sweep sound
    /// @param startFreq Starting frequency in Hz
    /// @param endFreq Ending frequency in Hz
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateSweepDown(float startFreq, float endFreq, float duration);
    
    /// Create a big explosion sound
    /// @param size Explosion size (0.5-2.0)
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateBigExplosion(float size, float duration);
    
    /// Create a small explosion sound
    /// @param intensity Explosion intensity (0.5-2.0)
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateSmallExplosion(float intensity, float duration);
    
    /// Create a distant explosion sound
    /// @param distance Distance factor (0.5-2.0)
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateDistantExplosion(float distance, float duration);
    
    /// Create a metallic explosion sound
    /// @param shrapnel Shrapnel amount (0.5-2.0)
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateMetalExplosion(float shrapnel, float duration);
    
    /// Create an impact/bang sound
    /// @param intensity Impact intensity (0.5-2.0)
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateBang(float intensity, float duration);
    
    /// Create a random procedural beep sound
    /// @param seed Random seed value
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateRandomBeep(uint32_t seed, float duration);
    
    /// Play a sound from the sound bank by ID
    /// @param soundId Sound ID returned from soundCreate* functions
    /// @param volume Volume (0.0-1.0, default = 1.0)
    /// @param pan Stereo pan (-1.0 = left, 0.0 = center, 1.0 = right, default = 0.0)
    void soundPlay(uint32_t soundId, float volume = 1.0f, float pan = 0.0f);
    
    /// Free a sound from the sound bank
    /// @param soundId Sound ID to free
    /// @return true if sound was found and freed
    bool soundFree(uint32_t soundId);
    
    /// Free all sounds from the sound bank
    void soundFreeAll();
    
    /// Check if a sound exists in the sound bank
    /// @param soundId Sound ID to check
    /// @return true if sound exists
    bool soundExists(uint32_t soundId) const;
    
    /// Get number of sounds in the sound bank
    /// @return Number of stored sounds
    size_t soundGetCount() const;
    
    /// Get approximate memory usage of sound bank
    /// @return Memory usage in bytes
    size_t soundGetMemoryUsage() const;
    
    // -------------------------------------------------------------------------
    // Custom Synthesis - Phase 3
    // -------------------------------------------------------------------------
    
    /// Create a simple tone with specified waveform
    /// @param frequency Frequency in Hz
    /// @param duration Duration in seconds
    /// @param waveform Waveform type (0=SINE, 1=SQUARE, 2=SAWTOOTH, 3=TRIANGLE, 4=NOISE, 5=PULSE)
    /// @return Sound ID (0 = error)
    uint32_t soundCreateTone(float frequency, float duration, int waveform);
    
    /// Create a musical note with ADSR envelope
    /// @param note MIDI note number (0-127, where 60=middle C)
    /// @param duration Total duration in seconds
    /// @param waveform Waveform type (0=SINE, 1=SQUARE, 2=SAWTOOTH, 3=TRIANGLE, 4=NOISE, 5=PULSE)
    /// @param attack Attack time in seconds
    /// @param decay Decay time in seconds
    /// @param sustainLevel Sustain level (0.0-1.0)
    /// @param release Release time in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateNote(int note, float duration, int waveform,
                             float attack, float decay, float sustainLevel, float release);
    
    /// Create noise sound
    /// @param noiseType Noise type (0=WHITE, 1=PINK, 2=BROWN/RED)
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateNoise(int noiseType, float duration);
    
    // -------------------------------------------------------------------------
    // Advanced Synthesis - Phase 4
    // -------------------------------------------------------------------------
    
    /// Create FM synthesized sound
    /// @param carrierFreq Carrier frequency in Hz
    /// @param modulatorFreq Modulator frequency in Hz
    /// @param modIndex Modulation index (depth, typically 0.5-10.0)
    /// @param duration Duration in seconds
    /// @return Sound ID (0 = error)
    uint32_t soundCreateFM(float carrierFreq, float modulatorFreq, float modIndex, float duration);
    
    /// Create a tone with filter applied
    /// @param frequency Frequency in Hz
    /// @param duration Duration in seconds
    /// @param waveform Waveform type (0=SINE, 1=SQUARE, 2=SAWTOOTH, 3=TRIANGLE, 4=NOISE, 5=PULSE)
    /// @param filterType Filter type (0=NONE, 1=LOW_PASS, 2=HIGH_PASS, 3=BAND_PASS)
    /// @param cutoff Filter cutoff frequency in Hz
    /// @param resonance Filter resonance (0.0-1.0)
    /// @return Sound ID (0 = error)
    uint32_t soundCreateFilteredTone(float frequency, float duration, int waveform,
                                     int filterType, float cutoff, float resonance);
    
    /// Create a musical note with ADSR envelope and filter
    /// @param note MIDI note number (0-127, where 60=middle C)
    /// @param duration Total duration in seconds
    /// @param waveform Waveform type (0=SINE, 1=SQUARE, 2=SAWTOOTH, 3=TRIANGLE, 4=NOISE, 5=PULSE)
    /// @param attack Attack time in seconds
    /// @param decay Decay time in seconds
    /// @param sustainLevel Sustain level (0.0-1.0)
    /// @param release Release time in seconds
    /// @param filterType Filter type (0=NONE, 1=LOW_PASS, 2=HIGH_PASS, 3=BAND_PASS)
    /// @param cutoff Filter cutoff frequency in Hz
    /// @param resonance Filter resonance (0.0-1.0)
    /// @return Sound ID (0 = error)
    uint32_t soundCreateFilteredNote(int note, float duration, int waveform,
                                     float attack, float decay, float sustainLevel, float release,
                                     int filterType, float cutoff, float resonance);
    
    // -------------------------------------------------------------------------
    // Effects Chain - Phase 5
    // -------------------------------------------------------------------------
    
    /// Create a tone with reverb effect
    /// @param frequency Frequency in Hz
    /// @param duration Duration in seconds
    /// @param waveform Waveform type (0=SINE, 1=SQUARE, 2=SAWTOOTH, 3=TRIANGLE, 4=NOISE, 5=PULSE)
    /// @param roomSize Reverb room size (0.0-1.0)
    /// @param damping High frequency damping (0.0-1.0)
    /// @param wet Wet signal level (0.0-1.0)
    /// @return Sound ID (0 = error)
    uint32_t soundCreateWithReverb(float frequency, float duration, int waveform,
                                   float roomSize, float damping, float wet);
    
    /// Create a tone with delay/echo effect
    /// @param frequency Frequency in Hz
    /// @param duration Duration in seconds
    /// @param waveform Waveform type (0=SINE, 1=SQUARE, 2=SAWTOOTH, 3=TRIANGLE, 4=NOISE, 5=PULSE)
    /// @param delayTime Delay time in seconds
    /// @param feedback Feedback amount (0.0-1.0)
    /// @param mix Dry/wet mix (0.0-1.0)
    /// @return Sound ID (0 = error)
    uint32_t soundCreateWithDelay(float frequency, float duration, int waveform,
                                  float delayTime, float feedback, float mix);
    
    /// Create a tone with distortion effect
    /// @param frequency Frequency in Hz
    /// @param duration Duration in seconds
    /// @param waveform Waveform type (0=SINE, 1=SQUARE, 2=SAWTOOTH, 3=TRIANGLE, 4=NOISE, 5=PULSE)
    /// @param drive Distortion drive amount (0.0-10.0)
    /// @param tone Tone control (0.0-1.0)
    /// @param level Output level (0.0-1.0)
    /// @return Sound ID (0 = error)
    uint32_t soundCreateWithDistortion(float frequency, float duration, int waveform,
                                       float drive, float tone, float level);
    
    // =========================================================================
    // Music Playback (ABC Notation)
    // =========================================================================
    
    /// Play music from ABC notation string
    /// @param abcNotation ABC notation string
    /// @param volume Volume (0.0-1.0)
    void playMusic(const std::string& abcNotation, float volume = 1.0f);
    
    /// Stop music playback
    void stopMusic();
    
    /// Pause music playback
    void pauseMusic();
    
    /// Resume music playback
    void resumeMusic();
    
    /// Check if music is playing
    /// @return true if music is currently playing
    bool isMusicPlaying() const;
    
    /// Get current music state
    /// @return Current music state
    MusicState getMusicState() const;
    
    /// Set music volume
    /// @param volume Volume (0.0-1.0)
    void setMusicVolume(float volume);
    
    /// Get music volume
    /// @return Volume (0.0-1.0)
    float getMusicVolume() const;
    
    /// Render ABC notation music to WAV file (offline rendering)
    /// @param abcNotation ABC notation string
    /// @param outputPath Output WAV file path
    /// @param durationSeconds Duration to render in seconds (0 = auto-detect from ABC)
    /// @param sampleRate Sample rate (default: 48000)
    /// @return true on success, false on error
    bool abcRenderToWAV(const std::string& abcNotation, const std::string& outputPath,
                        float durationSeconds = 0.0f, int sampleRate = 48000);
    
    // =========================================================================
    // Music Bank API (ID-based Music Management)
    // =========================================================================
    
    /// Load music from ABC notation string
    /// @param abcNotation ABC notation string
    /// @return Unique music ID (0 = invalid/error)
    uint32_t musicLoadString(const std::string& abcNotation);
    
    /// Load music from ABC file
    /// @param filename Path to ABC file
    /// @return Unique music ID (0 = invalid/error)
    uint32_t musicLoadFile(const std::string& filename);
    
    /// Play music by ID
    /// @param musicId Music ID from musicLoadString or musicLoadFile
    /// @param volume Volume (0.0-1.0)
    void musicPlay(uint32_t musicId, float volume = 1.0f);
    
    /// Check if music ID exists in the bank
    /// @param musicId Music ID
    /// @return true if music exists
    bool musicExists(uint32_t musicId) const;
    
    /// Get music title by ID
    /// @param musicId Music ID
    /// @return Title string (empty if not found)
    std::string musicGetTitle(uint32_t musicId) const;
    
    /// Get music composer by ID
    /// @param musicId Music ID
    /// @return Composer string (empty if not found)
    std::string musicGetComposer(uint32_t musicId) const;
    
    /// Get music key signature by ID
    /// @param musicId Music ID
    /// @return Key string (empty if not found)
    std::string musicGetKey(uint32_t musicId) const;
    
    /// Get music tempo by ID
    /// @param musicId Music ID
    /// @return Tempo in BPM (0.0 if not found)
    float musicGetTempo(uint32_t musicId) const;
    
    /// Free a music piece by ID
    /// @param musicId Music ID to free
    /// @return true if music was found and freed
    bool musicFree(uint32_t musicId);
    
    /// Free all music from the bank
    void musicFreeAll();
    
    /// Get number of music pieces in the bank
    /// @return Number of stored music pieces
    size_t musicGetCount() const;
    
    /// Get total memory usage of music bank (approximate)
    /// @return Memory usage in bytes
    size_t musicGetMemoryUsage() const;
    
    // =========================================================================
    // Utility
    // =========================================================================
    
    /// Convert MIDI note number to frequency
    /// @param midiNote MIDI note number (0-127)
    /// @return Frequency in Hz
    static float noteToFrequency(int midiNote);
    
    /// Convert frequency to MIDI note number
    /// @param frequency Frequency in Hz
    /// @return MIDI note number
    static int frequencyToNote(float frequency);
    
    /// Stop all audio (sounds and music)
    void stopAll();
    
    // =========================================================================
    // Voice Controller (Persistent Voice Synthesis for Advanced Audio Programming)
    // =========================================================================
    
    /// Set voice waveform
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param waveform Waveform type (0=Silence, 1=Sine, 2=Square, 3=Sawtooth, 4=Triangle, 5=Noise, 6=Pulse)
    void voiceSetWaveform(int voiceNum, int waveform);
    
    /// Set voice frequency in Hz
    /// @param voiceNum Voice number (1-based)
    /// @param frequencyHz Frequency in Hz
    void voiceSetFrequency(int voiceNum, float frequencyHz);
    
    /// Set voice note by MIDI note number
    /// @param voiceNum Voice number (1-based)
    /// @param midiNote MIDI note number (0-127, middle C = 60)
    void voiceSetNote(int voiceNum, int midiNote);
    
    /// Set voice note by note name
    /// @param voiceNum Voice number (1-based)
    /// @param noteName Note name (e.g., "C-4", "A#3", "Gb5")
    void voiceSetNoteName(int voiceNum, const std::string& noteName);
    
    /// Set voice envelope (ADSR)
    /// @param voiceNum Voice number (1-based)
    /// @param attackMs Attack time in milliseconds
    /// @param decayMs Decay time in milliseconds
    /// @param sustainLevel Sustain level (0.0 to 1.0)
    /// @param releaseMs Release time in milliseconds
    void voiceSetEnvelope(int voiceNum, float attackMs, float decayMs, float sustainLevel, float releaseMs);
    
    /// Set voice gate (on = start/sustain note, off = release)
    /// @param voiceNum Voice number (1-based)
    /// @param gateOn Gate state (true = on, false = off)
    void voiceSetGate(int voiceNum, bool gateOn);
    
    /// Set voice volume
    /// @param voiceNum Voice number (1-based)
    /// @param volume Volume level (0.0 to 1.0)
    void voiceSetVolume(int voiceNum, float volume);
    
    /// Set pulse width for pulse waveform
    /// @param voiceNum Voice number (1-based)
    /// @param pulseWidth Pulse width (0.0 to 1.0, 0.5 = square wave)
    void voiceSetPulseWidth(int voiceNum, float pulseWidth);
    
    /// Enable/disable filter routing for voice
    /// @param voiceNum Voice number (1-based)
    /// @param enabled Route through global filter
    void voiceSetFilterRouting(int voiceNum, bool enabled);
    
    /// Set global filter type
    /// @param filterType Filter type (0=None, 1=LowPass, 2=HighPass, 3=BandPass)
    void voiceSetFilterType(int filterType);
    
    /// Set global filter cutoff frequency
    /// @param cutoffHz Cutoff frequency in Hz
    void voiceSetFilterCutoff(float cutoffHz);
    
    /// Set global filter resonance
    /// @param resonance Resonance (1.0 = none, higher = more resonant)
    void voiceSetFilterResonance(float resonance);
    
    /// Enable/disable global filter
    /// @param enabled Filter enabled
    void voiceSetFilterEnabled(bool enabled);
    
    /// Set voice master volume
    /// @param volume Master volume (0.0 to 1.0)
    void voiceSetMasterVolume(float volume);
    
    /// Get voice master volume
    /// @return Master volume (0.0 to 1.0)
    float voiceGetMasterVolume() const;
    
    /// Reset all voices (gate off, clear state)
    void voiceResetAll();
    
    /// Get number of active voices (gate on)
    /// @return Number of active voices
    int voiceGetActiveCount() const;
    
    /// Check if VOICES_END_PLAY is currently playing
    /// @return true if voices playback buffer is currently playing
    bool voicesArePlaying() const;
    
    /// Set voice render mode (direct to WAV file or live playback)
    /// @param enable Enable render-to-file mode
    /// @param outputPath WAV file path for rendering (ignored if enable=false)
    void voiceSetRenderMode(bool enable, const std::string& outputPath);
    
    /// Render voice audio to sound slot
    /// @param slotNum Sound slot number (1-based)
    /// @param volume Playback volume (0.0 to 1.0)
    /// @param duration Duration in seconds (0 = auto-detect from gate off)
    /// @return Sound ID that was stored in the slot
    uint32_t voiceRenderToSlot(int slotNum, float volume, float duration);
    
    // =========================================================================
    // Voice Controller Extended API - Stereo & Spatial
    // =========================================================================
    
    /// Set voice stereo pan position
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param pan Pan position (-1.0 = left, 0.0 = center, 1.0 = right)
    void voiceSetPan(int voiceNum, float pan);
    
    // =========================================================================
    // Voice Controller Extended API - SID-Style Modulation
    // =========================================================================
    
    /// Enable ring modulation from source voice
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param sourceVoice Source voice for modulation (1-8, 0 = off)
    void voiceSetRingMod(int voiceNum, int sourceVoice);
    
    /// Enable hard sync from source voice
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param sourceVoice Source voice for sync (1-8, 0 = off)
    void voiceSetSync(int voiceNum, int sourceVoice);
    
    /// Set portamento (pitch glide) time
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param time Glide time in seconds
    void voiceSetPortamento(int voiceNum, float time);
    
    /// Set voice detuning in cents
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param cents Detuning in cents (-100 to +100)
    void voiceSetDetune(int voiceNum, float cents);
    
    // =========================================================================
    // Voice Controller Extended API - Delay Effects
    // =========================================================================
    
    /// Enable/disable delay effect for voice
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param enabled Enable state
    void voiceSetDelayEnable(int voiceNum, bool enabled);
    
    /// Set delay time
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param time Delay time in seconds
    void voiceSetDelayTime(int voiceNum, float time);
    
    /// Set delay feedback amount
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param feedback Feedback (0.0 to 0.95)
    void voiceSetDelayFeedback(int voiceNum, float feedback);
    
    /// Set delay wet/dry mix
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param mix Wet mix (0.0 = dry, 1.0 = wet)
    void voiceSetDelayMix(int voiceNum, float mix);
    
    // =========================================================================
    // Voice Controller Extended API - LFO Controls
    // =========================================================================
    
    /// Set LFO waveform type
    /// @param lfoNum LFO number (1-based, 1-4)
    /// @param waveform Waveform type (0=sine, 1=triangle, 2=square, 3=sawtooth, 4=random)
    void lfoSetWaveform(int lfoNum, int waveform);
    
    /// Set LFO rate in Hz
    /// @param lfoNum LFO number (1-based, 1-4)
    /// @param rateHz Rate in Hz
    void lfoSetRate(int lfoNum, float rateHz);
    
    /// Reset LFO phase to start
    /// @param lfoNum LFO number (1-based, 1-4)
    void lfoReset(int lfoNum);
    
    /// Route LFO to pitch (vibrato)
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param lfoNum LFO number (1-4, 0 = off)
    /// @param depthCents Modulation depth in cents
    void lfoToPitch(int voiceNum, int lfoNum, float depthCents);
    
    /// Route LFO to volume (tremolo)
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param lfoNum LFO number (1-4, 0 = off)
    /// @param depth Modulation depth (0.0 to 1.0)
    void lfoToVolume(int voiceNum, int lfoNum, float depth);
    
    /// Route LFO to filter cutoff (auto-wah)
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param lfoNum LFO number (1-4, 0 = off)
    /// @param depthHz Modulation depth in Hz
    void lfoToFilter(int voiceNum, int lfoNum, float depthHz);
    
    /// Route LFO to pulse width (auto-PWM)
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param lfoNum LFO number (1-4, 0 = off)
    /// @param depth Modulation depth (0.0 to 1.0)
    void lfoToPulseWidth(int voiceNum, int lfoNum, float depth);
    
    // =========================================================================
    // Voice Controller Extended API - Physical Modeling
    // =========================================================================
    
    /// Set physical modeling type
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param modelType Model type (0=plucked_string, 1=struck_bar, 2=blown_tube, 3=drumhead, 4=glass)
    void voiceSetPhysicalModel(int voiceNum, int modelType);
    
    /// Set physical model damping
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param damping Damping (0.0 = none, 1.0 = max)
    void voiceSetPhysicalDamping(int voiceNum, float damping);
    
    /// Set physical model brightness
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param brightness Brightness (0.0 = dark, 1.0 = bright)
    void voiceSetPhysicalBrightness(int voiceNum, float brightness);
    
    /// Set physical model excitation strength
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param excitation Excitation (0.0 = gentle, 1.0 = violent)
    void voiceSetPhysicalExcitation(int voiceNum, float excitation);
    
    /// Set physical model resonance
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param resonance Resonance (0.0 = damped, 1.0 = max)
    void voiceSetPhysicalResonance(int voiceNum, float resonance);
    
    /// Set string tension (string models only)
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param tension Tension (0.0 = loose, 1.0 = tight)
    void voiceSetPhysicalTension(int voiceNum, float tension);
    
    /// Set air pressure (wind models only)
    /// @param voiceNum Voice number (1-based, 1-8)
    /// @param pressure Pressure (0.0 = gentle, 1.0 = strong)
    void voiceSetPhysicalPressure(int voiceNum, float pressure);
    
    /// Trigger physical model excitation
    /// @param voiceNum Voice number (1-based, 1-8)
    void voicePhysicalTrigger(int voiceNum);
    
    // =========================================================================
    // Voice Script System (Non-blocking Sound Effects)
    // =========================================================================
    
    /// Define a voice script (compile and store)
    /// @param name Script name (unique identifier)
    /// @param source VoiceScript source code
    /// @param error Output error message if compilation fails
    /// @return true on success, false on error
    bool voiceScriptDefine(const std::string& name, const std::string& source, std::string& error);
    
    /// Play a voice script (non-blocking)
    /// @param name Script name
    /// @param bpm Tempo in beats per minute (default: 120)
    /// @return true if script was found and started
    bool voiceScriptPlay(const std::string& name, float bpm = 120.0f);
    
    /// Stop currently playing script
    void voiceScriptStop();
    
    /// Check if a script is currently playing
    /// @return true if playing
    bool voiceScriptIsPlaying() const;
    
    /// Remove a script from library
    /// @param name Script name
    /// @return true if found and removed
    bool voiceScriptRemove(const std::string& name);
    
    /// Clear all scripts from library
    void voiceScriptClear();
    
    /// Check if a script exists
    /// @param name Script name
    /// @return true if exists
    bool voiceScriptExists(const std::string& name) const;
    
    /// Get list of all script names
    /// @return Vector of script names
    std::vector<std::string> voiceScriptList() const;
    
    /// Load a voice script from file
    /// @param path Path to .vscript file
    /// @param name Script name (if empty, uses filename)
    /// @return true on success
    bool voiceScriptLoad(const std::string& path, const std::string& name = "");
    
    /// Render a voice script to WAV file
    /// Render VoiceScript to WAV file (offline rendering)
    /// 
    /// @param scriptName Name of the script to render
    /// @param outputPath Path for output WAV file
    /// @param durationSeconds Maximum duration to render (0 = auto-detect from script)
    /// @param sampleRate Sample rate for WAV file (default: 48000)
    /// @param bpm Tempo in beats per minute (default: 120)
    /// @param fastRender Enable fast rendering mode with coarser update rate (default: false)
    /// @return true on success, false on error
    bool voiceScriptRenderToWAV(const std::string& scriptName,
                                 const std::string& outputPath,
                                 float durationSeconds = 0.0f,
                                 int sampleRate = 48000,
                                 float bpm = 120.0f,
                                 bool fastRender = false);
    
    /// Render a voice script and save to sound bank (SYNCHRONOUS/BLOCKING)
    /// 
    /// This function renders the VoiceScript offline in the calling thread and blocks
    /// until rendering is complete. This is intentional - the caller needs the sound ID
    /// immediately to store or play the sound. Render time ≈ audio duration.
    /// 
    /// Uses a temporary voice controller for offline rendering (not the background player).
    /// 
    /// @param scriptName Name of the script to render
    /// @param durationSeconds Maximum duration to render (0 = auto-detect from script)
    /// @param sampleRate Sample rate for WAV file (default: 48000)
    /// @param bpm Tempo in beats per minute (default: 120)
    /// @param fastRender Enable fast rendering mode with coarser update rate (default: false)
    /// @return Sound ID (> 0) on success, 0 on failure
    uint32_t voiceScriptSaveToBank(const std::string& scriptName,
                                    float durationSeconds = 0.0f,
                                    int sampleRate = 48000,
                                    float bpm = 120.0f,
                                    bool fastRender = false);
    
    // =========================================================================
    // SID Music (Commodore 64 SID Chip Emulation)
    // =========================================================================
    
    /// Load a SID file from disk
    /// @param path Full path to .sid file
    /// @return SID ID (> 0) on success, 0 on failure
    uint32_t sidLoadFile(const std::string& path);
    
    /// Load a SID file from memory buffer
    /// @param data Pointer to SID file data
    /// @param size Size of data in bytes
    /// @return SID ID (> 0) on success, 0 on failure
    uint32_t sidLoadMemory(const uint8_t* data, size_t size);
    
    /// Play a loaded SID
    /// @param id SID ID from sidLoadFile or sidLoadMemory
    /// @param subtune Subtune number (0 = default subtune, 1+ = specific subtune)
    /// @param volume Volume level (0.0 to 1.0)
    void sidPlay(uint32_t id, int subtune = 0, float volume = 1.0f);
    
    /// Stop SID playback
    void sidStop();
    
    /// Pause SID playback
    void sidPause();
    
    /// Resume SID playback
    void sidResume();
    
    /// Check if SID is playing
    /// @return true if a SID is currently playing
    bool sidIsPlaying() const;
    
    /// Set SID playback volume
    /// @param volume Volume level (0.0 to 1.0)
    void sidSetVolume(float volume);
    
    /// Select a subtune to play
    /// @param id SID ID
    /// @param subtune Subtune number (1-based)
    /// @return true if successful
    bool sidSetSubtune(uint32_t id, int subtune);
    
    /// Get currently selected subtune
    /// @param id SID ID
    /// @return Current subtune number (0 if not found)
    int sidGetSubtune(uint32_t id) const;
    
    /// Get total number of subtunes
    /// @param id SID ID
    /// @return Number of subtunes (0 if not found)
    int sidGetSubtuneCount(uint32_t id) const;
    
    /// Get default subtune number
    /// @param id SID ID
    /// @return Default subtune number (1-based, 0 if not found)
    int sidGetDefaultSubtune(uint32_t id) const;
    
    /// Get SID title
    /// @param id SID ID
    /// @return Title string (empty if not found)
    std::string sidGetTitle(uint32_t id) const;
    
    /// Get SID author/composer
    /// @param id SID ID
    /// @return Author string (empty if not found)
    std::string sidGetAuthor(uint32_t id) const;
    
    /// Get SID copyright info
    /// @param id SID ID
    /// @return Copyright string (empty if not found)
    std::string sidGetCopyright(uint32_t id) const;
    
    /// Check if SID exists in bank
    /// @param id SID ID
    /// @return true if SID exists
    bool sidExists(uint32_t id) const;
    
    /// Free a loaded SID
    /// @param id SID ID
    /// @return true if SID was found and freed
    bool sidFree(uint32_t id);
    
    /// Free all loaded SIDs
    void sidFreeAll();
    
    /// Get number of loaded SIDs
    /// @return Count of SIDs in bank
    size_t sidGetCount() const;
    
    /// Get total memory used by SIDs
    /// @return Memory usage in bytes
    size_t sidGetMemoryUsage() const;
    
    /// Set SID emulation quality
    /// @param quality Quality level (0=Fast, 1=Medium, 2=Good, 3=Best)
    void sidSetQuality(int quality);
    
    /// Set SID chip model
    /// @param model Chip model (0=MOS6581, 1=MOS8580, 2=Auto)
    void sidSetChipModel(int model);
    
    /// Set SID playback speed multiplier
    /// @param speed Speed multiplier (1.0 = normal, 2.0 = double speed, etc.)
    void sidSetSpeed(float speed);
    
    /// Set maximum number of SID chips to emulate
    /// @param maxSids Number of SID chips (1-3)
    void sidSetMaxSids(int maxSids);
    
    /// Get maximum number of SID chips
    /// @return Maximum SID chips (1-3)
    int sidGetMaxSids() const;
    
    /// Render a SID file to WAV (offline rendering)
    /// @param sidId SID ID (from sidLoadFile or sidLoadMemory)
    /// @param outputPath Output WAV file path
    /// @param durationSeconds Duration to render in seconds (default: 180 = 3 minutes)
    /// @param subtune Subtune to render (0 = default)
    /// @return true on success, false on error
    bool sidRenderToWAV(uint32_t sidId, const std::string& outputPath, 
                        float durationSeconds = 180.0f, int subtune = 0);
    
    /// Render a SID file to sound bank (offline rendering to memory)
    /// @param sidId SID ID (from sidLoadFile or sidLoadMemory)
    /// @param durationSeconds Duration to render in seconds (default: 180 = 3 minutes)
    /// @param subtune Subtune to render (0 = default)
    /// @param sampleRate Sample rate (default: 48000)
    /// @param fastRender Enable fast rendering mode with coarser update rate (default: false)
    /// @return Sound ID (> 0) on success, 0 on failure
    uint32_t sidRenderToBank(uint32_t sidId, float durationSeconds = 180.0f, 
                             int subtune = 0, int sampleRate = 48000, bool fastRender = false);
    
    /// Render ABC notation to sound bank (offline rendering to memory)
    /// @param abcNotation ABC notation string
    /// @param durationSeconds Duration to render in seconds (0 = auto-detect)
    /// @param sampleRate Sample rate (default: 48000)
    /// @param fastRender Enable fast rendering mode with coarser update rate (default: false)
    /// @return Sound ID (> 0) on success, 0 on failure
    uint32_t abcRenderToBank(const std::string& abcNotation, float durationSeconds = 0.0f,
                             int sampleRate = 48000, bool fastRender = false);
    
    /// Get current SID playback time
    /// @return Playback time in seconds
    float sidGetTime() const;
    
    // =============================================================================
    // VOICES Timeline System - Record and render voice command sequences
    // =============================================================================
    
    /// Start recording voice commands to timeline
    void voicesStartRecording();
    
    /// Advance beat cursor (used by VOICE_WAIT)
    /// @param beats Number of beats to advance
    void voicesAdvanceBeatCursor(float beats);
    
    /// Set tempo for VOICES timeline
    /// @param bpm Beats per minute (e.g., 120 for standard tempo)
    void voicesSetTempo(float bpm);
    
    /// Render timeline and save to sound bank slot
    /// @param slot Sound bank slot number
    /// @param volume Playback volume (0.0-1.0)
    void voicesEndAndSaveToSlot(int slot, float volume);
    
    /// Render timeline and save to sound bank, returning the assigned slot ID
    /// @param volume Playback volume (0.0-1.0)
    /// @return Sound ID assigned by sound bank (0 on error)
    uint32_t voicesEndAndReturnSlot(float volume);
    
    /// Render timeline and play immediately
    void voicesEndAndPlay();
    
    /// Render timeline and save to WAV file
    /// @param filename Output WAV filename
    void voicesEndAndSaveToWAV(const std::string& filename);
    
    // Voice command types for timeline recording
    enum class VoiceCommandType {
        SET_WAVEFORM,
        SET_FREQUENCY,
        SET_NOTE,
        SET_VOLUME,
        SET_ENVELOPE,
        SET_GATE,
        SET_PAN,
        SET_PHYSICAL_MODEL,
        SET_PHYSICAL_DAMPING,
        SET_PHYSICAL_BRIGHTNESS,
        SET_PHYSICAL_EXCITATION,
        SET_PHYSICAL_RESONANCE,
        SET_PHYSICAL_TENSION,
        SET_PHYSICAL_PRESSURE,
        SET_PULSE_WIDTH,
        SET_FILTER_ROUTING,
        SET_FILTER_TYPE,
        SET_FILTER_CUTOFF,
        SET_FILTER_RESONANCE,
        SET_FILTER_ENABLED,
        SET_DELAY_ENABLE,
        SET_DELAY_TIME,
        SET_DELAY_FEEDBACK,
        SET_DELAY_MIX,
        SET_RING_MOD,
        SET_SYNC,
        SET_PORTAMENTO,
        SET_DETUNE,
        LFO_SET_WAVEFORM,
        LFO_SET_RATE,
        LFO_RESET,
        LFO_TO_PITCH,
        LFO_TO_VOLUME,
        LFO_TO_FILTER,
        LFO_TO_PULSEWIDTH,
        PHYSICAL_TRIGGER,
        SET_TEMPO,
    };
    
    // Voice command structure for timeline recording
    struct VoiceCommand {
        float beat;
        VoiceCommandType type;
        int voice;
        union {
            int intValue;
            float floatValue;
            struct { float a, b, c, d; } envelope;
            struct { int lfo; float amount; } lfoRoute;
        } param;
    };
    
    // Record voice commands to timeline (if recording is active)
    void recordVoiceCommand(int voice, VoiceCommandType type, int intValue);
    void recordVoiceCommand(int voice, VoiceCommandType type, float floatValue);
    void recordVoiceCommand(int voice, VoiceCommandType type, float a, float b, float c, float d);
    void recordVoiceCommand(int voice, VoiceCommandType type, int lfo, float amount);
    
    // Record voice commands at explicit beat positions (for _AT commands)
    void recordVoiceCommandAtBeat(int voice, float beat, VoiceCommandType type, int intValue);
    void recordVoiceCommandAtBeat(int voice, float beat, VoiceCommandType type, float floatValue);
    void recordVoiceCommandAtBeat(int voice, float beat, VoiceCommandType type, float a, float b, float c, float d);
    void recordVoiceCommandAtBeat(int voice, float beat, VoiceCommandType type, int lfo, float amount);
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    
    // Helper methods for VOICES timeline rendering
    bool renderVoicesTimeline(std::vector<float>& outBuffer);
    void executeVoiceCommand(VoiceController& vc, const VoiceCommand& cmd);
    bool saveWAVFile(const std::string& filename, const std::vector<float>& audioData, float sampleRate);
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_AUDIO_MANAGER_H