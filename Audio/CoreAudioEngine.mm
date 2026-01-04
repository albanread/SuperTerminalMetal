//
// CoreAudioEngine.mm
// SuperTerminal v2
//
// Stub/adapter for CoreAudio functionality
// Provides compatibility layer for MidiEngine
//

#include "CoreAudioEngine.h"
#include "SynthEngine.h"
#import <Foundation/Foundation.h>

namespace SuperTerminal {

CoreAudioEngine::CoreAudioEngine()
    : m_initialized(false)
    , m_synthEngine(nullptr)
{
}

CoreAudioEngine::~CoreAudioEngine() {
    shutdown();
}

bool CoreAudioEngine::initialize() {
    if (m_initialized) {
        return true;
    }

    // Minimal initialization - most audio handled by AVAudioEngine
    m_initialized = true;

    NSLog(@"CoreAudioEngine: Initialized (stub/adapter mode)");
    return true;
}

void CoreAudioEngine::shutdown() {
    if (!m_initialized) {
        return;
    }

    m_initialized = false;
    m_synthEngine = nullptr;
}

} // namespace SuperTerminal
