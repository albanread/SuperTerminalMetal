//
//  VoiceScript.h
//  SuperTerminal Framework - Voice Script System
//
//  Bytecode compiler and interpreter for non-blocking voice synthesis effects
//  Allows defining sound effects as scripts that run in background
//
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef SUPERTERMINAL_VOICE_SCRIPT_H
#define SUPERTERMINAL_VOICE_SCRIPT_H

#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <memory>

namespace SuperTerminal {

// Forward declarations
class VoiceController;

// =============================================================================
// Bytecode Format
// =============================================================================

/// Voice script opcodes
enum class VoiceOpCode : uint8_t {
    // Voice control commands
    SET_WAVEFORM = 0x01,      // voice(u8), waveform(u8)
    SET_FREQUENCY,            // voice(u8), hz(f32)
    SET_NOTE,                 // voice(u8), note(u8)
    SET_ENVELOPE,             // voice(u8), attack(f32), decay(f32), sustain(f32), release(f32)
    SET_GATE,                 // voice(u8), state(u8)
    SET_VOLUME,               // voice(u8), volume(f32)
    SET_PULSE_WIDTH,          // voice(u8), width(f32)
    
    // SID-inspired modulation
    SET_RING_MOD,             // voice(u8), source_voice(u8)
    SET_SYNC,                 // voice(u8), source_voice(u8)
    SET_TEST_BIT,             // voice(u8), state(u8)
    SET_WAVEFORM_COMBO,       // voice(u8), waveform1(u8), waveform2(u8)
    SET_PORTAMENTO,           // voice(u8), time_seconds(f32)
    SET_DETUNE,               // voice(u8), cents(f32)
    PLAY_NOTE,                // voice(u8), note(u8), duration_seconds(f32)
    
    // Stereo and spatial control
    SET_PAN,                  // voice(u8), pan(f32) - -1.0 (left) to 1.0 (right)
    
    // Delay effect control
    SET_DELAY_ENABLE,         // voice(u8), enabled(u8)
    SET_DELAY_TIME,           // voice(u8), time_seconds(f32)
    SET_DELAY_FEEDBACK,       // voice(u8), feedback(f32) - 0.0 to 0.95
    SET_DELAY_MIX,            // voice(u8), wet_mix(f32) - 0.0 to 1.0
    
    // Filter control commands
    SET_FILTER_TYPE,          // type(u8)
    SET_FILTER_CUTOFF,        // hz(f32)
    SET_FILTER_RESONANCE,     // q(f32)
    SET_FILTER_ENABLE,        // state(u8)
    SET_FILTER_ROUTE,         // voice(u8), enabled(u8)
    
    // Control flow
    WAIT,                     // beats(f32) - wait for specified number of beats
    WAIT_RANDOM,              // min_beats(f32), max_beats(f32) - wait random duration
    TEMPO,                    // bpm(f32) - set tempo in beats per minute
    LOOP_START,               // loop_id(u8), start(f32), end(f32), step(f32)
    LOOP_NEXT,                // loop_id(u8)
    
    // Expressions (for computed values)
    PUSH_VAR,                 // var_id(u8) - push variable value
    PUSH_CONST,               // value(f32) - push constant
    PUSH_RANDOM,              // min(f32), max(f32) - push random value in range
    ADD,                      // pop 2, push result
    SUB,                      // pop 2, push result
    MUL,                      // pop 2, push result
    DIV,                      // pop 2, push result
    
    // Master controls
    SET_MASTER_VOLUME,        // volume(f32) - global volume control
    
    // LFO controls
    LFO_WAVEFORM,             // lfo_num(u8), waveform(u8)
    LFO_RATE,                 // lfo_num(u8), rate_hz(f32)
    LFO_RESET,                // lfo_num(u8)
    LFO_TO_PITCH,             // voice(u8), lfo_num(u8), depth_cents(f32)
    LFO_TO_VOLUME,            // voice(u8), lfo_num(u8), depth(f32)
    LFO_TO_FILTER,            // voice(u8), lfo_num(u8), depth_hz(f32)
    LFO_TO_PULSEWIDTH,        // voice(u8), lfo_num(u8), depth(f32)
    
    // Physical modeling controls
    SET_PHYSICAL_MODEL,       // voice(u8), model_type(u8) - 0=string, 1=bar, 2=tube, 3=drum
    SET_PHYSICAL_DAMPING,     // voice(u8), damping(f32) - 0.0 to 1.0
    SET_PHYSICAL_BRIGHTNESS,  // voice(u8), brightness(f32) - 0.0 to 1.0
    SET_PHYSICAL_EXCITATION,  // voice(u8), excitation(f32) - 0.0 to 1.0
    SET_PHYSICAL_RESONANCE,   // voice(u8), resonance(f32) - 0.0 to 1.0
    SET_PHYSICAL_TENSION,     // voice(u8), tension(f32) - 0.0 to 1.0 (string models)
    SET_PHYSICAL_PRESSURE,    // voice(u8), pressure(f32) - 0.0 to 1.0 (tube models)
    PHYSICAL_TRIGGER,         // voice(u8) - trigger physical model excitation
    
    // Debug control
    VOICESCRIPT_DEBUG,        // enabled(u8) - enable/disable debug logging
    
    // Program control
    END = 0xFF                // End of script
};

/// Compiled voice script bytecode
struct VoiceScriptBytecode {
    std::vector<uint8_t> code;      // Bytecode instructions
    std::string name;                // Script name
    float estimatedBeats;            // Estimated duration in beats
    
    VoiceScriptBytecode() : estimatedBeats(0.0f) {}
};

// =============================================================================
// Compiler
// =============================================================================

/// Compiles voice script source code to bytecode
class VoiceScriptCompiler {
public:
    VoiceScriptCompiler();
    ~VoiceScriptCompiler();
    
    /// Compile source code to bytecode
    /// @param source Voice script source code
    /// @param name Script name
    /// @param error Output error message if compilation fails
    /// @return Compiled bytecode, or nullptr on error
    std::unique_ptr<VoiceScriptBytecode> compile(const std::string& source,
                                                   const std::string& name,
                                                   std::string& error);
    
private:
    struct Token {
        enum Type {
            COMMAND,      // VOICE_WAVEFORM, etc.
            NUMBER,       // 123, 45.6
            COMMA,        // ,
            FOR,          // FOR
            TO,           // TO
            STEP,         // STEP
            NEXT,         // NEXT
            WAIT,         // WAIT
            WAIT_RANDOM,  // WAIT_RANDOM
            TEMPO,        // TEMPO
            END_OF_LINE,
            END_OF_FILE
        };
        
        Type type;
        std::string text;
        float numValue;
        int line;
    };
    
    // Lexer
    std::vector<Token> tokenize(const std::string& source, std::string& error);
    
    // Parser/code generator
    bool parseStatement(std::vector<Token>& tokens, size_t& pos,
                       VoiceScriptBytecode& bytecode, std::string& error);
    bool parseVoiceCommand(std::vector<Token>& tokens, size_t& pos,
                          VoiceScriptBytecode& bytecode, std::string& error);
    bool parseForLoop(std::vector<Token>& tokens, size_t& pos,
                     VoiceScriptBytecode& bytecode, std::string& error);
    bool parseWait(std::vector<Token>& tokens, size_t& pos,
                  VoiceScriptBytecode& bytecode, std::string& error);
    bool parseWaitRandom(std::vector<Token>& tokens, size_t& pos,
                        VoiceScriptBytecode& bytecode, std::string& error);
    bool parseTempo(std::vector<Token>& tokens, size_t& pos,
                   VoiceScriptBytecode& bytecode, std::string& error);
    
    // Parse number with optional postfix operations (dividedBy, multiplyBy)
    bool parseNumberWithOps(std::vector<Token>& tokens, size_t& pos,
                           float& result, std::string& error, int line);
    
    // Expression parser (simple, supports: number, var, +, -, *, /)
    bool parseExpression(std::vector<Token>& tokens, size_t& pos,
                        VoiceScriptBytecode& bytecode, std::string& error);
    
    // Bytecode emission helpers
    void emitU8(VoiceScriptBytecode& bytecode, uint8_t value);
    void emitU16(VoiceScriptBytecode& bytecode, uint16_t value);
    void emitF32(VoiceScriptBytecode& bytecode, float value);
    
    // Loop tracking
    struct LoopInfo {
        uint8_t loopId;
        size_t startPos;
        std::string varName;
    };
    std::vector<LoopInfo> m_loopStack;
    uint8_t m_nextLoopId;
    
    // Named constants (WAVE_SINE, MODEL_GLASS, etc.)
    std::unordered_map<std::string, float> m_constants;
};

// =============================================================================
// Interpreter
// =============================================================================

/// Interprets and executes voice script bytecode
class VoiceScriptInterpreter {
public:
    VoiceScriptInterpreter(VoiceController* voiceController);
    ~VoiceScriptInterpreter();
    
    /// Start executing a script
    /// @param bytecode Compiled bytecode to execute
    /// @param bpm Tempo in beats per minute (default: 120)
    void start(const VoiceScriptBytecode* bytecode, float bpm = 120.0f);
    
    /// Stop current execution
    void stop();
    
    /// Update interpreter (call with delta time)
    /// @param deltaTime Time elapsed since last update in seconds
    /// @return true if script is still running, false if finished
    bool update(float deltaTime);
    
    /// Set tempo (BPM)
    /// @param bpm Beats per minute
    void setTempo(float bpm);
    
    /// Get current tempo
    /// @return Beats per minute
    float getTempo() const { return m_beatsPerSecond * 60.0f; }
    
    /// Check if interpreter is currently running a script
    /// @return true if running
    bool isRunning() const { return m_running; }
    
    /// Get current script name
    /// @return Script name or empty if not running
    std::string getCurrentScriptName() const;
    
private:
    // Execution state
    const VoiceScriptBytecode* m_bytecode;
    VoiceController* m_voiceController;
    size_t m_pc;                    // Program counter
    bool m_running;
    float m_waitBeats;              // Beats to wait
    float m_beatsPerSecond;         // BPM / 60
    float m_beatAccumulator;        // Accumulated time for beat counting
    
    // Loop state
    struct LoopState {
        uint8_t loopId;
        float current;
        float end;
        float step;
        size_t startPC;
    };
    std::vector<LoopState> m_loopStack;
    
    // Expression evaluation stack
    std::vector<float> m_stack;
    
    // Variables (loop counters)
    std::unordered_map<uint8_t, float> m_variables;
    
    // Bytecode reading helpers
    uint8_t readU8();
    uint16_t readU16();
    float readF32();
    
    // Instruction execution
    bool executeInstruction();
    
    // Stack operations
    void push(float value);
    float pop();
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_VOICE_SCRIPT_H