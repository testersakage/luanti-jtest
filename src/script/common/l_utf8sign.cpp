// src/script/common/l_utf8sign.cpp
#include "l_utf8sign.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include "client/sdl2_font.h"  // ★これを追加！ sdl2_font の存在を教える
#include "settings.h"          // g_settings を使うため
#include "client/utf8_fontengine.h"   // UTF8FontEngine を使うため

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "common/c_internal.h"
#include "common/c_converter.h" // これも念のため追加
#include "log.h"

UTF8SignManager* UTF8SignManager::m_instance = nullptr;

UTF8SignManager* UTF8SignManager::getInstance() {
	if (!m_instance) m_instance = new UTF8SignManager();
	return m_instance;
}

UTF8SignManager::UTF8SignManager() {
	// 1. まずは「職人のデフォルト値」で土台を作る
	ft.cache_size = 256;

	// 2. minetest.conf (g_settings) から主人の意向を読み取る
	// ※ g_settings が使える状態であることを前提とします
	if (g_settings) {
		if (g_settings->exists("utf8_ft_cache")) {
			u32 conf_val = g_settings->getU32("utf8_ft_cache");

			// 3. 「防波堤 (2048)」で安全を確保
			if (conf_val > 2048) conf_val = 2048;
			if (conf_val == 0) conf_val = 256; // 0設定は流石に勘弁

			ft.cache_size = conf_val;
		}
	}

	actionstream << "UTF8SignManager: Font cache initialized with size " 
		<< ft.cache_size << std::endl;
}

// --- 看板の物差し（共通ロジック） ---
u32 UTF8SignManager::getTextWidth(const std::string &text) {
	u32 total_w = 0;
	size_t i = 0;
	while (i < text.length()) {
		u32 cp = 0;
		unsigned char c = (unsigned char)text[i];
		size_t char_len = 1;

		// UTF-8デコードロジックの修正
		if (c < 0x80) {
			cp = c;
			char_len = 1;
		} else if ((c & 0xE0) == 0xC0 && i + 1 < text.length()) {
			cp = ((c & 0x1F) << 6) | (text[i + 1] & 0x3F);
			char_len = 2;
		} else if ((c & 0xF0) == 0xE0 && i + 2 < text.length()) {
			cp = ((c & 0x0F) << 12) | ((text[i + 1] & 0x3F) << 6) | (text[i + 2] & 0x3F);
			char_len = 3;
		} else if ((c & 0xF8) == 0xF0 && i + 3 < text.length()) {
			cp = ((c & 0x07) << 18) | ((text[i + 1] & 0x3F) << 12) | ((text[i + 2] & 0x3F) << 6) | (text[i + 3] & 0x3F);
			char_len = 4;
		}

		// 改行コードなどは幅 0 と判定させる（看板の幅計算を狂わせないため）
		if (cp == '\n' || cp == '\r') {
			// 何もしない（または行をリセットするロジックが必要なら別途検討）
		} else {
			// 設定読み込み
			auto &atlas = UTF8SignManager::getInstance()->atlas;
			// 職人の黄金律
			// 半角 (ASCII + 半角カナ) は 6px、それ以外は 12px
			if (cp <= 0x7F || (cp >= 0xFF61 && cp <= 0xFF9F)) {
				total_w += 6;
//				total_w += atlas.ex_char_w_base;
			} else {
				total_w += 12;
//				total_w += atlas.ex_char_w_base * 2;
			}
		}
		i += char_len;
	}
	return total_w;
}

namespace l_utf8sign {

// 新設：minetest.utf8sign.get_debug_status()
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

// minetest.utf8sign.set_config(table)
int l_set_config(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	auto &mgr = *UTF8SignManager::getInstance();

	// 1. atlas グループの取得
	lua_getfield(L, 1, "atlas");
	if (lua_istable(L, -1)) {
		mgr.atlas.sign_width   = getintfield_default(L, -1, "sign_width",   mgr.atlas.sign_width);
		mgr.atlas.char_w       = getintfield_default(L, -1, "char_w",       mgr.atlas.char_w);
		mgr.atlas.line_height  = getintfield_default(L, -1, "line_height",  mgr.atlas.line_height);
		mgr.atlas.padding_x    = getintfield_default(L, -1, "padding_x",    mgr.atlas.padding_x);
		mgr.atlas.ex_max_lines = getintfield_default(L, -1, "ex_max_lines", mgr.atlas.ex_max_lines);
		mgr.atlas.ex_char_w_base = getintfield_default(L, -1, "ex_char_w_base", mgr.atlas.ex_char_w_base);
	}
	lua_pop(L, 1);

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

		lua_getfield(L, -1, "sign_width");
		if (!lua_isnil(L, -1)) { // 型チェック関数を使う
			warningstream << "l_utf8sign: 'sign_width' in set_config is DEPRECATED and ignored. "
				<< "The width is now automatically determined by the texture spec "
				<< "(e.g., [utf8combineft:WIDTHxHEIGHT:...)." << std::endl;
		}
		lua_pop(L, 1);
		mgr.ft.line_height = getintfield_default(L, -1, "line_height", mgr.ft.line_height);
		mgr.ft.max_lines   = getintfield_default(L, -1, "max_lines",   mgr.ft.max_lines);
		mgr.ft.char_w_base = getintfield_default(L, -1, "char_w_base", mgr.ft.char_w_base);
		lua_getfield(L, -1, "padding_x");
		if (!lua_isnil(L, -1)) {
			warningstream << "l_utf8sign: 'padding_x' in set_config is ignored. "
			<< "Specify combine position in texture string (e.g., :x,y@401400) instead." << std::endl;
		}
		lua_pop(L, 1);
		lua_getfield(L, -1, "padding_y");
		if (!lua_isnil(L, -1)) {
			warningstream << "l_utf8sign: 'padding_y' in set_config is ignored. "
			<< "Specify combine position in texture string (e.g., :x,y@401400) instead." << std::endl;
		}
		lua_pop(L, 1);
		// --- 合成モードの取得 ---
		// Lua側で blend_mode = 1 と書けば ALPHA になる
		int b_mode = getintfield_default(L, -1, "blend_mode", (int)mgr.ft.blend_mode);
		mgr.ft.blend_mode = (SignBlendMode)b_mode;
	}
	lua_pop(L, 1);

	return 0;
}

// minetest.utf8sign.get_config()
int l_get_config(lua_State *L) {
	auto &mgr = *UTF8SignManager::getInstance();
	lua_newtable(L);

	// atlasテーブルを作成
	lua_newtable(L);
	setintfield(L, -1, "sign_width",      mgr.atlas.sign_width);
	setintfield(L, -1, "char_w",          mgr.atlas.char_w);
	setintfield(L, -1, "line_height",     mgr.atlas.line_height);
	setintfield(L, -1, "padding_x",       mgr.atlas.padding_x);
	setintfield(L, -1, "ex_max_lines",    mgr.atlas.ex_max_lines);
	setintfield(L, -1, "ex_char_w_base",  mgr.atlas.ex_char_w_base);
	lua_setfield(L, -2, "atlas");

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
	setintfield(L, -1, "sign_width",  mgr.ft.sign_width);
	setintfield(L, -1, "line_height", mgr.ft.line_height);
	setintfield(L, -1, "max_lines",   mgr.ft.max_lines);
	setintfield(L, -1, "char_w_base", mgr.ft.char_w_base);
	setintfield(L, -1, "padding_x",   mgr.ft.padding_x);
	setintfield(L, -1, "padding_y",   mgr.ft.padding_y);
	setintfield(L, -1, "blend_mode",  (int)mgr.ft.blend_mode);
	setintfield(L, -1, "cache_size",   mgr.ft.cache_size);
	lua_setfield(L, -2, "ft");

	return 1;
}

// minetest.utf8sign.get_text_size(text)
int l_get_text_size(lua_State *L) {
	std::string text = luaL_checkstring(L, 1);
	u32 w = UTF8SignManager::getInstance()->getTextWidth(text);
	lua_pushnumber(L, w);
	return 1;
}

// 1. 在庫確認
static int l_get_cache_count(lua_State *L) {
	lua_pushinteger(L, UTF8FontEngine::getCacheCount());
	return 1;
}

// 2. Cache_sizeの現在の設定値確認
static int l_get_cache_size(lua_State *L) {
	u32 size = UTF8SignManager::getInstance()->ft.cache_size;
	lua_pushinteger(L, size);
	return 1;
}

// 3. Cache_sizeの大きさ変更（上限2048）
static int l_set_cache_size(lua_State *L) {
	u32 new_size = (u32)luaL_checkinteger(L, 1);
	if (new_size > 2048) new_size = 2048; // ★防波堤

	UTF8SignManager::getInstance()->ft.cache_size = new_size;
	return 0;
}

// 4. 蔵の大掃除
static int l_clear_cache(lua_State *L) {
	UTF8FontEngine::clearCache();
	return 0;
}


// 共通の登録関数
void Initialize(lua_State *L, int top) {
	lua_newtable(L);
	lua_pushcfunction(L, l_set_config);
	lua_setfield(L, -2, "set_config");
	lua_pushcfunction(L, l_get_config);
	lua_setfield(L, -2, "get_config");
	lua_pushcfunction(L, l_get_text_size);
	lua_setfield(L, -2, "get_text_size");
	lua_pushcfunction(L, l_get_debug_status); // ★これを追加
	lua_setfield(L, -2, "get_debug_status");

	lua_pushcfunction(L, l_get_cache_count);
	lua_setfield(L, -2, "get_cache_count");
	lua_pushcfunction(L, l_get_cache_size);
	lua_setfield(L, -2, "get_cache_size");
	lua_pushcfunction(L, l_set_cache_size);
	lua_setfield(L, -2, "set_cache_size");
	lua_pushcfunction(L, l_clear_cache);
	lua_setfield(L, -2, "clear_cache");
	lua_setfield(L, top, "utf8sign");
}

} // namespace l_utf8sign
