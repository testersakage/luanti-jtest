// src/mcl/stacktrace.h
#pragma once
#include <lua.hpp>

namespace stacktrace {
	// ─── 👑 【MCL共通・ハッカーの引数スタック逆探知検品マシーン】 ───
	// Lua側から降ってきた引数（1～top）の中から、指定されたメソッド（関数名）を
	// 内部に持っている本物のテーブルの部屋番号（インデックス）を一撃であぶり出します！
	int find_table_by_method(lua_State *L, const char *method_name);
}
