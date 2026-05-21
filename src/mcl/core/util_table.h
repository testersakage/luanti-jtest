// src/mcl/core/util_table.h
#pragma once
#include <lua.hpp>

namespace util_table {
	// ─── 🏆 【table拡張・C++ネイティブ最速処理エンジンの公式目次】 ───
	int l_table_update(lua_State *L);
	int l_table_reverse(lua_State *L);
	int l_table_max_index(lua_State *L);
}
