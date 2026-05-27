// src/mcl/core/util_object.h
#pragma once
#include <lua.hpp>

namespace util_object {

	// object.lua
//	int l_props_changed(lua_State *L);
//	int l_get_object_center(lua_State *L);
//	int l_target_eye_height(lua_State *L);
//	int l_target_eye_pos(lua_State *L);
//	int l_set_bone_position(lua_State *L);
	int l_rotation_to_irrlicht(lua_State *L);
}
