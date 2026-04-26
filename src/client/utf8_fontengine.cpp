// src/client/utf8_fontengine.cpp
// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2014 sapier <sapier at gmx dot net>
#define FT_DEBUG_VIEW 0 // 1 にすると診断モード起動！

// 1. Irrlichtの型を使うための「親玉」を一番上に持ってくる
#include "irrlichttypes.h" 
#include "utf8_fontatlas.h"
#include "utf8_fontengine.h"
#include "porting.h" // これが必要
#include "filesys.h" // これで DIR_DELIM が使えるようになる

#include "fontengine.h"	   // g_fontengine にアクセスするため
#include "util/string.h"	   // utf8_to_wide 用
#include "../utf8_53.h"		// あなたが作った UTF-8 ロジック
#include "../script/common/l_utf8sign.h"		// 追加

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

#include "sdl2_font.h" // これを忘れずに

struct RenderTask { // TTF用の構造体
    std::vector<u32> codes;
    u32 start_x = 0;
    u32 start_y = 0;
};

UTF8FontEngine::UTF8FontEngine()
{
	// コンストラクタでは「初期化（init）」をあえてしない！
	// すべては renderutf8combineft の「遅延初期化」に任せる。
	// これが、今のあなたの環境で「16px」を確実に通すための最善策です。
	
	infostream << "UTF8FontEngine: Created. Initialization deferred until first render." << std::endl;
}

UTF8FontEngine::~UTF8FontEngine()
{
	// クリーンアップも忘れずに
	sdl2_font::cleanup();
}

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




// UTF8FontEngine.cpp 内の parseUtf8Spec
RenderTask UTF8FontEngine::parseUtf8Spec(const std::string &spec)
{
	RenderTask task; // メモ用紙を実体化

	size_t size_pos = spec.find(":");
	size_t coord_pos = spec.find(":", size_pos + 1);
	size_t color_pos = spec.find("@", coord_pos + 1);

	if (coord_pos != std::string::npos && color_pos != std::string::npos) {
		std::string coords = spec.substr(coord_pos + 1, color_pos - (coord_pos + 1));
		size_t comma = coords.find(",");
		if (comma != std::string::npos) {
			// ここで開始座標(例: 15, 14)を取得！
			u32 start_x = std::stoul(coords.substr(0, comma));
			u32 start_y = std::stoul(coords.substr(comma + 1));

			// これを cursor_x, cursor_y の初期値にセットする
			task.start_x = start_x;
			task.start_y = start_y;
		}
	}

	std::vector<u32> codes;
	size_t start_pos = spec.find("UTF8:");
	if (start_pos == std::string::npos) return task;

	// "UTF8:" の後から最後までを一旦取り出す
	std::string list = spec.substr(start_pos + 5);
	
	// もし末尾に "]" が付いていたら除去する（職人の皿洗い）
	if (!list.empty() && list.back() == ']') {
		list.pop_back();
	}

	std::stringstream ss(list);
	std::string item;
	while (std::getline(ss, item, ',')) {
		if (item.empty()) continue;
		try {
			// ここで 10進数としてパース
			task.codes.push_back(std::stoul(item));
		} catch (...) { continue; }
	}
	return task;
}



// 関数 UTF8FontEngine::renderUtf8Combine(void *dest_img_ptr, const std::string &command)
void UTF8FontEngine::renderUtf8Combine(void *dest_img_ptr, const std::string &command)
{
	if (!m_atlas) m_atlas = new UTF8FontAtlas();
	if (!dest_img_ptr) return;
	video::IImage *dest_img = reinterpret_cast<video::IImage*>(dest_img_ptr);

	// --- 0. 共通の物差し（Manager）から最新設定を取得 ---
	UTF8AtlasConfig &cfg = UTF8SignManager::getInstance()->atlas;

	// --- 1. 終端チェックと外枠剥離 ---
	if (command.empty() || command.front() != '[' || command.back() != ']') {
		actionstream << "UTF8FontEngine: Syntax Error (Missing []): " << command << std::endl;
		return;
	}

	std::string inner = command.substr(1, command.length() - 2);

	// --- 2. 大ブロックの分離 (コロンによる分割) ---
	size_t first_colon = inner.find(':');
	size_t second_colon = inner.find(':', first_colon + 1);

	if (first_colon == std::string::npos || second_colon == std::string::npos) {
		actionstream << "UTF8FontEngine: Syntax Error (Missing colons): " << command << std::endl;
		return;
	}

	std::string size_part = inner.substr(first_colon + 1, second_colon - first_colon - 1);
	std::string content_part = inner.substr(second_colon + 1);

	// --- 3. 詳細パース：キャンバスサイズ ---
	// デフォルト値を cfg.sign_width に変更
	u32 canvas_w = cfg.sign_width, canvas_h = 115; 
	size_t x_pos = size_part.find('x');
	if (x_pos != std::string::npos) {
		canvas_w = mystoi(size_part.substr(0, x_pos));
		canvas_h = mystoi(size_part.substr(x_pos + 1));
	}

	dest_img->fill(video::SColor(0, 0, 0, 0));

	// --- 4. 詳細パース：座標・色・テキスト ---
	size_t equal = content_part.find('=');
	if (equal == std::string::npos) {
		actionstream << "UTF8FontEngine: Syntax Error (Missing '='): " << command << std::endl;
		return;
	}

	std::string settings = content_part.substr(0, equal);
	std::string raw_text = content_part.substr(equal + 1);

	// ★ 座標の初期値を設定値 (padding_x) に同期
	u32 start_x = cfg.padding_x; 
	u32 start_y = 2; // ここも将来的に cfg.padding_y を追加可能
	video::SColor target_color(255, 255, 255, 255);

	// カラーコード解析
	size_t at_sign = settings.find('@');
	if (at_sign != std::string::npos) {
		std::string hex_str = settings.substr(at_sign + 1);
		try {
			unsigned long color_val = std::stoul(hex_str, nullptr, 16);
			target_color = video::SColor(255, (color_val >> 16) & 0xFF, (color_val >> 8) & 0xFF, color_val & 0xFF);
		} catch (...) {}
	}

	// 座標解析（コマンド指定があれば上書き）
	size_t comma = settings.find(',');
	if (comma != std::string::npos) {
		start_x = mystoi(settings.substr(0, comma));
		size_t y_end = (at_sign != std::string::npos) ? at_sign : settings.length();
		start_y = mystoi(settings.substr(comma + 1, y_end - (comma + 1)));
	}

	if (raw_text.empty()) return;

	// 改行幅の計算（余白を考慮）
	int wrap_width = canvas_w - (start_x * 2); 
	if (wrap_width <= 0) wrap_width = canvas_w;
	std::vector<std::string> lines = utf8_53::get_lines(raw_text, wrap_width);

	u32 y_cursor = start_y;
	for (const std::string &line_str : lines) {
		// 描画の高さ安全チェックも、リクエストサイズ(14)ではなく設定値に準拠
		if (y_cursor + cfg.line_height > canvas_h) break; 

		u32 x_cursor = start_x;
		std::vector<int> cps = utf8_53::to_codepoints(line_str);

		for (int cp : cps) {
			u32 current_w = 12; 
			try {
				ImageRGBA glyph = m_atlas->getGlyphImage(cp);
				if (x_cursor + glyph.width > canvas_w) break;
				
				current_w = glyph.width;

				for (int gy = 0; gy < (int)glyph.height; gy++) {
					for (int gx = 0; gx < (int)glyph.width; gx++) {
						int i = (gy * glyph.width + gx) * 4;

						video::SColor pixel_color = target_color;
						pixel_color.setAlpha(glyph.data[i + 3]); 

						if (pixel_color.getAlpha() > 0) {
							// 14px高アトラスなら gy=13 (14px目) まで描画される
							dest_img->setPixel(x_cursor + gx, y_cursor + gy, pixel_color);
						}
					}
				}
			} catch (...) {}
			
			x_cursor += current_w; 
			if (x_cursor >= canvas_w) break; 
		}
		
		// ★ ここが近代化の核心：行送りを cfg.line_height (14px) に変更
		y_cursor += cfg.line_height; 
	}
}

// Render UTF-8 Combine FreeType
void UTF8FontEngine::renderutf8combineft(video::IImage *baseimg, const std::string &spec)
{
	if (!baseimg) return;

	UTF8FTConfig &cfg = UTF8SignManager::getInstance()->ft;

	// --- 職人の「心変わり」チェック ---
	// 現在読み込み済みの情報と、最新の設定を突き合わせる
	bool needs_init = false;
	if (sdl2_font::get_library_ptr() == nullptr) {
		needs_init = true;
	} else if (sdl2_font::get_last_path() != cfg.ttf_name || 
	           sdl2_font::get_last_index() != cfg.font_index ||
	           sdl2_font::get_last_size() != cfg.font_size) {
		// パス、インデックス、サイズのどれかが変わってたらリロード！
		needs_init = true;
	}

	if (needs_init) {
		actionstream << "SDL2Font: Reloading font due to config change." << std::endl;
		sdl2_font::init(cfg.ttf_name, cfg.font_size, cfg.font_index);
	}

	u32 baseline = cfg.baseline_y;
	bool aa      = cfg.antialias;
	u32 f_size   = cfg.font_size;

	if (sdl2_font::get_library_ptr() == nullptr) {
		std::string font_path = g_settings->get("utf8_font_path");

		// 第3引数に cfg.font_index を追加して、3つの引数で呼ぶ！
		sdl2_font::init(font_path, cfg.font_size, cfg.font_index);
	}


/*
	UTF8FTConfig &cfg = UTF8SignManager::getInstance()->ft;

	// 全ての設定を Config から奪い取る
	u32 baseline = cfg.baseline_y;
	bool aa      = cfg.antialias;
	u32 f_size   = cfg.font_size;

	// --- フォント設定の遅延初期化（sdl2_fontdでは初期化できないから） ---
	if (sdl2_font::get_library_ptr() == nullptr) {
		// minetest.confからフォントパスを取得して初期化
		std::string font_path = g_settings->get("utf8_font_path");
//		if (font_path.empty()) font_path = "NotoSansMonoCJKjp-Regular.otf";
//		u16 f_size = cfg.font_size;

		sdl2_font::init(font_path, f_size);
	}
*/

	// ---------------------------------------------------------

#if FT_DEBUG_VIEW
	u32 w = baseimg->getDimension().Width;
	u32 h = baseimg->getDimension().Height;
	u32 q = w / 4; // 1/4ずつの区切り

	// --- [1/4] 窓口： imagesource からの到達確認 ---
	// 濃いグレー（不透明）
	for(u32 y=0; y<h; y++)
		for(u32 x=0; x<q; x++)
			baseimg->setPixel(x, y, video::SColor(255, 50, 50, 50));

	// --- [2/4] 初期化： FT_Init_FreeType の成否 ---
	int ft_err = sdl2_font::get_last_error();
	video::SColor color2;

	if (ft_err == 0 && sdl2_font::get_library_ptr() != nullptr) {
		color2 = video::SColor(255, 0, 255, 0); // 成功なら「緑」
	} else {
		// 失敗なら「赤」の輝度でエラー番号を表現する
		// errが0だと真っ黒になってしまうので、存在確認のために最低限の明るさ(50)を足すか、
		// あるいは純粋に err そのものを叩き込む
		u8 red_value = (u8)(ft_err & 0xFF);
		// 青(100)の上に、赤(エラー番号)を乗せる
		color2 = video::SColor(255, red_value, 0, 100);
	}
	for(u32 y=0; y<h; y++)
		for(u32 x=q; x<q*2; x++)
			baseimg->setPixel(x, y, color2);

	// --- [3/4] 読込： FT_New_Face の成否 ---
	bool ft_face_ok = (sdl2_font::get_face_ptr() != nullptr);
	video::SColor color3 = ft_face_ok ? video::SColor(255, 0, 255, 0) : video::SColor(255, 255, 0, 0);
	for(u32 y=0; y<h; y++)
		for(u32 x=q*2; x<q*3; x++)
			baseimg->setPixel(x, y, color3);

	// --- [4/4] 予約： 現時点では黒 ---
	for(u32 y=0; y<h; y++)
		for(u32 x=q*3; x<w; x++)
			baseimg->setPixel(x, y, video::SColor(255, 0, 0, 0));
#endif

//	std::vector<u32> codes = parseUtf8Spec(spec);
//	if (codes.empty()) return;


	// パース結果をローカルな変数「task」として受け取る
	RenderTask task = parseUtf8Spec(spec);
	if (task.codes.empty()) return;

	// SignManager ではなく、この看板専用の task から座標を取る
	u32 cursor_x = (task.start_x == 0) ? cfg.padding_x : task.start_x;
	u32 cursor_y = (task.start_y == 0) ? cfg.padding_y : task.start_y;
	u32 line_height = cfg.line_height; // ★1行の高さ（器の高さ）
	u32 max_w = baseimg->getDimension().Width;
	u32 max_h = baseimg->getDimension().Height;
//	const bool use_aa = true; // まずは最高画質のアンチエイリアスONで！

	for (u32 code : task.codes) {
		// 1. 改行コード(10)が来たら、問答無用で次へ
		if (code == 10) { 
			cursor_x = (task.start_x == 0) ? cfg.padding_x : task.start_x;  // 改行時も base_x に戻る
			cursor_y += line_height;
			continue;
		}

		// 2. 自動折り返し：端まで来たら次の行へ（プロポーショナル対応）
		// とりあえず全角幅(cfg.font_size)を基準に判定
		if (cursor_x + cfg.font_size > max_w) {
			cursor_x = task.start_x;
			cursor_y += line_height;
		}

		// 3. 看板の底（下端）を突き抜けそうなら描画終了
		if (cursor_y + line_height > max_h) break;

		// --- (ここから描画処理) ---
		u8 char_rgba[32 * 32 * 4] = {0}; // 高さは18px確保
		if (sdl2_font::render_to_buffer(code, char_rgba, 32, 32, aa, f_size, baseline)) {
			u32 advance = sdl2_font::get_last_char_advance();

			// 転写時に cursor_y を加味する
			for (u32 y = 0; y < cfg.line_height; y++) {
				for (u32 x = 0; x < (cfg.font_size + 8); x++) {
					int src_idx = (y * 32 + x) * 4;
					u8 a = char_rgba[src_idx + 3];
					if (a > 0) {
						// ★ cursor_y を足して、正しい行に描く
						baseimg->setPixel(cursor_x + x, cursor_y + y, video::SColor(a, 0, 0, 0));
					}
				}
			}
			cursor_x += advance;
		}
	}


	/*
	// 2. 看板の「物理規格」を定義 (SignManagerの黄金比を継承)
	const u32 char_w = 12;
	const u32 char_h = 14;
	const bool use_aa = true; // まずは最高画質のアンチエイリアスONで！

	u32 cursor_x = 0;
	u32 max_w = baseimg->getDimension().Width;

	u32 actual_advance = 0; // ここで宣言しておけば else でも見える
	for (u32 code : codes) {
		// 看板からはみ出すなら終了
		if (cursor_x + char_w > max_w) break;

		// 1文字分のRGBA作業バッファ
		u8 char_rgba[char_w * char_h * 4] = {0};

		// 3. FreeTypeで「12x14の器」に文字を焼く
		// ここで 11pxベースライン合わせ が自動で行われる
		if (sdl2_font::render_to_buffer(code, char_rgba, char_w, char_h, use_aa)) {
			// フォントから歩幅を取得
			actual_advance = sdl2_font::get_last_char_advance(); 
			// 安全装置：もし歩幅が0なら、文字コードから推測してフリーズを防ぐ
			if (actual_advance == 0) {
				actual_advance = (code <= 0xFF) ? 6 : char_w;
			}
			// 4. 焼き上がったドットを baseimg に一粒ずつ丁寧に転写
			for (u32 y = 0; y < char_h; y++) {
				for (u32 x = 0; x < char_w; x++) {
					int src_idx = (y * char_w + x) * 4;
					u8 a = char_rgba[src_idx + 3]; // FreeTypeのアルファ値
					
					if (a > 0) {
						// 職人のこだわり：白文字(255,255,255)にAAのアルファを乗せる
						baseimg->setPixel(cursor_x + x, y, video::SColor(a, 0, 0, 0));
					}
				}
			}
		} else {
			// 無限ループ防止
			actual_advance = (code <= 0xFF) ? 6 : char_w;
		}
		// 次の文字へ進む
		cursor_x += actual_advance;
	}
	*/
}
 