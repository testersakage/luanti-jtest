/*
 * client_unicode.h
 * Unicode / CJK text processing for Luanti client
 *
 * This class provides:
 *  - UTF-8 → Unicode codepoint decoding
 *  - Unicode → glyph index mapping (Atlas 12x12 bitmap font)
 *  - Unicode → glyph index mapping (TTF / FreeType, variable size)
 *
 *  NOTE:
 *    This is client-side only. No server dependencies.
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

class ClientUnicode
{
public:
    // Constructor / destructor
    ClientUnicode() = default;
    ~ClientUnicode() = default;

    /*--------------------------------------------------------------
     * Loading mapping files
     *--------------------------------------------------------------*/

    // Load CJK atlas mapping (font2unimg output)
    bool loadCJKAtlasMap(const std::string &path);

    // Load full Unicode mapping (future use)
    bool loadUnicodeMap(const std::string &path);

    /*--------------------------------------------------------------
     * UTF-8 decoding
     *--------------------------------------------------------------*/

    // UTF-8 → Unicode codepoints
    std::vector<uint32_t> utf8ToCodepoints(const std::string &text) const;

    /*--------------------------------------------------------------
     * Codepoint → glyph index
     *--------------------------------------------------------------*/

    // Unicode codepoints → atlas glyph indices (12x12 bitmap font)
    std::vector<int> codepointsToAtlasGlyphs(const std::vector<uint32_t> &cps) const;

    // Unicode codepoints → TTF glyph indices (variable size)
    std::vector<int> codepointsToGlyphs(const std::vector<uint32_t> &cps) const;

    /*--------------------------------------------------------------
     * High-level API (UTF-8 → glyph indices)
     *--------------------------------------------------------------*/

    // UTF-8 → atlas glyph indices (12x12 bitmap font)
    std::vector<int> textToAtlasGlyphs(const std::string &text) const;

    // UTF-8 → TTF glyph indices (variable size)
    std::vector<int> textToGlyphs(const std::string &text) const;

    /*--------------------------------------------------------------
     * Query functions
     *--------------------------------------------------------------*/

    bool hasAtlasGlyph(uint32_t cp) const;
    bool hasTTFGlyph(uint32_t cp) const;

    int getAtlasGlyphIndex(uint32_t cp) const;
    int getTTFGlyphIndex(uint32_t cp) const;

private:
    // Internal lookup helpers
    int lookupAtlasGlyph(uint32_t cp) const;
    int lookupTTFGlyph(uint32_t cp) const;

    // Mapping tables
    std::unordered_map<uint32_t, int> m_cjk_atlas_map; // 12x12 bitmap font
    std::unordered_map<uint32_t, int> m_unicode_map;   // TTF / FreeType (future)
};

