// src/mcl/entities/burning.h
#pragma once
#include <lua.hpp>

namespace burning {
	// mcl_burning 由来の周辺ノード・炎上消火環境一斉スキャン（native_ 規律）
	int l_native_check_burning_environment(lua_State *L);
}
