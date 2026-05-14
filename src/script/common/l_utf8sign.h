// src/script/common/l_utf8sign.h
#pragma once

// compile switch
#ifndef UTF8_ATLAS
    #define UTF8_ATLAS 0        // 標準Atlas (Irrlicht依存)
#endif

#ifndef UTF8_SDL2_ATLAS
    #define UTF8_SDL2_ATLAS 1   // EX Atlas (SDL2/SDL_image)
#endif

#ifndef UTF8_SDL2_FREETYPE
    #define UTF8_SDL2_FREETYPE 0 // FreeTypeエンジン (SDL2_ttf相当)
#endif

#include "lua_api/l_base.h"
#include "irrlichttypes.h"
#include "SColor.h" 
#include "../../utf8_53.h" 
#include <string>
#include <vector>

/* Mineclonia mcl_signs/init.lua の定数設定
	SIGN_WIDTH = 115		看板の物理サイズ（縦横）
	LINE_LENGTH = 15		一行の最大文字数（半角）
	NUMBER_OF_LINES = 4		最大行数
	LINE_HEIGHT = 14		一行の高さ（行間込みのpx）
	CHAR_WIDTH = 5			一文字の横幅(px)
	SIGN_GLOW_INTENSITY = 14 看板文字の明るさ
	CR_CODEPOINT = utf8.codepoint("\r")		改行コードポイント
	WRAP_CODEPOINT = utf8.codepoint("‐")	ラップコードポイント
	DEFAULT_COLOR = "#000000"				初期設定色
*/


#if UTF8_ATLAS
// アトラス
struct UTF8STDAtlas {
	u32 st_sign_width = 115;
	u32 st_char_w_han = 6;     // 半角幅の基準 
	u32 st_char_w_zen = 12;    // JSONのglyph_wと連動
	u32 st_line_height = 14;   // JSONのglyph_hと連動
	u32 st_padding_x = 0;
	u32 st_padding_y = 0;
	u32 st_max_lines = 4;
	// JSONから引き継いだ情報を保持する場所を追加
//	std::string current_atlas_id = "none";
//	bool alpha_reverse = true;
//	u32 grid_size = 14;
	// Cache Setting
	u32 page_cache = 4;
	std::string st_atlas_path = ""; // atlasの場所
};
#endif

// - - - - - - - - - - - - - - - -
#if UTF8_SDL2_ATLAS
// --- Atlas 定義情報の保管場所 ---
struct AtlasDefinition {
	std::string mod_name;      // ID
	std::string sub_path;      // 現場の住所
	std::string file_pattern;  // 画像の命名規則
	u32 grid_columns = 32;     // atlas画像の文字数（横）
	u32 grid_size = 14;        // 16pxなどの器
	u32 glyph_w = 12;          // 抜き出す幅
	u32 glyph_h = 14;          // 抜き出す高さ
	bool alpha_reverse = true; // 反転フラグ
	std::vector<utf8_53::WidthRange> half_width_ranges; // 半角指定エリアの構造体
};
// --- スキャンで見つかった「有効な仕入れ先」の情報 ---
struct ResolvedAtlas {
	AtlasDefinition def;
	std::string full_path;     // PathExistsで確認済みの絶対パス
};


// --- Atlas Render 情報格納場所 ---
struct UTF8AtlasConfig {
	u32 ex_sign_width = 115;
	u32 ex_char_w_han = 6;     // 半角幅の基準 
	u32 ex_char_w_zen = 12;    // JSONのglyph_wと連動
	u32 ex_line_height = 14;   // JSONのglyph_hと連動
	u32 ex_padding_x = 0;
	u32 ex_padding_y = 0;
	u32 ex_max_lines = 4;
	// JSONから引き継いだ情報を保持する場所を追加
	std::string ex_current_atlas_id = "none";
	std::string ex_sub_path;      // 現場の住所
	std::string ex_file_pattern;  // 画像の命名規則
	u32 ex_grid_columns = 32;     // atlas画像の文字数（横）
	u32 ex_grid_size = 14;        // 16pxなどの器
//	u32 ex_glyph_w = 12;          // 抜き出す幅
//	u32 ex_glyph_h = 14;          // 抜き出す高さ
	bool ex_alpha_reverse = true; // 反転フラグ

	// utf8_53 名前空間に定義されている WidthRange 構造体の動的ベクター配列（vector）として保持します。
	std::vector<utf8_53::WidthRange> ex_half_width_ranges;

	// EX Cache Setting
	u32 ex_char_cache = 256;
	u32 ex_page_cache = 4;
};
#endif

// - - - - - - - - - - - - - - - -
#if UTF8_SDL2_FREETYPE

enum class SignBlendMode : int {
	OVERWRITE = 0, // パキパキドット
	ALPHA     = 1, // アルファ合成
	MULTIPLY  = 2  // 乗算（インク染み込み）
};

// FreeType 看板用 構造体
struct UTF8FTConfig {
	// Font File
	std::string ttf_name = "../fonts/NotoSansCJKjp-Regular.otf";
	int font_index = 0;

	// Font Setting
	u32 font_size = 16;
	u32 baseline_y = 14;
	video::SColor default_color = video::SColor(255, 0, 0, 0);
	bool antialias = false;

	// combine setting
	u32 ft_sign_width = 115;
	u32 ft_line_height = 18;
	u32 ft_max_lines = 4;
	u32 ft_char_w_han = 10;
	u32 ft_char_w_zen = 20;
	u32 ft_padding_x = 0;
	u32 ft_padding_y = 0;
	SignBlendMode blend_mode = SignBlendMode::OVERWRITE; // デフォルトは上書き

	// FT Cache Setting
	u32 cache_size = 256; 

	// 診断用
	std::string last_ft_error = "None";
	int last_codes_size = 0;
	std::string last_spec = "";
	std::string last_font_path = "";
};

#endif

// サーバー・クライアント両方のメモリに個別に存在する「物差し」
class UTF8SignManager {
public:

	static UTF8SignManager* getInstance();

#if UTF8_ATLAS
    UTF8STDAtlas st_atlas;
#endif

#if UTF8_SDL2_ATLAS
	UTF8AtlasConfig ex;

	// 見つかったAtlasを登録する窓口
//	void registerAtlas(const AtlasDefinition &def, const std::string &path);
	void registerAtlas(const AtlasDefinition &def, const std::string &path);

	// 使用するAtlasプロファイルの選択
	void selectAtlas(const std::string &id);

	// Atlasプロファイルをjsonから読み込み
//	void loadGrimoire(const std::string &path = "");
	void loadGrimoire(const std::string &override_path);

	// Atlasプロファイルの内容を参照する
	const ResolvedAtlas& getSelectedAtlas() const;

	// 外の世界から「名簿」を安全に覗き見るための窓口
//	const std::vector<ResolvedAtlas>& getAvailableAtlases() const {
//		return m_available_atlases;
//	}
#endif

#if UTF8_SDL2_FREETYPE
	UTF8FTConfig ft;
#endif
	
//	// 文字列の幅を現在の設定から計算する共通ロジック
//	u32 getTextWidth(const std::string &text);

private:
	UTF8SignManager();
	static UTF8SignManager *m_instance;

#if UTF8_SDL2_ATLAS
	// Atlas定義リストを保管！
	std::vector<ResolvedAtlas> m_available_atlases;
#endif
};

namespace l_utf8sign {
	// --- 共通設定 (minetest.utf8sign.*) ---
	int l_set_config(lua_State *L);
	int l_get_config(lua_State *L);

#if UTF8_ATLAS
	// --- Standard Atlas専用 (minetest.utf8sign.st.*) ---
	int l_st_get_page_cache(lua_State *L);
#endif

#if UTF8_SDL2_ATLAS
	// --- Extended Atlas専用 (minetest.utf8sign.ex.*) ---
	int l_ex_load_atlas_config(lua_State *L);
	int l_ex_get_atlas_status(lua_State *L);
	int l_ex_get_char_cache(lua_State *L);
	int l_ex_get_page_cache(lua_State *L);
#endif

#if UTF8_SDL2_FREETYPE
	// --- FreeType専用 (minetest.utf8sign.ft.*) ---
	int l_ft_get_cache_size(lua_State *L);
	int l_ft_get_cache_count(lua_State *L);
	int l_ft_set_cache_size(lua_State *L);
	int l_ft_clear_cache(lua_State *L);
#endif

	// API登録用
	void Initialize(lua_State *L, int top);
}
