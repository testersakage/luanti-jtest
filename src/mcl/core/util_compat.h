// src/mcl/core/util_compat.h
#pragma once
#include <lua.hpp>

namespace util_compat {
	// ─── 🏆 【第3章完全閉幕：互換部屋の全機能を完全に飲み込んだ無敵の目次】 ───
	int l_vector_random_direction(lua_State *L);   // #1
	int l_connected_players(lua_State *L);         // #2
	int l_get_node_raw(lua_State *L);              // #3
	int l_time_to_day_night_ratio(lua_State *L);   // #4
}
