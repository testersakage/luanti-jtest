// src/script/common/l_utf8sign.cpp
#include "l_utf8sign.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include "client/sdl2_font.h"  // sdl2_font の存在を教える
#include "settings.h"          // g_settings を使うため
#include "client/utf8_fontengine.h"   // UTF8FontEngine を使うため
#include "client/utf8_fontatlas.h"

#include "porting.h"      // porting::path_share 用
#include "filesys.h"      // fs::PathExists 用
#include "util/string.h"  // DIR_DELIM 用（あるいは get_separator() 等）
#include "settings.h"     // g_settings 用
#include "json/json.h"
#include <fstream>
#include <sstream>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "common/c_internal.h"
#include "common/c_converter.h" // これも念のため追加
#include "log.h"

// 0=無効 , 1=有効
#define UTF8_ATLAS 1
#define UTF8_SDL2_ATLAS 0
#define UTF8_SDL2_FREETYPE 0

UTF8SignManager* UTF8SignManager::m_instance = nullptr;

UTF8SignManager* UTF8SignManager::getInstance() {
	if (!m_instance) m_instance = new UTF8SignManager();
	return m_instance;
}

#if UTF8_SDL2_ATLAS

void UTF8SignManager::registerAtlas(const AtlasDefinition &def, const std::string &path)
{
	// 重複登録のチェック：すでに同じパスが名簿にあれば、何もしない
	for (const auto &existing : m_available_atlases) {
		if (existing.full_path == path)
			return;
	}

	ResolvedAtlas res = { def, path };
	m_available_atlases.push_back(res);

	actionstream << "UTF8SignManager: Registered Atlas Resource [" << def.mod_name 
	             << "] at " << path << std::endl;
}


void UTF8SignManager::loadGrimoire(const std::string &override_path)
{
	// 二度読み防止
	static bool grimoire_loaded = false;
	if (grimoire_loaded) return;

	std::string grimoire_path = override_path;

	// 引数がない場合は、これまでの minetest.conf 経由のパスを作る
	if (grimoire_path.empty()) {
		std::string grimoire_name = g_settings->get("utf8_atlas_config");
		if (grimoire_name.empty()) return;
		grimoire_path = std::string(porting::path_share) + DIR_DELIM + "fonts" + DIR_DELIM + grimoire_name;
	}

	if (fs::PathExists(grimoire_path)) {
		actionstream << "UTF8SignManager: Found Atlas config." << std::endl;
	} else {
		// あくまで警告に留め、動作は止めない
		infostream << "UTF8SignManager: Not found Atlas config." << std::endl;
		return;
	}

	std::ifstream ifs(grimoire_path);
	Json::Value root;
	Json::Reader reader;

	if (reader.parse(ifs, root)) {
//		actionstream << "UTF8SignManager: [Atlas Config Parsing] " << grimoire_path << std::endl;

		const Json::Value atlases = root["target_atlases"];
		for (u32 i = 0; i < atlases.size(); ++i) {
			const Json::Value &entry = atlases[i];
			
			AtlasDefinition def;
			def.mod_name      = entry.get("id", "unknown").asString();
			def.sub_path      = entry.get("path", "").asString();
			def.file_pattern  = entry.get("file_pattern", "uni%02x.png").asString();
			def.grid_columns  = entry.get("grid_columns", 32).asUInt();
			def.grid_size     = entry.get("grid_size", 14).asUInt();
			def.glyph_w       = entry.get("glyph_w", 12).asUInt();
			def.glyph_h       = entry.get("glyph_h", 14).asUInt();
			def.alpha_reverse = entry.get("alpha_reverse", true).asBool();

			std::string full_path = std::string(porting::path_user) + DIR_DELIM + def.sub_path;

			// チェック用のファイル名を file_pattern から生成 (0番目のページ)
			char check_file[256];
			snprintf(check_file, sizeof(check_file), def.file_pattern.c_str(), 0);

			if (fs::PathExists(full_path + DIR_DELIM + check_file)) {
				this->registerAtlas(def, full_path);

				// ★ ここで「知識」を「肉体(Config)」へ同期！
				this->atlas.ex_char_w_zen    = def.glyph_w;
				this->atlas.ex_line_height   = def.glyph_h;
				// IDや反転フラグ、グリッドサイズもConfig側に記録しておく
				this->atlas.current_atlas_id = def.mod_name; 
//				this->atlas.grid_columns     = def.grid_columns;
				this->atlas.grid_size        = def.grid_size;
				this->atlas.alpha_reverse    = def.alpha_reverse;
				// 半角幅を自動で「全角の半分」に設定する職人技
				this->atlas.ex_char_w_han    = def.glyph_w / 2;

				actionstream << "UTF8SignManager: Config auto-synced with Atlas. " << std::endl;
			} else {
				errorstream << "UTF8SignManager: Target Atlas not found at: " << full_path << "/" << check_file << std::endl;
			}
		}
		grimoire_loaded = true;
//		actionstream << "UTF8Sign: [Grimoire Parsing Success] " << grimoire_path << std::endl;
	}

}

#endif

UTF8SignManager::UTF8SignManager() {

#if UTF8_ATLAS
	st_atlas.page_cache = 4;

	if (g_settings) {
		if (g_settings->exists("utf8_st_page_cache")) {
			u32 conf_val = g_settings->getU32("utf8_st_page_cache");
			// 「防波堤 (8)」で安全を確保
			if (conf_val > 8) conf_val = 8;
			st_atlas.page_cache = conf_val;
		}
	}

	actionstream << "UTF8SignManager: Font cache initialized with page: " 
		<< st_atlas.page_cache << std::endl;
#endif

#if UTF8_SDL2_ATLAS
	ex.char_cache = 32;

	if (g_settings) {
		if (g_settings->exists("utf8_ex_char_cache")) {
			u32 conf_val = g_settings->getU32("utf8_ex_char_cache");
			if (conf_val > 128) conf_val = 128;
			ex.char_cache = conf_val;
		}
	}

	ex.page_cache = 4;

	if (g_settings) {
		if (g_settings->exists("utf8_ex_page_cache")) {
			u32 conf_val = g_settings->getU32("utf8_ex_page_cache");
			if (conf_val > 8) conf_val = 8;
			ex.page_cache = conf_val;
		}
	}

	actionstream << "UTF8SignManager: Font cache initialized with size: " 
		<< ex.char_cache << " / page: " << ex.page_size << std::endl;
#endif

#if UTF8_SDL2_FREETYPE
	ft.cache_size = 256;
	if (g_settings) {
		if (g_settings->exists("utf8_ft_cache")) {
			u32 conf_val = g_settings->getU32("utf8_ft_cache");
			// 3. 「防波堤 (2048)」で安全を確保
			if (conf_val > 2048) conf_val = 2048;
			ft.cache_size = conf_val;
		}
	}

	actionstream << "UTF8SignManager: Font cache initialized with size: " 
		<< ft.cache_size << std::endl;
#endif
}


namespace l_utf8sign {

#if UTF8_SDL2_FREETYPE

// minetest.utf8sign.get_debug_status()
int l_get_debug_status(lua_State *L) {
	auto &ft_cfg = UTF8SignManager::getInstance()->ft; 
	lua_newtable(L);
	lua_pushstring(L, ft_cfg.last_spec.c_str());
	lua_setfield(L, -2, "spec");
	lua_pushinteger(L, ft_cfg.last_codes_size);
	lua_setfield(L, -2, "codes_size");

	// sdl2_font から最新の「自白データ」を奪い取って Lua のテーブルに詰める
	lua_pushstring(L, sdl2_font::get_last_path().c_str());
	lua_setfield(L, -2, "cpp_path"); // ★これが Lua の status.cpp_path になる

	lua_pushinteger(L, sdl2_font::get_last_error());
	lua_setfield(L, -2, "last_error"); // ★これが Lua の status.last_error になる
	return 1;
}

#endif

// minetest.utf8sign.set_config(table)
int l_set_config(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	auto &mgr = *UTF8SignManager::getInstance();

#if UTF8_SDL2_ATLAS

	// 1. atlas グループの取得
	lua_getfield(L, 1, "atlas");
	if (lua_istable(L, -1)) {
		lua_getfield(L, -1, "ex_sign_width");
		if (!lua_isnil(L, -1)) { // 型チェック関数を使う
			warningstream << "l_utf8sign: 'ex_sign_width' in set_config is DEPRECATED and ignored. "
				<< "The width is now automatically determined by the texture spec "
				<< "(e.g., [utf8combineex:WIDTHxHEIGHT:...)." << std::endl;
		}
		lua_pop(L, 1);
		mgr.atlas.ex_char_w_han   = getintfield_default(L, -1, "ex_char_w_han",   mgr.atlas.ex_char_w_han);
		mgr.atlas.ex_char_w_zen   = getintfield_default(L, -1, "ex_char_w_zen",   mgr.atlas.ex_char_w_zen);
		mgr.atlas.ex_line_height  = getintfield_default(L, -1, "ex_line_height",  mgr.atlas.ex_line_height);
		lua_getfield(L, -1, "ex_padding_x");
		if (!lua_isnil(L, -1)) {
			warningstream << "l_utf8sign: 'ex_padding_x' in set_config is ignored. "
			<< "Specify combine position in texture string (e.g., :x,y@401400) instead." << std::endl;
		}
		lua_pop(L, 1);
		lua_getfield(L, -1, "ex_padding_y");
		if (!lua_isnil(L, -1)) {
			warningstream << "l_utf8sign: 'ex_padding_y' in set_config is ignored. "
			<< "Specify combine position in texture string (e.g., :x,y@401400) instead." << std::endl;
		}
		lua_pop(L, 1);
		mgr.atlas.ex_max_lines = getintfield_default(L, -1, "ex_max_lines", mgr.atlas.ex_max_lines);
		lua_getfield(L, -1, "alpha_reverse");
		if (!lua_isnil(L, -1)) {
			warningstream << "l_utf8sign: 'alpha_reverse' cannot be set via set_config."
			<< " Please define it in your Atlas JSON." << std::endl;
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

#endif
#if UTF8_SDL2_FREETYPE

	// 2. ft グループの取得
	lua_getfield(L, 1, "ft");
	if (lua_istable(L, -1)) {
		// 文字列の更新（ttf_nameなど）が必要な場合は lua_getfield して lua_tostring 
		lua_getfield(L, -1, "ttf_name");
		if (lua_isstring(L, -1)) {
			mgr.ft.ttf_name = lua_tostring(L, -1);
		}
		lua_pop(L, 1);
		mgr.ft.font_index  = getintfield_default(L, -1, "font_index",  mgr.ft.font_index);

		mgr.ft.font_size   = getintfield_default(L, -1, "font_size",   mgr.ft.font_size);
		mgr.ft.baseline_y  = getintfield_default(L, -1, "baseline_y",  mgr.ft.baseline_y);
		// default_color はLua側からは参照専用（設定不可）
		lua_getfield(L, -1, "default_color");
		if (!lua_isnil(L, -1)) { // 型チェック関数を使う
			warningstream << "l_utf8sign: 'default_color' in set_config is ignored. "
			<< "Specify color in texture string (e.g., @401400) instead." << std::endl;
		}
		lua_pop(L, 1);
		mgr.ft.antialias   = getboolfield_default(L, -1, "antialias",  mgr.ft.antialias);

		lua_getfield(L, -1, "ft_sign_width");
		if (!lua_isnil(L, -1)) { // 型チェック関数を使う
			warningstream << "l_utf8sign: 'ft_sign_width' in set_config is DEPRECATED and ignored. "
				<< "The width is now automatically determined by the texture spec "
				<< "(e.g., [utf8combineft:WIDTHxHEIGHT:...)." << std::endl;
		}
		lua_pop(L, 1);
		mgr.ft.ft_line_height = getintfield_default(L, -1, "ft_line_height", mgr.ft.ft_line_height);
		mgr.ft.ft_max_lines   = getintfield_default(L, -1, "ft_max_lines",   mgr.ft.ft_max_lines);
		mgr.ft.ft_char_w_han  = getintfield_default(L, -1, "ft_char_w_han",  mgr.ft.ft_char_w_han);
		mgr.ft.ft_char_w_zen  = getintfield_default(L, -1, "ft_char_w_zen",  mgr.ft.ft_char_w_zen);
		lua_getfield(L, -1, "ft_padding_x");
		if (!lua_isnil(L, -1)) {
			warningstream << "l_utf8sign: 'ft_padding_x' in set_config is ignored. "
			<< "Specify combine position in texture string (e.g., :x,y@401400) instead." << std::endl;
		}
		lua_pop(L, 1);
		lua_getfield(L, -1, "ft_padding_y");
		if (!lua_isnil(L, -1)) {
			warningstream << "l_utf8sign: 'ft_padding_y' in set_config is ignored. "
			<< "Specify combine position in texture string (e.g., :x,y@401400) instead." << std::endl;
		}
		lua_pop(L, 1);
		// --- 合成モードの取得 ---
		// Lua側で blend_mode = 1 と書けば ALPHA になる
		int b_mode = getintfield_default(L, -1, "blend_mode", (int)mgr.ft.blend_mode);
		mgr.ft.blend_mode = (SignBlendMode)b_mode;
	}
	lua_pop(L, 1);

#endif

	return 0;
}

// minetest.utf8sign.get_config()
int l_get_config(lua_State *L) {
	auto &mgr = *UTF8SignManager::getInstance();
	lua_newtable(L);

#if UTF8_SDL2_ATLAS

	// atlasテーブルを作成
	lua_newtable(L);
	
	// --- JSON由来の最新ステータスを報告 ---
	lua_pushstring(L, mgr.atlas.current_atlas_id.c_str());
	lua_setfield(L, -2, "id"); // 現在アクティブなAtlas ID
	
	lua_pushboolean(L, mgr.atlas.alpha_reverse);
	lua_setfield(L, -2, "alpha_reverse");

	setintfield(L, -1, "grid_size",       mgr.atlas.grid_size);

	// --- 既存のConfig項目 ---
	setintfield(L, -1, "ex_sign_width",   mgr.atlas.ex_sign_width);
	setintfield(L, -1, "ex_char_w_han",   mgr.atlas.ex_char_w_han);
	setintfield(L, -1, "ex_char_w_zen",   mgr.atlas.ex_char_w_zen);
	setintfield(L, -1, "ex_line_height",  mgr.atlas.ex_line_height);
	setintfield(L, -1, "ex_padding_x",    mgr.atlas.ex_padding_x);
	setintfield(L, -1, "ex_padding_y",    mgr.atlas.ex_padding_y);
	setintfield(L, -1, "ex_max_lines",    mgr.atlas.ex_max_lines);

	lua_setfield(L, -2, "atlas");

#endif
#if UTF8_SDL2_FREETYPE

	// ftテーブルを作成
	lua_newtable(L);
	// --- 文字列（フォントパス） ---
	lua_pushstring(L, mgr.ft.ttf_name.c_str());
	lua_setfield(L, -2, "ttf_name");
	setintfield(L, -1, "font_index",  mgr.ft.font_index);

	setintfield(L, -1, "font_size",   mgr.ft.font_size);
	setintfield(L, -1, "baseline_y",  mgr.ft.baseline_y);
	// --- カラーテーブル（default_color） ---
	lua_newtable(L); // カラー用の小テーブル作成
	setintfield(L, -1, "r", mgr.ft.default_color.getRed());
	setintfield(L, -1, "g", mgr.ft.default_color.getGreen());
	setintfield(L, -1, "b", mgr.ft.default_color.getBlue());
	setintfield(L, -1, "a", mgr.ft.default_color.getAlpha());
	lua_setfield(L, -2, "default_color"); // ftテーブルに紐付け	
	setboolfield(L, -1, "antialias",  mgr.ft.antialias);
	// 現在の描画モードを判定して Lua に伝える
	FT_Face face = (FT_Face)sdl2_font::get_face_ptr();
	bool is_actually_gray = false;
	if (face && face->glyph) {
		// PixelModeが GRAY(2) ならアンチエイリアスが効いている
		is_actually_gray = (face->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);
	}
	setboolfield(L, -1, "actually_antialiased", is_actually_gray);

	// --- 基本項目 ---
	setintfield(L, -1, "ft_sign_width",  mgr.ft.ft_sign_width);
	setintfield(L, -1, "ft_line_height", mgr.ft.ft_line_height);
	setintfield(L, -1, "ft_max_lines",   mgr.ft.ft_max_lines);
	setintfield(L, -1, "ft_char_w_han",  mgr.ft.ft_char_w_han);
	setintfield(L, -1, "ft_char_w_zen",  mgr.ft.ft_char_w_zen);
	setintfield(L, -1, "ft_padding_x",   mgr.ft.ft_padding_x);
	setintfield(L, -1, "ft_padding_y",   mgr.ft.ft_padding_y);
	setintfield(L, -1, "blend_mode",  (int)mgr.ft.blend_mode);
	setintfield(L, -1, "cache_size",   mgr.ft.cache_size);
	lua_setfield(L, -2, "ft");

#endif

	return 1;
}

#if UTF8_ATLAS

//  Cache 使用量
int l_st_get_page_cache(lua_State *L) {
	lua_pushinteger(L, UTF8FontAtlas::getPageCache());
	return 1;
}

#endif
#if UTF8_SDL2_ATLAS
#endif
#if UTF8_SDL2_FREETYPE

//  Cache 使用量
static int l_get_cache_count(lua_State *L) {
	lua_pushinteger(L, UTF8FontEngine::getCacheCount());
	return 1;
}

//  Cache_sizeの現在の設定値確認
static int l_get_cache_size(lua_State *L) {
	u32 size = UTF8SignManager::getInstance()->ft.cache_size;
	lua_pushinteger(L, size);
	return 1;
}

//  Cache_sizeの大きさ変更（上限2048）
static int l_set_cache_size(lua_State *L) {
	u32 new_size = (u32)luaL_checkinteger(L, 1);
	if (new_size > 2048) new_size = 2048; // ★防波堤

	UTF8SignManager::getInstance()->ft.cache_size = new_size;
	return 0;
}

//  Cacheの消去
static int l_clear_cache(lua_State *L) {
	UTF8FontEngine::clearCache();
	return 0;
}

#endif

#if UTF8_SDL2_ATLAS
// minetest.utf8sign.load_atlas_config(path)
int l_utf8sign_load_atlas_config(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	UTF8SignManager::getInstance()->loadGrimoire(std::string(path));
	return 0;
}
#endif

// 共通の登録関数
void Initialize(lua_State *L, int top) {
	lua_newtable(L); // minetest.utf8sign テーブル

	// --- 共通設定 ---
	lua_pushcfunction(L, l_set_config);
	lua_setfield(L, -2, "set_config");
	lua_pushcfunction(L, l_get_config);
	lua_setfield(L, -2, "get_config");

#if UTF8_ATLAS
	// --- minetest.utf8sign.st サブテーブル ---
	lua_newtable(L);
	lua_pushcfunction(L, l_st_get_page_cache);
	lua_setfield(L, -2, "get_page_cache");
	lua_setfield(L, -2, "st");
#endif

#if UTF8_SDL2_ATLAS
	// --- minetest.utf8sign.ex サブテーブル ---
	lua_newtable(L);
	lua_pushcfunction(L, l_utf8sign_load_atlas_config);
	lua_setfield(L, -2, "load_atlas_config");
	// (今後追加する) get_cache_size などをここに
	lua_setfield(L, -2, "ex");
#endif

#if UTF8_SDL2_FREETYPE
	// --- minetest.utf8sign.ft サブテーブル ---
	lua_newtable(L); 
	lua_pushcfunction(L, l_get_cache_count);
	lua_setfield(L, -2, "get_cache_count");
	lua_pushcfunction(L, l_get_cache_size);
	lua_setfield(L, -2, "get_cache_size");
	lua_pushcfunction(L, l_set_cache_size);
	lua_setfield(L, -2, "set_cache_size");
	lua_pushcfunction(L, l_clear_cache);
	lua_setfield(L, -2, "clear_cache");
	lua_pushcfunction(L, l_get_debug_status); // FT用デバッグとしてここに配置
	lua_setfield(L, -2, "get_debug_status");
	lua_setfield(L, -2, "ft"); // utf8sign.ft として登録
#endif

	lua_setfield(L, top, "utf8sign");
}

} // namespace l_utf8sign
