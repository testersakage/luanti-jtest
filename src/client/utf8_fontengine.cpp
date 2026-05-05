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

// 0=無効 , 1=有効
#define UTF8_ATLAS 1
#define UTF8_SDL2_ATLAS 0
#define UTF8_SDL2_FREETYPE 0

UTF8FontEngine::UTF8FontEngine()
{
	// コンストラクタでは「初期化（init）」をあえてしない！
	// すべては renderutf8combineft の「遅延初期化」に任せる。
	// これが、今のあなたの環境で「16px」を確実に通すための最善策です。
	
	infostream << "UTF8FontEngine: Created. Initialization deferred until first render." << std::endl;
}

UTF8FontEngine::~UTF8FontEngine()
{
#if UTF8_SDL2_FREETYPE
	// クリーンアップも忘れずに
	sdl2_font::cleanup();
	// キャッシュクリア(?)
#endif

}

/**
 * 補足:
 * RenderingEngine や IGUIFont などの複雑な部分は 
 * g_fontengine->getFont() が内部で処理してくれるため、
 * 私たちが直接触る必要はありません。
 */
// 1. インクルードの後にこれを追加（irr:: を省略できるようにする）
//using namespace irr;


// UTF8FontEngine.cpp 内の parseUtf8Spec
RenderTask UTF8FontEngine::parseUtf8Spec(const std::string &spec)
{
	RenderTask task;

	// --- 1. エンジンタイプの判別 ---
	if (spec.find("[utf8combineex:") != std::string::npos) {
		task.engine_type = UTF8EngineType::EX;
	} else if (spec.find("[utf8combineft:") != std::string::npos) {
		task.engine_type = UTF8EngineType::FT;
	} else {
		task.engine_type = UTF8EngineType::STD; // 旧Atlas
	}

	// 区切り文字の位置を特定
	size_t first_colon  = spec.find(":");
	size_t second_colon = spec.find(":", first_colon + 1);
	size_t color_pos    = spec.find("@", second_colon + 1);
	size_t utf8_tag_pos = spec.find("UTF8:");
	size_t equal_pos    = spec.find("=");

	// --- 2. キャンバスサイズ (例: 230x164) ---
	if (first_colon != std::string::npos && second_colon != std::string::npos) {
		std::string s = spec.substr(first_colon + 1, second_colon - (first_colon + 1));
		size_t x_pos = s.find("x");
		if (x_pos != std::string::npos) {
			try {
				task.sign_width  = std::stoul(s.substr(0, x_pos));
				task.sign_height = std::stoul(s.substr(x_pos + 1));
			} catch (...) {}
		}
	}

	// --- 3. 書き出し座標 (例: 15,14) ---
	size_t coord_end = (color_pos != std::string::npos) ? color_pos : utf8_tag_pos;
	if (second_colon != std::string::npos && coord_end != std::string::npos) {
		std::string s = spec.substr(second_colon + 1, coord_end - (second_colon + 1));
		size_t comma = s.find(",");
		if (comma != std::string::npos) {
			try {
				task.start_x = std::stoul(s.substr(0, comma));
				task.start_y = std::stoul(s.substr(comma + 1));
			} catch (...) {}
		}
	}

	// --- 4. 文字色 (@RRGGBB) ---
	if (color_pos != std::string::npos) {
		std::string hex = spec.substr(color_pos + 1, 6);
		try {
			u32 val = std::stoul(hex, nullptr, 16);
			task.color = video::SColor(255, (val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF);
		} catch (...) {}
	}
/*
	// --- 5. 文字コード列 ---
	if (utf8_tag_pos != std::string::npos) {
		std::string list = spec.substr(utf8_tag_pos + 5);
		if (!list.empty() && list.back() == ']') list.pop_back();

		std::stringstream ss(list);
		std::string item;
		while (std::getline(ss, item, ',')) {
			if (item.empty()) continue;
			try {
				task.codes.push_back(std::stoul(item));
			} catch (...) {}
		}
	}
*/
	// --- 5. テキスト内容の解析 (ここが今回の肝) ---
	if (utf8_tag_pos != std::string::npos) {
		// 【新形式】 UTF8:123,456...
		std::string list = spec.substr(utf8_tag_pos + 5);
		if (!list.empty() && list.back() == ']') list.pop_back();

		std::stringstream ss(list);
		std::string item;
		while (std::getline(ss, item, ',')) {
			if (item.empty()) continue;
			try {
				u32 code = std::stoul(item);
				task.codes.push_back(code);
				// STD版のために文字列も復元
//				utf8_53::push_char(task.raw_text, (int)code);
			} catch (...) {}
		}
	} else if (equal_pos != std::string::npos) {
		// 【旧形式】 =テキスト
		std::string raw = spec.substr(equal_pos + 1);
		if (!raw.empty() && raw.back() == ']') raw.pop_back();
		
		task.raw_text = raw;
		// FT/EX版のためにコードポイント配列も生成 (utf8_53の知恵を借りる)
//		task.codes = utf8_53::to_codepoints(raw);
	}

	return task;
}

#if UTF8_ATLAS
//  staticメンバの実体(Cache)を定義
UTF8FontAtlas* UTF8FontEngine::m_atlas_cache = nullptr;

/*
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
*/

// 関数 UTF8FontEngine::renderUtf8Combine(void *dest_img_ptr, const std::string &command)
void UTF8FontEngine::renderUtf8Combine(void *dest_img_ptr, const std::string &command)
{
/*
	actionstream << "RenderUTF8Combine: Old Atlas Glyph Engine Called!" << std::endl;

	if (!m_atlas_cache) m_atlas_cache = new UTF8FontAtlas();
	if (!dest_img_ptr) return;
	video::IImage *dest_img = reinterpret_cast<video::IImage*>(dest_img_ptr);

	// --- 0. 共通の物差し（Manager）から最新設定を取得 ---
	UTF8STDAtlas &cfg = UTF8SignManager::getInstance()->st_atlas;

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
	// デフォルト値を cfg.st_sign_width に変更
	u32 canvas_w = cfg.st_sign_width, canvas_h = 115; 
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
	u32 start_x = cfg.st_padding_x; 
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
// 関数移設に伴う変更
// std::vector<std::string> lines = utf8_53::get_lines(raw_text, wrap_width);
	std::vector<std::string> lines = utf8_53::generate_lines(
	raw_text, 
	wrap_width, 
	cfg.st_char_w_han, 
	cfg.st_char_w_zen
	);
*/

	actionstream << "RenderUTF8Combine: Old Atlas Glyph Engine Called!" << std::endl;

	// ---  準備：キャンバスと蔵の確認 ---
	if (!m_atlas_cache) m_atlas_cache = new UTF8FontAtlas();
	if (!dest_img_ptr) return;
	video::IImage *dest_img = reinterpret_cast<video::IImage*>(dest_img_ptr);
	dest_img->fill(video::SColor(0, 0, 0, 0));

	// ---  共通マネージャーから最新設定を取得 ---
	UTF8STDAtlas &cfg = UTF8SignManager::getInstance()->st_atlas;

	// ---  【大掃除の成果】パースを万能窓口に丸投げ！ ---
	RenderTask task = parseUtf8Spec(command);
	if (task.raw_text.empty()) return;

	// ---  パース済みの設計図（task）から設定を適用 ---
	u32 canvas_w  = (task.sign_width > 0) ? task.sign_width : cfg.st_sign_width;
	u32 canvas_h  = (task.sign_height > 0) ? task.sign_height : cfg.st_line_height;
	u32 start_x   = (task.start_x == 0)  ? cfg.st_padding_x : task.start_x;
	u32 start_y   = (task.start_y == 0)  ? 2 : task.start_y;
	video::SColor target_color = task.color;

	// ---  文字列整形：新しい万能物差しで改行位置を決定 ---
	int wrap_width = canvas_w - (start_x * 2);
	if (wrap_width <= 0) wrap_width = canvas_w;

	std::vector<std::string> lines = utf8_53::generate_lines(
		task.raw_text, 
		wrap_width, 
		cfg.st_char_w_han, 
		cfg.st_char_w_zen
	);

	u32 y_cursor = start_y;
	for (const std::string &line_str : lines) {
		// 描画の高さ安全チェックも、リクエストサイズ(14)ではなく設定値に準拠
		if (y_cursor + cfg.st_line_height > canvas_h) break; 

		u32 x_cursor = start_x;
		std::vector<int> cps = utf8_53::to_codepoints(line_str);

		for (int cp : cps) {
			u32 current_w = 12; 
			try {
				ImageRGBA glyph = m_atlas_cache->getGlyphImage(cp);
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
		
		// 行送りを cfg.line_height (14px) に変更
		y_cursor += cfg.st_line_height; 
	}
}
#endif

#if UTF8_SDL2_ATLAS

// Atlasからピクセルを抜き出して蔵(Glyph)の形にする
bool UTF8FontEngine::extractAtlasGlyph(u32 code, FTCachedGlyph &out_glyph)
{
	// 1. マネージャー（知識層）から情報を仕入れる
	auto manager = UTF8SignManager::getInstance();
	if (manager->getAvailableAtlases().empty()) {
		actionstream << "EXTRACT: No Atlas available!" << std::endl;
		return false;
	}

	const auto &atlases = manager->getAvailableAtlases();
	// ひとまずは最初のアトラスを使用
	const auto &res = atlases[0];
	const auto &def = res.def;

	// 2. 座標計算 (32x8などの独自レイアウトに対応)
	u32 page = (code >> 8) & 0xFF;
	u32 char_index = code & 0xFF; // 1ページ(256文字)内の通し番号

	// JSONの grid_columns (32等) を使って行列を算出
	u32 cols = def.grid_columns; 
	u32 col = char_index % cols;
	u32 row = char_index / cols;

	// 3. 画像ロード
	video::IVideoDriver *driver = RenderingEngine::get_video_driver(); 
	char filename[256];
	snprintf(filename, sizeof(filename), def.file_pattern.c_str(), page);
	std::string full_path = res.full_path + DIR_DELIM + filename;

	video::IImage *img = driver->createImageFromFile(full_path.c_str());
	if (!img) return false;

	// 4. 蔵(Glyph)の箱を準備
	out_glyph.bitmap.clear();
	out_glyph.width = def.glyph_w; // 12
	out_glyph.rows  = def.glyph_h; // 14
	
	// 配置設定 (行間0で成立する14px設計を尊重)
	out_glyph.bitmap_left = 0; 
	out_glyph.bitmap_top  = def.glyph_h; 
	// 半角(han)は全角(zen)の半分として歩幅を設定
	out_glyph.advance     = (code < 128) ? (def.glyph_w / 2) : def.glyph_w; 

	// 5. 職人の「スライス ＆ 抽出」ループ
	for (u32 y = 0; y < def.glyph_h; y++) {
		for (u32 x = 0; x < def.glyph_w; x++) {
			// grid_size(14) 単位で座標を特定。+1などの微調整は画像に合わせて
			u32 px = (col * def.grid_size) + x; 
			u32 py = (row * def.grid_size) + y;
			
			if (px < img->getDimension().Width && py < img->getDimension().Height) {
				video::SColor color = img->getPixel(px, py);
				
				// アルファ値を採用（フォント画像が白黒ならRed値を流用するのも手）
				u8 alpha = color.getAlpha();

				// 魔導書の指示があれば反転
				if (def.alpha_reverse) {
					alpha = 255 - alpha;
				}
				out_glyph.bitmap.push_back(alpha);
			} else {
				out_glyph.bitmap.push_back(0); // 範囲外は透明
			}
		}
	}

	img->drop(); 
	return true;
}
// Atlas EX Cahe制御用 静的変数
std::map<u64, EXCachedChar> UTF8FontEngine::m_char_cache;
std::map<u64, EXCachedPage> UTF8FontEngine::m_page_cache;

void UTF8FontEngine::renderutf8combineex(void *dest_img_ptr, const std::string &command)
{
	actionstream << "RenderUTF8Combine: SDL2 Atlas Glyph Engine Called!" << std::endl;
	if (!dest_img_ptr) return;
	video::IImage *dest_img = reinterpret_cast<video::IImage*>(dest_img_ptr);

	// 1. 共通のパース関数で「注文票」を受け取る
	RenderTask task = parseUtf8Spec(command);
	if (task.codes.empty()) return;

	auto manager = UTF8SignManager::getInstance();
	// 知識がなければロード
	if (manager->getAvailableAtlases().empty()) {
		manager->loadGrimoire();
	}

	bool use_cache = (manager->ft.cache_size > 0);
	auto &ex_cfg = manager->atlas;

	//  描画開始位置の決定
	u32 cursor_x = (task.start_x == 0) ? ex_cfg.ex_padding_x : task.start_x;
	u32 cursor_y = (task.start_y == 0) ? ex_cfg.ex_padding_y : task.start_y;
	u32 line_h   = ex_cfg.ex_line_height; 

	u32 max_w = dest_img->getDimension().Width;
	u32 margin_right = ex_cfg.ex_padding_x;
	
	FTCachedGlyph temp_glyph; // 直接抽出用の受け皿

	//  Atlas抽出 ＆ 転写ループ
	for (u32 code : task.codes) {
		if (code == 10) { // 改行
			cursor_x = (task.start_x == 0) ? ex_cfg.ex_padding_x : task.start_x;
			cursor_y += line_h;
			continue;
		}
		FTCachedGlyph* glyph = nullptr;

		if (use_cache) {
			// --- 蔵（キャッシュ）から引き出すルート ---
			glyph = getOrCacheGlyph(code, nullptr, 0);
		} else {
			// --- 現場（直接抽出）から運ぶルート ---
			if (extractAtlasGlyph(code, temp_glyph)) glyph = &temp_glyph;
		}
/*
		// 蔵（キャッシュ）から12x14のドットを召喚
		// ※内部で extractAtlasGlyph が呼ばれ、JSONの設定通りに動く [INDEX: 5]
		FTCachedGlyph* glyph = getOrCacheGlyph(code, nullptr, 0); 
*/
		if (glyph) {
/*
			// 蔵から出した文字が「空っぽ」じゃないかログで白状させる
    actionstream << "DEBUG_GLYPH: Code=" << code 
                 << " Size=" << glyph->width << "x" << glyph->rows 
                 << " BitmapSize=" << glyph->bitmap.size() << std::endl;
*/
			// 半角なら han(6px)、全角なら glyph->width(12px)
			bool half = (code <= 0x00FF) || (code >= 0xFF61 && code <= 0xFF9F);
			u32 advance = half ? ex_cfg.ex_char_w_han : ex_cfg.ex_char_w_zen;

			// もし右端を突破しそうなら、描く前に改行！
			if (cursor_x + advance > max_w - margin_right) {
				cursor_x = (task.start_x == 0) ? ex_cfg.ex_padding_x : task.start_x;
				cursor_y += line_h;
			}

			// --- 3. 転写処理 ---
			u32 draw_w = half ? ex_cfg.ex_char_w_han : glyph->width;
			for (u32 y = 0; y < glyph->rows; y++) {
				if (cursor_y + y >= dest_img->getDimension().Height) break; // 縦の限界突破防止
				for (u32 x = 0; x < draw_w; x++) {
					u32 idx = y * glyph->width + x;
					u8 alpha = glyph->bitmap[idx];
					if (alpha > 0) {
						video::SColor color = task.color;
						color.setAlpha(alpha); 
						dest_img->setPixel(cursor_x + x, cursor_y + y, color);
					}
				}
			}

			// 4. カーソルを進める
			cursor_x += advance;
		}
	}
}
#endif

#if UTF8_SDL2_FREETYPE
// FreeType Cahe制御用 静的変数
std::map<u64, FTCachedGlyph> UTF8FontEngine::m_glyph_cache;
std::list<u64> UTF8FontEngine::m_cache_order;
bool UTF8FontEngine::m_cache_zero_bypass = false; // 初回にサイズ0ならtrueにする
size_t UTF8FontEngine::m_max_cache_size = 256;    // デフォルト256文字程度

// FreeType 専用の蔵（キャッシュ）制御
FTCachedGlyph* UTF8FontEngine::getOrCacheGlyph(u32 code, void* face_ptr, u32 load_flags) {
	// --- 1. zeroフラグによる高速バイパス ---
	// キャッシュサイズが0なら、二度とMapを検索させない
	if (m_cache_zero_bypass) return nullptr;

	//  共通マネージャーから FT 専用の物差しを取得
	auto manager = UTF8SignManager::getInstance();
	const auto &cfg = manager->ft;
	
	// サイズが0ならここでフラグを立てて終了（以降の処理を全スキップ）
	if (m_max_cache_size == 0) {
//		actionstream << "getOrCacheGlyph: Cache Bypss Flag:" << m_cache_zero_bypass << std::endl;
		m_cache_zero_bypass = true;
		return nullptr;
	}

	//  鍵の生成 (サイズ + コードポイント)
	u64 cache_key = ((u64)cfg.font_size << 32) | (u64)code;

	//  キャッシュを覗く
	auto it = m_glyph_cache.find(cache_key);
	if (it != m_glyph_cache.end()) {
		actionstream << "getOrCacheGlyph: Found data in Cache." << std::endl;
		return &it->second;
	}

	//  実体化（FreeType 一本勝負！）
	FTCachedGlyph cg;
	FT_Face face = (FT_Face)face_ptr;
	if (!face || FT_Load_Char(face, code, load_flags)) return nullptr;

	// データ転写
	FT_GlyphSlot slot = face->glyph;
	u32 data_size = slot->bitmap.rows * slot->bitmap.pitch;
	if (data_size > 0) {
		cg.bitmap.assign(slot->bitmap.buffer, slot->bitmap.buffer + data_size);
	}

	// FT の生データを正直に格納
	cg.width       = slot->bitmap.width;
	cg.rows        = slot->bitmap.rows;
	cg.pitch       = slot->bitmap.pitch;
	cg.bitmap_left = slot->bitmap_left;
	cg.bitmap_top  = slot->bitmap_top;
	cg.advance     = (u32)(slot->advance.x >> 6);
	cg.pixel_mode  = slot->bitmap.pixel_mode;

	// ---  Cacheの整理（FIFO機能） ---
	// 容量オーバーなら、リストの先頭（一番古い鍵）を蔵から追い出す
	while (m_glyph_cache.size() >= m_max_cache_size && !m_cache_order.empty()) {
		u64 oldest_key = m_cache_order.front();
		m_cache_order.pop_front();
		m_glyph_cache.erase(oldest_key);
	}

	// Cacheに納めて、その場所を教える
	m_glyph_cache[cache_key] = std::move(cg);
	m_cache_order.push_back(cache_key);

//	size_t xCount = getCacheCount();
//	actionstream << "getOrCacheGlyph: Cache Count:" << xCount << std::endl;
	return &m_glyph_cache[cache_key];
}

// Render UTF-8 Combine FreeType
void UTF8FontEngine::renderutf8combineft(video::IImage *baseimg, const std::string &spec)
{
	actionstream << "RenderUTF8Combine: SDL2 FreeType Engine Called!" << std::endl;

	if (!baseimg) return;

	auto &mgr = *UTF8SignManager::getInstance();
	auto &cfg = mgr.ft;

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

	if (sdl2_font::get_library_ptr() == nullptr) {
		std::string font_path = g_settings->get("utf8_font_path");

		// 第3引数に cfg.font_index を追加して、3つの引数で呼ぶ！
		sdl2_font::init(font_path, cfg.font_size, cfg.font_index);
	}

	// パース結果をローカルな変数「task」として受け取る
	RenderTask task = parseUtf8Spec(spec);
	if (task.codes.empty()) return;

	// SignManager ではなく、この看板専用の task から座標を取る
	u32 cursor_x = (task.start_x == 0) ? cfg.ft_padding_x : task.start_x;
	u32 cursor_y = (task.start_y == 0) ? cfg.ft_padding_y : task.start_y;
	u32 line_height = cfg.ft_line_height; // ★1行の高さ（器の高さ）
	u32 max_w = (task.sign_width > 0) ? task.sign_width : cfg.ft_sign_width;
	u32 max_h = baseimg->getDimension().Height;
	video::SColor target_color = task.color;

	for (u32 code : task.codes) {
		// 1. 改行コード(10)が来たら、問答無用で次へ
		if (code == 10) { 
			cursor_x = (task.start_x == 0) ? cfg.ft_padding_x : task.start_x;  // 改行時も base_x に戻る
			cursor_y += line_height;
			continue;
		}
//
		// --- ここでキャッシュを先に召喚！ ---
		FT_Face face = (FT_Face)sdl2_font::get_face_ptr();
		if (!face) continue;
		u32 load_flags = FT_LOAD_RENDER | (cfg.antialias ? FT_LOAD_TARGET_NORMAL : FT_LOAD_TARGET_MONO);
		
		FTCachedGlyph* cg = getOrCacheGlyph(code, face, load_flags);
		if (!cg) {
			// 万が一キャッシュが取れない場合、utf8_53の物差しで安全にスキップ
			cursor_x += (utf8_53::get_char_width(code) == 1) ? cfg.ft_char_w_han : cfg.ft_char_w_zen;
			continue;
		}

		// キャッシュから得た「本当の歩幅」で、次の文字が入るか判定
		u32 char_advance = (cg->advance > 0) ? cg->advance : cfg.ft_char_w_han;

		if (cursor_x + char_advance > max_w) {
			cursor_x = (task.start_x == 0) ? cfg.ft_padding_x : task.start_x;
			cursor_y += line_height;
		}
//
		// 看板の底（下端）を突き抜けそうなら描画終了
		if (cursor_y + line_height > max_h) break;

		//  【転写】Cacheにあるピクセルを看板に直接刻む
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
		//  【進む】文字送りもCacheから
		cursor_x += char_advance;
	}

}

// FreeType API Cache count
u32 UTF8FontEngine::getCacheCount() {
    return (u32)m_glyph_cache.size();
}

// FreeType API Cache clear
void UTF8FontEngine::clearCache() {
    m_glyph_cache.clear();
    actionstream << "FT_CACHE: Manual Clear." << std::endl;
}
#endif

 