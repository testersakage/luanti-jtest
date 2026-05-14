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


// static メンバ変数の実体定義を忘れずに！
//std::map<int, ImageRGBA> UTF8FontAtlas::m_atlas_pages;

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

/* --- 1. 切り出し関数（上に置くか、プロトタイプ宣言が必要） --- */
/**
static ImageRGBA crop_glyph_12x12(const ImageRGBA &src, int gx, int gy)
{
	ImageRGBA out;
	out.width = 12;
	out.height = 12;
	out.data.resize(12 * 12 * 4);

	for (int yy = 0; yy < 12; yy++) {
		for (int xx = 0; xx < 12; xx++) {
			int sx = gx + xx;
			int sy = gy + yy;
			if (sx >= src.width || sy >= src.height) continue;

			int src_i = (sy * src.width + sx) * 4;
			int dst_i = (yy * 12 + xx) * 4;
			for (int k = 0; k < 4; k++) out.data[dst_i + k] = src.data[src_i + k];
		}
	}
	return out;
}
*/

#if UTF8_ATLAS
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

std::map<int, ImageRGBA> UTF8FontAtlas::m_st_pages;
std::list<int> UTF8FontAtlas::m_st_page_order;
size_t UTF8FontAtlas::m_st_max_pages = 4;

// STD Atlas 用
const ImageRGBA &UTF8FontAtlas::loadPage(int page)
{
	//  Cache（map）を確認
	auto it = m_st_pages.find(page);
	if (it != m_st_pages.end()) return it->second;

	//  Cacheの掃除（FIFO）: 5枚目が必要になったら、一番古い1枚を捨てる
	while (m_st_pages.size() >= m_st_max_pages && !m_st_page_order.empty()) {
		int oldest = m_st_page_order.front();
		m_st_page_order.pop_front();
		m_st_pages.erase(oldest); 
		infostream << "UTF8FontAtlas: FIFO Evicted old page: " << oldest << std::endl;
	}
/*
	//  パス生成（将来のポータビリティを考慮して整理）
	char filename[512];
	// ※ここはあなたの環境に合わせて、あるいは後でポータブルなパス取得に置き換え
	snprintf(filename, sizeof(filename), 
//		"C:/msys64/home/localuser/luanti/games/mineclonia-jtest/mods/ITEMS/mcl_signs/textures/unicode_page_%02x.png", 
		"C:/msys64/home/localuser/luanti/mods/mod_utf8sign_sample/textures/unicode_page_%02x.png", 
//		"unicode_page_%02x.png", 
		page);

	infostream << "UTF8FontAtlas: Loading new ST-page: " << filename << " (Total: " << m_st_pages.size() + 1 << ")" << std::endl;
*/

	// マネージャーからパスを拝借
	auto &st_cfg = UTF8SignManager::getInstance()->st_atlas;
	std::string base_path = st_cfg.st_atlas_path;

	char filename[512];
	// atlas_path には "textures/unicode_page_%02x.png" と書いてある想定
	snprintf(filename, sizeof(filename), st_cfg.st_atlas_path.c_str(), page);

	infostream << "UTF8FontAtlas: Loading via full-pattern: " << filename << std::endl;

	//  ロードと格納（std::move で所有権をスマートに移譲）
	ImageRGBA img = load_png_rgba(filename);
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

    // 1. ページのロード
    const ImageRGBA &atlas = loadPage(page);

    // 2. ★ アトラスの実サイズから「1行の高さ」を動的に算出
    // アトラスは 256文字(32x8グリッド)なので、高さ / 8行 でステップを出す
    // 96pxなら 12、112pxなら 14 と自動判定される
    u32 actual_line_h = atlas.height / 8;

    // 3. ★ 職人の黄金律を適用 (幅の判定)
    // ASCII(00) および 半角カナ(FF) の範囲を 6px、それ以外を 12px とする
    int current_w = 12;
    if (page == 0x00 && index <= 0x7F) {
        current_w = 6;
    } else if (page == 0xFF && (index >= 0x61 && index <= 0x9F)) {
        current_w = 6;
    }

    // 4. 座標計算
    // 横(gx)は常に 12px 刻みのグリッド
    // 縦(gy)はステップ高(12 or 14)に応じた位置を計算
    int gx = (index % 32) * 12;
    int gy = (index / 32) * actual_line_h;

    // 5. 切り出し実行 (高さは画像の実態に合わせる)
    return crop_glyph_custom(atlas, gx, gy, current_w, actual_line_h);
}

// st Atlas API Cache count
u32 UTF8FontAtlas::getPageCache() {
    return (u32)m_st_pages.size();
}
#endif

#if UTF8_SDL2_ATLAS

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

std::map<int, ImageRGBA> UTF8FontAtlas::m_char_cache; // 文字画像
std::map<int, ImageRGBA> UTF8FontAtlas::m_page_cache; // ページ画像
std::list<int> UTF8FontAtlas::m_ex_page_order;        // 登録順（FIFO用）
size_t UTF8FontAtlas::m_ex_max_pages = 4;           // minetest.confから読み込む上限

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

	int page = codepoint / 256;
	int index = codepoint % 256;

	// ページのロード
	const ImageRGBA &atlas = loadPageEX(page);

	// マネージャーから「設計図（Definition）」を取得
	auto &mgr = *UTF8SignManager::getInstance();
	const AtlasDefinition &def = mgr.getSelectedAtlas().def;

	// --- ★動的な「1部屋の幅」の算出 ---
	// 画像の全幅を列数で割ることで、12px規格か16px規格かを自動判別
	u32 cell_w = atlas.width / def.grid_columns;
	
	// 縦の歩幅（JSONの grid_size があれば優先、なければ 8行分割）
	u32 step_h = (def.grid_size > 0) ? def.grid_size : (atlas.height / 8);

	// --- ★切り出し幅(current_w)の決定 ---
	u32 current_w = cell_w; // 基本は「部屋の幅いっぱい」
	
	// 半角判定（0x00: ASCII, 0xFF: 半角カナ）
	if ((page == 0x00 && index <= 0x7F) || (page == 0xFF && (index >= 0x61 && index <= 0x9F))) {
		// 部屋の幅の半分を「半角」として扱う
		current_w = cell_w / 2;
	}

	// --- ★座標計算 ---
	// 常に「cell_w (実寸の歩幅)」を基準にすることで、ズレを物理的に排除
	int gx = (index % def.grid_columns) * cell_w;
	int gy = (index / def.grid_columns) * step_h;

	// 切り出し実行
	return crop_glyph_custom(atlas, gx, gy, (int)current_w, (int)step_h);
}
#endif

