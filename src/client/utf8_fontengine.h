// src/client/utf8_fontengine.h
#pragma once

//#include "../irrlichttypes.h"
//#include "../irrlichttypes_video.h" // これが必要
#include <string>
#include <vector>

class UTF8FontEngine {
public:
	/**
	 * 指定したUTF-8文字列が、現在のフォント設定で
	 * 何ピクセルの幅になるかを返します。
	 */
	static unsigned int getTextWidth(const std::string &text);

	/**
	 * 指定したピクセル幅を超えないように、
	 * UTF-8文字列に改行(\n)を挿入、または分割したリストを返します。
	 */
	static std::vector<std::string> wrapText(const std::string &text, unsigned int max_pixel_width);

	/**
	 * 指定したピクセル幅に収まる最大長まで
	 * UTF-8文字列を切り詰めて返します。
	 */
	static std::string truncateText(const std::string &text, unsigned int max_pixel_width);

	/**
	 * 手動改行(\n)を考慮しつつ、指定幅で自動改行された行リストを生成します。
	 * mcl_signs の init.lua のロジックを C++ で再現したものです。
	 */
	static std::vector<std::string> generateLines(const std::string &text, unsigned int max_pixel_width);

	/**
	 * imagesource.cpp からの [utf8combine 命令を受け取り、看板をレンダリングします。
	 */
	static void renderUtf8Combine(void *dest_img_ptr, const std::string &command);

	/**
	 * フォントエンジンから指定した文字のグリフ（画像）を取得します。
	 * 仮想スクリーンバッファ方式により、フォントの種類を問わずキャプチャ可能です。
	 */
	static void* getGlyphImage(wchar_t c);

private:
	// インスタンス化させないためのプライベートコンストラクタ
	UTF8FontEngine() {}
};
