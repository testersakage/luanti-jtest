// src/client/utf8_fontengine.cpp
// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2014 sapier <sapier at gmx dot net>

// 1. Irrlichtの型を使うための「親玉」を一番上に持ってくる
#include "irrlichttypes.h" 
#include "utf8_fontatlas.h"
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

// ★ staticメンバの実体を定義
UTF8FontAtlas* UTF8FontEngine::m_atlas = nullptr;

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
	if (!m_atlas) m_atlas = new UTF8FontAtlas();
	if (!dest_img_ptr) return;
	video::IImage *dest_img = reinterpret_cast<video::IImage*>(dest_img_ptr);

	// --- 1. 終端チェックと外枠剥離 ---
	// 最後に ']' があることを前提に、その手前までを解析対象とする
	if (command.empty() || command.front() != '[' || command.back() != ']') {
		// 構文エラーをチャット/ログに通知
		actionstream << "UTF8FontEngine: Syntax Error (Missing []): " << command << std::endl;
		return;
	}

	// [ ] を取り除いた中身: "utf8combine:115x115:15,2@RRGGBB=Text"
	std::string inner = command.substr(1, command.length() - 2);

	// --- 2. 大ブロックの分離 (コロンによる分割) ---
	size_t first_colon = inner.find(':');
	size_t second_colon = inner.find(':', first_colon + 1);

	if (first_colon == std::string::npos || second_colon == std::string::npos) {
		actionstream << "UTF8FontEngine: Syntax Error (Missing colons): " << command << std::endl;
		return;
	}

	// 各ブロックの抽出
	std::string size_part = inner.substr(first_colon + 1, second_colon - first_colon - 1); // "115x115"
	std::string content_part = inner.substr(second_colon + 1); // "15,2@RRGGBB=Text"

	// --- 3. 詳細パース：キャンバスサイズ ---
	u32 canvas_w = 115, canvas_h = 115;
	size_t x_pos = size_part.find('x');
	if (x_pos != std::string::npos) {
		canvas_w = mystoi(size_part.substr(0, x_pos));
		canvas_h = mystoi(size_part.substr(x_pos + 1));
	}

	// キャンバスの初期化
	dest_img->fill(video::SColor(0, 0, 0, 0));

	// --- 4. 詳細パース：座標・色・テキスト ---
	size_t equal = content_part.find('=');
	if (equal == std::string::npos) {
		actionstream << "UTF8FontEngine: Syntax Error (Missing '='): " << command << std::endl;
		return;
	}

	std::string settings = content_part.substr(0, equal);
	std::string raw_text = content_part.substr(equal + 1); // ここで純粋な「中身」が確定

	u32 start_x = 15, start_y = 2;
	video::SColor target_color(255, 255, 255, 255); // デフォルト白
	size_t at_sign = settings.find('@');
	size_t comma = settings.find(',');

	// カラーコード (@RRGGBB) の解析
	if (at_sign != std::string::npos) {
		std::string hex_str = settings.substr(at_sign + 1);
		try {
			unsigned long color_val = std::stoul(hex_str, nullptr, 16);
			target_color = video::SColor(255, 
				(color_val >> 16) & 0xFF, 
				(color_val >> 8) & 0xFF, 
				color_val & 0xFF);
		} catch (...) {
			target_color = video::SColor(255, 255, 255, 255);
		}
	}

	// 座標の解析
	if (comma != std::string::npos) {
		start_x = mystoi(settings.substr(0, comma));
		// Y座標はカンマから「@」の手前まで、または末尾まで
		size_t y_end = (at_sign != std::string::npos) ? at_sign : settings.length();
		start_y = mystoi(settings.substr(comma + 1, y_end - (comma + 1)));
	}

	if (raw_text.empty()) return;
	// renderUtf8Combine 内、raw_text 確定直後
	warningstream << "[utf8_debug] text_size: " << raw_text.length() << " first_char: " << (int)raw_text[0] << std::endl;

	// 2. 動的な改行幅の計算
	int wrap_width = canvas_w - (start_x * 2); 
	if (wrap_width <= 0) wrap_width = canvas_w;
	std::vector<std::string> lines = utf8_53::get_lines(raw_text, wrap_width);

	u32 y_cursor = start_y;
	for (const std::string &line_str : lines) {
		// ★ 行の処理に入る「直前」で、今から書く行がハミ出さないか厳格にチェック
		if (y_cursor + 12 > canvas_h) break; 

		u32 x_cursor = start_x;
		std::vector<int> cps = utf8_53::to_codepoints(line_str);

		for (int cp : cps) {
			u32 current_w = 12; 
			try {
				ImageRGBA glyph = m_atlas->getGlyphImage(cp);
				// 右端ブレーキ：次の文字がキャンバスからはみ出すならその行は終了
				if (x_cursor + glyph.width > canvas_w) break;
				
				current_w = glyph.width;

				for (int gy = 0; gy < 12; gy++) {
					for (int gx = 0; gx < (int)glyph.width; gx++) {
						int i = (gy * glyph.width + gx) * 4;
						
						// ★ ここで色を適用！
						// アトラスから Alpha だけ抜き出し、色は target_color を使う
						video::SColor pixel_color = target_color;
						pixel_color.setAlpha(glyph.data[i + 3]); 

						if (pixel_color.getAlpha() > 0) {
							dest_img->setPixel(x_cursor + gx, y_cursor + gy, pixel_color);
						}
					}
				}
			} catch (...) {}
			
			// 文字を一つ書き終えたら、その幅分だけ右へ進める
			x_cursor += current_w; 
			// ★ 130 固定を canvas_w に変更！
			if (x_cursor >= canvas_w) break; 
		}
		
		// 次の行へ（12px+1pxの余白 = 13px）
		y_cursor += 13; 
//		if (y_cursor + 12 > canvas_h) break; 
	}
}
 