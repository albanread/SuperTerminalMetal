// FontAtlas.h
// SuperTerminal v2.0 - Font Atlas Management
// Phase 1 Week 2: Bitmap font loading and texture atlas generation

#ifndef SUPERTERMINAL_FONTATLAS_H
#define SUPERTERMINAL_FONTATLAS_H

#include <Metal/Metal.h>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace SuperTerminal {

/// @brief Metrics for a single glyph in the font
struct GlyphMetrics {
    int width;          ///< Glyph width in pixels
    int height;         ///< Glyph height in pixels
    int bearingX;       ///< Horizontal bearing (offset from origin)
    int bearingY;       ///< Vertical bearing (offset from baseline)
    int advance;        ///< Horizontal advance to next glyph

    // Atlas coordinates (normalized 0.0-1.0)
    float atlasX;       ///< X coordinate in atlas texture
    float atlasY;       ///< Y coordinate in atlas texture
    float atlasWidth;   ///< Width in atlas texture
    float atlasHeight;  ///< Height in atlas texture
};

/// @brief Font atlas containing all glyphs as a single texture
///
/// FontAtlas manages bitmap fonts by packing all character glyphs into
/// a single Metal texture. This allows efficient rendering of text with
/// a single draw call per text grid.
///
/// Supported font formats:
/// - Bitmap fonts (8x8, 8x16, etc.)
/// - TrueType fonts (rasterized to bitmap)
/// - Custom font formats
class FontAtlas {
public:
    /// Constructor
    /// @param device Metal device for texture creation
    FontAtlas(id<MTLDevice> device);

    /// Destructor
    ~FontAtlas();

    // Disable copy
    FontAtlas(const FontAtlas&) = delete;
    FontAtlas& operator=(const FontAtlas&) = delete;

    // =========================================================================
    // Font Loading
    // =========================================================================

    /// Load a built-in bitmap font
    /// @param name Font name ("vga_8x16", "c64_8x8", etc.)
    /// @return true if loaded successfully
    bool loadBuiltinFont(const std::string& name);

    /// Load a bitmap font from memory
    /// @param pixels Raw pixel data (grayscale or RGBA)
    /// @param width Total image width
    /// @param height Total image height
    /// @param glyphWidth Width of each glyph
    /// @param glyphHeight Height of each glyph
    /// @param firstChar First character code point in the font
    /// @param lastChar Last character code point in the font
    /// @return true if loaded successfully
    bool loadBitmapFont(const uint8_t* pixels, int width, int height,
                       int glyphWidth, int glyphHeight,
                       uint32_t firstChar, uint32_t lastChar);

    /// Load a TrueType font and rasterize to bitmap
    /// @param path Path to .ttf file
    /// @param pixelSize Font size in pixels
    /// @return true if loaded successfully
    bool loadTrueTypeFont(const std::string& path, int pixelSize);

    /// Load an Unscii font (preferred method)
    /// @param variant Unscii variant ("unscii-8", "unscii-16", "unscii-16-full")
    /// @param pixelSize Font size in pixels (optional, uses default if 0)
    /// @return true if loaded successfully
    bool loadUnsciiFont(const std::string& variant, int pixelSize = 0);

    /// Load the default Unscii font (unscii-16 for best readability)
    /// @return true if loaded successfully
    bool loadDefaultUnsciiFont();

    /// Generate font atlas from individual glyph images
    /// @param glyphs Vector of glyph pixel data
    /// @return true if generated successfully
    bool generateAtlas(const std::vector<std::vector<uint8_t>>& glyphs);

    // =========================================================================
    // Texture Access
    // =========================================================================

    /// Get the Metal texture containing the font atlas
    /// @return Metal texture, or nil if not loaded
    id<MTLTexture> getTexture() const { return texture_; }

    /// Get atlas texture width
    /// @return Width in pixels
    int getAtlasWidth() const { return atlasWidth_; }

    /// Get atlas texture height
    /// @return Height in pixels
    int getAtlasHeight() const { return atlasHeight_; }

    // =========================================================================
    // Glyph Metrics
    // =========================================================================

    /// Get metrics for a specific character
    /// @param codepoint Unicode code point
    /// @return Glyph metrics, or default metrics if not found
    GlyphMetrics getGlyphMetrics(uint32_t codepoint) const;

    /// Get the standard glyph width (for monospace fonts)
    /// @return Glyph width in pixels
    int getGlyphWidth() const { return glyphWidth_; }

    /// Get the standard glyph height (for monospace fonts)
    /// @return Glyph height in pixels
    int getGlyphHeight() const { return glyphHeight_; }

    /// Check if a character is available in the font
    /// @param codepoint Unicode code point
    /// @return true if glyph exists
    bool hasGlyph(uint32_t codepoint) const;

    // =========================================================================
    // UV Coordinate Helpers
    // =========================================================================

    /// Get texture coordinates for a character
    /// @param codepoint Unicode code point
    /// @param u0 Output: left texture coordinate
    /// @param v0 Output: top texture coordinate
    /// @param u1 Output: right texture coordinate
    /// @param v1 Output: bottom texture coordinate
    void getTextureCoords(uint32_t codepoint,
                         float& u0, float& v0, float& u1, float& v1) const;

    /// Get texture coordinates for character (alternative interface)
    /// @param codepoint Unicode code point
    /// @return Array of 4 floats [u0, v0, u1, v1]
    std::array<float, 4> getTexCoords(uint32_t codepoint) const;

    // =========================================================================
    // Font Information
    // =========================================================================

    /// Get font name
    /// @return Font name string
    const std::string& getName() const { return fontName_; }

    /// Get first character code point in font
    /// @return First code point
    uint32_t getFirstChar() const { return firstChar_; }

    /// Get last character code point in font
    /// @return Last code point
    uint32_t getLastChar() const { return lastChar_; }

    /// Get number of glyphs in font
    /// @return Glyph count
    size_t getGlyphCount() const { return glyphs_.size(); }

    /// Check if font is monospace
    /// @return true if all glyphs have same width
    bool isMonospace() const { return isMonospace_; }

    // =========================================================================
    // Note: Character mapping (ASCII 128-255 to Unicode) is handled by
    // CharacterMapping class in Display/CharacterMapping.h
    // =========================================================================

    // =========================================================================
    // Built-in Fonts
    // =========================================================================

    /// Get list of available built-in fonts
    /// @return Vector of font names
    static std::vector<std::string> getBuiltinFontNames();

    /// Get default font name
    /// @return Default font name ("unscii-16")
    static std::string getDefaultFontName() { return "unscii-16"; }

    /// Get list of available Unscii font variants
    /// @return Vector of Unscii variant names
    static std::vector<std::string> getUnsciiVariants();

private:
    // =========================================================================
    // Private Helper Methods
    // =========================================================================

    /// Create Metal texture from pixel data
    /// @param pixels Pixel data (grayscale or RGBA)
    /// @param width Texture width
    /// @param height Texture height
    /// @param format Pixel format
    /// @return true if created successfully
    bool createTexture(const uint8_t* pixels, int width, int height,
                      MTLPixelFormat format);

    /// Pack glyphs into atlas texture (simple grid layout)
    /// @param glyphs Vector of glyph pixel data
    /// @param glyphWidth Width of each glyph
    /// @param glyphHeight Height of each glyph
    /// @return Packed atlas pixel data
    std::vector<uint8_t> packGlyphsToAtlas(
        const std::vector<std::vector<uint8_t>>& glyphs,
        int glyphWidth, int glyphHeight);

    /// Calculate optimal atlas dimensions
    /// @param numGlyphs Number of glyphs to pack
    /// @param glyphWidth Width of each glyph
    /// @param glyphHeight Height of each glyph
    /// @param outWidth Output atlas width
    /// @param outHeight Output atlas height
    void calculateAtlasDimensions(size_t numGlyphs, int glyphWidth, int glyphHeight,
                                 int& outWidth, int& outHeight);

    /// Load built-in VGA 8x16 font
    bool loadVGA8x16Font();

    /// Load built-in C64 8x8 font
    bool loadC648x8Font();

    /// Load built-in Apple II 8x8 font
    bool loadAppleII8x8Font();

    /// Load pre-generated Unscii atlas
    /// @param variant Unscii variant name
    /// @param pixelSize Font size (optional)
    /// @return true if loaded successfully
    bool loadUnsciiAtlas(const std::string& variant, int pixelSize = 0);

    /// Find Unscii atlas file path
    /// @param variant Unscii variant name
    /// @return Atlas file path or empty string if not found
    std::string findUnsciiAtlasPath(const std::string& variant);

    // =========================================================================
    // Member Variables
    // =========================================================================

    id<MTLDevice> device_;              ///< Metal device
    id<MTLTexture> texture_;            ///< Atlas texture

    std::string fontName_;              ///< Font name
    int glyphWidth_;                    ///< Standard glyph width
    int glyphHeight_;                   ///< Standard glyph height
    int atlasWidth_;                    ///< Atlas texture width
    int atlasHeight_;                   ///< Atlas texture height

    uint32_t firstChar_;                ///< First character code point
    uint32_t lastChar_;                 ///< Last character code point

    std::vector<GlyphMetrics> glyphs_;  ///< Metrics for each glyph
    std::unordered_map<uint32_t, size_t> charMap_;  ///< Maps codepoint to glyph index
    bool isMonospace_;                  ///< True if monospace font

    // Grid layout info (for simple bitmap fonts)
    int glyphsPerRow_;                  ///< Number of glyphs per row in atlas
    int glyphRows_;                     ///< Number of rows in atlas
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_FONTATLAS_H
