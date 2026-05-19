// src/client/utf8_fontengine.cpp

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

	// --- 5. テキスト内容の解析 (エンジンごとに「合図」を決める) ---
	if (task.engine_type == UTF8EngineType::EX || task.engine_type == UTF8EngineType::FT) {
		// --- 【EX/FT専用】 ":UTF8:" を探す ---
		size_t pos = spec.find(":UTF8:");
		if (pos != std::string::npos) {
			std::string list = spec.substr(pos + 6);
			if (!list.empty() && list.back() == ']') list.pop_back();

			std::stringstream ss(list);
			std::string item;
			while (std::getline(ss, item, ',')) {
				if (item.empty()) continue;
				try {
					u32 code = std::stoul(item);
					task.codes.push_back(code);
					// 物差し（改行計算）のために文字列を復元
					utf8_53::push_char(task.raw_text, (int)code);
				} catch (...) {}
			}
		}
	} else {
		// --- 【旧Atlas専用】 "=" を探す ---
		size_t pos = spec.find("=");
		if (pos != std::string::npos) {
			std::string raw = spec.substr(pos + 1);
			if (!raw.empty() && raw.back() == ']') raw.pop_back();
			
			task.raw_text = raw;
			task.codes = utf8_53::to_codepoints(raw);
		}
	}

	return task;
}

#if UTF8_ATLAS

// 関数 UTF8FontEngine::renderUtf8Combine(void *dest_img_ptr, const std::string &command)
void UTF8FontEngine::renderUtf8Combine(void *dest_img_ptr, const std::string &command)
{
	actionstream << "RenderUTF8Combine: Standard Atlas Glyph Engine Called!" << std::endl;

	// ---  準備：キャンバスと蔵の確認 ---
//	if (!m_atlas_cache) m_atlas_cache = new UTF8FontAtlas();
	if (!dest_img_ptr) return;
	video::IImage *dest_img = reinterpret_cast<video::IImage*>(dest_img_ptr);
	dest_img->fill(video::SColor(0, 0, 0, 0));

	// ---  共通マネージャーから最新設定を取得 ---
	UTF8STDAtlas &cfg = UTF8SignManager::getInstance()->st;

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
			u32 current_w = cfg.st_char_w_zen; // デフォルトは全角幅
			try {
				ImageRGBA glyph = UTF8FontAtlas::getGlyphImage(cp);
				if (x_cursor + glyph.width > canvas_w) break;

				// ─── 【本題】このST看板のスイッチがONなら、全半角比率を取得 ───
				bool is_half = false;
				if (cfg.uax_half_switch) {
					float ratio = utf8_53::get_char_width_ratio(static_cast<uint32_t>(cp));
					if (ratio == 0.5f) {
						is_half = true;
					}
				} else {
					// スイッチがOFFの場合でも、ASCIIと半角カナは固定で半角扱い（安全ガード）
					is_half = (cp >= 0x0020 && cp <= 0x007E) || (cp >= 0xFF61 && cp <= 0xFF9F);
				}

				// 判定された正しい歩幅を適用（半角なら st_char_w_han、全角なら st_char_w_zen）
				u32 advance = is_half ? cfg.st_char_w_han : cfg.st_char_w_zen;
				current_w = advance; // 次の文字への歩幅をこの値で確定させる！

				for (int gy = 0; gy < (int)glyph.height; gy++) {
					//  【重要】xループの上限を glyph.width ではなく、
					// 判定された正しい歩幅（advance: 6px または 12px）にクリップする！
					// これにより、STアトラスの右半分にある余白ドットのスタンプを物理的に遮断します。
					for (int gx = 0; gx < (int)advance; gx++) {
						if (gx >= (int)glyph.width) break;
						int i = (gy * glyph.width + gx) * 4;

						video::SColor pixel_color = target_color;
						pixel_color.setAlpha(glyph.data[i + 3]); 

						if (pixel_color.getAlpha() > 0) {
							dest_img->setPixel(x_cursor + gx, y_cursor + gy, pixel_color);
						}
					}
				}
			} catch (...) {}
			
			x_cursor += current_w; // 正しい歩幅（6px または 12px）だけカーソルを右に進める
			if (x_cursor >= canvas_w) break; 
		}
		// 行送りを cfg.line_height (14px) に変更
		y_cursor += cfg.st_line_height; 
	}
}
#endif

#if UTF8_SDL2_ATLAS

void UTF8FontEngine::renderutf8combineex(void *dest_img_ptr, const std::string &command)
{
	actionstream << "RenderUTF8Combine: SDL2 Extended Atlas Engine Called!" << std::endl;

	if (!dest_img_ptr) return;
	video::IImage *dest_img = reinterpret_cast<video::IImage*>(dest_img_ptr);
	dest_img->fill(video::SColor(0, 0, 0, 0));

	// 1. 注文票をパース（DDE対応窓口）
	RenderTask task = parseUtf8Spec(command);
	if (task.raw_text.empty() && task.codes.empty()) return;

	// 2. 司令塔（Manager）から指示書を取得
	auto &cfg = UTF8SignManager::getInstance()->ex;

	// 3. 改行位置の計算（utf8_53の知恵を拝借）
	u32 start_x = (task.start_x == 0) ? cfg.ex_padding_x : task.start_x;
	u32 start_y = (task.start_y == 0) ? cfg.ex_padding_y : task.start_y;
	u32 wrap_w  = dest_img->getDimension().Width - (start_x * 2);

	std::vector<std::string> lines = utf8_53::generate_lines(
		task.raw_text, wrap_w, cfg.ex_char_w_han, cfg.ex_char_w_zen);

	// 4. 描画ループ：各行を Atlas からスタンプ
	u32 cursor_y = start_y;
	for (const std::string &line_str : lines) {
		std::vector<int> codes = utf8_53::to_codepoints(line_str);
		u32 cursor_x = start_x;

		for (u32 code : codes) {
			// 二段構えキャッシュ完結型の画像取得
			ImageRGBA glyph = UTF8FontAtlas::getGlyphImageEX(code);

			if (!glyph.data.empty()) {
				// ─── 【消失前再現】Config内の動的ルールを使って現場で全半角ジャッジ ───
				bool is_half = false;
				
				// ASCII と 半角カナ は固定で半角扱い（安全ガード）
				if ((code >= 0x0020 && code <= 0x007E) || (code >= 0xFF61 && code <= 0xFF9F)) {
					is_half = true;
				} else {
					// 魔導書（JSON）から Config に同期された例外範囲を現場でスキャン
					for (const auto &range : cfg.ex_half_width_ranges) {
						if (code >= range.start && code <= range.end) {
							is_half = true;
							break;
						}
					}
				}

				// 判定された正しい歩幅を適用（半角なら 8px、全角なら 16px）
				u32 advance = is_half ? cfg.ex_char_w_han : cfg.ex_char_w_zen;

				// --- ピクセル転写（この上限を advance に絞ることで黒背景を完全遮断！） ---
				for (int y = 0; y < glyph.height; y++) {
					u32 dy = cursor_y + y;
					if (dy >= dest_img->getDimension().Height) break;

					for (int x = 0; x < (int)advance; x++) {
						if (x >= glyph.width) break;
						u32 dx = cursor_x + x;
						if (dx >= dest_img->getDimension().Width) break;

						// Atlas（モノクロPNG）の Alpha を拾って色を乗せる
						int src_idx = (y * glyph.width + x) * 4;
						u8 alpha = glyph.data[src_idx + 3];

						if (alpha > 0) {
							video::SColor color = task.color;
							color.setAlpha(alpha);
							dest_img->setPixel(dx, dy, color);
						}
					}
				}
				cursor_x += advance; // 正しい歩幅（8px または 16px）だけ右に進める
			}
		}
		cursor_y += cfg.ex_line_height;
	}
}
#endif

#if UTF8_SDL2_FREETYPE
// FreeType Cahe制御用 静的変数
std::map<u64, FTCachedGlyph> UTF8FontEngine::m_glyph_cache;
std::list<u64> UTF8FontEngine::m_cache_order;
bool UTF8FontEngine::m_cache_zero_bypass = false; // 初回にサイズ0ならtrueにする
size_t UTF8FontEngine::m_max_cache_size = 256;    // デフォルト256文字程度

// FreeType API Get Max Cache Size
u32 UTF8FontEngine::getMaxCacheSize() {
    return (u32)m_max_cache_size;
}

// FreeType API Get Cache count
u32 UTF8FontEngine::getCacheCount() {
    return (u32)m_glyph_cache.size();
}

// FreeType API Set Max Cache Size
void UTF8FontEngine::setMaxCacheSize(u32 max_size) {
    m_max_cache_size = max_size;
    actionstream << "FT_CACHE: Set Max size: " << m_max_cache_size << std::endl;
}

// FreeType API Set Cache clear
void UTF8FontEngine::clearCache() {
    m_glyph_cache.clear();
    m_cache_order.clear();
    actionstream << "FT_CACHE: Manual Clear." << std::endl;
}

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
#endif

 