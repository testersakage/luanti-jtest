// src/mcl/core/util_shape.h
#pragma once
#include <lua.hpp>

namespace util_shape {
	// ─── 🏆 【第3章完全閉幕：shape.lua 1117行・全9大数理APIの公式確定目次】 ───
	int l_decompose_aabbs(lua_State *L);    // #1
	int l_region_op(lua_State *L);          // #2
	int l_region_evaluate(lua_State *L);    // #3 👑 これが必要です！
	int l_any_occupied_p(lua_State *L);     // #4 👑 これが必要です！
	int l_region_volume(lua_State *L);      // #5
	int l_region_equal_p(lua_State *L);     // #6
	int l_region_walk(lua_State *L);        // #7
	int l_region_simplify(lua_State *L);    // #8
	int l_region_select_face(lua_State *L); // #9
}
