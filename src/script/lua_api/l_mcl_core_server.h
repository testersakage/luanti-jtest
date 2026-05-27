// src/scripit/lua_api/l_mcl_core_server.h
#pragma once
#include <lua.hpp>

namespace l_mcl_core_server {
	void bind_mainthread_CORE(lua_State *L);
	void bind_mainthread_ENTITIES(lua_State *L);
//	void bind_mcl_util_mainthread(lua_State *L);
	void bind_multithread_CORE(lua_State *L);
//	void bind_mcl_util_multithread(lua_State *L);
	void Initialize(lua_State *L, int top);
//	void dump_lua_stack(lua_State *L, const std::string &marker);
}
