// src/mcl/core/util_item.h
#pragma once
#include <lua.hpp>

namespace util_item {
	// ─── 🏆 【第3章完全閉幕：item.luaの5大機能を完全に飲み込んだ無敵の目次】 ───
	int l_get_burntime(lua_State *L);
	int l_is_fuel(lua_State *L);
	int l_calculate_durability(lua_State *L);
	int l_use_item_durability(lua_State *L);
	int l_is_item_or_in_group(lua_State *L);
}
