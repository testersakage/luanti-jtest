// src/mcl/core/util_misc.h
#pragma once
#include <lua.hpp>

namespace util_misc {
	// mcl_util/misc.lua
	int l_generate_uuid(lua_State *L);
	int l_get_nodepos(lua_State *L);
	int l_calculate_knockback(lua_State *L);
}
