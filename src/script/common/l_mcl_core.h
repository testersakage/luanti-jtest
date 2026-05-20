// src/script/common/l_mcl_core.h
#pragma once

extern "C" {
#include <lua.h>
}

namespace l_mcl_core {
	// Lua側へ minetest.mcl_core テーブルと配下の爆速APIを一斉大開放する登録関数
	void Initialize(lua_State *L, int top);
}
