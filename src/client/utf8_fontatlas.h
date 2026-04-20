#pragma once

#include "irrlichttypes.h"
#include <vector>
#include <string>
#include <map>

// シンプルなピクセルバッファ構造体
struct ImageRGBA {
	int width;
	int height;
	std::vector<unsigned char> data;
};

class UTF8FontAtlas {
public:
	UTF8FontAtlas() = default;
	~UTF8FontAtlas() = default;

	/**
	 * コードポイント(Unicode)を受け取り、12x12px のピクセルデータを返します。
	 * 内部でアトラス(unicode_page_xx.png)のロードと座標計算(32x8)を行います。
	 */
	static ImageRGBA getGlyphImage(int codepoint);

private:
//	static ImageRGBA crop_glyph_12x12(const ImageRGBA &src, int gx, int gy);
	/**
	 * 指定したページ(0-255)のアトラス画像をロードし、メモリに保持します。
	 */
	static const ImageRGBA &loadPage(int page);

	// ロード済みのアトラス画像をページ番号をキーにキャッシュ
	// (一度読んだページは二度とディスクから読まない)
	static std::map<int, ImageRGBA> m_atlas_pages;
};
