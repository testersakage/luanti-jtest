// src/mcl/core/util_table.cpp
#include "util_table.h"

namespace util_table {

// ❌ 1. table.update(t, ...) のC++最速ネイティブ実装
int l_table_update(lua_State *L)
{
	int top = lua_gettop(L);
	if (top < 1) return 0;
	luaL_checktype(L, 1, LUA_TTABLE); // 第1引数: ベーステーブル t

	for (int i = 2; i <= top; i++) {
		if (lua_istable(L, i)) {
			lua_pushnil(L);
			while (lua_next(L, i) != 0) {
				// スタック状態: [-2] = key, [-1] = value
				lua_pushvalue(L, -2); // keyを最上部へコピー
				lua_pushvalue(L, -2); // valueを最上部へコピー
				lua_settable(L, 1);    // t[key] = value を執行！
				lua_pop(L, 1);        // 古いvalueをポップして次へ
			}
		}
	}
//	lua_pushvalue(L, 1); // 戻り値として t をそのまま返す
	lua_settop(L, 1); 
	return 1;
}

// ❌ 2. table.reverse(t) のC++最速ネイティブ実装
int l_table_reverse(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	int n = lua_objlen(L, 1); // 配列の長さをネイティブ取得 (#t)
	
	int a = 1;
	int b = n;
	while (a < b) {
		lua_rawgeti(L, 1, a); // t[a]
		lua_rawgeti(L, 1, b); // t[b]
		
		lua_rawseti(L, 1, a); // t[a] = t[b]
		lua_rawseti(L, 1, b); // t[b] = t[a]
		
		a++;
		b--;
	}
	return 0; // 戻り値なし
}

// ❌ 3. table.max_index(t) のC++最速ネイティブ実装
int l_table_max_index(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	int max_idx = 0;
	
	lua_pushnil(L);
	while (lua_next(L, 1) != 0) {
		if (lua_isnumber(L, -2)) {
			int k = lua_tointeger(L, -2);
			if (k > max_idx) max_idx = k;
		}
		lua_pop(L, 1); // valueをポップ
	}
	lua_pushinteger(L, max_idx);
	return 1;
}

} // namespace util_table
