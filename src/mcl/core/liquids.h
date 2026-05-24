// src/mcl/core/liquids.h
#pragma once
#include <lua.hpp>
#include <vector>
#include <string>

namespace liquids {

	// 流体API関数
	int l_liquids_find_flow_direction(lua_State *L);
//	int l_liquids_calculate_spread(lua_State *L);
//	int l_liquids_bulk_update_nodes(lua_State *L);

	// ???
//	int l_liquids_init_cache(lua_State *L);       // 起動時に流体IDを一括キャッシュ
//	int l_liquids_tick_native(lua_State *L);      // コルーチンの重いハッシュ探索をC++で丸ごと強奪完食！
//	int l_liquids_does_need_update(lua_State *L); // 周囲5マスの水位検門をFPU最速走査

	// flowlib
	int l_liquids_quick_flow_native(lua_State *L);
}
