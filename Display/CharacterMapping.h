// CharacterMapping.h
// SuperTerminal v2.0 - ASCII to Unicode Character Mapping
// Maps 8-bit ASCII (128-255) to Unicode box drawing and special characters

#ifndef SUPERTERMINAL_CHARACTERMAPPING_H
#define SUPERTERMINAL_CHARACTERMAPPING_H

#include <cstdint>

namespace SuperTerminal {

/// @brief Character mapping utilities for ASCII to Unicode conversion
///
/// Provides mapping between 8-bit ASCII codes (0-255) and Unicode codepoints.
/// Characters 0-127 pass through unchanged (standard ASCII).
/// Characters 128-255 map to Unicode box drawing, block elements, and symbols.
class CharacterMapping {
public:
    /// Map 8-bit ASCII character (0-255) to Unicode codepoint
    /// Characters 128-255 map to box drawing and extended characters
    /// @param asciiChar 8-bit character code (0-255)
    /// @return Unicode codepoint for rendering
    static uint32_t mapAsciiToUnicode(uint8_t asciiChar);

    /// Map Unicode codepoint back to 8-bit ASCII if possible
    /// @param unicode Unicode codepoint
    /// @return ASCII character (0-255) or 0 if no mapping
    static uint8_t mapUnicodeToAscii(uint32_t unicode);

    /// Check if an ASCII character has a Unicode mapping
    /// @param asciiChar 8-bit character code (0-255)
    /// @return true if character maps to a displayable glyph
    static bool hasAsciiMapping(uint8_t asciiChar);

private:
    /// Initialize reverse mapping table
    static void initializeReverseMapping();
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_CHARACTERMAPPING_H