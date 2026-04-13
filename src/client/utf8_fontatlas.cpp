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
static ImageRGBA crop_glyph_custom(const ImageRGBA &src, int gx, int gy, int target_w)
{
	ImageRGBA out;
	out.width = target_w;
	out.height = 12;
	out.data.resize(target_w * 12 * 4);

	for (int yy = 0; yy < 12; yy++) {
		for (int xx = 0; xx < target_w; xx++) {
			int sx = gx + xx;
			int sy = gy + yy;
			if (sx >= src.width || sy >= src.height) continue;

			int src_i = (sy * src.width + sx) * 4;
			int dst_i = (yy * target_w + xx) * 4;
			for (int k = 0; k < 4; k++) out.data[dst_i + k] = src.data[src_i + k];
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
	// 【ログ】0などの低位コードポイントが来たら報告
	if (codepoint < 32) {
		infostream << "UTF8FontAtlas: Low codepoint (possible ghost): 0x" 
				   << std::hex << codepoint << std::dec << std::endl;
	}

	if (codepoint < 0) throw std::runtime_error("Invalid CP");

	int page = codepoint / 256;
	int index = codepoint % 256;

	// ASCIIページ(00)なら幅6px、それ以外は12pxとして出荷
	int current_w = (page == 0) ? 6 : 12;

	// 座標計算
	int gx = (index % 32) * 12; // 読み取り開始位置は常に12px刻み
	int gy = (index / 32) * 12;

	const ImageRGBA &atlas = loadPage(page);

	// 自作の可変幅切り出し関数を呼ぶ
	return crop_glyph_custom(atlas, gx, gy, current_w);	
//	return crop_glyph_12x12(atlas, gx, gy);
}
