// src/mcl/core/util_item.h
#pragma once
#include <lua.hpp>

namespace util_item {
	// mcl_util/item.lua
//	int l_get_burntime(lua_State *L);
//	int l_is_fuel(lua_State *L);
	int l_calculate_durability(lua_State *L);
	int l_use_item_durability(lua_State *L);
//	int l_is_item_or_in_group(lua_State *L);
}
