// src/client/utf8_fontengine.cpp
// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2014 sapier <sapier at gmx dot net>

// 1. Irrlichtの型を使うための「親玉」を一番上に持ってくる
#include "irrlichttypes.h" 
#include "utf8_fontengine.h"

#include "fontengine.h"       // g_fontengine にアクセスするため
#include "util/string.h"       // utf8_to_wide 用
#include "../utf8_53.h"        // あなたが作った UTF-8 ロジック

#include <IVideoDriver.h> 
#include <ITexture.h> 
#include "renderingengine.h"

// これを追加（Irrlichtのフォントの実体定義を読み込む）
#include <IGUIFont.h> 
#include <IGUIFontBitmap.h> 
#include <IGUISpriteBank.h> 
#include <IImage.h> // IImage の操作に必要
#include <SColor.h>

#include <sstream> // これを忘れずに！
#include <vector>
#include <string>

/**
 * 補足:
 * RenderingEngine や IGUIFont などの複雑な部分は 
 * g_fontengine->getFont() が内部で処理してくれるため、
 * 私たちが直接触る必要はありません。
 */
// 1. インクルードの後にこれを追加（irr:: を省略できるようにする）
//using namespace irr;
 
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

//   関数 UTF8FontEngine::generateLines(const std::string &text, u32 max_pixel_width)
 std::vector<std::string> UTF8FontEngine::generateLines(const std::string &text, u32 max_pixel_width)
{
	std::vector<std::string> final_lines;
	std::stringstream ss(text);
	std::string segment;

	// まずは手動改行(\n)で分割
	while (std::getline(ss, segment, '\n')) {
		// 分割された各行に対して、自作の wrapText (自動改行) を適用
		std::vector<std::string> wrapped = UTF8FontEngine::wrapText(segment, max_pixel_width);
		
		if (wrapped.empty()) {
			// 空行（改行だけの行）を維持する
			final_lines.push_back("");
		} else {
			// 自動改行された結果をすべて追加
			final_lines.insert(final_lines.end(), wrapped.begin(), wrapped.end());
		}
	}
	
	// 入力文字列が改行で終わっている場合、最後の空行を拾うためのケア
	if (!text.empty() && text.back() == '\n') {
		final_lines.push_back("");
	}

	return final_lines;
}

//  関数 void* UTF8FontEngine::getGlyphImage(wchar_t c)
void* UTF8FontEngine::getGlyphImage(wchar_t c)
{
    video::IVideoDriver* driver = RenderingEngine::get_video_driver();
    gui::IGUIFont *font = g_fontengine->getFont(12); // 12px
    if (!font) font = g_fontengine->getFont(); // 失敗したらデフォルトにフォールバック
    if (!driver || !font) return nullptr;

    wchar_t temp_str[] = {c, 0};
    core::dimension2du dim = font->getDimension(temp_str);
    if (dim.Width == 0) return nullptr;

    // 1. 文字を描くための「一時的なテクスチャ（VRAM上のキャンバス）」を作る
    video::ITexture* render_tex = driver->addRenderTargetTexture(dim, "glyph_tmp_rt");
    if (!render_tex) return nullptr;

    // 2. 描画先を画面から「このテクスチャ」へ切り替える
    driver->setRenderTarget(render_tex, true, true, video::SColor(0,0,0,0));

    // 3. 描画命令！ (これでテクスチャに文字が書き込まれる)
    font->draw(temp_str, core::rect<s32>(0, 0, dim.Width, dim.Height), 
               video::SColor(255, 255, 255, 255));

    // 4. 描画先を元（画面）に戻す
    driver->setRenderTarget(nullptr);

    // 5. 【重要】テクスチャ（VRAM）から画像（RAM/IImage）へピクセルを引き抜く
    video::IImage* glyph_img = driver->createImage(render_tex, core::position2d<s32>(0,0), dim);

    if (glyph_img) {
        // --- ここでアルファ補正（2値化）を注入！ ---
        core::dimension2du size = glyph_img->getDimension();
        for (u32 y = 0; y < size.Height; y++) {
            for (u32 x = 0; x < size.Width; x++) {
                video::SColor pixel = glyph_img->getPixel(x, y);
                
                // しきい値（例えば30）以上なら、完全不透明(255)にブースト
                // これで「消えかかっていた横棒」がクッキリ浮かび上がります
                if (pixel.getAlpha() > 79) {
                    pixel.setAlpha(255);
                    // 看板側で色が反転したり黒くなったりするのを防ぐため、
                    // RGBも最大値（白）にしておくのが安全です
                    pixel.setRed(255);
                    pixel.setGreen(255);
                    pixel.setBlue(255);
                } else {
                    pixel.setAlpha(0);
                }
                glyph_img->setPixel(x, y, pixel);
            }
        }
    }

    // 6. 使い終わったテクスチャを消去
    driver->removeTexture(render_tex);

    return (void*)glyph_img;
}
// 関数 UTF8FontEngine::renderUtf8Combine(void *dest_img_ptr, const std::string &command)
void UTF8FontEngine::renderUtf8Combine(void *dest_img_ptr, const std::string &command)
{
	if (!dest_img_ptr) return;
	video::IImage *dest_img = reinterpret_cast<video::IImage*>(dest_img_ptr);

	// 1. 看板キャンバスの初期化（全透明）
	dest_img->fill(video::SColor(0, 0, 0, 0));

	// 2. [utf8combine:115x115: 以降の生テキストを抽出
	size_t last_colon = command.find_last_of(':');
	if (last_colon == std::string::npos) return;
	std::string raw_text = command.substr(last_colon + 1);
	if (raw_text.empty()) return;

	// 3. Luaのお手本通りに改行リストを生成（幅100px制限）
	std::vector<std::string> lines = UTF8FontEngine::generateLines(raw_text, 114);

	// 4. 描画位置の黄金比設定 (y=2pxから開始、13pxステップ)
	u32 y_cursor = 2;
	for (const std::string &line_str : lines) {
		std::wstring wline = utf8_to_wide(line_str);
		u32 x_cursor = 16; // 左端の余白

		for (wchar_t c : wline) {
			video::IImage* glyph = (video::IImage*)UTF8FontEngine::getGlyphImage(c);
			if (glyph) {
				// 看板キャンバスへスタンプ
				glyph->copyTo(dest_img, core::position2d<s32>(x_cursor, y_cursor));
				x_cursor += glyph->getDimension().Width;
				glyph->drop();
			}
		}
		// 次の行へ移動。看板の高さを考慮して一定位置で打ち切り
		y_cursor += 13; 
		if (y_cursor > 54) break; 
	}
}

 