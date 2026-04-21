// src/client/utf8_fontatlas.cpp
#include "utf8_fontatlas.h"
#include "client/renderingengine.h"
#include "irrlichttypes.h"
#include "IImage.h"
#include "IVideoDriver.h"
#include "utf8_53.h"
#include <stdexcept>

// ★ static メンバ変数の実体定義を忘れずに！
std::map<int, ImageRGBA> UTF8FontAtlas::m_atlas_pages;

// 1. 切り出し関数を可変幅(target_w)対応に
static ImageRGBA crop_glyph_custom(const ImageRGBA &src, int gx, int gy, int target_w, int target_h)
{
	ImageRGBA out;
	out.width = target_w;
	out.height = target_h; // ★ 12 固定から引数へ
	out.data.resize(target_w * target_h * 4);

	for (int yy = 0; yy < target_h; yy++) { // ★ ループも target_h 回に
		for (int xx = 0; xx < target_w; xx++) {
			int sx = gx + xx;
			int sy = gy + yy;
			
			// 念のための境界チェック
			if (sx < 0 || sx >= (int)src.width || sy < 0 || sy >= (int)src.height)
				continue;

			int src_i = (sy * src.width + sx) * 4;
			int dst_i = (yy * target_w + xx) * 4;
			
			for (int k = 0; k < 4; k++) {
				out.data[dst_i + k] = src.data[src_i + k];
			}
		}
	}
	return out;
}

/* --- 1. 切り出し関数（上に置くか、プロトタイプ宣言が必要） --- */
/**
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
			if (sx >= src.width || sy >= src.height) continue;

			int src_i = (sy * src.width + sx) * 4;
			int dst_i = (yy * 12 + xx) * 4;
			for (int k = 0; k < 4; k++) out.data[dst_i + k] = src.data[src_i + k];
		}
	}
	return out;
}
*/

/* --- 2. PNGロード関数 --- */
static ImageRGBA load_png_rgba(const std::string &path)
{
	video::IVideoDriver *driver = RenderingEngine::get_video_driver();
	if (!driver) throw std::runtime_error("No driver");

	video::ITexture* tex = driver->getTexture(path.c_str());
	if (!tex) throw std::runtime_error("Cannot load: " + path);

	video::IImage *img = driver->createImage(tex, core::position2d<s32>(0,0), tex->getSize());
	ImageRGBA out;
	out.width = img->getDimension().Width;
	out.height = img->getDimension().Height;
	out.data.resize(out.width * out.height * 4);

	for (u32 y = 0; y < (u32)out.height; y++) {
		for (u32 x = 0; x < (u32)out.width; x++) {
			video::SColor c = img->getPixel(x, y);
			u32 i = (y * out.width + x) * 4;
			out.data[i + 0] = c.getRed();
			out.data[i + 1] = c.getGreen();
			out.data[i + 2] = c.getBlue();
			out.data[i + 3] = c.getAlpha();
		}
	}
	img->drop();
	return out;
}

/* --- 3. ページロード（キャッシュ付き） --- */
const ImageRGBA &UTF8FontAtlas::loadPage(int page)
{
	auto it = m_atlas_pages.find(page);
	if (it != m_atlas_pages.end()) return it->second;

	char filename[512];
	snprintf(filename, sizeof(filename), "unicode_page_%02x.png", page);

	// 【ログ】新しくロードする瞬間を記録
	infostream << "UTF8FontAtlas: Attempting to load new page: " << filename << std::endl;
	// utf8_fontatlas.cpp の loadPage 内を一時的に改造
	snprintf(filename, sizeof(filename), "C:/msys64/home/localuser/luanti/games/mineclonia-jtest/mods/ITEMS/mcl_signs/textures/unicode_page_%02x.png", page);

	ImageRGBA img = load_png_rgba(filename);
	m_atlas_pages[page] = std::move(img);
	return m_atlas_pages[page];
}

/* --- 4. メインの切り出し関数 --- */
ImageRGBA UTF8FontAtlas::getGlyphImage(int codepoint)
{
    if (codepoint < 0) throw std::runtime_error("Invalid CP");

    // 【ログ】制御文字などの低位コードポイント報告
    if (codepoint < 32) {
        infostream << "UTF8FontAtlas: Low codepoint (possible ghost): 0x" 
                   << std::hex << codepoint << std::dec << std::endl;
    }

    int page = codepoint / 256;
    int index = codepoint % 256;

    // 1. ページのロード
    const ImageRGBA &atlas = loadPage(page);

    // 2. ★ アトラスの実サイズから「1行の高さ」を動的に算出
    // アトラスは 256文字(32x8グリッド)なので、高さ / 8行 でステップを出す
    // 96pxなら 12、112pxなら 14 と自動判定される
    u32 actual_line_h = atlas.height / 8;

    // 3. ★ 職人の黄金律を適用 (幅の判定)
    // ASCII(00) および 半角カナ(FF) の範囲を 6px、それ以外を 12px とする
    int current_w = 12;
    if (page == 0x00 && index <= 0x7F) {
        current_w = 6;
    } else if (page == 0xFF && (index >= 0x61 && index <= 0x9F)) {
        current_w = 6;
    }

    // 4. 座標計算
    // 横(gx)は常に 12px 刻みのグリッド
    // 縦(gy)はステップ高(12 or 14)に応じた位置を計算
    int gx = (index % 32) * 12;
    int gy = (index / 32) * actual_line_h;

    // 5. 切り出し実行 (高さは画像の実態に合わせる)
    return crop_glyph_custom(atlas, gx, gy, current_w, actual_line_h);
}
