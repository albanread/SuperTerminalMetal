//
//  VoiceScript.cpp
//  SuperTerminal Framework - Voice Script System
//
//  Implementation of bytecode compiler and interpreter for voice scripts
//
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "VoiceScript.h"
#include "VoiceController.h"
#include <sstream>
#include <cctype>
#include <cstring>
#include <algorithm>
#include <fstream>

namespace SuperTerminal {

// Debug logging to file
static std::ofstream g_voiceScriptLog;
static bool g_logInitialized = false;
static bool g_debugLoggingEnabled = true;  // Default: enabled

static void initVoiceScriptLog() {
    if (!g_logInitialized) {
        g_voiceScriptLog.open("/tmp/voicescript_debug.log", std::ios::out | std::ios::trunc);
        if (g_voiceScriptLog.is_open()) {
            g_voiceScriptLog << "=== VoiceScript Debug Log ===" << std::endl;
            g_voiceScriptLog.flush();
        }
        g_logInitialized = true;
    }
}

static void logVoiceScript(const std::string& message) {
    if (!g_debugLoggingEnabled) return;  // Check flag first
    if (!g_logInitialized) {
        initVoiceScriptLog();
    }
    if (g_voiceScriptLog.is_open()) {
        g_voiceScriptLog << message << std::endl;
        g_voiceScriptLog.flush();
    }
}

// =============================================================================
// VoiceScriptCompiler Implementation
// =============================================================================

VoiceScriptCompiler::VoiceScriptCompiler()
    : m_nextLoopId(0)
{
    // Initialize named constants map
    // Waveform types
    m_constants["WAVE_SILENCE"] = 0;
    m_constants["WAVE_SINE"] = 1;
    m_constants["WAVE_SQUARE"] = 2;
    m_constants["WAVE_SAWTOOTH"] = 3;
    m_constants["WAVE_TRIANGLE"] = 4;
    m_constants["WAVE_NOISE"] = 5;
    m_constants["WAVE_PULSE"] = 6;
    m_constants["WAVE_PHYSICAL"] = 7;
    
    // Physical model types
    m_constants["MODEL_STRING"] = 0;
    m_constants["MODEL_BAR"] = 1;
    m_constants["MODEL_BELL"] = 1;  // Alias
    m_constants["MODEL_TUBE"] = 2;
    m_constants["MODEL_FLUTE"] = 2;  // Alias
    m_constants["MODEL_DRUM"] = 3;
    m_constants["MODEL_GLASS"] = 4;
    
    // Filter types
    m_constants["FILTER_NONE"] = 0;
    m_constants["FILTER_LOWPASS"] = 1;
    m_constants["FILTER_HIGHPASS"] = 2;
    m_constants["FILTER_BANDPASS"] = 3;
    
    // LFO waveforms
    m_constants["LFO_SINE"] = 0;
    m_constants["LFO_SQUARE"] = 1;
    m_constants["LFO_SAW"] = 2;
    m_constants["LFO_TRIANGLE"] = 3;
    m_constants["LFO_RANDOM"] = 4;
}

VoiceScriptCompiler::~VoiceScriptCompiler() {
}

std::unique_ptr<VoiceScriptBytecode> VoiceScriptCompiler::compile(
    const std::string& source,
    const std::string& name,
    std::string& error)
{
    error.clear();
    m_loopStack.clear();
    m_nextLoopId = 0;

    // Tokenize
    std::vector<Token> tokens = tokenize(source, error);
    if (!error.empty()) {
        return nullptr;
    }

    // Create bytecode
    auto bytecode = std::make_unique<VoiceScriptBytecode>();
    bytecode->name = name;

    // Parse and generate code
    size_t pos = 0;
    while (pos < tokens.size() && tokens[pos].type != Token::END_OF_FILE) {
        if (tokens[pos].type == Token::END_OF_LINE) {
            pos++;
            continue;
        }

        if (!parseStatement(tokens, pos, *bytecode, error)) {
            return nullptr;
        }
    }

    // Check for unclosed loops
    if (!m_loopStack.empty()) {
        error = "Unclosed FOR loop";
        return nullptr;
    }

    // Emit END instruction
    emitU8(*bytecode, static_cast<uint8_t>(VoiceOpCode::END));

    return bytecode;
}

std::vector<VoiceScriptCompiler::Token> VoiceScriptCompiler::tokenize(
    const std::string& source,
    std::string& error)
{
    std::vector<Token> tokens;
    size_t pos = 0;
    int line = 1;

    while (pos < source.length()) {
        char ch = source[pos];

        // Skip whitespace (except newlines)
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            pos++;
            continue;
        }

        // Newline
        if (ch == '\n') {
            tokens.push_back({Token::END_OF_LINE, "\n", 0.0f, line});
            line++;
            pos++;
            continue;
        }

        // Comments (REM or ')
        if (ch == '\'') {
            // Skip to end of line
            while (pos < source.length() && source[pos] != '\n') {
                pos++;
            }
            continue;
        }
        
        // Check for REM with word boundary
        if (pos + 3 <= source.length() && source.substr(pos, 3) == "REM") {
            // Ensure it's followed by whitespace or newline (word boundary)
            if (pos + 3 >= source.length() || 
                source[pos + 3] == ' ' || source[pos + 3] == '\t' || 
                source[pos + 3] == '\n' || source[pos + 3] == '\r') {
                // Skip to end of line
                while (pos < source.length() && source[pos] != '\n') {
                    pos++;
                }
                continue;
            }
        }

        // Comma
        if (ch == ',') {
            tokens.push_back({Token::COMMA, ",", 0.0f, line});
            pos++;
            continue;
        }

        // Equals sign
        if (ch == '=') {
            tokens.push_back({Token::COMMAND, "=", 0.0f, line});
            pos++;
            continue;
        }

        // Numbers
        if (std::isdigit(ch) || (ch == '-' && pos + 1 < source.length() && std::isdigit(source[pos + 1]))) {
            size_t start = pos;
            if (ch == '-') pos++;
            while (pos < source.length() && (std::isdigit(source[pos]) || source[pos] == '.')) {
                pos++;
            }
            std::string numStr = source.substr(start, pos - start);
            float value = std::stof(numStr);
            tokens.push_back({Token::NUMBER, numStr, value, line});
            continue;
        }

        // Identifiers/Commands
        if (std::isalpha(ch) || ch == '_') {
            size_t start = pos;
            while (pos < source.length() && (std::isalnum(source[pos]) || source[pos] == '_')) {
                pos++;
            }
            std::string text = source.substr(start, pos - start);

            // Convert to uppercase for comparison
            std::string upper = text;
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

            // Check for keywords
            if (upper == "FOR") {
                tokens.push_back({Token::FOR, text, 0.0f, line});
            } else if (upper == "TO") {
                tokens.push_back({Token::TO, text, 0.0f, line});
            } else if (upper == "STEP") {
                tokens.push_back({Token::STEP, text, 0.0f, line});
            } else if (upper == "NEXT") {
                tokens.push_back({Token::NEXT, text, 0.0f, line});
            } else if (upper == "WAIT_RANDOM") {
                tokens.push_back({Token::WAIT_RANDOM, text, 0.0f, line});
            } else if (upper == "WAIT") {
                tokens.push_back({Token::WAIT, text, 0.0f, line});
            } else if (upper == "TEMPO") {
                tokens.push_back({Token::TEMPO, text, 0.0f, line});
            } else {
                // Check if this is a named constant
                auto it = m_constants.find(upper);
                if (it != m_constants.end()) {
                    // It's a constant - convert to NUMBER token
                    tokens.push_back({Token::NUMBER, text, it->second, line});
                } else {
                    tokens.push_back({Token::COMMAND, text, 0.0f, line});
                }
            }
            continue;
        }

        // Unknown character
        char errorMsg[256];
        snprintf(errorMsg, sizeof(errorMsg), "Unexpected character '%c' (0x%02X) at line %d, position %zu", 
                ch, (unsigned char)ch, line, pos);
        error = errorMsg;
        return {};
    }

    tokens.push_back({Token::END_OF_FILE, "", 0.0f, line});
    return tokens;
}

bool VoiceScriptCompiler::parseStatement(
    std::vector<Token>& tokens,
    size_t& pos,
    VoiceScriptBytecode& bytecode,
    std::string& error)
{
    if (pos >= tokens.size()) {
        return true;
    }

    Token& tok = tokens[pos];

    // FOR loop
    if (tok.type == Token::FOR) {
        return parseForLoop(tokens, pos, bytecode, error);
    }

    // NEXT
    if (tok.type == Token::NEXT) {
        if (m_loopStack.empty()) {
            error = "NEXT without FOR";
            return false;
        }

        LoopInfo loop = m_loopStack.back();
        m_loopStack.pop_back();

        // Emit LOOP_NEXT
        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::LOOP_NEXT));
        emitU8(bytecode, loop.loopId);

        pos++;
        return true;
    }

    // WAIT
    if (tok.type == Token::WAIT) {
        return parseWait(tokens, pos, bytecode, error);
    }

    // WAIT_RANDOM
    if (tok.type == Token::WAIT_RANDOM) {
        return parseWaitRandom(tokens, pos, bytecode, error);
    }

    // TEMPO
    if (tok.type == Token::TEMPO) {
        return parseTempo(tokens, pos, bytecode, error);
    }

    // Voice command
    if (tok.type == Token::COMMAND) {
        return parseVoiceCommand(tokens, pos, bytecode, error);
    }

    error = "Unexpected token at line " + std::to_string(tok.line);
    return false;
}

bool VoiceScriptCompiler::parseVoiceCommand(
    std::vector<Token>& tokens,
    size_t& pos,
    VoiceScriptBytecode& bytecode,
    std::string& error)
{
    Token& cmd = tokens[pos];
    std::string upper = cmd.text;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    pos++;

    // Helper to emit a value (either constant, variable, or function call)
    auto emitValue = [&](VoiceScriptBytecode& bc) -> bool {
        if (pos >= tokens.size()) {
            error = "Expected value at line " + std::to_string(cmd.line);
            return false;
        }
        
        if (tokens[pos].type == Token::NUMBER) {
            // Direct constant
            emitU8(bc, static_cast<uint8_t>(VoiceOpCode::PUSH_CONST));
            emitF32(bc, tokens[pos].numValue);
            pos++;
            return true;
        } else if (tokens[pos].type == Token::COMMAND) {
            std::string cmdName = tokens[pos].text;
            std::string upperCmd = cmdName;
            std::transform(upperCmd.begin(), upperCmd.end(), upperCmd.begin(), ::toupper);
            
            // Check for RANDOM function
            if (upperCmd == "RANDOM") {
                pos++; // Skip RANDOM
                // Expect (min, max)
                // Note: We don't have full expression parser, so just expect two numbers
                if (pos >= tokens.size() || tokens[pos].type != Token::NUMBER) {
                    error = "RANDOM expects min value at line " + std::to_string(cmd.line);
                    return false;
                }
                float minVal = tokens[pos++].numValue;
                
                if (pos >= tokens.size() || tokens[pos].type != Token::COMMA) {
                    error = "RANDOM expects comma at line " + std::to_string(cmd.line);
                    return false;
                }
                pos++; // Skip comma
                
                if (pos >= tokens.size() || tokens[pos].type != Token::NUMBER) {
                    error = "RANDOM expects max value at line " + std::to_string(cmd.line);
                    return false;
                }
                float maxVal = tokens[pos++].numValue;
                
                emitU8(bc, static_cast<uint8_t>(VoiceOpCode::PUSH_RANDOM));
                emitF32(bc, minVal);
                emitF32(bc, maxVal);
                return true;
            }
            
            // Variable reference
            std::string varName = cmdName;
            // Find variable ID from loop stack
            uint8_t varId = 255; // Invalid
            for (const auto& loop : m_loopStack) {
                if (loop.varName == varName) {
                    varId = loop.loopId;
                    break;
                }
            }
            if (varId == 255) {
                error = "Undefined variable '" + varName + "' at line " + std::to_string(cmd.line);
                return false;
            }
            emitU8(bc, static_cast<uint8_t>(VoiceOpCode::PUSH_VAR));
            emitU8(bc, varId);
            pos++;
            return true;
        } else {
            error = "Expected number, variable, or function at line " + std::to_string(cmd.line);
            return false;
        }
    };

    // Expect parameters
    auto expectNumber = [&]() -> bool {
        if (pos >= tokens.size() || (tokens[pos].type != Token::NUMBER && tokens[pos].type != Token::COMMAND)) {
            error = "Expected number or variable at line " + std::to_string(cmd.line);
            return false;
        }
        return true;
    };

    auto expectComma = [&]() -> bool {
        if (pos >= tokens.size() || tokens[pos].type != Token::COMMA) {
            error = "Expected comma at line " + std::to_string(cmd.line);
            return false;
        }
        pos++;
        return true;
    };

    // VOICE_WAVEFORM voice, waveform
    if (upper == "VOICE_WAVEFORM") {
        if (!expectNumber()) return false;
        
        // Voice is always a constant for now
        if (tokens[pos].type != Token::NUMBER) {
            error = "Voice number must be a constant at line " + std::to_string(cmd.line);
            return false;
        }
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        
        // Waveform can be constant or variable
        bool isConst = (tokens[pos].type == Token::NUMBER);
        uint8_t waveform = 0;
        if (isConst) {
            waveform = static_cast<uint8_t>(tokens[pos++].numValue);
        }
        
        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_WAVEFORM));
        emitU8(bytecode, voice);
        
        if (isConst) {
            emitU8(bytecode, waveform);
        } else {
            // Emit expression that will push value to stack
            if (!emitValue(bytecode)) return false;
        }
        return true;
    }

    // VOICE_FREQUENCY voice, hz
    if (upper == "VOICE_FREQUENCY") {
        if (!expectNumber()) return false;
        
        // Voice is always a constant
        if (tokens[pos].type != Token::NUMBER) {
            error = "Voice number must be a constant at line " + std::to_string(cmd.line);
            return false;
        }
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        
        // Frequency can be constant or variable
        bool isConst = (tokens[pos].type == Token::NUMBER);
        float hz = 0.0f;
        if (isConst) {
            if (!parseNumberWithOps(tokens, pos, hz, error, cmd.line)) return false;
        } else {
            if (!expectNumber()) return false;
        }
        
        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_FREQUENCY));
        emitU8(bytecode, voice);
        
        if (isConst) {
            emitF32(bytecode, hz);
        } else {
            // Emit expression that will push value to stack
            if (!emitValue(bytecode)) return false;
        }
        return true;
    }

    // VOICE_NOTE voice, note
    if (upper == "VOICE_NOTE") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        uint8_t note = static_cast<uint8_t>(tokens[pos++].numValue);

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_NOTE));
        emitU8(bytecode, voice);
        emitU8(bytecode, note);
        return true;
    }

    // VOICE_ENVELOPE voice, a, d, s, r
    if (upper == "VOICE_ENVELOPE") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        float attack, decay, sustain, release;
        if (!parseNumberWithOps(tokens, pos, attack, error, cmd.line)) return false;
        if (!expectComma()) return false;
        if (!parseNumberWithOps(tokens, pos, decay, error, cmd.line)) return false;
        if (!expectComma()) return false;
        if (!parseNumberWithOps(tokens, pos, sustain, error, cmd.line)) return false;
        if (!expectComma()) return false;
        if (!parseNumberWithOps(tokens, pos, release, error, cmd.line)) return false;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_ENVELOPE));
        emitU8(bytecode, voice);
        emitF32(bytecode, attack);
        emitF32(bytecode, decay);
        emitF32(bytecode, sustain);
        emitF32(bytecode, release);
        return true;
    }

    // VOICE_GATE voice, state
    if (upper == "VOICE_GATE") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        uint8_t state = static_cast<uint8_t>(tokens[pos++].numValue);

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_GATE));
        emitU8(bytecode, voice);
        emitU8(bytecode, state);
        return true;
    }

    // VOICE_VOLUME voice, volume
    if (upper == "VOICE_VOLUME") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        float volume;
        if (!parseNumberWithOps(tokens, pos, volume, error, cmd.line)) return false;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_VOLUME));
        emitU8(bytecode, voice);
        emitF32(bytecode, volume);
        return true;
    }

    // VOICE_PULSE_WIDTH voice, width
    if (upper == "VOICE_PULSE_WIDTH") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        float width;
        if (!parseNumberWithOps(tokens, pos, width, error, cmd.line)) return false;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_PULSE_WIDTH));
        emitU8(bytecode, voice);
        emitF32(bytecode, width);
        return true;
    }

    // VOICE_PAN voice, pan
    if (upper == "VOICE_PAN") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        float pan;
        if (!parseNumberWithOps(tokens, pos, pan, error, cmd.line)) return false;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_PAN));
        emitU8(bytecode, voice);
        emitF32(bytecode, pan);
        return true;
    }

    // VOICE_DELAY_ENABLE voice, enabled
    if (upper == "VOICE_DELAY_ENABLE") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        uint8_t enabled = static_cast<uint8_t>(tokens[pos++].numValue);

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_DELAY_ENABLE));
        emitU8(bytecode, voice);
        emitU8(bytecode, enabled);
        return true;
    }

    // VOICE_DELAY_TIME voice, time
    if (upper == "VOICE_DELAY_TIME") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        float time;
        if (!parseNumberWithOps(tokens, pos, time, error, cmd.line)) return false;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_DELAY_TIME));
        emitU8(bytecode, voice);
        emitF32(bytecode, time);
        return true;
    }

    // VOICE_DELAY_FEEDBACK voice, feedback
    if (upper == "VOICE_DELAY_FEEDBACK") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        float feedback;
        if (!parseNumberWithOps(tokens, pos, feedback, error, cmd.line)) return false;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_DELAY_FEEDBACK));
        emitU8(bytecode, voice);
        emitF32(bytecode, feedback);
        return true;
    }

    // VOICE_DELAY_MIX voice, mix
    if (upper == "VOICE_DELAY_MIX") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        float mix;
        if (!parseNumberWithOps(tokens, pos, mix, error, cmd.line)) return false;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_DELAY_MIX));
        emitU8(bytecode, voice);
        emitF32(bytecode, mix);
        return true;
    }

    // VOICE_FILTER_TYPE type
    if (upper == "VOICE_FILTER_TYPE") {
        if (!expectNumber()) return false;
        uint8_t type = static_cast<uint8_t>(tokens[pos++].numValue);

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_FILTER_TYPE));
        emitU8(bytecode, type);
        return true;
    }

    // VOICE_FILTER_CUTOFF hz
    if (upper == "VOICE_FILTER_CUTOFF") {
        float hz;
        if (!parseNumberWithOps(tokens, pos, hz, error, cmd.line)) return false;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_FILTER_CUTOFF));
        emitF32(bytecode, hz);
        return true;
    }

    // VOICE_FILTER_RESONANCE q
    if (upper == "VOICE_FILTER_RESONANCE") {
        float q;
        if (!parseNumberWithOps(tokens, pos, q, error, cmd.line)) return false;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_FILTER_RESONANCE));
        emitF32(bytecode, q);
        return true;
    }

    // VOICE_FILTER_ENABLE state
    if (upper == "VOICE_FILTER_ENABLE") {
        if (!expectNumber()) return false;
        uint8_t state = static_cast<uint8_t>(tokens[pos++].numValue);

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_FILTER_ENABLE));
        emitU8(bytecode, state);
        return true;
    }

    // VOICE_FILTER_ROUTE voice, enabled
    if (upper == "VOICE_FILTER_ROUTE") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        uint8_t enabled = static_cast<uint8_t>(tokens[pos++].numValue);

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_FILTER_ROUTE));
        emitU8(bytecode, voice);
        emitU8(bytecode, enabled);
        return true;
    }

    // VOICE_RING_MOD voice, source_voice
    if (upper == "VOICE_RING_MOD") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        uint8_t sourceVoice = static_cast<uint8_t>(tokens[pos++].numValue);

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_RING_MOD));
        emitU8(bytecode, voice);
        emitU8(bytecode, sourceVoice);
        return true;
    }

    // VOICE_SYNC voice, source_voice
    if (upper == "VOICE_SYNC") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        uint8_t sourceVoice = static_cast<uint8_t>(tokens[pos++].numValue);

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_SYNC));
        emitU8(bytecode, voice);
        emitU8(bytecode, sourceVoice);
        return true;
    }

    // VOICE_TEST voice, state
    if (upper == "VOICE_TEST") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        uint8_t state = static_cast<uint8_t>(tokens[pos++].numValue);

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_TEST_BIT));
        emitU8(bytecode, voice);
        emitU8(bytecode, state);
        return true;
    }

    // VOICE_WAVEFORM_COMBO voice, waveform1, waveform2
    if (upper == "VOICE_WAVEFORM_COMBO") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        uint8_t waveform1 = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        uint8_t waveform2 = static_cast<uint8_t>(tokens[pos++].numValue);

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_WAVEFORM_COMBO));
        emitU8(bytecode, voice);
        emitU8(bytecode, waveform1);
        emitU8(bytecode, waveform2);
        return true;
    }

    // VOICE_PORTAMENTO voice, time_seconds
    if (upper == "VOICE_PORTAMENTO") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        float timeSeconds = tokens[pos++].numValue;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_PORTAMENTO));
        emitU8(bytecode, voice);
        emitF32(bytecode, timeSeconds);
        return true;
    }

    // VOICE_DETUNE voice, cents
    if (upper == "VOICE_DETUNE") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        float cents = tokens[pos++].numValue;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_DETUNE));
        emitU8(bytecode, voice);
        emitF32(bytecode, cents);
        return true;
    }

    // VOICE_NOTE_PLAY voice, note, duration
    if (upper == "VOICE_NOTE_PLAY") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        uint8_t note = static_cast<uint8_t>(tokens[pos++].numValue);
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        float duration = tokens[pos++].numValue;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::PLAY_NOTE));
        emitU8(bytecode, voice);
        emitU8(bytecode, note);
        emitF32(bytecode, duration);
        return true;
    }

    // MASTER_VOLUME level
    if (upper == "MASTER_VOLUME") {
        if (!expectNumber()) return false;
        float volume = tokens[pos++].numValue;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_MASTER_VOLUME));
        emitF32(bytecode, volume);
        return true;
    }

    // VOICESCRIPT_DEBUG 0|1
    if (upper == "VOICESCRIPT_DEBUG") {
        if (!expectNumber()) return false;
        uint8_t enabled = static_cast<uint8_t>(tokens[pos++].numValue);

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::VOICESCRIPT_DEBUG));
        emitU8(bytecode, enabled);
        return true;
    }

    // LFO_WAVEFORM lfo_num, waveform
    if (upper == "LFO_WAVEFORM") {
        if (!expectNumber()) return false;
        uint8_t lfoNum = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        uint8_t waveform = static_cast<uint8_t>(tokens[pos++].numValue);

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::LFO_WAVEFORM));
        emitU8(bytecode, lfoNum);
        emitU8(bytecode, waveform);
        return true;
    }

    // LFO_RATE lfo_num, rate_hz
    if (upper == "LFO_RATE") {
        if (!expectNumber()) return false;
        uint8_t lfoNum = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        float rateHz = tokens[pos++].numValue;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::LFO_RATE));
        emitU8(bytecode, lfoNum);
        emitF32(bytecode, rateHz);
        return true;
    }

    // LFO_RESET lfo_num
    if (upper == "LFO_RESET") {
        if (!expectNumber()) return false;
        uint8_t lfoNum = static_cast<uint8_t>(tokens[pos++].numValue);

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::LFO_RESET));
        emitU8(bytecode, lfoNum);
        return true;
    }

    // LFO_TO_PITCH voice, lfo_num, depth_cents
    if (upper == "LFO_TO_PITCH") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        uint8_t lfoNum = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        float depthCents = tokens[pos++].numValue;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::LFO_TO_PITCH));
        emitU8(bytecode, voice);
        emitU8(bytecode, lfoNum);
        emitF32(bytecode, depthCents);
        return true;
    }

    // LFO_TO_VOLUME voice, lfo_num, depth
    if (upper == "LFO_TO_VOLUME") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        uint8_t lfoNum = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        float depth = tokens[pos++].numValue;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::LFO_TO_VOLUME));
        emitU8(bytecode, voice);
        emitU8(bytecode, lfoNum);
        emitF32(bytecode, depth);
        return true;
    }

    // LFO_TO_FILTER voice, lfo_num, depth_hz
    if (upper == "LFO_TO_FILTER") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        uint8_t lfoNum = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        float depthHz = tokens[pos++].numValue;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::LFO_TO_FILTER));
        emitU8(bytecode, voice);
        emitU8(bytecode, lfoNum);
        emitF32(bytecode, depthHz);
        return true;
    }

    // LFO_TO_PULSEWIDTH voice, lfo_num, depth
    if (upper == "LFO_TO_PULSEWIDTH") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        uint8_t lfoNum = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        float depth = tokens[pos++].numValue;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::LFO_TO_PULSEWIDTH));
        emitU8(bytecode, voice);
        emitU8(bytecode, lfoNum);
        emitF32(bytecode, depth);
        return true;
    }

    // VOICE_PHYSICAL_MODEL voice, model_type
    if (upper == "VOICE_PHYSICAL_MODEL") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        uint8_t modelType = static_cast<uint8_t>(tokens[pos++].numValue);

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_PHYSICAL_MODEL));
        emitU8(bytecode, voice);
        emitU8(bytecode, modelType);
        return true;
    }

    // VOICE_PHYSICAL_DAMPING voice, damping
    if (upper == "VOICE_PHYSICAL_DAMPING") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        float damping = tokens[pos++].numValue;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_PHYSICAL_DAMPING));
        emitU8(bytecode, voice);
        emitF32(bytecode, damping);
        return true;
    }

    // VOICE_PHYSICAL_BRIGHTNESS voice, brightness
    if (upper == "VOICE_PHYSICAL_BRIGHTNESS") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        float brightness = tokens[pos++].numValue;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_PHYSICAL_BRIGHTNESS));
        emitU8(bytecode, voice);
        emitF32(bytecode, brightness);
        return true;
    }

    // VOICE_PHYSICAL_EXCITATION voice, excitation
    if (upper == "VOICE_PHYSICAL_EXCITATION") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        float excitation = tokens[pos++].numValue;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_PHYSICAL_EXCITATION));
        emitU8(bytecode, voice);
        emitF32(bytecode, excitation);
        return true;
    }

    // VOICE_PHYSICAL_RESONANCE voice, resonance
    if (upper == "VOICE_PHYSICAL_RESONANCE") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        float resonance = tokens[pos++].numValue;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_PHYSICAL_RESONANCE));
        emitU8(bytecode, voice);
        emitF32(bytecode, resonance);
        return true;
    }

    // VOICE_PHYSICAL_TENSION voice, tension
    if (upper == "VOICE_PHYSICAL_TENSION") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        float tension = tokens[pos++].numValue;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_PHYSICAL_TENSION));
        emitU8(bytecode, voice);
        emitF32(bytecode, tension);
        return true;
    }

    // VOICE_PHYSICAL_PRESSURE voice, pressure
    if (upper == "VOICE_PHYSICAL_PRESSURE") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);
        
        if (!expectComma()) return false;
        if (!expectNumber()) return false;
        float pressure = tokens[pos++].numValue;

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::SET_PHYSICAL_PRESSURE));
        emitU8(bytecode, voice);
        emitF32(bytecode, pressure);
        return true;
    }

    // VOICE_PHYSICAL_TRIGGER voice
    if (upper == "VOICE_PHYSICAL_TRIGGER") {
        if (!expectNumber()) return false;
        uint8_t voice = static_cast<uint8_t>(tokens[pos++].numValue);

        emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::PHYSICAL_TRIGGER));
        emitU8(bytecode, voice);
        return true;
    }

    error = "Unknown command '" + cmd.text + "' at line " + std::to_string(cmd.line);
    return false;
}

bool VoiceScriptCompiler::parseForLoop(
    std::vector<Token>& tokens,
    size_t& pos,
    VoiceScriptBytecode& bytecode,
    std::string& error)
{
    // FOR var = start TO end STEP step
    Token& forTok = tokens[pos++];

    if (pos >= tokens.size() || tokens[pos].type != Token::COMMAND) {
        error = "Expected variable name after FOR";
        return false;
    }
    std::string varName = tokens[pos++].text;

    // Skip '=' if present (optional)
    if (pos < tokens.size() && tokens[pos].type == Token::COMMAND && tokens[pos].text == "=") {
        pos++;
    }

    // Start value
    if (pos >= tokens.size() || tokens[pos].type != Token::NUMBER) {
        error = "Expected start value in FOR loop";
        return false;
    }
    float start = tokens[pos++].numValue;

    // TO
    if (pos >= tokens.size() || tokens[pos].type != Token::TO) {
        error = "Expected TO in FOR loop";
        return false;
    }
    pos++;

    // End value
    if (pos >= tokens.size() || tokens[pos].type != Token::NUMBER) {
        error = "Expected end value in FOR loop";
        return false;
    }
    float end = tokens[pos++].numValue;

    // STEP (optional, default 1)
    float step = 1.0f;
    if (pos < tokens.size() && tokens[pos].type == Token::STEP) {
        pos++;
        if (pos >= tokens.size() || tokens[pos].type != Token::NUMBER) {
            error = "Expected step value after STEP";
            return false;
        }
        step = tokens[pos++].numValue;
    }

    // Emit LOOP_START
    uint8_t loopId = m_nextLoopId++;
    emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::LOOP_START));
    emitU8(bytecode, loopId);
    emitF32(bytecode, start);
    emitF32(bytecode, end);
    emitF32(bytecode, step);

    // Track loop for NEXT
    LoopInfo info;
    info.loopId = loopId;
    info.startPos = bytecode.code.size();
    info.varName = varName;
    m_loopStack.push_back(info);

    return true;
}

bool VoiceScriptCompiler::parseWait(
    std::vector<Token>& tokens,
    size_t& pos,
    VoiceScriptBytecode& bytecode,
    std::string& error)
{
    int line = tokens[pos].line;
    pos++; // Skip WAIT

    float beats;
    if (!parseNumberWithOps(tokens, pos, beats, error, line)) {
        return false;
    }

    emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::WAIT));
    emitF32(bytecode, beats);

    bytecode.estimatedBeats += beats;

    return true;
}

bool VoiceScriptCompiler::parseWaitRandom(
    std::vector<Token>& tokens,
    size_t& pos,
    VoiceScriptBytecode& bytecode,
    std::string& error)
{
    Token& waitTok = tokens[pos];
    pos++;

    // Expect two numbers: min and max
    if (pos >= tokens.size() || tokens[pos].type != Token::NUMBER) {
        error = "WAIT_RANDOM expects min value at line " + std::to_string(waitTok.line);
        return false;
    }
    float minBeats = tokens[pos++].numValue;

    if (pos >= tokens.size() || tokens[pos].type != Token::COMMA) {
        error = "WAIT_RANDOM expects comma at line " + std::to_string(waitTok.line);
        return false;
    }
    pos++; // Skip comma

    if (pos >= tokens.size() || tokens[pos].type != Token::NUMBER) {
        error = "WAIT_RANDOM expects max value at line " + std::to_string(waitTok.line);
        return false;
    }
    float maxBeats = tokens[pos++].numValue;

    emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::WAIT_RANDOM));
    emitF32(bytecode, minBeats);
    emitF32(bytecode, maxBeats);

    return true;
}

bool VoiceScriptCompiler::parseTempo(
    std::vector<Token>& tokens,
    size_t& pos,
    VoiceScriptBytecode& bytecode,
    std::string& error)
{
    pos++; // Skip TEMPO

    if (pos >= tokens.size() || tokens[pos].type != Token::NUMBER) {
        error = "Expected number after TEMPO";
        return false;
    }

    float bpm = tokens[pos++].numValue;

    emitU8(bytecode, static_cast<uint8_t>(VoiceOpCode::TEMPO));
    emitF32(bytecode, bpm);

    return true;
}

bool VoiceScriptCompiler::parseNumberWithOps(
    std::vector<Token>& tokens,
    size_t& pos,
    float& result,
    std::string& error,
    int line)
{
    if (pos >= tokens.size() || tokens[pos].type != Token::NUMBER) {
        error = "Expected number at line " + std::to_string(line);
        return false;
    }
    
    result = tokens[pos++].numValue;
    
    // Check for postfix operations
    if (pos < tokens.size() && tokens[pos].type == Token::COMMAND) {
        std::string op = tokens[pos].text;
        std::transform(op.begin(), op.end(), op.begin(), ::toupper);
        
        if (op == "DIVIDEDBY") {
            pos++; // Skip DIVIDEDBY
            if (pos >= tokens.size() || tokens[pos].type != Token::NUMBER) {
                error = "Expected number after DIVIDEDBY at line " + std::to_string(line);
                return false;
            }
            float divisor = tokens[pos++].numValue;
            if (divisor == 0.0f) {
                error = "Division by zero at line " + std::to_string(line);
                return false;
            }
            result /= divisor;
        } else if (op == "MULTIPLYBY") {
            pos++; // Skip MULTIPLYBY
            if (pos >= tokens.size() || tokens[pos].type != Token::NUMBER) {
                error = "Expected number after MULTIPLYBY at line " + std::to_string(line);
                return false;
            }
            float multiplier = tokens[pos++].numValue;
            result *= multiplier;
        }
    }
    
    return true;
}

void VoiceScriptCompiler::emitU8(VoiceScriptBytecode& bytecode, uint8_t value) {
    bytecode.code.push_back(value);
}

void VoiceScriptCompiler::emitU16(VoiceScriptBytecode& bytecode, uint16_t value) {
    bytecode.code.push_back(static_cast<uint8_t>(value & 0xFF));
    bytecode.code.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}


void VoiceScriptCompiler::emitF32(VoiceScriptBytecode& bytecode, float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(float));
    bytecode.code.push_back(static_cast<uint8_t>(bits & 0xFF));
    bytecode.code.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
    bytecode.code.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
    bytecode.code.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
}

// =============================================================================
// VoiceScriptInterpreter Implementation
// =============================================================================

VoiceScriptInterpreter::VoiceScriptInterpreter(VoiceController* voiceController)
    : m_bytecode(nullptr)
    , m_voiceController(voiceController)
    , m_pc(0)
    , m_running(false)
    , m_waitBeats(0.0f)
    , m_beatsPerSecond(2.0f)
    , m_beatAccumulator(0.0f)
{
}

VoiceScriptInterpreter::~VoiceScriptInterpreter() {
    stop();
}

void VoiceScriptInterpreter::start(const VoiceScriptBytecode* bytecode, float bpm) {
    if (!bytecode || !m_voiceController) {
        logVoiceScript("Start failed: null bytecode or controller");
        return;
    }

    m_bytecode = bytecode;
    m_pc = 0;
    m_running = true;
    m_waitBeats = 0.0f;
    m_beatAccumulator = 0.0f;
    m_beatsPerSecond = bpm / 60.0f;
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Started script '%s' at %.1f BPM (%.3f beats/sec), estimated %.1f beats",
            bytecode->name.c_str(), bpm, m_beatsPerSecond, bytecode->estimatedBeats);
    logVoiceScript(buf);
    
    printf("[VoiceScript] Starting playback:\n");
    printf("  Script: %s\n", bytecode->name.c_str());
    printf("  BPM: %.1f\n", bpm);
    printf("  Beats/sec: %.3f\n", m_beatsPerSecond);
    printf("  Estimated duration: %.1f beats\n", bytecode->estimatedBeats);
    m_loopStack.clear();
    m_stack.clear();
    m_variables.clear();
}

void VoiceScriptInterpreter::stop() {
    m_running = false;
    m_bytecode = nullptr;
    m_pc = 0;
    m_waitBeats = 0.0f;
    m_beatAccumulator = 0.0f;
    m_loopStack.clear();
    m_stack.clear();
    m_variables.clear();
}

bool VoiceScriptInterpreter::update(float deltaTime) {
    if (!m_running || !m_bytecode || !m_voiceController) {
        logVoiceScript("Update: not running or invalid state");
        return false;
    }

    // Accumulate beats
    m_beatAccumulator += deltaTime * m_beatsPerSecond;

    static int updateCount = 0;
    updateCount++;
    
    // Log every 60 frames (~1 second at 60fps)
    if (updateCount % 60 == 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Update #%d: deltaTime=%.4f, beatAccum=%.3f, waitBeats=%.3f, PC=%zu", 
                updateCount, deltaTime, m_beatAccumulator, m_waitBeats, m_pc);
        logVoiceScript(buf);
    }

    // Handle wait
    if (m_waitBeats > 0.0f) {
        if (m_beatAccumulator >= m_waitBeats) {
            char buf[128];
            snprintf(buf, sizeof(buf), "WAIT complete: waited %.3f beats (accumulator was %.3f)", 
                    m_waitBeats, m_beatAccumulator);
            logVoiceScript(buf);
            m_beatAccumulator -= m_waitBeats;
            m_waitBeats = 0.0f;
        } else {
            // Still waiting
            if (updateCount % 60 == 0) {
                char buf[128];
                snprintf(buf, sizeof(buf), "Still waiting: %.3f of %.3f beats elapsed", 
                        m_beatAccumulator, m_waitBeats);
                logVoiceScript(buf);
            }
            return true;
        }
    }

    // Execute instructions until we hit WAIT or END
    int instructionsExecuted = 0;
    while (m_running && m_waitBeats <= 0.0f) {
        if (!executeInstruction()) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Execution ended at PC=%zu", m_pc);
            logVoiceScript(buf);
            m_running = false;
            return false;
        }
        instructionsExecuted++;
        
        // Safety check - don't execute too many in one frame
        if (instructionsExecuted > 100) {
            char buf[128];
            snprintf(buf, sizeof(buf), "WARNING: Executed %d instructions in one frame! Possible infinite loop at PC=%zu",
                    instructionsExecuted, m_pc);
            logVoiceScript(buf);
            break;
        }
    }

    if (instructionsExecuted > 0 && instructionsExecuted < 100) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Executed %d instructions this frame, now at PC=%zu", 
                instructionsExecuted, m_pc);
        logVoiceScript(buf);
    }

    return m_running;
}

void VoiceScriptInterpreter::setTempo(float bpm) {
    if (bpm > 0.0f) {
        m_beatsPerSecond = bpm / 60.0f;
    }
}

std::string VoiceScriptInterpreter::getCurrentScriptName() const {
    if (m_bytecode) {
        return m_bytecode->name;
    }
    return "";
}

uint8_t VoiceScriptInterpreter::readU8() {
    if (m_pc >= m_bytecode->code.size()) {
        m_running = false;
        return 0;
    }
    return m_bytecode->code[m_pc++];
}

uint16_t VoiceScriptInterpreter::readU16() {
    uint8_t lo = readU8();
    uint8_t hi = readU8();
    return static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8);
}


float VoiceScriptInterpreter::readF32() {
    uint32_t bits = 0;
    bits |= static_cast<uint32_t>(readU8());
    bits |= static_cast<uint32_t>(readU8()) << 8;
    bits |= static_cast<uint32_t>(readU8()) << 16;
    bits |= static_cast<uint32_t>(readU8()) << 24;
    float value;
    std::memcpy(&value, &bits, sizeof(float));
    return value;
}

void VoiceScriptInterpreter::push(float value) {
    m_stack.push_back(value);
}

float VoiceScriptInterpreter::pop() {
    if (m_stack.empty()) {
        return 0.0f;
    }
    float value = m_stack.back();
    m_stack.pop_back();
    return value;
}

bool VoiceScriptInterpreter::executeInstruction() {
    if (m_pc >= m_bytecode->code.size()) {
        m_running = false;
        return false;
    }

    VoiceOpCode opcode = static_cast<VoiceOpCode>(readU8());

    switch (opcode) {
        case VoiceOpCode::SET_WAVEFORM: {
            uint8_t voice = readU8();
            uint8_t waveform = readU8();
            // Check if next byte is PUSH_CONST, PUSH_VAR, or PUSH_RANDOM (waveform from stack)
            if (waveform == static_cast<uint8_t>(VoiceOpCode::PUSH_CONST) || 
                waveform == static_cast<uint8_t>(VoiceOpCode::PUSH_VAR) ||
                waveform == static_cast<uint8_t>(VoiceOpCode::PUSH_RANDOM)) {
                m_pc--; // Back up to re-read the opcode
                if (!executeInstruction()) return false; // Execute push
                waveform = static_cast<uint8_t>(pop());
            }
            m_voiceController->setWaveform(voice, static_cast<VoiceWaveform>(waveform));
            char buf[64];
            snprintf(buf, sizeof(buf), "SET_WAVEFORM: voice=%d, waveform=%d", voice, waveform);
            logVoiceScript(buf);
            break;
        }

        case VoiceOpCode::SET_FREQUENCY: {
            uint8_t voice = readU8();
            // Check if next is an opcode (variable) or a float (constant)
            size_t checkPos = m_pc;
            uint8_t nextByte = readU8();
            m_pc = checkPos; // Reset
            
            float hz = 0.0f;
            if (nextByte == static_cast<uint8_t>(VoiceOpCode::PUSH_CONST) || 
                nextByte == static_cast<uint8_t>(VoiceOpCode::PUSH_VAR) ||
                nextByte == static_cast<uint8_t>(VoiceOpCode::PUSH_RANDOM)) {
                if (!executeInstruction()) return false; // Execute push
                hz = pop();
            } else {
                hz = readF32();
            }
            
            m_voiceController->setFrequency(voice, hz);
            char buf[64];
            snprintf(buf, sizeof(buf), "SET_FREQUENCY: voice=%d, hz=%.1f", voice, hz);
            logVoiceScript(buf);
            break;
        }

        case VoiceOpCode::SET_NOTE: {
            uint8_t voice = readU8();
            uint8_t note = readU8();
            m_voiceController->setNote(voice, note);
            break;
        }

        case VoiceOpCode::SET_ENVELOPE: {
            uint8_t voice = readU8();
            float attack = readF32();
            float decay = readF32();
            float sustain = readF32();
            float release = readF32();
            m_voiceController->setEnvelope(voice, attack, decay, sustain, release);
            char buf[128];
            snprintf(buf, sizeof(buf), "SET_ENVELOPE: voice=%d, ADSR=(%.1f,%.1f,%.2f,%.1f)ms", 
                    voice, attack, decay, sustain, release);
            logVoiceScript(buf);
            break;
        }

        case VoiceOpCode::SET_GATE: {
            uint8_t voice = readU8();
            uint8_t state = readU8();
            m_voiceController->setGate(voice, state != 0);
            char buf[64];
            snprintf(buf, sizeof(buf), "SET_GATE: voice=%d, state=%s", voice, state ? "ON" : "OFF");
            logVoiceScript(buf);
            break;
        }

        case VoiceOpCode::SET_VOLUME: {
            uint8_t voice = readU8();
            float volume = readF32();
            m_voiceController->setVolume(voice, volume);
            char buf[64];
            snprintf(buf, sizeof(buf), "SET_VOLUME: voice=%d, volume=%.2f", voice, volume);
            logVoiceScript(buf);
            break;
        }

        case VoiceOpCode::SET_PULSE_WIDTH: {
            uint8_t voice = readU8();
            float width = readF32();
            m_voiceController->setPulseWidth(voice, width);
            break;
        }

        case VoiceOpCode::SET_PAN: {
            uint8_t voice = readU8();
            float pan = readF32();
            m_voiceController->setPan(voice, pan);
            break;
        }

        case VoiceOpCode::SET_DELAY_ENABLE: {
            uint8_t voice = readU8();
            uint8_t enabled = readU8();
            m_voiceController->setDelayEnabled(voice, enabled != 0);
            break;
        }

        case VoiceOpCode::SET_DELAY_TIME: {
            uint8_t voice = readU8();
            float time = readF32();
            m_voiceController->setDelayTime(voice, time);
            break;
        }

        case VoiceOpCode::SET_DELAY_FEEDBACK: {
            uint8_t voice = readU8();
            float feedback = readF32();
            m_voiceController->setDelayFeedback(voice, feedback);
            break;
        }

        case VoiceOpCode::SET_DELAY_MIX: {
            uint8_t voice = readU8();
            float mix = readF32();
            m_voiceController->setDelayMix(voice, mix);
            break;
        }

        case VoiceOpCode::SET_FILTER_TYPE: {
            uint8_t type = readU8();
            m_voiceController->setFilterType(static_cast<VoiceFilterType>(type));
            break;
        }

        case VoiceOpCode::SET_FILTER_CUTOFF: {
            float hz = readF32();
            m_voiceController->setFilterCutoff(hz);
            break;
        }

        case VoiceOpCode::SET_FILTER_RESONANCE: {
            float q = readF32();
            m_voiceController->setFilterResonance(q);
            break;
        }

        case VoiceOpCode::SET_FILTER_ENABLE: {
            uint8_t state = readU8();
            m_voiceController->setFilterEnabled(state != 0);
            break;
        }

        case VoiceOpCode::SET_FILTER_ROUTE: {
            uint8_t voice = readU8();
            uint8_t enabled = readU8();
            m_voiceController->setFilterRouting(voice, enabled != 0);
            break;
        }

        case VoiceOpCode::WAIT: {
            m_waitBeats = readF32();
            char buf[128];
            snprintf(buf, sizeof(buf), "WAIT instruction: %.3f beats (%.3f seconds at BPM=%.1f)", 
                    m_waitBeats, m_waitBeats / m_beatsPerSecond, m_beatsPerSecond * 60.0f);
            logVoiceScript(buf);
            return true;
        }

        case VoiceOpCode::WAIT_RANDOM: {
            float minBeats = readF32();
            float maxBeats = readF32();
            // Generate random value between min and max
            float randValue = minBeats + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * (maxBeats - minBeats);
            m_waitBeats = randValue;
            char buf[128];
            snprintf(buf, sizeof(buf), "WAIT_RANDOM instruction: %.3f beats (min=%.3f, max=%.3f)", 
                    randValue, minBeats, maxBeats);
            logVoiceScript(buf);
            return true;
        }

        case VoiceOpCode::TEMPO: {
            float bpm = readF32();
            setTempo(bpm);
            char buf[64];
            snprintf(buf, sizeof(buf), "TEMPO instruction: BPM set to %.1f (%.3f beats/sec)", 
                    bpm, m_beatsPerSecond);
            logVoiceScript(buf);
            return true;
        }

        case VoiceOpCode::SET_RING_MOD: {
            uint8_t voice = readU8();
            uint8_t sourceVoice = readU8();
            m_voiceController->setRingMod(voice, sourceVoice);
            char buf[64];
            snprintf(buf, sizeof(buf), "SET_RING_MOD: voice=%d, source=%d", voice, sourceVoice);
            logVoiceScript(buf);
            break;
        }

        case VoiceOpCode::SET_SYNC: {
            uint8_t voice = readU8();
            uint8_t sourceVoice = readU8();
            m_voiceController->setSync(voice, sourceVoice);
            char buf[64];
            snprintf(buf, sizeof(buf), "SET_SYNC: voice=%d, source=%d", voice, sourceVoice);
            logVoiceScript(buf);
            break;
        }

        case VoiceOpCode::SET_TEST_BIT: {
            uint8_t voice = readU8();
            uint8_t state = readU8();
            m_voiceController->setTestBit(voice, state != 0);
            char buf[64];
            snprintf(buf, sizeof(buf), "SET_TEST_BIT: voice=%d, state=%d", voice, state);
            logVoiceScript(buf);
            break;
        }

        case VoiceOpCode::SET_WAVEFORM_COMBO: {
            uint8_t voice = readU8();
            uint8_t waveform1 = readU8();
            uint8_t waveform2 = readU8();
            m_voiceController->setWaveformCombination(voice, 
                static_cast<VoiceWaveform>(waveform1),
                static_cast<VoiceWaveform>(waveform2));
            char buf[64];
            snprintf(buf, sizeof(buf), "SET_WAVEFORM_COMBO: voice=%d, wf1=%d, wf2=%d", 
                    voice, waveform1, waveform2);
            logVoiceScript(buf);
            break;
        }

        case VoiceOpCode::SET_PORTAMENTO: {
            uint8_t voice = readU8();
            float timeSeconds = readF32();
            m_voiceController->setPortamento(voice, timeSeconds);
            char buf[64];
            snprintf(buf, sizeof(buf), "SET_PORTAMENTO: voice=%d, time=%.3f", voice, timeSeconds);
            logVoiceScript(buf);
            break;
        }

        case VoiceOpCode::SET_DETUNE: {
            uint8_t voice = readU8();
            float cents = readF32();
            m_voiceController->setDetune(voice, cents);
            char buf[64];
            snprintf(buf, sizeof(buf), "SET_DETUNE: voice=%d, cents=%.1f", voice, cents);
            logVoiceScript(buf);
            break;
        }

        case VoiceOpCode::PLAY_NOTE: {
            uint8_t voice = readU8();
            uint8_t note = readU8();
            float duration = readF32();
            m_voiceController->playNote(voice, note, duration);
            char buf[64];
            snprintf(buf, sizeof(buf), "PLAY_NOTE: voice=%d, note=%d, duration=%.3f", 
                    voice, note, duration);
            logVoiceScript(buf);
            break;
        }

        case VoiceOpCode::LOOP_START: {
            uint8_t loopId = readU8();
            float start = readF32();
            float end = readF32();
            float step = readF32();

            LoopState loop;
            loop.loopId = loopId;
            loop.current = start;
            loop.end = end;
            loop.step = step;
            loop.startPC = m_pc;

            m_loopStack.push_back(loop);
            m_variables[loopId] = start;
            
            char buf[128];
            snprintf(buf, sizeof(buf), "LOOP_START: id=%d, start=%.1f, end=%.1f, step=%.1f", 
                    loopId, start, end, step);
            logVoiceScript(buf);
            break;
        }

        case VoiceOpCode::LOOP_NEXT: {
            uint8_t loopId = readU8();

            if (m_loopStack.empty()) {
                m_running = false;
                return false;
            }

            LoopState& loop = m_loopStack.back();
            if (loop.loopId != loopId) {
                m_running = false;
                return false;
            }

            // Increment counter
            loop.current += loop.step;
            m_variables[loopId] = loop.current;

            // Check if we should continue
            bool shouldContinue = (loop.step > 0) ? (loop.current <= loop.end) : (loop.current >= loop.end);

            if (shouldContinue) {
                // Jump back to loop start
                char buf[64];
                snprintf(buf, sizeof(buf), "LOOP_NEXT: id=%d, current=%.1f (continue)", loopId, loop.current);
                logVoiceScript(buf);
                m_pc = loop.startPC;
            } else {
                // Exit loop
                char buf[64];
                snprintf(buf, sizeof(buf), "LOOP_NEXT: id=%d, current=%.1f (exit)", loopId, loop.current);
                logVoiceScript(buf);
                m_loopStack.pop_back();
            }
            break;
        }

        case VoiceOpCode::PUSH_VAR: {
            uint8_t varId = readU8();
            float value = m_variables[varId];
            push(value);
            char buf[64];
            snprintf(buf, sizeof(buf), "PUSH_VAR: id=%d, value=%.1f", varId, value);
            logVoiceScript(buf);
            break;
        }

        case VoiceOpCode::PUSH_CONST: {
            float value = readF32();
            push(value);
            char buf[64];
            snprintf(buf, sizeof(buf), "PUSH_CONST: value=%.1f", value);
            logVoiceScript(buf);
            break;
        }

        case VoiceOpCode::ADD: {
            float b = pop();
            float a = pop();
            push(a + b);
            break;
        }

        case VoiceOpCode::SUB: {
            float b = pop();
            float a = pop();
            push(a - b);
            break;
        }

        case VoiceOpCode::MUL: {
            float b = pop();
            float a = pop();
            push(a * b);
            break;
        }

        case VoiceOpCode::DIV: {
            float b = pop();
            float a = pop();
            if (b != 0.0f) {
                push(a / b);
            } else {
                push(0.0f);
            }
            break;
        }

        case VoiceOpCode::SET_MASTER_VOLUME: {
            float volume = readF32();
            m_voiceController->setMasterVolume(volume);
            logVoiceScript("SET_MASTER_VOLUME: volume=" + std::to_string(volume));
            break;
        }

        case VoiceOpCode::VOICESCRIPT_DEBUG: {
            uint8_t enabled = readU8();
            g_debugLoggingEnabled = (enabled != 0);
            logVoiceScript(std::string("Debug logging ") + (enabled ? "ENABLED" : "DISABLED"));
            break;
        }

        case VoiceOpCode::PUSH_RANDOM: {
            float minVal = readF32();
            float maxVal = readF32();
            // Simple random using C++ rand()
            float randValue = minVal + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * (maxVal - minVal);
            push(randValue);
            logVoiceScript("PUSH_RANDOM: min=" + std::to_string(minVal) + 
                          ", max=" + std::to_string(maxVal) + 
                          ", result=" + std::to_string(randValue));
            break;
        }

        case VoiceOpCode::LFO_WAVEFORM: {
            uint8_t lfoNum = readU8();
            uint8_t waveform = readU8();
            m_voiceController->setLFOWaveform(lfoNum, static_cast<LFOWaveform>(waveform));
            logVoiceScript("LFO_WAVEFORM: lfo=" + std::to_string(lfoNum) + 
                          ", waveform=" + std::to_string(waveform));
            break;
        }

        case VoiceOpCode::LFO_RATE: {
            uint8_t lfoNum = readU8();
            float rateHz = readF32();
            m_voiceController->setLFORate(lfoNum, rateHz);
            logVoiceScript("LFO_RATE: lfo=" + std::to_string(lfoNum) + 
                          ", rate=" + std::to_string(rateHz) + " Hz");
            break;
        }

        case VoiceOpCode::LFO_RESET: {
            uint8_t lfoNum = readU8();
            m_voiceController->resetLFO(lfoNum);
            logVoiceScript("LFO_RESET: lfo=" + std::to_string(lfoNum));
            break;
        }

        case VoiceOpCode::LFO_TO_PITCH: {
            uint8_t voice = readU8();
            uint8_t lfoNum = readU8();
            float depthCents = readF32();
            m_voiceController->setLFOToPitch(voice, lfoNum, depthCents);
            logVoiceScript("LFO_TO_PITCH: voice=" + std::to_string(voice) + 
                          ", lfo=" + std::to_string(lfoNum) + 
                          ", depth=" + std::to_string(depthCents) + " cents");
            break;
        }

        case VoiceOpCode::LFO_TO_VOLUME: {
            uint8_t voice = readU8();
            uint8_t lfoNum = readU8();
            float depth = readF32();
            m_voiceController->setLFOToVolume(voice, lfoNum, depth);
            logVoiceScript("LFO_TO_VOLUME: voice=" + std::to_string(voice) + 
                          ", lfo=" + std::to_string(lfoNum) + 
                          ", depth=" + std::to_string(depth));
            break;
        }

        case VoiceOpCode::LFO_TO_FILTER: {
            uint8_t voice = readU8();
            uint8_t lfoNum = readU8();
            float depthHz = readF32();
            m_voiceController->setLFOToFilter(voice, lfoNum, depthHz);
            logVoiceScript("LFO_TO_FILTER: voice=" + std::to_string(voice) + 
                          ", lfo=" + std::to_string(lfoNum) + 
                          ", depth=" + std::to_string(depthHz) + " Hz");
            break;
        }

        case VoiceOpCode::LFO_TO_PULSEWIDTH: {
            uint8_t voice = readU8();
            uint8_t lfoNum = readU8();
            float depth = readF32();
            m_voiceController->setLFOToPulseWidth(voice, lfoNum, depth);
            logVoiceScript("LFO_TO_PULSEWIDTH: voice=" + std::to_string(voice) + 
                          ", lfo=" + std::to_string(lfoNum) + 
                          ", depth=" + std::to_string(depth));
            break;
        }

        case VoiceOpCode::SET_PHYSICAL_MODEL: {
            uint8_t voice = readU8();
            uint8_t modelType = readU8();
            m_voiceController->setPhysicalModel(voice, static_cast<PhysicalModelType>(modelType));
            logVoiceScript("SET_PHYSICAL_MODEL: voice=" + std::to_string(voice) + 
                          ", model=" + std::to_string(modelType));
            break;
        }

        case VoiceOpCode::SET_PHYSICAL_DAMPING: {
            uint8_t voice = readU8();
            float damping = readF32();
            m_voiceController->setPhysicalDamping(voice, damping);
            logVoiceScript("SET_PHYSICAL_DAMPING: voice=" + std::to_string(voice) + 
                          ", damping=" + std::to_string(damping));
            break;
        }

        case VoiceOpCode::SET_PHYSICAL_BRIGHTNESS: {
            uint8_t voice = readU8();
            float brightness = readF32();
            m_voiceController->setPhysicalBrightness(voice, brightness);
            logVoiceScript("SET_PHYSICAL_BRIGHTNESS: voice=" + std::to_string(voice) + 
                          ", brightness=" + std::to_string(brightness));
            break;
        }

        case VoiceOpCode::SET_PHYSICAL_EXCITATION: {
            uint8_t voice = readU8();
            float excitation = readF32();
            m_voiceController->setPhysicalExcitation(voice, excitation);
            logVoiceScript("SET_PHYSICAL_EXCITATION: voice=" + std::to_string(voice) + 
                          ", excitation=" + std::to_string(excitation));
            break;
        }

        case VoiceOpCode::SET_PHYSICAL_RESONANCE: {
            uint8_t voice = readU8();
            float resonance = readF32();
            m_voiceController->setPhysicalResonance(voice, resonance);
            logVoiceScript("SET_PHYSICAL_RESONANCE: voice=" + std::to_string(voice) + 
                          ", resonance=" + std::to_string(resonance));
            break;
        }

        case VoiceOpCode::SET_PHYSICAL_TENSION: {
            uint8_t voice = readU8();
            float tension = readF32();
            m_voiceController->setPhysicalTension(voice, tension);
            logVoiceScript("SET_PHYSICAL_TENSION: voice=" + std::to_string(voice) + 
                          ", tension=" + std::to_string(tension));
            break;
        }

        case VoiceOpCode::SET_PHYSICAL_PRESSURE: {
            uint8_t voice = readU8();
            float pressure = readF32();
            m_voiceController->setPhysicalPressure(voice, pressure);
            logVoiceScript("SET_PHYSICAL_PRESSURE: voice=" + std::to_string(voice) + 
                          ", pressure=" + std::to_string(pressure));
            break;
        }

        case VoiceOpCode::PHYSICAL_TRIGGER: {
            uint8_t voice = readU8();
            m_voiceController->triggerPhysical(voice);
            logVoiceScript("PHYSICAL_TRIGGER: voice=" + std::to_string(voice));
            break;
        }

        case VoiceOpCode::END: {
            m_running = false;
            return false;
        }

        default: {
            // Unknown opcode
            m_running = false;
            return false;
        }
    }

    return true;
}

} // namespace SuperTerminal