// src/mcl/core/util_object.h
#pragma once
#include <lua.hpp>

namespace util_object {
	// ─── 🏆 【第3章完全閉幕：object.luaの6大数理・物理筋肉を完全に飲み込んだ無敵の目次】 ───
	int l_props_changed(lua_State *L);
	int l_get_object_center(lua_State *L);
	int l_target_eye_height(lua_State *L);
	int l_target_eye_pos(lua_State *L);
	int l_set_bone_position(lua_State *L);
	int l_rotation_to_irrlicht(lua_State *L);
}
