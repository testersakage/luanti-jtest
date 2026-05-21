// src/mcl/core/util_misc.h
#pragma once
#include <lua.hpp>

namespace util_misc {
	// ─── 🏆 【第3章完全結実：misc.luaの3大筋肉を一斉に司る無敵の目次】 ───
	int l_generate_uuid(lua_State *L);
	int l_get_nodepos(lua_State *L);
	int l_calculate_knockback(lua_State *L);
}
