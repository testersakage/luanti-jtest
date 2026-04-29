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

#include <ft2build.h>
#include FT_FREETYPE_H 

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
	u32 sign_width = 0; 
	video::SColor color = video::SColor(255, 0, 0, 0); // デフォルト黒
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

	size_t utf8_pos = spec.find("UTF8:", color_pos != std::string::npos ? color_pos : coord_pos);

	// --- 座標のパース ---
	if (coord_pos != std::string::npos && color_pos != std::string::npos) {
		std::string coords = spec.substr(coord_pos + 1, color_pos - (coord_pos + 1));
		size_t comma = coords.find(",");
		if (comma != std::string::npos) {
			task.start_x = std::stoul(coords.substr(0, comma));
			task.start_y = std::stoul(coords.substr(comma + 1));
		}
	}

	// --- 色のパース (@RRGGBB形式) ---
	if (color_pos != std::string::npos && utf8_pos != std::string::npos) {
		// "@" の直後から "UTF8:" の手前までを切り出す
		std::string hex_str = spec.substr(color_pos + 1, utf8_pos - (color_pos + 2)); // ":" を考慮
		if (hex_str.length() >= 6) {
			try {
				u32 color_val = std::stoul(hex_str.substr(0, 6), nullptr, 16);
				task.color = video::SColor(255, 
					(color_val >> 16) & 0xFF, 
					(color_val >> 8) & 0xFF, 
					color_val & 0xFF);
			} catch (...) {}
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


// 実体宣言
std::map<u64, FTCachedGlyph> UTF8FontEngine::m_glyph_cache;

FTCachedGlyph* UTF8FontEngine::getOrCacheGlyph(u32 code, void* face_ptr, u32 load_flags) {
	UTF8FTConfig &cfg = UTF8SignManager::getInstance()->ft;
	u64 cache_key = ((u64)cfg.font_size << 32) | (u64)code;
	
	FT_Face face = (FT_Face)face_ptr;
	auto it = m_glyph_cache.find(cache_key);
	if (it != m_glyph_cache.end()) {
#if FT_DEBUG_VIEW
		actionstream << "FT_CACHE: Memory Load Key=" << cache_key << std::endl;
#endif
		return &it->second;
	}

	// --- Cacheになければ FreeType に最高の状態で焼かせる ---
	if (FT_Load_Char(face, code, load_flags)) return nullptr;

	FT_GlyphSlot slot = face->glyph;
	FTCachedGlyph &cg = m_glyph_cache[cache_key];

	// ピクセルデータを丸ごとCacheへ転写
	u32 size = slot->bitmap.rows * slot->bitmap.pitch;
	if (size > 0) {
		cg.bitmap.assign(slot->bitmap.buffer, slot->bitmap.buffer + size);
	}

	// 寸法とモードを完璧に記録
	cg.width       = slot->bitmap.width;
	cg.rows        = slot->bitmap.rows;
	cg.pitch       = slot->bitmap.pitch;
	cg.bitmap_left = slot->bitmap_left;
	cg.bitmap_top  = slot->bitmap_top;
	cg.advance     = (u32)(slot->advance.x >> 6);
	cg.pixel_mode  = slot->bitmap.pixel_mode;

#if FT_DEBUG_VIEW
	actionstream << "FT_CACHE: Stored Key=" << cache_key << " (" << cg.width << "x" << cg.rows << ")" << std::endl;
#endif
	return &cg;
}

// FreeType Cache
u32 UTF8FontEngine::getCacheCount() {
    return (u32)m_glyph_cache.size();
}

// FreeType Cache
void UTF8FontEngine::clearCache() {
    m_glyph_cache.clear();
    actionstream << "FT_CACHE: Manual Clear." << std::endl;
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

//	u32 baseline = cfg.baseline_y;
//	bool aa      = cfg.antialias;
//	u32 f_size   = cfg.font_size;

	if (sdl2_font::get_library_ptr() == nullptr) {
		std::string font_path = g_settings->get("utf8_font_path");

		// 第3引数に cfg.font_index を追加して、3つの引数で呼ぶ！
		sdl2_font::init(font_path, cfg.font_size, cfg.font_index);
	}


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


	// パース結果をローカルな変数「task」として受け取る
	RenderTask task = parseUtf8Spec(spec);
	if (task.codes.empty()) return;

	// SignManager ではなく、この看板専用の task から座標を取る
	u32 cursor_x = (task.start_x == 0) ? cfg.padding_x : task.start_x;
	u32 cursor_y = (task.start_y == 0) ? cfg.padding_y : task.start_y;
	u32 line_height = cfg.line_height; // ★1行の高さ（器の高さ）
	u32 max_w = (task.sign_width > 0) ? task.sign_width : baseimg->getDimension().Width;
	u32 max_h = baseimg->getDimension().Height;
	video::SColor target_color = task.color;

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
		FT_Face face = (FT_Face)sdl2_font::get_face_ptr();
		if (!face) continue;

		// 1. 【Cacheから出す】
		// アンチエイリアス設定などを反映したロードフラグを準備
		u32 load_flags = FT_LOAD_RENDER | (cfg.antialias ? FT_LOAD_TARGET_NORMAL : FT_LOAD_TARGET_MONO);
		FTCachedGlyph* cg = getOrCacheGlyph(code, face, load_flags);
		if (!cg) continue;

		// 2. 【転写】Cacheにあるピクセルを看板に直接刻む
		for (u32 y = 0; y < cg->rows; y++) {
			for (u32 x = 0; x < cg->width; x++) {
				u8 alpha = 0;
				// モードに応じてアルファ値を抽出
				if (cg->pixel_mode == FT_PIXEL_MODE_GRAY) {
					alpha = cg->bitmap[y * cg->pitch + x];
				} else if (cg->pixel_mode == FT_PIXEL_MODE_MONO) {
					u8 byte = cg->bitmap[y * cg->pitch + (x / 8)];
					alpha = (byte & (0x80 >> (x % 8))) ? 255 : 0;
				}

				if (alpha == 0) continue;

				// ベクターの時に学んだ「黄金の座標計算」
				int tx = (int)cursor_x + cg->bitmap_left + (int)x;
				int ty = (int)cursor_y + ((int)cfg.baseline_y - cg->bitmap_top) + (int)y;

				if (tx >= 0 && ty >= 0 && tx < (int)max_w && ty < (int)max_h) {
					// 以前決めた target_color で描画
					baseimg->setPixel((u32)tx, (u32)ty, 
						video::SColor(alpha, target_color.getRed(), target_color.getGreen(), target_color.getBlue()));
				}
			}
		}
		// 3. 【進む】文字送りもCacheから
		cursor_x += cg->advance;
	}

}
 