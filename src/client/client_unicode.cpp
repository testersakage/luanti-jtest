#include "client_unicode.h"

/*--------------------------------------------------------------
 * Loading mapping files
 *--------------------------------------------------------------*/

bool ClientUnicode::loadCJKAtlasMap(const std::string &path)
{
    // TODO: implement loading of atlas mapping (font2unimg output)
    return false;
}

bool ClientUnicode::loadUnicodeMap(const std::string &path)
{
    // TODO: implement loading of full Unicode mapping
    return false;
}

/*--------------------------------------------------------------
 * UTF-8 decoding
 *--------------------------------------------------------------*/
std::vector<uint32_t> ClientUnicode::utf8ToCodepoints(const std::string &text) const
{
    std::vector<uint32_t> out;
    const unsigned char *s = (const unsigned char *)text.data();
    size_t len = text.size();
    size_t i = 0;

    while (i < len) {
        uint32_t cp = 0;
        unsigned char c = s[i];

        if (c < 0x80) {
            // ASCII
            cp = c;
            i += 1;
        }
        else if ((c >> 5) == 0x6) {
            // 2 bytes: 110xxxxx 10xxxxxx
            if (i + 1 >= len) break;
            cp = ((c & 0x1F) << 6) |
                 (s[i+1] & 0x3F);
            i += 2;
        }
        else if ((c >> 4) == 0xE) {
            // 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
            if (i + 2 >= len) break;
            cp = ((c & 0x0F) << 12) |
                 ((s[i+1] & 0x3F) << 6) |
                 (s[i+2] & 0x3F);
            i += 3;
        }
        else if ((c >> 3) == 0x1E) {
            // 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            if (i + 3 >= len) break;
            cp = ((c & 0x07) << 18) |
                 ((s[i+1] & 0x3F) << 12) |
                 ((s[i+2] & 0x3F) << 6) |
                 (s[i+3] & 0x3F);
            i += 4;
        }
        else {
            // 不正な UTF-8 → U+FFFD に置き換え
            cp = 0xFFFD;
            i += 1;
        }

        out.push_back(cp);
    }

    return out;
}


/*--------------------------------------------------------------
 * Codepoint → glyph index
 *--------------------------------------------------------------*/

std::vector<int> ClientUnicode::codepointsToAtlasGlyphs(const std::vector<uint32_t> &cps) const
{
    // TODO: implement atlas glyph lookup
    return {};
}

std::vector<int> ClientUnicode::codepointsToGlyphs(const std::vector<uint32_t> &cps) const
{
    // TODO: implement TTF glyph lookup
    return {};
}

/*--------------------------------------------------------------
 * High-level API (UTF-8 → glyph indices)
 *--------------------------------------------------------------*/

std::vector<int> ClientUnicode::textToAtlasGlyphs(const std::string &text) const
{
    // TODO: implement UTF-8 → atlas glyph pipeline
    return {};
}

std::vector<int> ClientUnicode::textToGlyphs(const std::string &text) const
{
    // TODO: implement UTF-8 → TTF glyph pipeline
    return {};
}

/*--------------------------------------------------------------
 * Query functions
 *--------------------------------------------------------------*/

bool ClientUnicode::hasAtlasGlyph(uint32_t cp) const
{
    // TODO: check atlas map
    return false;
}

bool ClientUnicode::hasTTFGlyph(uint32_t cp) const
{
    // TODO: check TTF map
    return false;
}

int ClientUnicode::getAtlasGlyphIndex(uint32_t cp) const
{
    // TODO: lookup atlas glyph index
    return -1;
}

int ClientUnicode::getTTFGlyphIndex(uint32_t cp) const
{
    // TODO: lookup TTF glyph index
    return -1;
}

/*--------------------------------------------------------------
 * Internal lookup helpers
 *--------------------------------------------------------------*/

int ClientUnicode::lookupAtlasGlyph(uint32_t cp) const
{
    // TODO: internal atlas lookup
    return -1;
}

int ClientUnicode::lookupTTFGlyph(uint32_t cp) const
{
    // TODO: internal TTF lookup
    return -1;
}
