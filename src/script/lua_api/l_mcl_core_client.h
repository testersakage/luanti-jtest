#pragma once
#include <lua.hpp>

namespace l_mcl_core_client {
	void bind_mcl_util_mainthread(lua_State *L);
	void bind_mcl_util_multithread(lua_State *L);
	void Initialize(lua_State *L, int top);
	void InitializeAsync(lua_State *L, int top);
}
