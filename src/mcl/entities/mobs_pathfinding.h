// src/mcl/entities/mobs_pathfinding.h
#pragma once
#include <lua.hpp>

namespace entities {

	// 👑 【最速A*経路探索筋肉群】：見る以外のスタック操作を一切しない絶対規律窓口
	int l_mobs_register_villager_native(lua_State *L);
	int l_mobs_check_poi_valid_native(lua_State *L);
	int l_mobs_filter_trades_native(lua_State *L);

	// 💎 今回強奪した 1〜1687行目の脳髄 A* アルゴリズム直撃API
	int l_mobs_set_pathfinding_speed_native(lua_State *L);
	int l_mobs_find_path_native(lua_State *L);

}
