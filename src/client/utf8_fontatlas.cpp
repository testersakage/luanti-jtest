// src/client/utf8_fontatlas.cpp

#include "utf8_fontatlas.h"
#include "client/renderingengine.h"
#include "irrlichttypes.h"
#include "IImage.h"
#include "IVideoDriver.h"
#include "utf8_53.h"
#include "../script/common/l_utf8sign.h"
#include "SDL2/SDL_image.h"
#include <stdexcept>


// 1. 切り出し関数を可変幅(target_w)対応に
ImageRGBA UTF8FontAtlas::crop_glyph_custom(const ImageRGBA &src, int gx, int gy, int target_w, int target_h)
{
	ImageRGBA out;
	out.width = target_w;
	out.height = target_h; // ★ 12 固定から引数へ
	out.data.resize(target_w * target_h * 4);

	for (int yy = 0; yy < target_h; yy++) { // ★ ループも target_h 回に
		for (int xx = 0; xx < target_w; xx++) {
			int sx = gx + xx;
			int sy = gy + yy;
			
			// 念のための境界チェック
			if (sx < 0 || sx >= (int)src.width || sy < 0 || sy >= (int)src.height)
				continue;

			int src_i = (sy * src.width + sx) * 4;
			int dst_i = (yy * target_w + xx) * 4;
			
			for (int k = 0; k < 4; k++) {
				out.data[dst_i + k] = src.data[src_i + k];
			}
		}
	}
	return out;
}


#if UTF8_ATLAS
// キャッシュ関連
std::map<int, ImageRGBA> UTF8FontAtlas::m_st_pages;
std::list<int> UTF8FontAtlas::m_st_page_order;
size_t UTF8FontAtlas::m_st_max_pages = 4;

// st Atlas API Cache count
void UTF8FontAtlas::getPageCacheSt(u32 &page_count, u32 &max_pages) {
	page_count = (u32)m_page_cache.size();
	max_pages = (u32)m_ex_max_pages;
}

void UTF8FontAtlas::setMaxCacheSizeSt(u32 max_pages) {
	m_st_max_pages = max_pages;
	actionstream << "ST_CACHE: Set Max size: " << m_st_max_pages << std::endl;
}

/* --- 2. PNGロード関数 --- */
ImageRGBA UTF8FontAtlas::load_png_rgba(const std::string &path)
{
	video::IVideoDriver *driver = RenderingEngine::get_video_driver();
	if (!driver) throw std::runtime_error("No driver");

	video::ITexture* tex = driver->getTexture(path.c_str());
	if (!tex) throw std::runtime_error("Cannot load: " + path);

	video::IImage *img = driver->createImage(tex, core::position2d<s32>(0,0), tex->getSize());
	ImageRGBA out;
	out.width = img->getDimension().Width;
	out.height = img->getDimension().Height;
	out.data.resize(out.width * out.height * 4);

	for (u32 y = 0; y < (u32)out.height; y++) {
		for (u32 x = 0; x < (u32)out.width; x++) {
			video::SColor c = img->getPixel(x, y);
			u32 i = (y * out.width + x) * 4;
			out.data[i + 0] = c.getRed();
			out.data[i + 1] = c.getGreen();
			out.data[i + 2] = c.getBlue();
			out.data[i + 3] = c.getAlpha();
		}
	}
	img->drop();
	return out;
}

// STD Atlas 用
const ImageRGBA &UTF8FontAtlas::loadPage(int page)
{
	auto &st_cfg = UTF8SignManager::getInstance()->st;

	//  Cache（map）を確認
	auto it = m_st_pages.find(page);
	if (it != m_st_pages.end()) return it->second;

	//  Cacheの掃除（FIFO）: 5枚目が必要になったら、一番古い1枚を捨てる
	while (m_st_pages.size() >= st_cfg.st_page_cache && !m_st_page_order.empty()) {
		int oldest = m_st_page_order.front();
		m_st_page_order.pop_front();
		m_st_pages.erase(oldest); 
		infostream << "UTF8FontAtlas: FIFO Evicted old page: " << oldest << std::endl;
	}

	// マネージャーからパスを拝借
	std::string base_path = st_cfg.st_atlas_path;

	char filename[512];
	// atlas_path には "textures/unicode_page_%02x.png" と書いてある想定
	snprintf(filename, sizeof(filename), st_cfg.st_atlas_path.c_str(), page);

	infostream << "UTF8FontAtlas: Loading via full-pattern: " << filename << std::endl;

	//  ロードと格納（std::move で所有権をスマートに移譲）
	ImageRGBA img = load_png_rgba(filename);

	// ─── 【本題】Unifont用のアルファ反転フラグが ON なら、ページ丸ごと一括反転！ ───
	if (st_cfg.st_alpha_reverse && !img.data.empty()) {
		for (size_t i = 0; i < img.data.size(); i += 4) {
			img.data[i + 3] = 255 - img.data[i + 3]; // アルファチャンネルを反転（白黒反転）
		}
		infostream << "UTF8FontAtlas: Alpha reverse applied to ST page: " << page << std::endl;
	}

	m_st_pages[page] = std::move(img);
	m_st_page_order.push_back(page);

	return m_st_pages[page];
}

/* --- 4. メインの切り出し関数 --- */
ImageRGBA UTF8FontAtlas::getGlyphImage(int codepoint)
{
	if (codepoint < 0) throw std::runtime_error("Invalid CP");

	// 【ログ】制御文字などの低位コードポイント報告
	if (codepoint < 32) {
		infostream << "UTF8FontAtlas: Low codepoint (possible ghost): 0x" 
		           << std::hex << codepoint << std::dec << std::endl;
	}

	int page = codepoint / 256;
	int index = codepoint % 256;

	// 1. ページのロード (loadPage内で st_alpha_reverse による反転は一括処理済)
	const ImageRGBA &atlas = loadPage(page);

	// 2. 共通マネージャーから現在のST看板用Config（指示書）を取得
	UTF8STDAtlas &cfg = UTF8SignManager::getInstance()->st;

	// ─── 【EXエンジン互換】画像の全幅と列数から「1部屋の幅・高さ」を全自動判別！ ───
	// これにより、12pxフォント画像でも16pxのUnifontでも、座標のズレが物理的に100%排除されます
	u32 cell_w = atlas.width / cfg.st_grid_columns;
	u32 step_h = (cfg.st_grid_size > 0) ? cfg.st_grid_size : (atlas.height / 8);

	// 3. ─── 切り出し幅(current_w)の動的ジャッジ（UAX #11） ───
	u32 current_w = cell_w; // 基本は「部屋の幅いっぱい（全角）」

	bool is_half = false;
	if (cfg.uax_half_switch) {
		// A. スイッチがONなら、大元インフラ（utf8_53）の動的仕分けベクターをスキャン
		float ratio = utf8_53::get_char_width_ratio(static_cast<uint32_t>(codepoint));
		if (ratio == 0.5f) {
			is_half = true;
		}
	} else {
		// B. スイッチがOFFの場合の、ASCII と 半角カナ 固定セーフティガード
		if (page == 0x00 && index <= 0x7F) {
			is_half = true;
		} else if (page == 0xFF && (index >= 0x61 && index <= 0x9F)) {
			is_half = true;
		}
	}

	// 半角判定なら、部屋の幅の「半分」を切り出し幅として採用！
	if (is_half) {
		current_w = cell_w / 2;
	}

	// 4. ─── ⭕ 座標計算 ───
	// 常に「cell_w / step_h (自動計算された実寸の歩幅)」を基準に切り出し位置を特定！
	int gx = (index % cfg.st_grid_columns) * cell_w;
	int gy = (index / cfg.st_grid_columns) * step_h;

	// 5. 完璧にクリップされた幅で余白を遮断して切り出し実行！
	return crop_glyph_custom(atlas, gx, gy, current_w, step_h);
}
#endif

#if UTF8_SDL2_ATLAS

std::map<int, ImageRGBA> UTF8FontAtlas::m_char_cache; // 文字画像
std::map<int, ImageRGBA> UTF8FontAtlas::m_page_cache; // ページ画像
std::list<int> UTF8FontAtlas::m_ex_char_order;        // 登録順（FIFO用）
std::list<int> UTF8FontAtlas::m_ex_page_order;        // 登録順（FIFO用）
size_t UTF8FontAtlas::m_ex_max_chars = 64;           // minetest.confから読み込む上限
size_t UTF8FontAtlas::m_ex_max_pages = 4;           // minetest.confから読み込む上限

// ex Atlas API Cache count
void UTF8FontAtlas::getMaxCacheSizeEx(u32 &max_chars, u32 &max_pages) {
	max_chars = (u32)m_ex_max_chars;
	max_pages = (u32)m_ex_max_pages;
}

void UTF8FontAtlas::getCacheCountEx(u32 &char_count, u32 &page_count) {
	char_count = (u32)m_char_cache.size();
	page_count = (u32)m_page_cache.size();
}

void UTF8FontAtlas::setMaxCacheSizeEx(u32 max_chars, u32 max_pages) {
	m_ex_max_chars = max_chars;
	m_ex_max_pages = max_pages;
	actionstream << "EX_CACHE: Set Max size: " << m_ex_max_chars << " / " << m_ex_max_pages << std::endl;
}

void UTF8FontAtlas::clearCacheEx(bool char_flag, bool page_flag) {
	if (char_flag) {
		m_char_cache.clear();
	}
	if (page_flag) {
		m_page_cache.clear();
		m_ex_page_order.clear(); // 【重要】FIFO管理リストも一緒に大掃除してゴーストバグを根絶！
	}
	actionstream << "EX_CACHE: Manual Clear (Char: " << char_flag << " / Page: " << page_flag << ")." << std::endl;
}

ImageRGBA UTF8FontAtlas::load_png_rgbaEX(const std::string &path)
{
	// 1. SDL_image で読み込み
	SDL_Surface* loaded_surface = IMG_Load(path.c_str());
	if (!loaded_surface) {
		// ここでエラーが出れば「パスの間違い」が確定
		errorstream << "UTF8FontAtlas: EX Load Failed: " << path 
					<< " Error: " << IMG_GetError() << std::endl;
		return ImageRGBA();
	}

	// 2. ★【心臓部】RGBA 32bit 形式に強制変換
	// これにより、どんなPNGでも「R,G,B,A」の順で並ぶことが保証される
	SDL_Surface* optimized = SDL_ConvertSurfaceFormat(loaded_surface, SDL_PIXELFORMAT_RGBA32, 0);
	SDL_FreeSurface(loaded_surface);

	if (!optimized) return ImageRGBA();

	// 3. ImageRGBA 構造体へ詰め替え
	ImageRGBA out;
	out.width = optimized->w;
	out.height = optimized->h;
	
	// ピクセルデータを一気にベクタへコピー
	size_t data_size = out.width * out.height * 4;
	out.data.assign((unsigned char*)optimized->pixels, 
					(unsigned char*)optimized->pixels + data_size);

	SDL_FreeSurface(optimized);
	
	// 4. 自白ログ（debug.txt や --info で確認可能）
	infostream << "UTF8FontAtlas: EX-Load Success [" << out.width << "x" << out.height 
			   << "] Path: " << path << std::endl;

	return out;
}

// EX Atlas 用
const ImageRGBA &UTF8FontAtlas::loadPageEX(int page)
{
	auto &mgr = *UTF8SignManager::getInstance();
	const ResolvedAtlas &resolved = mgr.getSelectedAtlas(); 

	//  Cache（map）を確認
	auto it = m_page_cache.find(page);
	if (it != m_page_cache.end()) return it->second;

	//  Cacheの掃除（FIFO）: 5枚目が必要になったら、一番古い1枚を捨てる
	while (m_page_cache.size() >= m_ex_max_pages && !m_ex_page_order.empty()) {
		int oldest = m_ex_page_order.front();
		m_ex_page_order.pop_front();
		m_page_cache.erase(oldest); 
		infostream << "UTF8FontAtlas: FIFO Evicted old page: " << oldest << std::endl;
	}

	// 「仕様書」に従ってパスを生成
	char filename[512];
	std::string pattern = resolved.full_path + "/" + resolved.def.file_pattern;
	snprintf(filename, sizeof(filename), pattern.c_str(), page);

	// ─── 【消失前再現】5.15.2の相対パス迷子（full_path空っぽバグ）を強制救済 ───
	std::string final_path(filename);
	if (resolved.full_path.empty() || final_path.find("//") == 0 || final_path.find("/") == 0) {
		// full_path が死んでいる場合は、魔導書本来の sub_path（例: mods/signs_lib/...）を使って再結合
		final_path = resolved.def.sub_path + "/" + resolved.def.file_pattern;
		
		char fallback_filename[512];
		snprintf(fallback_filename, sizeof(fallback_filename), final_path.c_str(), page);
		final_path = fallback_filename;
	}

	//  ロードと格納（引数を強制救済済みの final_path に差し替える！）
	ImageRGBA img = load_png_rgbaEX(final_path);
	infostream << "UTF8FontAtlas: Image Load Check -> Width: " << img.width 
		<< " Height: " << img.height 
		<< " DataSize: " << img.data.size() << std::endl;

	// alpha_reverse page単位で一括処理
		if (resolved.def.alpha_reverse) {
		for (size_t i = 0; i < img.data.size(); i += 4) {
			// RGBを反転させる（白→黒、黒→白）
//			img.data[i]     = 255 - img.data[i];     // R
//			img.data[i + 1] = 255 - img.data[i + 1]; // G
//			img.data[i + 2] = 255 - img.data[i + 2]; // B
			img.data[i + 3] = 255 - img.data[i + 3]; // A
		}
		infostream << "UTF8FontAtlas: Alpha reverse applied to page: " << page << std::endl;
	}
	m_page_cache[page] = std::move(img);
	m_ex_page_order.push_back(page);

	return m_page_cache[page];
}

ImageRGBA UTF8FontAtlas::getGlyphImageEX(int codepoint)
{
	if (codepoint < 0) return ImageRGBA();

	// ─── 🛡️ 【新設 1. 最速キャッシュ部屋チェック】 ───
	// もし、すでに過去に切り出し（crop）が終わって文字キャッシュの蔵に保管されていれば、
	// 面倒な座標計算や画像ロード、crop 処理を 100% 完全にスキップして、蔵から一瞬で爆速返却！
	if (m_char_cache.find(codepoint) != m_char_cache.end()) {
		return m_char_cache[codepoint];
	}

	// ─── 🚨 ここから先は、蔵に文字が無かった（初登場の文字）時だけの、最初で最後の切り出し処理 ───
	int page = codepoint / 256;
	int index = codepoint % 256;

	// ページのロード
	const ImageRGBA &atlas = loadPageEX(page);

	// マネージャーから「設計図（Definition）」を取得
	auto &mgr = *UTF8SignManager::getInstance();
	const AtlasDefinition &def = mgr.getSelectedAtlas().def;

	// 動的な「1部屋の幅」の算出
	u32 cell_w = atlas.width / def.grid_columns;
	u32 step_h = (def.grid_size > 0) ? def.grid_size : (atlas.height / 8);
	u32 current_w = cell_w; 
	
	// 半角判定
	if ((page == 0x00 && index <= 0x7F) || (page == 0xFF && (index >= 0x61 && index <= 0x9F))) {
		current_w = cell_w / 2;
	}

	// 座標計算
	int gx = (index % def.grid_columns) * cell_w;
	int gy = (index / def.grid_columns) * step_h;

	// ハサミで切り出し実行
	ImageRGBA glyph = crop_glyph_custom(atlas, gx, gy, (int)current_w, (int)step_h);

	// ─── 🛡️ 【新設 2. 文字キャッシュへの格納 ＆ FIFOリミッター発動！】 ───
	// メモリ内の文字キャッシュ数が、設定された上限（デフォルト64文字）に達しているかチェック
	if (m_char_cache.size() >= m_ex_max_chars && !m_ex_char_order.empty()) {
		// 一番古くに入室した文字コードを特定して追い出す（メモリ解放）
		int oldest_cp = m_ex_char_order.front();
		m_ex_char_order.pop_front();
		m_char_cache.erase(oldest_cp);
	}

	// 今回切り出したピクセルデータを、文字キャッシュの部屋へ安全に常駐保管！
	m_char_cache[codepoint] = glyph;
	
	// 新入りの文字コードを、最新メンバーとして順番待ちリストの末尾へ登録！
	m_ex_char_order.push_back(codepoint);

	return glyph; // 焼き上がった極上の文字データを返却！
}
#endif

