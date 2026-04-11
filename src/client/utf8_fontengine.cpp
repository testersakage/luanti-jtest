// src/client/utf8_fontengine.cpp
// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2014 sapier <sapier at gmx dot net>

#include "utf8_fontengine.h"

#include "fontengine.h"       // g_fontengine にアクセスするため
#include "util/string.h"       // utf8_to_wide 用
#include "../utf8_53.h"        // あなたが作った UTF-8 ロジック

// これを追加（Irrlichtのフォントの実体定義を読み込む）
#include <IGUIFont.h> 

#include <vector>
#include <string>
/**
 * 補足:
 * RenderingEngine や IGUIFont などの複雑な部分は 
 * g_fontengine->getFont() が内部で処理してくれるため、
 * 私たちが直接触る必要はありません。
 */
 
u32 UTF8FontEngine::getTextWidth(const std::string &text)
{
    if (text.empty())
        return 0;

    // getFont() が返す IGUIFont には getTextWidth はなく、
    // getDimension で core::dimension2d<u32> を取得し、その Width を参照する
    return g_fontengine->getFont()->getDimension(utf8_to_wide(text).c_str()).Width;
}

std::vector<std::string> UTF8FontEngine::wrapText(const std::string &text, u32 max_pixel_width)
{
	std::vector<std::string> lines;
	if (text.empty())
		return lines;

	std::string current_line = "";
	size_t pos = 0;
	int cp;

	while (utf8_53::get_next_char(text, pos, cp)) {
		std::string next_char;
		utf8_53::push_char(next_char, cp);

		// 現在の行に次の1文字を足した時の「本物のピクセル幅」を確認
		if (getTextWidth(current_line + next_char) > max_pixel_width) {
			// 幅を超えたら現在の行を確定させ、新しい行を開始
			lines.push_back(current_line);
			current_line = next_char;
		} else {
			current_line += next_char;
		}
	}

	if (!current_line.empty())
		lines.push_back(current_line);

	return lines;
}

std::string UTF8FontEngine::truncateText(const std::string &text, u32 max_pixel_width)
{
	std::string result = "";
	size_t pos = 0;
	int cp;

	while (utf8_53::get_next_char(text, pos, cp)) {
		std::string next_char;
		utf8_53::push_char(next_char, cp);

		// 次の1文字を足すと幅を超えるなら、そこで終了
		if (getTextWidth(result + next_char) > max_pixel_width)
			break;

		result += next_char;
	}

	return result;
}
 
 