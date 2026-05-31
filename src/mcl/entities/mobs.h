// src/mcl/entities/mobs.h
#pragma once
#include <lua.hpp>

namespace mobs {
	// mcl_mobs/api.lua 由来：全個体独立タイマー一斉高速減算
	int l_native_update_mob_timers(lua_State *L);

	// mcl_mobs/api.lua 由来：足元・頭部の3面周辺環境ノード一括高速スキャン
	int l_native_mob_environment_scan(lua_State *L);

	// 👑【第13章・大本丸】：pathfinding.lua 由来の 3次元A*最速経路探索（native_ 規律）
	int l_native_gwp_compute_path(lua_State *L);
}
