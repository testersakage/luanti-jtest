// src/mcl/core/util_ringbuffer.h
#pragma once
#include <lua.hpp>

namespace util_ringbuffer {
	// ─── 🏆 【第3章新設：循環バッファ・シリアライズ最速乗っ取り筋肉の公式目次】 ───
	int l_rb_insert(lua_State *L);
	int l_rb_indexof(lua_State *L);
	int l_rb_insert_if_not_exists(lua_State *L);
	int l_rb_serialize(lua_State *L);
}
