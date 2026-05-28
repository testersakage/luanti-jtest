// src/mcl/core/damage.h
#pragma once
#include <lua.hpp>

namespace damage {
	// mcl_damage.from_mt 由来の文字列高速解析・属性正規化（native_ 規律）
	int l_native_from_mt(lua_State *L);
}
