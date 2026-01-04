//
// ABCParser.mm
// SuperTerminal v2
//
// ABC Music Notation Parser Implementation


#include "ABCParser.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <cmath>
#include <algorithm>
#import <Foundation/Foundation.h>

namespace SuperTerminal {

// =============================================================================
// ABCNote Implementation
// =============================================================================

int ABCNote::toMIDINote() const {
    if (isRest) return -1;

    // Convert pitch to semitones from C
    int semitone = 0;
    switch (toupper(pitch)) {
        case 'C': semitone = 0; break;
        case 'D': semitone = 2; break;
        case 'E': semitone = 4; break;
        case 'F': semitone = 5; break;
        case 'G': semitone = 7; break;
        case 'A': semitone = 9; break;
        case 'B': semitone = 11; break;
        default: return 60; // Default to middle C
    }

    // Apply accidental
    semitone += accidental;

    // Calculate MIDI note: C4 = 60
    int midiNote = (octave * 12) + semitone;

    // Clamp to valid MIDI range
    return std::max(0, std::min(127, midiNote));
}

ABCNote ABCNote::fromMIDINote(int midiNote, float duration) {
    ABCNote note;
    note.octave = midiNote / 12;
    int semitone = midiNote % 12;

    // Convert semitone to pitch
    static const char* pitches = "CCDDEFFGGAAB";
    static const int accidentals[] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

    note.pitch = pitches[semitone];
    note.accidental = accidentals[semitone];
    note.duration = duration;
    note.isRest = false;

    return note;
}

// =============================================================================
// KeySignature Implementation
// =============================================================================

int KeySignature::getAccidental(char pitch) const {
    // Map of sharps/flats per key
    static const std::unordered_map<std::string, std::unordered_map<char, int>> keyAccidentals = {
        {"C",  {}},
        {"G",  {{'F', 1}}},
        {"D",  {{'F', 1}, {'C', 1}}},
        {"A",  {{'F', 1}, {'C', 1}, {'G', 1}}},
        {"E",  {{'F', 1}, {'C', 1}, {'G', 1}, {'D', 1}}},
        {"B",  {{'F', 1}, {'C', 1}, {'G', 1}, {'D', 1}, {'A', 1}}},
        {"F#", {{'F', 1}, {'C', 1}, {'G', 1}, {'D', 1}, {'A', 1}, {'E', 1}}},
        {"C#", {{'F', 1}, {'C', 1}, {'G', 1}, {'D', 1}, {'A', 1}, {'E', 1}, {'B', 1}}},
        {"F",  {{'B', -1}}},
        {"Bb", {{'B', -1}, {'E', -1}}},
        {"Eb", {{'B', -1}, {'E', -1}, {'A', -1}}},
        {"Ab", {{'B', -1}, {'E', -1}, {'A', -1}, {'D', -1}}},
        {"Db", {{'B', -1}, {'E', -1}, {'A', -1}, {'D', -1}, {'G', -1}}},
        {"Gb", {{'B', -1}, {'E', -1}, {'A', -1}, {'D', -1}, {'G', -1}, {'C', -1}}},
        {"Cb", {{'B', -1}, {'E', -1}, {'A', -1}, {'D', -1}, {'G', -1}, {'C', -1}, {'F', -1}}},
    };

    auto it = keyAccidentals.find(key);
    if (it != keyAccidentals.end()) {
        auto accIt = it->second.find(toupper(pitch));
        if (accIt != it->second.end()) {
            return accIt->second;
        }
    }

    return 0; // Natural
}

// =============================================================================
// ABCTune Implementation
// =============================================================================

ABCVoice* ABCTune::getOrCreateVoice(const std::string& voiceId) {
    for (auto& voice : voices) {
        if (voice.id == voiceId) {
            return &voice;
        }
    }

    // Create new voice
    voices.emplace_back();
    voices.back().id = voiceId;
    voices.back().midiChannel = static_cast<int>(voices.size() - 1);
    return &voices.back();
}

void ABCTune::calculateDuration() {
    totalDuration = 0.0f;

    for (const auto& voice : voices) {
        if (!voice.notes.empty()) {
            const auto& lastNote = voice.notes.back();
            float voiceDuration = lastNote.startTime + lastNote.duration;
            totalDuration = std::max(totalDuration, voiceDuration);
        }
    }
}

// =============================================================================
// Parser State
// =============================================================================

struct ABCParser::ParseState {
    std::shared_ptr<ABCTune> tune;
    ABCVoice* currentVoice;
    float currentBeat;
    int currentLine;
    bool inHeader;
    bool hasKey; // Key signature parsed (K: marks end of header)

    // Bar tracking
    int currentBar;
    float beatsInCurrentBar;

    // Error tracking
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    ParseState()
        : tune(std::make_shared<ABCTune>())
        , currentVoice(nullptr)
        , currentBeat(0.0f)
        , currentLine(0)
        , inHeader(true)
        , hasKey(false)
        , currentBar(0)
        , beatsInCurrentBar(0.0f)
    {}

    void reset() {
        tune = std::make_shared<ABCTune>();
        currentVoice = nullptr;
        currentBeat = 0.0f;
        currentLine = 0;
        inHeader = true;
        hasKey = false;
        currentBar = 0;
        beatsInCurrentBar = 0.0f;
        errors.clear();
        warnings.clear();
    }
};

// =============================================================================
// ABCParser Implementation
// =============================================================================

ABCParser::ABCParser()
    : m_state(std::make_unique<ParseState>())
    , m_verbose(false)
    , m_defaultTempo(120)
{
}

ABCParser::~ABCParser() = default;

ABCParseResult ABCParser::parse(const std::string& abcText) {
    return parseImpl(abcText);
}

ABCParseResult ABCParser::parseFile(const std::string& filename) {
    @autoreleasepool {
        NSString* nsPath = [NSString stringWithUTF8String:filename.c_str()];
        NSError* error = nil;
        NSString* content = [NSString stringWithContentsOfFile:nsPath
                                                      encoding:NSUTF8StringEncoding
                                                         error:&error];

        if (error) {
            return ABCParseResult::error("Failed to read file: " +
                                       std::string([[error localizedDescription] UTF8String]));
        }

        return parseImpl([content UTF8String]);
    }
}

ABCParseResult ABCParser::parseImpl(const std::string& abcText) {
    m_state->reset();

    if (abcText.empty()) {
        return ABCParseResult::error("Empty ABC text");
    }

    // Split into lines
    std::vector<std::string> lines;
    std::istringstream stream(abcText);
    std::string line;

    while (std::getline(stream, line)) {
        // Remove carriage returns
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }

    try {
        parseLines(lines);

        // Finalize
        if (m_state->tune->voices.empty()) {
            // Create default voice if none specified
            m_state->currentVoice = m_state->tune->getOrCreateVoice("default");
        }

        m_state->tune->calculateDuration();

        // Apply default tempo if not set
        if (m_state->tune->tempo == 0) {
            m_state->tune->tempo = m_defaultTempo;
        }

        if (!m_state->errors.empty()) {
            std::string errorMsg = "Parse errors:\n";
            for (const auto& err : m_state->errors) {
                errorMsg += "  " + err + "\n";
            }
            return ABCParseResult::error(errorMsg);
        }

        return ABCParseResult(m_state->tune);

    } catch (const std::exception& e) {
        return ABCParseResult::error(std::string("Parse exception: ") + e.what());
    }
}

void ABCParser::parseLines(const std::vector<std::string>& lines) {
    for (size_t i = 0; i < lines.size(); ++i) {
        m_state->currentLine = static_cast<int>(i + 1);
        const std::string& line = lines[i];

        // Skip empty lines and comments
        if (line.empty() || line[0] == '%') {
            continue;
        }

        // Header or body?
        if (line.length() >= 2 && line[1] == ':' && std::isalpha(line[0])) {
            parseHeader(line);
        } else if (m_state->hasKey) {
            // Body (music notation)
            parseBody(line);
        }
    }
}

void ABCParser::parseHeader(const std::string& line) {
    if (line.length() < 3) return;

    char field = toupper(line[0]);
    std::string value = line.substr(2);

    // Trim whitespace
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);

    switch (field) {
        case 'X': parseReferenceNumber(value); break;
        case 'T': parseTitle(value); break;
        case 'C': parseComposer(value); break;
        case 'O': m_state->tune->origin = value; break;
        case 'R': m_state->tune->rhythm = value; break;
        case 'N': m_state->tune->notes = value; break;
        case 'M': parseMeter(value); break;
        case 'L': parseDefaultLength(value); break;
        case 'Q': parseTempo(value); break;
        case 'K':
            parseKey(value);
            m_state->hasKey = true;
            m_state->inHeader = false;
            break;
        case 'V': parseVoice(value); break;
        default:
            logWarning("Unknown header field: " + std::string(1, field));
            break;
    }
}

void ABCParser::parseReferenceNumber(const std::string& value) {
    try {
        m_state->tune->referenceNumber = std::stoi(value);
    } catch (...) {
        logError("Invalid reference number: " + value);
    }
}

void ABCParser::parseTitle(const std::string& value) {
    if (m_state->tune->title.empty()) {
        m_state->tune->title = value;
    } else {
        m_state->tune->title += " / " + value;
    }
}

void ABCParser::parseComposer(const std::string& value) {
    m_state->tune->composer = value;
}

void ABCParser::parseMeter(const std::string& value) {
    if (value == "C" || value == "c") {
        m_state->tune->timeSignature = TimeSignature(4, 4);
    } else if (value == "C|" || value == "c|") {
        m_state->tune->timeSignature = TimeSignature(2, 2);
    } else {
        // Parse fraction
        size_t slashPos = value.find('/');
        if (slashPos != std::string::npos) {
            try {
                int num = std::stoi(value.substr(0, slashPos));
                int den = std::stoi(value.substr(slashPos + 1));
                m_state->tune->timeSignature = TimeSignature(num, den);
            } catch (...) {
                logError("Invalid meter: " + value);
            }
        }
    }
}

void ABCParser::parseDefaultLength(const std::string& value) {
    size_t slashPos = value.find('/');
    if (slashPos != std::string::npos) {
        try {
            int num = std::stoi(value.substr(0, slashPos));
            int den = std::stoi(value.substr(slashPos + 1));
            m_state->tune->defaultNoteLength = static_cast<float>(num) / den;
        } catch (...) {
            logError("Invalid default length: " + value);
        }
    }
}

void ABCParser::parseTempo(const std::string& value) {
    // Simple tempo parsing: "120" or "1/4=120"
    size_t equalPos = value.find('=');
    std::string tempoStr;

    if (equalPos != std::string::npos) {
        tempoStr = value.substr(equalPos + 1);
    } else {
        tempoStr = value;
    }

    // Trim
    tempoStr.erase(0, tempoStr.find_first_not_of(" \t"));
    tempoStr.erase(tempoStr.find_last_not_of(" \t") + 1);

    try {
        m_state->tune->tempo = std::stoi(tempoStr);
    } catch (...) {
        logError("Invalid tempo: " + value);
    }
}

void ABCParser::parseKey(const std::string& value) {
    m_state->tune->keySignature = parseKeySignature(value);
}

void ABCParser::parseVoice(const std::string& value) {
    // Parse voice directive: "V:1" or "V:melody name="Melody""
    std::string voiceId = value;
    std::string voiceName;

    // Extract voice ID (everything before space or name=)
    size_t spacePos = voiceId.find(' ');
    if (spacePos != std::string::npos) {
        voiceName = voiceId.substr(spacePos + 1);
        voiceId = voiceId.substr(0, spacePos);
    }

    // Get or create voice
    m_state->currentVoice = m_state->tune->getOrCreateVoice(voiceId);

    // Parse name if present
    if (!voiceName.empty()) {
        size_t namePos = voiceName.find("name=");
        if (namePos != std::string::npos) {
            voiceName = voiceName.substr(namePos + 5);
            // Remove quotes
            if (!voiceName.empty() && voiceName[0] == '"') {
                voiceName = voiceName.substr(1);
            }
            if (!voiceName.empty() && voiceName.back() == '"') {
                voiceName.pop_back();
            }
            m_state->currentVoice->name = voiceName;
        }
    }

    if (m_state->currentVoice->name.empty()) {
        m_state->currentVoice->name = voiceId;
    }
}

void ABCParser::parseBody(const std::string& line) {
    // Ensure we have a voice
    if (!m_state->currentVoice) {
        m_state->currentVoice = m_state->tune->getOrCreateVoice("default");
    }

    // Check for voice directive in body
    if (line.length() >= 2 && line[0] == 'V' && line[1] == ':') {
        parseVoice(line.substr(2));
        return;
    }

    // Parse note sequence
    parseNoteSequence(line);
}

void ABCParser::parseNoteSequence(const std::string& sequence) {
    const char* ptr = sequence.c_str();

    while (*ptr) {
        skipWhitespace(ptr);

        if (*ptr == '\0') break;

        // Bar line
        if (*ptr == '|' || *ptr == ':') {
            ptr++;
            m_state->currentBar++;
            m_state->beatsInCurrentBar = 0.0f;
            continue;
        }

        // Chord symbol (in quotes)
        if (*ptr == '"') {
            auto chord = parseChord(ptr);
            if (chord.has_value()) {
                m_state->currentVoice->chords.push_back(*chord);
            }
            continue;
        }

        // Repeat markers
        if (*ptr == '[' || *ptr == ']') {
            ptr++;
            continue;
        }

        // Note or rest
        if (isNote(*ptr) || isRest(*ptr)) {
            ABCNote note = parseNote(ptr);
            note.startTime = m_state->currentBeat;
            m_state->currentVoice->notes.push_back(note);
            advanceBeat(note.duration);
            m_state->beatsInCurrentBar += note.duration;
            continue;
        }

        // Skip unknown characters
        ptr++;
    }
}

ABCNote ABCParser::parseNote(const char*& ptr) {
    ABCNote note;

    // Check for rest
    if (isRest(*ptr)) {
        note.isRest = true;
        ptr++;
        note.duration = parseDuration(ptr) * m_state->tune->defaultNoteLength;
        return note;
    }

    // Parse accidental
    note.accidental = parseAccidental(ptr);

    // Parse pitch
    char pitch = *ptr++;
    note.pitch = toUpperPitch(pitch);

    // Parse octave modifiers
    note.octave = parseOctave(ptr, pitch);

    // Apply key signature if no explicit accidental
    if (note.accidental == 0) {
        note.accidental = m_state->tune->keySignature.getAccidental(note.pitch);
    }

    // Parse duration
    note.duration = parseDuration(ptr) * m_state->tune->defaultNoteLength;

    // Check for tie
    if (*ptr == '-') {
        note.isTied = true;
        ptr++;
    }

    return note;
}

float ABCParser::parseDuration(const char*& ptr) {
    float multiplier = 1.0f;

    // Check for duration modifiers
    if (*ptr == '/') {
        ptr++;
        if (isdigit(*ptr)) {
            int divisor = *ptr - '0';
            ptr++;
            multiplier = 1.0f / divisor;
        } else {
            multiplier = 0.5f; // "/" alone means half
        }
    } else if (isdigit(*ptr)) {
        int numerator = 0;
        while (isdigit(*ptr)) {
            numerator = numerator * 10 + (*ptr - '0');
            ptr++;
        }

        if (*ptr == '/') {
            ptr++;
            if (isdigit(*ptr)) {
                int denominator = 0;
                while (isdigit(*ptr)) {
                    denominator = denominator * 10 + (*ptr - '0');
                    ptr++;
                }
                multiplier = static_cast<float>(numerator) / denominator;
            } else {
                multiplier = numerator / 2.0f;
            }
        } else {
            multiplier = static_cast<float>(numerator);
        }
    }

    return multiplier;
}

int ABCParser::parseAccidental(const char*& ptr) {
    int accidental = 0;

    if (*ptr == '^') {
        accidental = 1;
        ptr++;
        if (*ptr == '^') {
            accidental = 2;
            ptr++;
        }
    } else if (*ptr == '_') {
        accidental = -1;
        ptr++;
        if (*ptr == '_') {
            accidental = -2;
            ptr++;
        }
    } else if (*ptr == '=') {
        accidental = 0;
        ptr++;
    }

    return accidental;
}

int ABCParser::parseOctave(const char*& ptr, char pitch) {
    // Default octave: 4 for uppercase, 5 for lowercase
    int octave = isupper(pitch) ? 4 : 5;

    // Count apostrophes (raise octave) or commas (lower octave)
    while (*ptr == '\'') {
        octave++;
        ptr++;
    }

    while (*ptr == ',') {
        octave--;
        ptr++;
    }

    return octave;
}

std::optional<ABCChord> ABCParser::parseChord(const char*& ptr) {
    if (*ptr != '"') return std::nullopt;

    ptr++; // Skip opening quote

    std::string symbol;
    while (*ptr && *ptr != '"') {
        symbol += *ptr++;
    }

    if (*ptr == '"') {
        ptr++; // Skip closing quote
    }

    ABCChord chord(symbol, m_state->currentBeat, m_state->tune->defaultNoteLength * 4.0f);
    return chord;
}

void ABCParser::skipWhitespace(const char*& ptr) {
    while (*ptr && std::isspace(*ptr)) {
        ptr++;
    }
}

bool ABCParser::isNote(char c) const {
    return (c >= 'A' && c <= 'G') || (c >= 'a' && c <= 'g');
}

bool ABCParser::isRest(char c) const {
    return c == 'z' || c == 'Z' || c == 'x' || c == 'X';
}

char ABCParser::toUpperPitch(char c) const {
    return static_cast<char>(std::toupper(c));
}

void ABCParser::advanceBeat(float duration) {
    m_state->currentBeat += duration;
}

void ABCParser::logError(const std::string& message, int line) {
    int lineNum = (line > 0) ? line : m_state->currentLine;
    std::string msg = "Line " + std::to_string(lineNum) + ": " + message;
    m_state->errors.push_back(msg);

    if (m_verbose) {
        NSLog(@"[ABCParser] ERROR: %s", msg.c_str());
    }
}

void ABCParser::logWarning(const std::string& message, int line) {
    int lineNum = (line > 0) ? line : m_state->currentLine;
    std::string msg = "Line " + std::to_string(lineNum) + ": " + message;
    m_state->warnings.push_back(msg);

    if (m_verbose) {
        NSLog(@"[ABCParser] WARNING: %s", msg.c_str());
    }
}

void ABCParser::logInfo(const std::string& message) {
    if (m_verbose) {
        NSLog(@"[ABCParser] INFO: %s", message.c_str());
    }
}

// =============================================================================
// Static Methods
// =============================================================================

bool ABCParser::validate(const std::string& abcText) {
    // Basic validation: check for required fields
    bool hasReference = false;
    bool hasKey = false;

    std::istringstream stream(abcText);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.length() >= 2 && line[1] == ':') {
            char field = toupper(line[0]);
            if (field == 'X') hasReference = true;
            if (field == 'K') hasKey = true;
        }
    }

    return hasReference && hasKey;
}

int ABCParser::noteNameToMIDI(const std::string& noteName) {
    if (noteName.empty()) return 60; // Middle C

    char pitch = toupper(noteName[0]);
    int semitone = 0;

    switch (pitch) {
        case 'C': semitone = 0; break;
        case 'D': semitone = 2; break;
        case 'E': semitone = 4; break;
        case 'F': semitone = 5; break;
        case 'G': semitone = 7; break;
        case 'A': semitone = 9; break;
        case 'B': semitone = 11; break;
        default: return 60;
    }

    // Check for accidental
    size_t pos = 1;
    if (pos < noteName.length()) {
        if (noteName[pos] == '#') {
            semitone++;
            pos++;
        } else if (noteName[pos] == 'b') {
            semitone--;
            pos++;
        }
    }

    // Parse octave
    int octave = 4; // Default
    if (pos < noteName.length() && isdigit(noteName[pos])) {
        octave = noteName[pos] - '0';
    }

    return (octave * 12) + semitone;
}

std::string ABCParser::midiToNoteName(int midiNote) {
    static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    int octave = midiNote / 12;
    int semitone = midiNote % 12;

    return std::string(noteNames[semitone]) + std::to_string(octave);
}

// =============================================================================
// Helper Functions
// =============================================================================

float parseDurationString(const std::string& durationStr, float defaultLength) {
    if (durationStr.empty()) {
        return defaultLength;
    }

    // Parse fraction or number
    size_t slashPos = durationStr.find('/');
    if (slashPos != std::string::npos) {
        try {
            float num = std::stof(durationStr.substr(0, slashPos));
            float den = std::stof(durationStr.substr(slashPos + 1));
            return (num / den) * defaultLength;
        } catch (...) {
            return defaultLength;
        }
    }

    try {
        float multiplier = std::stof(durationStr);
        return multiplier * defaultLength;
    } catch (...) {
        return defaultLength;
    }
}

KeySignature parseKeySignature(const std::string& keyStr) {
    KeySignature sig;

    if (keyStr.empty()) {
        return sig; // Default C major
    }

    // Extract key letter and mode
    std::string key = keyStr;

    // Trim whitespace
    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);

    // Get first word (key)
    size_t spacePos = key.find(' ');
    if (spacePos != std::string::npos) {
        std::string mode = key.substr(spacePos + 1);
        key = key.substr(0, spacePos);

        // Check for minor mode
        if (mode.find("min") != std::string::npos ||
            mode.find("m") == 0 ||
            mode == "Minor") {
            sig.isMinor = true;
        }
    }

    // Check if key ends with 'm' (minor)
    if (!key.empty() && key.back() == 'm') {
        sig.isMinor = true;
        key.pop_back();
    }

    sig.key = key;

    // Calculate sharps/flats
    static const std::unordered_map<std::string, int> keySharps = {
        {"C", 0}, {"G", 1}, {"D", 2}, {"A", 3}, {"E", 4}, {"B", 5}, {"F#", 6}, {"C#", 7},
        {"F", -1}, {"Bb", -2}, {"Eb", -3}, {"Ab", -4}, {"Db", -5}, {"Gb", -6}, {"Cb", -7},
        {"Am", 0}, {"Em", 1}, {"Bm", 2}, {"F#m", 3}, {"C#m", 4}, {"G#m", 5}, {"D#m", 6}, {"A#m", 7},
        {"Dm", -1}, {"Gm", -2}, {"Cm", -3}, {"Fm", -4}, {"Bbm", -5}, {"Ebm", -6}, {"Abm", -7}
    };

    std::string lookupKey = sig.isMinor ? key + "m" : key;
    auto it = keySharps.find(lookupKey);
    if (it != keySharps.end()) {
        sig.sharps = it->second;
    }

    return sig;
}

int getInstrumentNumber(const std::string& instrumentName) {
    // Map common instrument names to MIDI program numbers
    static const std::unordered_map<std::string, int> instruments = {
        {"piano", 0}, {"acoustic_grand_piano", 0},
        {"bright_acoustic_piano", 1}, {"electric_grand_piano", 2},
        {"harpsichord", 6}, {"clavinet", 7},
        {"celesta", 8}, {"glockenspiel", 9},
        {"music_box", 10}, {"vibraphone", 11},
        {"marimba", 12}, {"xylophone", 13},
        {"tubular_bells", 14}, {"dulcimer", 15},
        {"organ", 16}, {"drawbar_organ", 16},
        {"guitar", 24}, {"acoustic_guitar", 24},
        {"electric_guitar", 27}, {"overdriven_guitar", 29},
        {"distortion_guitar", 30},
        {"bass", 32}, {"acoustic_bass", 32},
        {"electric_bass", 33}, {"slap_bass", 36},
        {"synth_bass", 38},
        {"violin", 40}, {"viola", 41}, {"cello", 42},
        {"contrabass", 43}, {"harp", 46},
        {"strings", 48}, {"string_ensemble", 48},
        {"synth_strings", 50}, {"choir", 52},
        {"trumpet", 56}, {"trombone", 57},
        {"tuba", 58}, {"horn", 60},
        {"brass", 61}, {"synth_brass", 62},
        {"saxophone", 64}, {"sax", 64},
        {"oboe", 68}, {"clarinet", 71},
        {"flute", 73}, {"recorder", 74},
        {"pan_flute", 75}, {"whistle", 78},
        {"lead", 80}, {"synth_lead", 80},
        {"pad", 88}, {"synth_pad", 88},
        {"fx", 96}, {"synth_fx", 96},
        {"drums", 128}, {"percussion", 128}
    };

    std::string lower = instrumentName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    auto it = instruments.find(lower);
    return (it != instruments.end()) ? it->second : 0;
}

} // namespace SuperTerminal
