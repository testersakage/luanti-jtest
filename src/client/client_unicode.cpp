#include "client_unicode.h"
#include "client/client.h"
#include "client/renderingengine.h"  // RenderingEngine
#include "irrlichttypes.h"
#include "IImage.h"
#include "IVideoDriver.h"

#include <fstream>
#include <stdexcept>

#include "cjk_common.h"  // ★ UTF-8 / Unicode 共通ロジック

/*--------------------------------------------------------------
 * Loading mapping files
 *--------------------------------------------------------------*/

bool ClientUnicode::loadCJKAtlasMap(const std::string &path)
{
    // font2unimg の出力形式：
    //   <hex codepoint> <gx> <gy>
    //   3042  12 24
    //   3044  24 24

    std::ifstream f(path);
    if (!f.is_open())
        return false;

    uint32_t cp;
    int gx, gy;

    while (f >> std::hex >> cp >> gx >> gy) {
        // cp → (gx, gy) を保存
        m_cjk_atlas_map[cp] = {gx, gy};
    }

    return true;
}

bool ClientUnicode::loadUnicodeMap(const std::string &path)
{
    // TTF 用（未実装）
    return false;
}

/*--------------------------------------------------------------
 * UTF-8 decoding
 *--------------------------------------------------------------*/
std::vector<uint32_t> ClientUnicode::utf8ToCodepoints(const std::string &text) const
{
    // ★ 旧実装を廃止し、cjk_common の UTF-8 デコードを使用
    return cjk::utf8_to_codepoints(text);
}

/*--------------------------------------------------------------
 * Codepoint → glyph index
 *--------------------------------------------------------------*/

std::vector<int> ClientUnicode::codepointsToAtlasGlyphs(const std::vector<uint32_t> &cps) const
{
    std::vector<int> out;
    out.reserve(cps.size());

    for (uint32_t cp : cps) {
        out.push_back(lookupAtlasGlyph(cp));
    }

    return out;
}

std::vector<int> ClientUnicode::codepointsToGlyphs(const std::vector<uint32_t> &cps) const
{
    // TTF 未実装
    return {};
}

/*--------------------------------------------------------------
 * High-level API (UTF-8 → glyph indices)
 *--------------------------------------------------------------*/

std::vector<int> ClientUnicode::textToAtlasGlyphs(const std::string &text) const
{
    return codepointsToAtlasGlyphs(utf8ToCodepoints(text));
}

std::vector<int> ClientUnicode::textToGlyphs(const std::string &text) const
{
    return codepointsToGlyphs(utf8ToCodepoints(text));
}

/*--------------------------------------------------------------
 * Query functions
 *--------------------------------------------------------------*/

bool ClientUnicode::hasAtlasGlyph(uint32_t cp) const
{
    return m_cjk_atlas_map.find(cp) != m_cjk_atlas_map.end();
}

bool ClientUnicode::hasTTFGlyph(uint32_t cp) const
{
    return m_unicode_map.find(cp) != m_unicode_map.end();
}

int ClientUnicode::getAtlasGlyphIndex(uint32_t cp) const
{
    return lookupAtlasGlyph(cp);
}

int ClientUnicode::getTTFGlyphIndex(uint32_t cp) const
{
    return lookupTTFGlyph(cp);
}

/*--------------------------------------------------------------
 * Internal lookup helpers
 *--------------------------------------------------------------*/

int ClientUnicode::lookupAtlasGlyph(uint32_t cp) const
{
    auto it = m_cjk_atlas_map.find(cp);
    if (it == m_cjk_atlas_map.end())
        return -1;

    int gx = it->second.first;
    int gy = it->second.second;

    // 12×12px グリフ → 32列 × 8行
    int col = gx / 12;
    int row = gy / 12;

    return row * 32 + col;
}

int ClientUnicode::lookupTTFGlyph(uint32_t cp) const
{
    // 未実装
    return -1;
}

/*--------------------------------------------------------------
 * ATLAS PNG を RGBA に読み込む（IrrlichtMt 使用）  (client-side only)
 *--------------------------------------------------------------*/
// ※ stb_image を廃止し、RenderingEngine から video driver を取得するように変更
static ImageRGBA load_png_rgba(const std::string &path)
{
    // 【変更点】getClient() は使えないため、RenderingEngine から driver を取得する
    video::IVideoDriver *driver = RenderingEngine::get_video_driver();
    if (!driver)
        throw std::runtime_error("No video driver");

    // 【変更点】stbi_load の代わりに IrrlichtMt の createImageFromFile を使用
    video::IImage *img = driver->createImageFromFile(path.c_str());
    if (!img)
        throw std::runtime_error("Cannot load PNG: " + path);

    ImageRGBA out;
    // 【変更点】IImage から width/height を取得
    out.width = img->getDimension().Width;
    out.height = img->getDimension().Height;

    // RGBA バッファを確保
    out.data.resize(out.width * out.height * 4);
    // 【変更点】stbi_load の raw バッファではなく、IImage::getPixel で RGBA を取得
    for (int y = 0; y < out.height; y++) {
        for (int x = 0; x < out.width; x++) {
            video::SColor c = img->getPixel(x, y);
            int i = (y * out.width + x) * 4;
            out.data[i + 0] = c.getRed();
            out.data[i + 1] = c.getGreen();
            out.data[i + 2] = c.getBlue();
            out.data[i + 3] = c.getAlpha();
        }
    }

    // 【変更点】stbi_image_free の代わりに Irrlicht の drop() を使用
    img->drop();

    return out;
}


const ImageRGBA &ClientUnicode::loadAtlasPage(int page)
{
    auto it = m_atlas_pages.find(page);
    if (it != m_atlas_pages.end())
        return it->second;

    char filename[64];
    snprintf(filename, sizeof(filename), "textures/unicode_page_%02X.png", page);

    ImageRGBA img = load_png_rgba(filename);
    m_atlas_pages[page] = std::move(img);

    return m_atlas_pages[page];
}

/*--------------------------------------------------------------
 * glyph index → 12×12 RGBA image
 *--------------------------------------------------------------*/

static ImageRGBA crop_glyph_12x12(const ImageRGBA &src, int gx, int gy)
{
    ImageRGBA out;
    out.width = 12;
    out.height = 12;
    out.data.resize(12 * 12 * 4);

    for (int yy = 0; yy < 12; yy++) {
        for (int xx = 0; xx < 12; xx++) {
            int sx = gx + xx;
            int sy = gy + yy;

            int src_i = (sy * src.width + sx) * 4;
            int dst_i = (yy * 12 + xx) * 4;

            out.data[dst_i + 0] = src.data[src_i + 0];
            out.data[dst_i + 1] = src.data[src_i + 1];
            out.data[dst_i + 2] = src.data[src_i + 2];
            out.data[dst_i + 3] = src.data[src_i + 3];
        }
    }

    return out;
}

ImageRGBA ClientUnicode::getAtlasGlyphImage(int glyph_index) const
{
    if (glyph_index < 0)
        throw std::runtime_error("Invalid glyph index");

    int page = glyph_index / 256;
    int index = glyph_index % 256;

    int col = index % 32;
    int row = index / 32;

    int gx = col * 12;
    int gy = row * 12;

    const ImageRGBA &atlas = const_cast<ClientUnicode*>(this)->loadAtlasPage(page);

    return crop_glyph_12x12(atlas, gx, gy);
}
