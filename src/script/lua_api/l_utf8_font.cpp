// src/script/lua_api/l_utf8_font.cpp
#include "l_utf8_font.h"
//#include "common/lua_inc.h"
#include "../../utf8_53.h" 
#include "irrlichttypes.h"

//  utf8.get_width(text) -> 物理ピクセル幅(6/12px)を計算
int LuaUTF8Font::l_get_width(lua_State *L)
{
	const char *text = luaL_checkstring(L, 1);
	
	// utf8_53 の論理計算を使用
	int width = utf8_53::get_total_pixel_width(text);
	
	lua_pushinteger(L, width);
	return 1;
}

//  utf8.truncate(text, max_px) -> 指定幅で安全にカット
int LuaUTF8Font::l_truncate(lua_State *L)
{
	const char *text = luaL_checkstring(L, 1);
	int max_w = (int)luaL_checkinteger(L, 2);

	// utf8_53 の論理カットを使用
	std::string result = utf8_53::truncate_to_pixel_width(text, max_w);

	lua_pushstring(L, result.c_str());
	return 1;
}

//  utf8.to_table(text) -> コードポイントの配列 {123, 456...} を返す ※逆変換
int LuaUTF8Font::l_to_table(lua_State *L)
{
	const char *text = luaL_checkstring(L, 1);
	
	// utf8_53 で Unpack (逆変換)
	std::vector<int> cps = utf8_53::to_codepoints(text);
	
	lua_newtable(L);
	for (size_t i = 0; i < cps.size(); i++) {
		lua_pushinteger(L, cps[i]);
		lua_rawseti(L, -2, i + 1); // Lua は 1 始まり
	}
	return 1;
}

// utf8.get_lines(text, max_px)
int LuaUTF8Font::l_get_lines(lua_State *L)
{
	const char *text = luaL_checkstring(L, 1);
	int max_w = (int)luaL_checkinteger(L, 2);

	std::vector<std::string> lines = utf8_53::get_lines(text, max_w);

	lua_newtable(L);
	for (size_t i = 0; i < lines.size(); i++) {
		lua_pushstring(L, lines[i].c_str());
		lua_rawseti(L, -2, i + 1);
	}
	return 1;
}

void LuaUTF8Font::Initialize(lua_State *L, int top)
{
	// 既存の "utf8" テーブルを取得
	lua_getglobal(L, "utf8");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setglobal(L, "utf8");
	}

	// --- utf8 名前空間への近代化 API 登録 ---

	// utf8.get_width(text)
	lua_pushcfunction(L, l_get_width);
	lua_setfield(L, -2, "get_width");

	// utf8.truncate(text, max_px)
	lua_pushcfunction(L, l_truncate);
	lua_setfield(L, -2, "truncate");

	// utf8.to_table(text)
	lua_pushcfunction(L, l_to_table);
	lua_setfield(L, -2, "to_table");

	// utf8.get_lines(text, max_px)
	lua_pushcfunction(L, l_get_lines);
	lua_setfield(L, -2, "get_lines");

	lua_pop(L, 1); // utf8テーブルをスタックから外す
}
