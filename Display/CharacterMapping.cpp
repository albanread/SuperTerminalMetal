// CharacterMapping.cpp
// SuperTerminal v2.0 - ASCII to Unicode Character Mapping Implementation
// Maps 8-bit ASCII (128-255) to Unicode box drawing and special characters
// compose key is ctrl p, but still hard to use.

#include "CharacterMapping.h"
#include <unordered_map>

namespace SuperTerminal {

// =============================================================================
// ASCII to Unicode Character Mapping Table
// =============================================================================
// Maps 8-bit ASCII codes (128-255) to Unicode box drawing and special characters
// This allows BASIC programs using CHR$(128+) to access box drawing characters

static const uint32_t ASCII_TO_UNICODE_MAP[256] = {
    // 0-127: Direct ASCII mapping (unchanged)
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F,
    0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
    0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F,

    // 128-255: Map to Unicode box drawing and special characters
    // 128-159: First set of box drawing (single lines)
    0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518, 0x251C, 0x2524,  // 128-135: ─ │ ┌ ┐ └ ┘ ├ ┤
    0x252C, 0x2534, 0x253C, 0x2501, 0x2503, 0x250F, 0x2513, 0x2517,  // 136-143: ┬ ┴ ┼ ━ ┃ ┏ ┓ ┗
    0x251B, 0x2523, 0x252B, 0x2533, 0x253B, 0x254B, 0x2550, 0x2551,  // 144-151: ┛ ┣ ┫ ┳ ┻ ╋ ═ ║
    0x2552, 0x2553, 0x2554, 0x2555, 0x2556, 0x2557, 0x2558, 0x2559,  // 152-159: ╒ ╓ ╔ ╕ ╖ ╗ ╘ ╙

    // 160-175: More box drawing
    0x255A, 0x255B, 0x255C, 0x255D, 0x255E, 0x255F, 0x2560, 0x2561,  // 160-167: ╚ ╛ ╜ ╝ ╞ ╟ ╠ ╡
    0x2562, 0x2563, 0x2564, 0x2565, 0x2566, 0x2567, 0x2568, 0x2569,  // 168-175: ╢ ╣ ╤ ╥ ╦ ╧ ╨ ╩

    // 176-191: Block elements and shading
    0x256A, 0x256B, 0x256C, 0x2580, 0x2584, 0x2588, 0x258C, 0x2590,  // 176-183: ╪ ╫ ╬ ▀ ▄ █ ▌ ▐
    0x2591, 0x2592, 0x2593, 0x2594, 0x2595, 0x2596, 0x2597, 0x2598,  // 184-191: ░ ▒ ▓ ▔ ▕ ▖ ▗ ▘

    // 192-207: More block elements
    0x2599, 0x259A, 0x259B, 0x259C, 0x259D, 0x259E, 0x259F, 0x25A0,  // 192-199: ▙ ▚ ▛ ▜ ▝ ▞ ▟ ■
    0x25A1, 0x25AA, 0x25AB, 0x25AC, 0x25B2, 0x25BA, 0x25BC, 0x25C4,  // 200-207: □ ▪ ▫ ▬ ▲ ► ▼ ◄

    // 208-223: Geometric shapes and symbols
    0x25CA, 0x25CB, 0x25CF, 0x25D8, 0x25D9, 0x263A, 0x263B, 0x263C,  // 208-215: ◊ ○ ● ◘ ◙ ☺ ☻ ☼
    0x2640, 0x2642, 0x2660, 0x2663, 0x2665, 0x2666, 0x266A, 0x266B,  // 216-223: ♀ ♂ ♠ ♣ ♥ ♦ ♪ ♫

    // 224-239: Mathematical and special symbols
    0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7,  // 224-231: NBSP ¡ ¢ £ ¤ ¥ ¦ §
    0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,  // 232-239: ¨ © ª « ¬ SHY ® ¯

    // 240-255: Extended Latin and special characters
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7,  // 240-247: ° ± ² ³ ´ µ ¶ ·
    0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF   // 248-255: ¸ ¹ º » ¼ ½ ¾ ¿
};

// Reverse mapping (Unicode to ASCII) - built on demand
static std::unordered_map<uint32_t, uint8_t> UNICODE_TO_ASCII_MAP;
static bool s_reverseMappingInitialized = false;

void CharacterMapping::initializeReverseMapping() {
    if (s_reverseMappingInitialized) return;

    for (int i = 128; i < 256; ++i) {
        uint32_t unicode = ASCII_TO_UNICODE_MAP[i];
        if (unicode != 0) {
            UNICODE_TO_ASCII_MAP[unicode] = static_cast<uint8_t>(i);
        }
    }
    s_reverseMappingInitialized = true;
}

// =============================================================================
// Public API
// =============================================================================

uint32_t CharacterMapping::mapAsciiToUnicode(uint8_t asciiChar) {
    return ASCII_TO_UNICODE_MAP[asciiChar];
}

uint8_t CharacterMapping::mapUnicodeToAscii(uint32_t unicode) {
    initializeReverseMapping();

    // Check for direct ASCII (0-127)
    if (unicode < 128) {
        return static_cast<uint8_t>(unicode);
    }

    // Check reverse mapping
    auto it = UNICODE_TO_ASCII_MAP.find(unicode);
    if (it != UNICODE_TO_ASCII_MAP.end()) {
        return it->second;
    }

    return 0; // No mapping
}

bool CharacterMapping::hasAsciiMapping(uint8_t asciiChar) {
    uint32_t unicode = mapAsciiToUnicode(asciiChar);
    return unicode != 0;
}

} // namespace SuperTerminal
