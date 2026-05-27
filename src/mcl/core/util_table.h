// src/mcl/core/util_table.h
#pragma once
#include <lua.hpp>

namespace util_table {
	// CORE/mcl_util/table.lua
	int l_table_update(lua_State *L);
	void table_update_deep_recursive(lua_State *L, int target_table_idx, int source_table_idx);
	int l_table_update_deep(lua_State *L);
	int l_table_keyset(lua_State *L);
//	int l_table_reverse(lua_State *L);
//	int l_table_max_index(lua_State *L);
}
