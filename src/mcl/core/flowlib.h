// src/mcl/core/util_flowlib.h
#pragma once
#include <lua.hpp>

namespace flowlib {
	// flowlib.lua 由来の最速流体物理演算（native_ 規律）
	int l_native_quick_flow(lua_State *L);

	// mcl_liquids/init.lua 由来の2大超重量スキャンエンジン（新設大開通）
	int l_native_does_sl_need_update(lua_State *L);
	int l_native_does_fl_need_update(lua_State *L);
}
