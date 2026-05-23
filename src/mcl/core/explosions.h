// src/mcl/core/util_explosions.h
#pragma once
#include <lua.hpp>
#include <vector>
#include <string>

namespace explosions {

	// 👑 【3大最速爆破筋肉】：見る（読み取る）以外のスタック操作を一切しない絶対規律API窓口群
	int l_explosions_raycast_sphere(lua_State *L);
	int l_explosions_calculate_damage(lua_State *L);
	int l_explosions_scorch_nodes(lua_State *L);

}
