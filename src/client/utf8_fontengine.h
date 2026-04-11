// src/client/utf8_fontengine.h
#pragma once

#include "irrlichttypes.h"
#include <string>
#include <vector>

/**
 * UTF8FontEngine
 * 
 * 既存の FontEngine をラップし、UTF-8文字列に対して
 * ピクセル精度の計算機能を提供します。
 */
class UTF8FontEngine {
public:
	/**
	 * 指定したUTF-8文字列が、現在のフォント設定で
	 * 何ピクセルの幅になるかを返します。
	 */
	static u32 getTextWidth(const std::string &text);

	/**
	 * 指定したピクセル幅を超えないように、
	 * UTF-8文字列に改行(\n)を挿入、または分割したリストを返します。
	 */
	static std::vector<std::string> wrapText(const std::string &text, u32 max_pixel_width);

	/**
	 * 指定したピクセル幅に収まる最大長まで
	 * UTF-8文字列を切り詰めて返します。
	 */
	static std::string truncateText(const std::string &text, u32 max_pixel_width);

private:
	// インスタンス化させないためのプライベートコンストラクタ
	UTF8FontEngine() {}
};
