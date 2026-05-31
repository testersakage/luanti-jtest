// src/mcl/spatial_common.cpp
#include "spatial_common.h"

namespace mcl_spatial {

	void get_node_raw_cpp(lua_State *L, int x, int y, int z, int& cid, int& param2) {
		lua_getglobal(L, "core");
		lua_getfield(L, -1, "get_node_raw");
		lua_pushinteger(L, x);
		lua_pushinteger(L, y);
		lua_pushinteger(L, z);
		lua_call(L, 3, 3);
		cid = static_cast<int>(lua_tointeger(L, -3));
		param2 = static_cast<int>(lua_tointeger(L, -1));
		lua_pop(L, 4);
	}

	std::string get_node_class_cpp(lua_State *L, int cid) {
		lua_getglobal(L, "core");
		lua_getfield(L, -1, "get_name_from_content_id");
		lua_pushinteger(L, cid);
		lua_call(L, 1, 1);
		std::string name = lua_isstring(L, -1) ? lua_tostring(L, -1) : "ignore";
		lua_pop(L, 2);

		lua_getglobal(L, "mcl_mobs");
		lua_getfield(L, -1, "gwp_basic_node_classes");
		lua_getfield(L, -1, name.c_str());
		std::string node_class = lua_isstring(L, -1) ? lua_tostring(L, -1) : "OPEN";
		lua_pop(L, 3);
		return node_class;
	}
}
