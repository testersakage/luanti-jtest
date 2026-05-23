#pragma once
#include <lua.hpp>

namespace l_mcl_core_server {
	void bind_mcl_util_mainthread(lua_State *L);
	void bind_mcl_util_multithread(lua_State *L);
//	void dump_lua_stack(lua_State *L, const std::string &marker);
	void Initialize(lua_State *L, int top);
	void InitializeAsync(lua_State *L, int top);
}
