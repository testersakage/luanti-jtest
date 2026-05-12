// src/script/lua_api/l_utf8_53_server.cpp
#include "l_utf8_53_server.h"
#include "common/c_types.h"
#include "../utf8_53.h"

// class定義でエラー出たら以下(ModApiServer側)に修正
//#include "lua_api/l_internal.h"
//#include "lua_api/l_base.h"

// --- 補助関数: インデックス変換 ---
static size_t get_pos_arg(lua_State *L, int arg_idx, size_t len, size_t default_val)
{
	if (lua_isnoneornil(L, arg_idx))
		return default_val;

	lua_Integer pos = luaL_checkinteger(L, arg_idx);
	if (pos > 0) return (size_t)pos - 1;
	else if (pos < 0) {
		if ((size_t)(-pos) > len) return 0;
		return len + (size_t)pos;
	}
	return 0;
}

// --- 補助関数: utf8.codes 用のイテレータ本体 ---
/**
 * Luaからの呼び出し形式: next_pos, code = iter(s, last_next_pos)
 */
static int iter_codes(lua_State *L)
{
	// 第1引数: 対象の文字列
	size_t len;
	const char *s_ptr = luaL_checklstring(L, 1, &len);
	
	// 第2引数: 前回の呼び出しで返した「次の開始位置」(0始まり)
	// 初回は Initialize で設定した 0 が入る
	size_t next_pos = (size_t)lua_tointeger(L, 2);

	// 文字列の終端に達していたら nil を返して終了
	if (next_pos >= len) {
		return 0;
	}

	int code_point;
	size_t current_pos = next_pos; // 今回の文字の開始位置を記録

	// get_next_char で next_pos を次の文字の先頭へ進める
	// 文字列のコピーを避けるため std::string_view 的な扱いで呼び出し
	std::string s(s_ptr, len); 
	if (utf8_53::get_next_char(s, next_pos, code_point)) {
		
		// 戻り値1: 次回の呼び出し時に第2引数として渡される値 (内部状態: 次の位置)
		lua_pushinteger(L, (lua_Integer)next_pos);
		
		// 戻り値2: Lua側の for i, code ... の 「i」 に入る値 (現在の位置: 1始まり)
		lua_pushinteger(L, (lua_Integer)current_pos + 1);
		
		// 戻り値3: Lua側の for i, code ... の 「code」 に入る値
		lua_pushinteger(L, (lua_Integer)code_point);
		
		// 合計3つの値を返す
		return 3;
	}
	
	// 解析に失敗した場合も終了
	return 0;
}

// --- Lua API 実装 ---

int LuaUTF8::l_utf8_char(lua_State *L) {
	int n = lua_gettop(L);
	std::string result;
	for (int i = 1; i <= n; i++) {
		utf8_53::push_char(result, (int)luaL_checkinteger(L, i));
	}
	lua_pushlstring(L, result.c_str(), result.length());
	return 1; 
}

int LuaUTF8::l_utf8_codes(lua_State *L) {
	luaL_checkstring(L, 1);
	lua_pushcfunction(L, iter_codes);
	lua_pushvalue(L, 1);
	lua_pushinteger(L, 0); // 初期位置
	return 3;
}

int LuaUTF8::l_utf8_codepoint(lua_State *L) {
	size_t len;
	const char *s_ptr = luaL_checklstring(L, 1, &len);
	std::string s(s_ptr, len);
	size_t i = get_pos_arg(L, 2, len, 0);
	size_t j = get_pos_arg(L, 3, len, i);

	int count = 0;
	size_t pos = i;
	while (pos <= j && pos < len) {
		int cp;
		if (!utf8_53::get_next_char(s, pos, cp))
			return luaL_error(L, "invalid UTF-8 code");
		lua_pushinteger(L, cp);
		count++;
	}
	return count; 
}

int LuaUTF8::l_utf8_len(lua_State *L)
{
	size_t byte_len;
	const char *s_ptr = luaL_checklstring(L, 1, &byte_len);
	std::string s(s_ptr, byte_len);

	size_t i = get_pos_arg(L, 2, byte_len, 0);
	size_t j = get_pos_arg(L, 3, byte_len, byte_len - 1);

	size_t err_pos = 0;
	int count = utf8_53::count_chars(s, i, j, err_pos);

	if (count >= 0) {
		lua_pushinteger(L, count);
		return 1;
	}
	lua_pushboolean(L, false);
	lua_pushinteger(L, err_pos + 1);
	return 2;
}

int LuaUTF8::l_utf8_offset(lua_State *L) {
	size_t len;
	const char *s_ptr = luaL_checklstring(L, 1, &len);
	std::string s(s_ptr, len);
	lua_Integer n = luaL_checkinteger(L, 2);
	size_t i = get_pos_arg(L, 3, len, (n >= 0) ? 0 : len);

	if (i > len) return luaL_error(L, "initial position out of string");

	if (n == 0) {
		while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--;
		lua_pushinteger(L, i + 1);
		return 1;
	}

	size_t pos = i;
	if (n > 0) {
		n--; 
		while (n > 0 && pos < len) {
			int cp;
			if (!utf8_53::get_next_char(s, pos, cp)) break;
			n--;
		}
	} else {
		// 負のnの処理：逆方向に文字の先頭バイトを探してスキャン
		while (n < 0 && pos > 0) {
			pos--; // 1バイト戻る
			// そのバイトが「後続バイト(10xxxxxx)」である間、さらに戻る
			while (pos > 0 && ((unsigned char)s[pos] & 0xC0) == 0x80) {
				pos--;
			}
			n++; // 1文字見つけたのでカウントアップ
		}
	}

	if (n == 0) lua_pushinteger(L, pos + 1);
	else lua_pushnil(L);
	return 1; 
}

 // utf8.eaw_width(s [, han_w, zen_w])  // East Asian Widthに基づく表示幅を返します。
int LuaUTF8::l_utf8_eaw_width(lua_State *L) {
	std::string s = luaL_checkstring(L, 1);
	// 引数があれば採用、なければデフォルト（6, 12）
	int han_w = (lua_isnumber(L, 2)) ? lua_tonumber(L, 2) : 6;
	int zen_w = (lua_isnumber(L, 3)) ? lua_tonumber(L, 3) : 12;

	// 先ほど改造した utf8_53::get_total_pixel_width へリレー！
//	lua_pushinteger(L, utf8_53::get_total_pixel_width(s, han_w, zen_w));
	lua_pushinteger(L, utf8_53::get_text_width(s, han_w, zen_w));
	return 1;
}

// utf8.eaw_truncate(s, max_px [, han_w, zen_w])  // 指定した表示幅で文字列を切り詰めます。
int LuaUTF8::l_utf8_eaw_truncate(lua_State *L) {
	size_t len;
	const char *s_ptr = luaL_checklstring(L, 1, &len);
	std::string s(s_ptr, len);
	int max_width = (int)luaL_checkinteger(L, 2);

	// --- 職人のバケツリレー開始 ---
	// 第3・第4引数があれば読み取り、なければデフォルト値（6, 12）
	int han_w = (lua_isnumber(L, 3)) ? (int)lua_tonumber(L, 3) : 6;
	int zen_w = (lua_isnumber(L, 4)) ? (int)lua_tonumber(L, 4) : 12;

	// 実装本体（utf8_53.cpp）にすべてを任せる！
//	std::string res = utf8_53::truncate_to_pixel_width(s, max_width, han_w, zen_w);
	std::string res = utf8_53::truncate_text(s, max_width, han_w, zen_w);

	lua_pushlstring(L, res.c_str(), res.length());
	return 1;
}

// UTF-8 Wrapper API

// utf8wrap.char_width(cp) -> 文字の論理幅(1 or 2)を返す
int LuaUTF8::l_utf8_eaw_char_width(lua_State *L) {
	int cp = (int)luaL_checkinteger(L, 1);
	lua_pushinteger(L, utf8_53::get_char_width(cp));
	return 1;
}

// w21: utf8wrap.width(s, han, zen)
int LuaUTF8::l_utf8wrap_width(lua_State *L) {
	std::string s = luaL_checkstring(L, 1);
	int han = (lua_isnumber(L, 2)) ? (int)lua_tonumber(L, 2) : 6;
	int zen = (lua_isnumber(L, 3)) ? (int)lua_tonumber(L, 3) : 12;
	lua_pushinteger(L, utf8_53::get_text_width(s, han, zen));
	return 1;
}

// w22: utf8wrap.truncate(s, max_px, han, zen)
int LuaUTF8::l_utf8wrap_truncate(lua_State *L) {
	std::string s = luaL_checkstring(L, 1);
	int max_px = (int)luaL_checkinteger(L, 2);
	int han = (lua_isnumber(L, 3)) ? (int)lua_tonumber(L, 3) : 6;
	int zen = (lua_isnumber(L, 4)) ? (int)lua_tonumber(L, 4) : 12;
	std::string res = utf8_53::truncate_text(s, max_px, han, zen);
	lua_pushlstring(L, res.c_str(), res.length());
	return 1;
}

// w23: utf8wrap.wrap(s, max_px, han, zen)
int LuaUTF8::l_utf8wrap_wrap(lua_State *L) {
	std::string s = luaL_checkstring(L, 1);
	int max_px = (int)luaL_checkinteger(L, 2);
	int han = (lua_isnumber(L, 3)) ? (int)lua_tonumber(L, 3) : 6;
	int zen = (lua_isnumber(L, 4)) ? (int)lua_tonumber(L, 4) : 12;
	std::vector<std::string> lines = utf8_53::wrap_text(s, max_px, han, zen);
	lua_newtable(L);
	for (size_t i = 0; i < lines.size(); ++i) {
		lua_pushstring(L, lines[i].c_str());
		lua_rawseti(L, -2, i + 1);
	}
	return 1;
}

// w24: utf8wrap.lines(s, max_px, han, zen)
int LuaUTF8::l_utf8wrap_lines(lua_State *L) {
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


// --- 登録リスト ---
// class定義でエラー出たら以下(ModApiServer側)に修正
//void ModApiServerCJK::Initialize(lua_State *L, int top)
void LuaUTF8::Initialize(lua_State *L, int top)
{
	static const luaL_Reg utf8_funcs[] = {
		{"char",	  LuaUTF8::l_utf8_char},
		{"codes",	 LuaUTF8::l_utf8_codes},
		{"codepoint", LuaUTF8::l_utf8_codepoint},
		{"len",	   LuaUTF8::l_utf8_len},
		{"offset",	LuaUTF8::l_utf8_offset},
		// Lua側から見える名前を eaw_... に設定
		{"eaw_width",	LuaUTF8::l_utf8_eaw_width},
		{"eaw_truncate", LuaUTF8::l_utf8_eaw_truncate},
		{NULL, NULL}
	};

	lua_newtable(L);
	luaL_setfuncs(L, utf8_funcs, 0);
	lua_pushstring(L, "[\0-\x7F\xC2-\xF4][\x80-\xBF]*");
	lua_setfield(L, -2, "charpattern");
	// グローバルに "utf8" として公開
	lua_setglobal(L, "utf8");

	// ---  utf8wrap テーブルの登録 ---
	static const luaL_Reg wrap_funcs[] = {
		{"width",    LuaUTF8::l_utf8wrap_width},
		{"truncate", LuaUTF8::l_utf8wrap_truncate},
		{"wrap",     LuaUTF8::l_utf8wrap_wrap},
		{"lines",    LuaUTF8::l_utf8wrap_lines},
		{NULL, NULL}
	};
	lua_newtable(L);
	luaL_setfuncs(L, wrap_funcs, 0);
	lua_setglobal(L, "utf8wrap"); // グローバルに公開！
}
