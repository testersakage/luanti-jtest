// src/script/lua_api/l_utf8_53_client.cpp
#include "l_utf8_53_client.h"
#include "../utf8_53.h"
//#include "irrlichttypes.h" // 不要かどうか不明

// w21: utf8wrap.width(s, han, zen)
int LuaUTF8Client::l_utf8wrap_width(lua_State *L) {
	std::string s = luaL_checkstring(L, 1);
	int han = (lua_isnumber(L, 2)) ? (int)lua_tonumber(L, 2) : 6;
	int zen = (lua_isnumber(L, 3)) ? (int)lua_tonumber(L, 3) : 12;
	lua_pushinteger(L, utf8_53::get_text_width(s, han, zen));
	return 1;
}

// w22: utf8wrap.truncate(s, max_px, han, zen)
int LuaUTF8Client::l_utf8wrap_truncate(lua_State *L) {
	std::string s = luaL_checkstring(L, 1);
	int max_px = (int)luaL_checkinteger(L, 2);
	int han = (lua_isnumber(L, 3)) ? (int)lua_tonumber(L, 3) : 6;
	int zen = (lua_isnumber(L, 4)) ? (int)lua_tonumber(L, 4) : 12;
	std::string res = utf8_53::truncate_text(s, max_px, han, zen);
	lua_pushlstring(L, res.c_str(), res.length());
	return 1;
}

// w23: utf8wrap.wrap(s, max_px, han, zen)
int LuaUTF8Client::l_utf8wrap_wrap(lua_State *L) {
	std::string s = luaL_checkstring(L, 1);
	int max_px = (int)luaL_checkinteger(L, 2);
	int han = (lua_isnumber(L, 3)) ? (int)lua_tonumber(L, 3) : 6;
	int zen = (lua_isnumber(L, 4)) ? (int)lua_tonumber(L, 4) : 12;
	// utf8_53::wrap_text (手動改行を無視してひたすら詰める方) を呼ぶ
	std::vector<std::string> lines = utf8_53::wrap_text(s, max_px, han, zen);
	lua_newtable(L);
	for (size_t i = 0; i < lines.size(); ++i) {
		lua_pushstring(L, lines[i].c_str());
		lua_rawseti(L, -2, i + 1);
	}
	return 1;
}

// w24: utf8wrap.lines(s, max_px, han, zen)
int LuaUTF8Client::l_utf8wrap_lines(lua_State *L) {
	std::string s = luaL_checkstring(L, 1);
	int max_px = (int)luaL_checkinteger(L, 2);
	int han = (lua_isnumber(L, 3)) ? (int)lua_tonumber(L, 3) : 6;
	int zen = (lua_isnumber(L, 4)) ? (int)lua_tonumber(L, 4) : 12;
	std::vector<std::string> lines = utf8_53::generate_lines(s, max_px, han, zen);
	lua_newtable(L);
	for (size_t i = 0; i < lines.size(); ++i) {
		lua_pushstring(L, lines[i].c_str());
		lua_rawseti(L, -2, i + 1);
	}
	return 1;
}

void LuaUTF8Client::Initialize(lua_State *L, int top) {
	static const luaL_Reg wrap_funcs[] = {
		{"width",    LuaUTF8Client::l_utf8wrap_width},
		{"truncate", LuaUTF8Client::l_utf8wrap_truncate},
		{"wrap",     LuaUTF8Client::l_utf8wrap_wrap},
		{"lines",    LuaUTF8Client::l_utf8wrap_lines},
		{NULL, NULL}
	};
	lua_newtable(L);
	luaL_setfuncs(L, wrap_funcs, 0); // 補助ライブラリ関数で登録
	lua_setglobal(L, "utf8wrap");   // サーバー側と名前を統一！
}
