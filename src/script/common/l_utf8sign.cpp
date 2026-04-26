#include "l_utf8sign.h"
#include "client/sdl2_font.h"  // ★これを追加！ sdl2_font の存在を教える

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
		mgr.ft.antialias   = getboolfield_default(L, -1, "antialias",  mgr.ft.antialias);

		mgr.ft.sign_width  = getintfield_default(L, -1, "sign_width",  mgr.ft.sign_width);
		mgr.ft.line_height = getintfield_default(L, -1, "line_height", mgr.ft.line_height);
		mgr.ft.max_lines   = getintfield_default(L, -1, "max_lines",   mgr.ft.max_lines);
		mgr.ft.char_w_base = getintfield_default(L, -1, "char_w_base", mgr.ft.char_w_base);
		mgr.ft.padding_x   = getintfield_default(L, -1, "padding_x",   mgr.ft.padding_x);
		mgr.ft.padding_y   = getintfield_default(L, -1, "padding_y",   mgr.ft.padding_y);
	}
	lua_pop(L, 1);

	return 0;

/*	
	UTF8SignConfig &cfg = UTF8SignManager::getInstance()->config;

	// Luaテーブルから値を取得し、C++の構造体を更新
	cfg.width       = getintfield_default(L, 1, "width", cfg.width);
	cfg.line_height = getintfield_default(L, 1, "line_height", cfg.line_height);
	cfg.max_lines   = getintfield_default(L, 1, "max_lines", cfg.max_lines);
	cfg.padding_x   = getintfield_default(L, 1, "padding_x", cfg.padding_x);
	cfg.char_w_base = getintfield_default(L, 1, "char_w", cfg.char_w_base);
	
	// 設定されたことをログに（デバッグ段階では重要）
	// actionstream << "UTF8Sign: Config updated. LineHeight=" << cfg.line_height << std::endl;
	
	return 0;
*/
}

// minetest.utf8sign.get_config()
int l_get_config(lua_State *L) {
	auto &mgr = *UTF8SignManager::getInstance();
	lua_newtable(L);

	// atlasテーブルを作成
	lua_newtable(L);
	setintfield(L, -1, "sign_width",   mgr.atlas.sign_width);
	setintfield(L, -1, "ex_max_lines", mgr.atlas.ex_max_lines);
	lua_setfield(L, -2, "atlas");

	// ftテーブルを作成
	lua_newtable(L);
	setintfield(L, -1, "font_size",   mgr.ft.font_size);
	setintfield(L, -1, "font_index",  mgr.ft.font_index);
	setboolfield(L, -1, "antialias",  mgr.ft.antialias);
	lua_setfield(L, -2, "ft");

	return 1;
/*
	UTF8SignConfig &cfg = UTF8SignManager::getInstance()->config;
	lua_newtable(L);
	setintfield(L, -1, "width", cfg.width);
	setintfield(L, -1, "line_height", cfg.line_height);
	setintfield(L, -1, "max_lines", cfg.max_lines);
	setintfield(L, -1, "padding_x", cfg.padding_x);
	setintfield(L, -1, "char_w", cfg.char_w_base);
	return 1;
*/
}

// minetest.utf8sign.get_text_size(text)
int l_get_text_size(lua_State *L) {
	std::string text = luaL_checkstring(L, 1);
	u32 w = UTF8SignManager::getInstance()->getTextWidth(text);
	lua_pushnumber(L, w);
	return 1;
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
	lua_setfield(L, top, "utf8sign");
}

} // namespace l_utf8sign
