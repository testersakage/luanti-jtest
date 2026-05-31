// src/mcl/core/damage.cpp
#include "damage.h"
#include "mcl/math_common.h" // 📐 純粋数理本体
#include <lua.hpp>
#include <string>

namespace damage {
	
	// 個別対応名義規律：mclcapi.native.from_mt
	int l_native_from_mt(lua_State *L) {
		luaL_checktype(L, 1, LUA_TTABLE);
		lua_getfield(L, 1, "_mcl_cached_reason");
		if (!lua_isnil(L, -1)) return 1;
		lua_pop(L, 1);

		lua_getfield(L, 1, "_mcl_reason");
		if (lua_istable(L, -1)) return 1;
		lua_pop(L, 1);

		lua_newtable(L); 
		lua_pushstring(L, "generic"); lua_setfield(L, -2, "type"); 

		lua_getfield(L, 1, "type"); std::string type_str = lua_isstring(L, -1) ? lua_tostring(L, -1) : ""; lua_pop(L, 1);
		lua_getfield(L, 1, "_mcl_type"); std::string mcl_type_str = lua_isstring(L, -1) ? lua_tostring(L, -1) : ""; lua_pop(L, 1);

		if (!mcl_type_str.empty()) {
			lua_pushstring(L, mcl_type_str.c_str()); lua_setfield(L, -2, "type");
		} else if (type_str == "fall") {
			lua_pushstring(L, "fall"); lua_setfield(L, -2, "type");
		} else if (type_str == "drown") {
			lua_pushstring(L, "drown"); lua_setfield(L, -2, "type");
		} else if (type_str == "punch") {
			lua_pushstring(L, "punch"); lua_setfield(L, -2, "type");
			lua_pushboolean(L, true); lua_setfield(L, -2, "_mcl_trigger_punch");
		}

		lua_pushnil(L); 
		while (lua_next(L, 1) != 0) {
			if (lua_type(L, -2) == LUA_TSTRING) {
				const char* key_cstr = lua_tostring(L, -2);
				std::string new_key;
				// 🎯 純粋数理本体（trim_prefix）をスタック操作0で直撃連打！
				if (mcl_math::trim_prefix(key_cstr, "_mcl_", new_key)) {
					lua_pushstring(L, new_key.c_str());
					lua_pushvalue(L, -2); 
					lua_settable(L, -5); 
				}
			}
			lua_pop(L, 1); 
		}
		return 1; 
	}
}
