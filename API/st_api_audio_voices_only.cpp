//
// st_api_audio_voices_only.cpp  
// Voice-only API for terminal shell (fbsh_voices)
//

#include "superterminal_api.h"
#include "st_api_context.h"
#include "../Audio/AudioManager.h"
#include "../Audio/Voice/VoiceController.h"
#include <cstring>
#include <mutex>
#include <unistd.h>

using namespace SuperTerminal;
using namespace STApi;

static std::mutex g_apiMutex;
#define ST_LOCK std::lock_guard<std::mutex> lock(g_apiMutex)
#define ST_CHECK_PTR(ptr, name) if (!ptr) { return; }
#define ST_CHECK_PTR_RET(ptr, name, ret) if (!ptr) { return ret; }

// Voice/LFO functions from st_api_audio.cpp
ST_API void st_voice_set_waveform(int voiceNum, int waveform) {
    ST_LOCK;
    auto* audio = ST_CONTEXT.audio();
    if (!audio) return;
    
    // Record to timeline if in recording mode
    audio->recordVoiceCommand(voiceNum, SuperTerminal::AudioManager::VoiceCommandType::SET_WAVEFORM, waveform);
    
    // Also execute immediately (for live mode or as fallback)
    auto* vc = audio->getVoiceController();
    if (vc) {
        vc->setWaveform(voiceNum, static_cast<VoiceWaveform>(waveform));
    }
}

ST_API void st_voice_set_frequency(int voiceNum, float frequencyHz) {
    ST_LOCK;
    auto* audio = ST_CONTEXT.audio();
    if (!audio) return;
    
    audio->recordVoiceCommand(voiceNum, SuperTerminal::AudioManager::VoiceCommandType::SET_FREQUENCY, frequencyHz);
    
    auto* vc = audio->getVoiceController();
    if (vc) {
        vc->setFrequency(voiceNum, frequencyHz);
    }
}

ST_API void st_voice_set_note(int voiceNum, int midiNote) {
    ST_LOCK;
    auto* audio = ST_CONTEXT.audio();
    if (!audio) return;
    
    audio->recordVoiceCommand(voiceNum, SuperTerminal::AudioManager::VoiceCommandType::SET_NOTE, midiNote);
    
    auto* vc = audio->getVoiceController();
    if (vc) {
        vc->setNote(voiceNum, midiNote);
    }
}

ST_API void st_voice_set_note_name(int voiceNum, const char* noteName) {
    if (!noteName) return;
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setNoteName(voiceNum, noteName);
    }
}

ST_API void st_voice_set_envelope(int voiceNum, float attackMs, float decayMs, float sustainLevel, float releaseMs) {
    ST_LOCK;
    auto* audio = ST_CONTEXT.audio();
    if (!audio) return;
    
    audio->recordVoiceCommand(voiceNum, SuperTerminal::AudioManager::VoiceCommandType::SET_ENVELOPE, 
                              attackMs, decayMs, sustainLevel, releaseMs);
    
    auto* vc = audio->getVoiceController();
    if (vc) {
        vc->setEnvelope(voiceNum, attackMs, decayMs, sustainLevel, releaseMs);
    }
}

ST_API void st_voice_set_gate(int voiceNum, int gateOn) {
    ST_LOCK;
    auto* audio = ST_CONTEXT.audio();
    if (!audio) return;
    
    audio->recordVoiceCommand(voiceNum, SuperTerminal::AudioManager::VoiceCommandType::SET_GATE, gateOn);
    
    auto* vc = audio->getVoiceController();
    if (vc) {
        vc->setGate(voiceNum, gateOn != 0);
    }
}

ST_API void st_voice_set_volume(int voiceNum, float volume) {
    ST_LOCK;
    auto* audio = ST_CONTEXT.audio();
    if (!audio) return;
    
    audio->recordVoiceCommand(voiceNum, SuperTerminal::AudioManager::VoiceCommandType::SET_VOLUME, volume);
    
    auto* vc = audio->getVoiceController();
    if (vc) {
        vc->setVolume(voiceNum, volume);
    }
}

ST_API void st_voice_set_pulse_width(int voiceNum, float pulseWidth) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setPulseWidth(voiceNum, pulseWidth);
    }
}

ST_API void st_voice_set_filter_routing(int voiceNum, int enabled) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setFilterRouting(voiceNum, enabled != 0);
    }
}

ST_API void st_voice_set_filter_type(int filterType) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setFilterType(static_cast<VoiceFilterType>(filterType));
    }
}

ST_API void st_voice_set_filter_cutoff(float cutoffHz) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setFilterCutoff(cutoffHz);
    }
}

ST_API void st_voice_set_filter_resonance(float resonance) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setFilterResonance(resonance);
    }
}

ST_API void st_voice_set_filter_enabled(int enabled) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setFilterEnabled(enabled != 0);
    }
}

ST_API void st_voice_set_master_volume(float volume) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setMasterVolume(volume);
    }
}

ST_API float st_voice_get_master_volume(void) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    return vc ? vc->getMasterVolume() : 0.0f;
}

ST_API void st_voice_reset_all(void) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->resetAllVoices();
    }
}

ST_API int st_voice_get_active_count(void) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    return vc ? vc->getActiveVoiceCount() : 0;
}

ST_API int st_voices_are_playing(void) {
    ST_LOCK;
    if (!ST_CONTEXT.audio()) return 0;
    return ST_CONTEXT.audio()->voicesArePlaying() ? 1 : 0;
}

ST_API void st_voice_direct(const char* destination) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    if (destination && strlen(destination) > 0) {
        ST_CONTEXT.audio()->voiceSetRenderMode(true, destination);
    } else {
        ST_CONTEXT.audio()->voiceSetRenderMode(false, "");
    }
}

ST_API uint32_t st_voice_direct_slot(int slotNum, float volume, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);
    return ST_CONTEXT.audio()->voiceRenderToSlot(slotNum, volume, duration);
}

ST_API void st_voice_set_pan(int voiceNum, float pan) {
    ST_LOCK;
    auto* audio = ST_CONTEXT.audio();
    if (!audio) return;
    
    audio->recordVoiceCommand(voiceNum, SuperTerminal::AudioManager::VoiceCommandType::SET_PAN, pan);
    
    auto* vc = audio->getVoiceController();
    if (vc) {
        vc->setPan(voiceNum, pan);
    }
}

ST_API void st_voice_set_ring_mod(int voiceNum, int sourceVoice) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setRingMod(voiceNum, sourceVoice);
    }
}

ST_API void st_voice_set_sync(int voiceNum, int sourceVoice) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setSync(voiceNum, sourceVoice);
    }
}

ST_API void st_voice_set_portamento(int voiceNum, float time) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setPortamento(voiceNum, time);
    }
}

ST_API void st_voice_set_detune(int voiceNum, float cents) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setDetune(voiceNum, cents);
    }
}

ST_API void st_voice_set_delay_enable(int voiceNum, int enabled) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setDelayEnabled(voiceNum, enabled != 0);
    }
}

ST_API void st_voice_set_delay_time(int voiceNum, float time) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setDelayTime(voiceNum, time);
    }
}

ST_API void st_voice_set_delay_feedback(int voiceNum, float feedback) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setDelayFeedback(voiceNum, feedback);
    }
}

ST_API void st_voice_set_delay_mix(int voiceNum, float mix) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setDelayMix(voiceNum, mix);
    }
}

ST_API void st_lfo_set_waveform(int lfoNum, int waveform) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setLFOWaveform(lfoNum, static_cast<LFOWaveform>(waveform));
    }
}

ST_API void st_lfo_set_rate(int lfoNum, float rateHz) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setLFORate(lfoNum, rateHz);
    }
}

ST_API void st_lfo_reset(int lfoNum) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->resetLFO(lfoNum);
    }
}

ST_API void st_lfo_to_pitch(int voiceNum, int lfoNum, float depthCents) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setLFOToPitch(voiceNum, lfoNum, depthCents);
    }
}

ST_API void st_lfo_to_volume(int voiceNum, int lfoNum, float depth) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setLFOToVolume(voiceNum, lfoNum, depth);
    }
}

ST_API void st_lfo_to_filter(int voiceNum, int lfoNum, float depthHz) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setLFOToFilter(voiceNum, lfoNum, depthHz);
    }
}

ST_API void st_lfo_to_pulsewidth(int voiceNum, int lfoNum, float depth) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setLFOToPulseWidth(voiceNum, lfoNum, depth);
    }
}

ST_API void st_voice_set_physical_model(int voiceNum, int model) {
    ST_LOCK;
    auto* audio = ST_CONTEXT.audio();
    if (!audio) return;
    
    audio->recordVoiceCommand(voiceNum, SuperTerminal::AudioManager::VoiceCommandType::SET_PHYSICAL_MODEL, model);
    
    auto* vc = audio->getVoiceController();
    if (vc) {
        vc->setPhysicalModel(voiceNum, static_cast<PhysicalModelType>(model));
    }
}

ST_API void st_voice_set_physical_damping(int voiceNum, float damping) {
    ST_LOCK;
    auto* audio = ST_CONTEXT.audio();
    if (!audio) return;
    
    audio->recordVoiceCommand(voiceNum, SuperTerminal::AudioManager::VoiceCommandType::SET_PHYSICAL_DAMPING, damping);
    
    auto* vc = audio->getVoiceController();
    if (vc) {
        vc->setPhysicalDamping(voiceNum, damping);
    }
}

ST_API void st_voice_set_physical_brightness(int voiceNum, float brightness) {
    ST_LOCK;
    auto* audio = ST_CONTEXT.audio();
    if (!audio) return;
    
    audio->recordVoiceCommand(voiceNum, SuperTerminal::AudioManager::VoiceCommandType::SET_PHYSICAL_BRIGHTNESS, brightness);
    
    auto* vc = audio->getVoiceController();
    if (vc) {
        vc->setPhysicalBrightness(voiceNum, brightness);
    }
}

ST_API void st_voice_set_physical_excitation(int voiceNum, float excitation) {
    ST_LOCK;
    auto* audio = ST_CONTEXT.audio();
    if (!audio) return;
    
    audio->recordVoiceCommand(voiceNum, SuperTerminal::AudioManager::VoiceCommandType::SET_PHYSICAL_EXCITATION, excitation);
    
    auto* vc = audio->getVoiceController();
    if (vc) {
        vc->setPhysicalExcitation(voiceNum, excitation);
    }
}

ST_API void st_voice_set_physical_resonance(int voiceNum, float resonance) {
    ST_LOCK;
    auto* audio = ST_CONTEXT.audio();
    if (!audio) return;
    
    audio->recordVoiceCommand(voiceNum, SuperTerminal::AudioManager::VoiceCommandType::SET_PHYSICAL_RESONANCE, resonance);
    
    auto* vc = audio->getVoiceController();
    if (vc) {
        vc->setPhysicalResonance(voiceNum, resonance);
    }
}

ST_API void st_voice_set_physical_tension(int voiceNum, float tension) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setPhysicalTension(voiceNum, tension);
    }
}

ST_API void st_voice_set_physical_pressure(int voiceNum, float pressure) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setPhysicalPressure(voiceNum, pressure);
    }
}

ST_API void st_voice_physical_trigger(int voiceNum) {
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->triggerPhysical(voiceNum);
    }
}

ST_API void st_voice_wait(float beats) {
    // Advance the beat cursor in the timeline
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voicesAdvanceBeatCursor(beats);
}

// =============================================================================
// VOICES Timeline System - Record and render voice sequences
// =============================================================================

ST_API void st_voices_start(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voicesStartRecording();
}

ST_API void st_voices_set_tempo(float bpm) {
    fprintf(stderr, "st_voices_set_tempo called with bpm=%.1f\n", bpm);
    fflush(stderr);
    ST_LOCK;
    fprintf(stderr, "st_voices_set_tempo: lock acquired\n");
    fflush(stderr);
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    fprintf(stderr, "st_voices_set_tempo: about to call voicesSetTempo\n");
    fflush(stderr);
    ST_CONTEXT.audio()->voicesSetTempo(bpm);
    fprintf(stderr, "st_voices_set_tempo: voicesSetTempo returned\n");
    fflush(stderr);
}

// Voice commands with explicit beat positions (for _AT commands)
ST_API void st_voice_waveform_at(int voiceNum, float beat, int waveform) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->recordVoiceCommandAtBeat(voiceNum, beat, SuperTerminal::AudioManager::VoiceCommandType::SET_WAVEFORM, waveform);
}

ST_API void st_voice_frequency_at(int voiceNum, float beat, float frequencyHz) {
    fprintf(stderr, "st_voice_frequency_at: voice=%d, beat=%.2f, freq=%.2f\n", voiceNum, beat, frequencyHz);
    fflush(stderr);
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    fprintf(stderr, "st_voice_frequency_at: about to call recordVoiceCommandAtBeat\n");
    fflush(stderr);
    ST_CONTEXT.audio()->recordVoiceCommandAtBeat(voiceNum, beat, SuperTerminal::AudioManager::VoiceCommandType::SET_FREQUENCY, frequencyHz);
    fprintf(stderr, "st_voice_frequency_at: recordVoiceCommandAtBeat completed\n");
    fflush(stderr);
}

ST_API void st_voice_envelope_at(int voiceNum, float beat, float attackMs, float decayMs, float sustainLevel, float releaseMs) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->recordVoiceCommandAtBeat(voiceNum, beat, SuperTerminal::AudioManager::VoiceCommandType::SET_ENVELOPE, attackMs, decayMs, sustainLevel, releaseMs);
}

ST_API void st_voice_gate_at(int voiceNum, float beat, int gateOn) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->recordVoiceCommandAtBeat(voiceNum, beat, SuperTerminal::AudioManager::VoiceCommandType::SET_GATE, gateOn);
}

ST_API void st_voice_volume_at(int voiceNum, float beat, float volume) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->recordVoiceCommandAtBeat(voiceNum, beat, SuperTerminal::AudioManager::VoiceCommandType::SET_VOLUME, volume);
}

ST_API void st_voice_pan_at(int voiceNum, float beat, float pan) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->recordVoiceCommandAtBeat(voiceNum, beat, SuperTerminal::AudioManager::VoiceCommandType::SET_PAN, pan);
}

ST_API void st_voice_filter_at(int voiceNum, float beat, float cutoffHz, float resonance, int filterType) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    // Record filter type
    ST_CONTEXT.audio()->recordVoiceCommandAtBeat(voiceNum, beat, SuperTerminal::AudioManager::VoiceCommandType::SET_FILTER_TYPE, filterType);
    // Record cutoff
    ST_CONTEXT.audio()->recordVoiceCommandAtBeat(voiceNum, beat, SuperTerminal::AudioManager::VoiceCommandType::SET_FILTER_CUTOFF, cutoffHz);
    // Record resonance
    ST_CONTEXT.audio()->recordVoiceCommandAtBeat(voiceNum, beat, SuperTerminal::AudioManager::VoiceCommandType::SET_FILTER_RESONANCE, resonance);
}

ST_API void st_voices_end_slot(int slot, float volume) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voicesEndAndSaveToSlot(slot, volume);
}

ST_API uint32_t st_voices_next_slot(float volume) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);
    return ST_CONTEXT.audio()->voicesEndAndReturnSlot(volume);
}

ST_API void st_voices_end_play(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voicesEndAndPlay();
}

ST_API void st_voices_end_save(const char* filename) {
    if (!filename) return;
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voicesEndAndSaveToWAV(filename);
}
