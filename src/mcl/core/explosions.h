// src/mcl/core/explosions.h
#pragma once
#include <lua.hpp>

namespace explosions {
	int l_native_compute_sphere_rays(lua_State *L);
	int l_native_calculate_impact(lua_State *L);
}
