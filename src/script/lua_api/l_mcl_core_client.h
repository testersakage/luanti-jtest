#pragma once
#include <lua.hpp>

namespace l_mcl_core_client {
	void bind_mcl_util_muscles(lua_State *L);
//	void dump_lua_stack(lua_State *L, const std::string &marker);
	void Initialize(lua_State *L, int top);
	void InitializeAsync(lua_State *L, int top);
}
