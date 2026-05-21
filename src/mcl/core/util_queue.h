// src/mcl/core/util_queue.h
#pragma once
#include <lua.hpp>

namespace util_queue {
	// ─── 🏆 【第3章完全閉幕：queue.luaの5大機能を完全に飲み込んだ無敵の目次】 ───
	int l_queue_enqueue(lua_State *L);
	int l_queue_dequeue(lua_State *L);
	int l_queue_peek(lua_State *L);
	int l_queue_size(lua_State *L);
	int l_queue_iterate(lua_State *L);
}
