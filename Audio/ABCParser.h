//
// ABCParser.h
// SuperTerminal v2
//
// ABC Music Notation Parser
// Converts ABC notation to structured musical data for MIDI playback
//
// Based on v1 implementation, modernized for v2 architecture
//

#ifndef SUPERTERMINAL_ABC_PARSER_H
#define SUPERTERMINAL_ABC_PARSER_H

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <memory>

namespace SuperTerminal {

// =============================================================================
// ABC Data Structures
// =============================================================================

/// Musical note representation
struct ABCNote {
    char pitch;              // A-G or rest 'z'
    int octave;              // 0-8 (4 = middle octave)
    int accidental;          // -2=double flat, -1=flat, 0=natural, 1=sharp, 2=double sharp
    float duration;          // Duration in beats (1.0 = quarter note in 4/4 time)
    bool isRest;             // True if this is a rest
    bool isTied;             // True if tied to next note
    float startTime;         // Start time in beats from beginning
    
    ABCNote()
        : pitch('C')
        , octave(4)
        , accidental(0)
        , duration(1.0f)
        , isRest(false)
        , isTied(false)
        , startTime(0.0f)
    {}
    
    /// Convert to MIDI note number (0-127)
    int toMIDINote() const;
    
    /// Convert from MIDI note number
    static ABCNote fromMIDINote(int midiNote, float duration = 1.0f);
};

/// Chord symbol (for accompaniment)
struct ABCChord {
    std::string symbol;      // e.g., "C", "Am", "G7", "Dm/F"
    float startTime;         // When chord starts (in beats)
    float duration;          // How long chord lasts
    
    ABCChord(const std::string& sym = "", float start = 0.0f, float dur = 1.0f)
        : symbol(sym), startTime(start), duration(dur)
    {}
};

/// Voice/track in a multi-voice tune
struct ABCVoice {
    std::string id;          // Voice identifier (e.g., "V:1")
    std::string name;        // Display name
    std::string clef;        // treble, bass, alto, etc.
    int midiChannel;         // MIDI channel (0-15)
    int midiInstrument;      // MIDI program number (0-127)
    int transpose;           // Semitone transposition
    std::vector<ABCNote> notes;
    std::vector<ABCChord> chords;
    
    ABCVoice()
        : midiChannel(0)
        , midiInstrument(0)  // Acoustic Grand Piano
        , transpose(0)
    {}
};

/// Time signature
struct TimeSignature {
    int numerator;           // Beats per bar
    int denominator;         // Note value (4 = quarter note)
    
    TimeSignature(int num = 4, int den = 4)
        : numerator(num), denominator(den)
    {}
    
    float beatsPerBar() const {
        return static_cast<float>(numerator) * (4.0f / denominator);
    }
};

/// Key signature
struct KeySignature {
    std::string key;         // e.g., "C", "Am", "G", "Dm"
    bool isMinor;            // Major or minor key
    int sharps;              // Positive for sharps, negative for flats
    
    KeySignature()
        : key("C"), isMinor(false), sharps(0)
    {}
    
    /// Get accidental for a given pitch in this key
    int getAccidental(char pitch) const;
};

/// Complete ABC tune
struct ABCTune {
    // Header information
    int referenceNumber;     // X: field
    std::string title;       // T: field
    std::string composer;    // C: field
    std::string origin;      // O: field
    std::string rhythm;      // R: field (jig, reel, waltz, etc.)
    std::string notes;       // N: field (comments)
    
    // Musical parameters
    TimeSignature timeSignature;  // M: field
    KeySignature keySignature;    // K: field
    float defaultNoteLength;      // L: field (e.g., 1/8, 1/4)
    int tempo;                     // Q: field (BPM)
    
    // Voice data
    std::vector<ABCVoice> voices;
    
    // Calculated properties
    float totalDuration;     // Total length in beats
    
    ABCTune()
        : referenceNumber(1)
        , defaultNoteLength(0.125f)  // 1/8 note
        , tempo(120)
        , totalDuration(0.0f)
    {}
    
    /// Get voice by ID, or create if doesn't exist
    ABCVoice* getOrCreateVoice(const std::string& voiceId);
    
    /// Calculate total duration from all voices
    void calculateDuration();
};

// =============================================================================
// Parser Result
// =============================================================================

/// Result of parsing operation
struct ABCParseResult {
    bool success;
    std::string errorMessage;
    int errorLine;
    int errorColumn;
    std::shared_ptr<ABCTune> tune;
    
    ABCParseResult()
        : success(false), errorLine(0), errorColumn(0)
    {}
    
    explicit ABCParseResult(std::shared_ptr<ABCTune> t)
        : success(true), errorLine(0), errorColumn(0), tune(std::move(t))
    {}
    
    static ABCParseResult error(const std::string& msg, int line = 0, int col = 0) {
        ABCParseResult result;
        result.success = false;
        result.errorMessage = msg;
        result.errorLine = line;
        result.errorColumn = col;
        return result;
    }
};

// =============================================================================
// ABC Parser
// =============================================================================

/// ABC Music Notation Parser
///
/// Parses ABC notation text into structured ABCTune objects suitable for
/// MIDI playback via MidiEngine.
///
/// Features:
/// - Standard ABC notation headers (X, T, C, M, L, K, etc.)
/// - Multi-voice support (V: directives)
/// - Note durations, accidentals, octaves
/// - Chords and accompaniment
/// - Repeats (basic support)
/// - Bar lines and measure tracking
///
/// Limitations:
/// - Advanced ornaments are simplified
/// - Some complex repeats may not be fully supported
/// - Grace notes are approximated
///
/// Thread Safety:
/// - Each parser instance is independent
/// - Safe to use one parser per thread
/// - No shared global state
///
/// Example:
/// ```cpp
/// ABCParser parser;
/// auto result = parser.parse(R"(
///     X:1
///     T:Simple Melody
///     M:4/4
///     L:1/4
///     K:C
///     C D E F | G A B c |
/// )");
/// 
/// if (result.success) {
///     midiEngine->playFromABC(result.tune);
/// } else {
///     std::cerr << "Parse error: " << result.error << std::endl;
/// }
/// ```
class ABCParser {
public:
    ABCParser();
    ~ABCParser();
    
    // =========================================================================
    // Main API
    // =========================================================================
    
    /// Parse ABC notation string
    /// @param abcText ABC notation text
    /// @return Parse result with tune or error
    ABCParseResult parse(const std::string& abcText);
    
    /// Parse ABC notation from file
    /// @param filename Path to ABC file
    /// @return Parse result with tune or error
    ABCParseResult parseFile(const std::string& filename);
    
    // =========================================================================
    // Configuration
    // =========================================================================
    
    /// Enable/disable verbose logging
    void setVerbose(bool verbose) { m_verbose = verbose; }
    
    /// Get verbose mode
    bool isVerbose() const { return m_verbose; }
    
    /// Set default tempo if not specified in ABC
    void setDefaultTempo(int bpm) { m_defaultTempo = bpm; }
    
    /// Get default tempo
    int getDefaultTempo() const { return m_defaultTempo; }
    
    // =========================================================================
    // Utility
    // =========================================================================
    
    /// Get parser version
    static std::string getVersion() { return "2.0.0"; }
    
    /// Validate ABC syntax without full parsing
    /// @param abcText ABC notation text
    /// @return true if basic syntax is valid
    static bool validate(const std::string& abcText);
    
    /// Convert note name to MIDI number (e.g., "C4" -> 60)
    static int noteNameToMIDI(const std::string& noteName);
    
    /// Convert MIDI number to note name (e.g., 60 -> "C4")
    static std::string midiToNoteName(int midiNote);

private:
    // =========================================================================
    // Internal State
    // =========================================================================
    
    struct ParseState;
    std::unique_ptr<ParseState> m_state;
    
    bool m_verbose;
    int m_defaultTempo;
    
    // =========================================================================
    // Parsing Implementation
    // =========================================================================
    
    // High-level parsing
    ABCParseResult parseImpl(const std::string& abcText);
    void parseLines(const std::vector<std::string>& lines);
    
    // Header parsing
    void parseHeader(const std::string& line);
    void parseReferenceNumber(const std::string& value);
    void parseTitle(const std::string& value);
    void parseComposer(const std::string& value);
    void parseMeter(const std::string& value);
    void parseDefaultLength(const std::string& value);
    void parseTempo(const std::string& value);
    void parseKey(const std::string& value);
    void parseVoice(const std::string& value);
    
    // Body parsing
    void parseBody(const std::string& line);
    void parseNoteSequence(const std::string& sequence);
    ABCNote parseNote(const char*& ptr);
    float parseDuration(const char*& ptr);
    int parseAccidental(const char*& ptr);
    int parseOctave(const char*& ptr, char pitch);
    
    // Chord parsing
    std::optional<ABCChord> parseChord(const char*& ptr);
    
    // Utilities
    void skipWhitespace(const char*& ptr);
    bool isNote(char c) const;
    bool isRest(char c) const;
    char toUpperPitch(char c) const;
    void advanceBeat(float duration);
    void logError(const std::string& message, int line = 0);
    void logWarning(const std::string& message, int line = 0);
    void logInfo(const std::string& message);
};

// =============================================================================
// Helper Functions
// =============================================================================

/// Convert ABC duration string to beats
/// @param durationStr ABC duration (e.g., "/2", "3/2", "2")
/// @param defaultLength Default note length
/// @return Duration in beats
float parseDurationString(const std::string& durationStr, float defaultLength);

/// Parse ABC key signature
/// @param keyStr Key string (e.g., "C", "Am", "G", "Dm")
/// @return Key signature structure
KeySignature parseKeySignature(const std::string& keyStr);

/// Get MIDI instrument number from instrument name
/// @param instrumentName Instrument name (e.g., "piano", "violin")
/// @return MIDI program number (0-127), or 0 if unknown
int getInstrumentNumber(const std::string& instrumentName);

} // namespace SuperTerminal

#endif // SUPERTERMINAL_ABC_PARSER_H