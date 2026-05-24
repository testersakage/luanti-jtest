// src/mcl/entities/mobs.h
#pragma once
#include <lua.hpp>

namespace entities {

	// 👑 【第4章・3大最速Mob筋肉】：見る以外のスタック操作を一切しない絶対規律API窓口群
	int l_mobs_register_villager_native(lua_State *L);
	int l_mobs_check_poi_valid_native(lua_State *L);
	int l_mobs_filter_trades_native(lua_State *L);

}
