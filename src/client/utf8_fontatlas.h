#pragma once

#include "irrlichttypes.h"
#include <vector>
#include <string>
#include <list>
#include <map>

// 0=無効 , 1=有効
#define UTF8_ATLAS 0
#define UTF8_SDL2_ATLAS 1
#define UTF8_SDL2_FREETYPE 0

//#if UTF8_ATLAS
// シンプルなピクセルバッファ構造体
struct ImageRGBA {
	int width;
	int height;
	std::vector<unsigned char> data;
};
//#endif

class UTF8FontAtlas {
public:
	UTF8FontAtlas() = default;
	~UTF8FontAtlas() = default;

#if UTF8_ATLAS
	// コードポイント(Unicode)を受け取り、12x12px のピクセルデータを返します。
	// 内部でアトラス(unicode_page_xx.png)のロードと座標計算(32x8)を行います。
	static ImageRGBA getGlyphImage(int codepoint);

	// Lua API
	static u32 getPageCache();
#endif

#if UTF8_SDL2_ATLAS
	// Engineから呼ばれる窓口。内部で getGlyphImageEX を使って FTCachedGlyph を組み立てる
//	static bool extractAtlasGlyph(u32 code, FTCachedGlyph &out_glyph);
	static ImageRGBA getGlyphImageEX(int codepoint);

	// Lua API
	//static u32 getPageCache();
#endif

private:
//	static ImageRGBA crop_glyph_12x12(const ImageRGBA &src, int gx, int gy);
	static ImageRGBA crop_glyph_custom(const ImageRGBA &src, int gx, int gy, int target_w, int target_h);

#if UTF8_ATLAS
	// --- Standard Atlas Cache管理 ---
	static std::map<int, ImageRGBA> m_st_pages; // ページ番号 -> ページ画像
	static std::list<int> m_st_page_order;      // 登録順（FIFO用）
	static size_t m_st_max_pages;           // minetest.confから読み込む上限

	// 指定したページ(0-255)のアトラス画像をロードし、メモリに保持します。
	static ImageRGBA load_png_rgba(const std::string &path);
	static const ImageRGBA &loadPage(int page);

	// ロード済みのアトラス画像をページ番号をキーにキャッシュ
	// (一度読んだページは二度とディスクから読まない)
	//	static std::map<int, ImageRGBA> m_atlas_pages;
#endif

#if UTF8_SDL2_ATLAS
	static std::map<int, ImageRGBA> m_char_cache; // 文字画像
	static std::map<int, ImageRGBA> m_page_cache; // ページ画像
	static std::list<int> m_ex_page_order;        // 登録順（FIFO用）
	static size_t m_ex_max_pages;           // minetest.confから読み込む上限

	static ImageRGBA load_png_rgbaEX(const std::string &path);
	static const ImageRGBA &loadPageEX(int page);
#endif

};
