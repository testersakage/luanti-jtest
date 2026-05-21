// src/mcl/core/util_queue.cpp
#include "util_queue.h"
#include "mcl/stacktrace.h" // 👑 あなたの創設した無敵の逆探知インフラを結合！

namespace util_queue {

// ❌ 1. q:enqueue(value) のC++完全移植
int l_queue_enqueue(lua_State *L)
{
	int target_idx = stacktrace::find_table_by_method(L, "size");
	if (target_idx == 0) return 0;

	int top = lua_gettop(L);
	luaL_checkany(L, top); // valueの回収

	lua_rawgeti(L, target_idx, 1); int back = lua_tointeger(L, -1); lua_pop(L, 1);
	
	lua_pushvalue(L, top);
	lua_rawseti(L, target_idx, back); // self[back] = value

	lua_pushinteger(L, back + 1);
	lua_rawseti(L, target_idx, 1); // self[1] = back + 1

	return 0;
}

// ❌ 2. q:dequeue() のC++完全移植
int l_queue_dequeue(lua_State *L)
{
	int target_idx = stacktrace::find_table_by_method(L, "size");
	if (target_idx == 0) return 0;

	lua_rawgeti(L, target_idx, 1); int back = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_rawgeti(L, target_idx, 2); int front = lua_tointeger(L, -1); lua_pop(L, 1);

	if (front >= back) return 0;

	lua_rawgeti(L, target_idx, front); // value = self[front]
	
	lua_pushnil(L);
	lua_rawseti(L, target_idx, front); // self[front] = nil

	lua_pushinteger(L, front + 1);
	lua_rawseti(L, target_idx, 2); // self[2] = front + 1

	return 1;
}

// ❌ 3. q:peek() のC++完全移植（先頭タスクの覗き見筋肉）
int l_queue_peek(lua_State *L)
{
	int target_idx = stacktrace::find_table_by_method(L, "size");
	if (target_idx == 0) return 0;

	lua_rawgeti(L, target_idx, 1); int back = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_rawgeti(L, target_idx, 2); int front = lua_tointeger(L, -1); lua_pop(L, 1);

	if (front >= back) return 0; // 空なら何も返さない

	lua_rawgeti(L, target_idx, front); // 先頭データを減らさずにただ読み込んでスタックへ積む
	return 1;
}

// ❌ 4. q:size() のC++完全移植
int l_queue_size(lua_State *L)
{
	int target_idx = stacktrace::find_table_by_method(L, "size");
	if (target_idx == 0) { lua_pushinteger(L, 0); return 1; }

	lua_rawgeti(L, target_idx, 1); int back = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_rawgeti(L, target_idx, 2); int front = lua_tointeger(L, -1); lua_pop(L, 1);

	lua_pushinteger(L, back - front);
	return 1;
}

// 👑 【最深部：iterateクロージャのC++ネイティブ超速イテレーター関数】
static int l_queue_iterator_next(lua_State *L)
{
	// Upvalueから、キュー本体（1）と現在のインデックス（2）を回収！
	lua_pushvalue(L, lua_upvalueindex(1)); // self
	int idx = lua_tointeger(L, lua_upvalueindex(2));

	lua_rawgeti(L, -1, 1); int back = lua_tointeger(L, -1); lua_pop(L, 1);

	if (back <= idx) {
		return 0; // すべて舐め終わったらループ終了！
	}

	// 次のインデックスへ進めてUpvalueへ再保存
	lua_pushinteger(L, idx + 1);
	lua_replace(L, lua_upvalueindex(2));

	lua_rawgeti(L, -1, idx); // self[idx] を取得
	return 1; // 1個の要素を Lua の for ループへ手渡す！
}

// ❌ 5. q:iterate() のC++完全移植（Lua無名関数生成のゴミを物理的に完全消滅させる筋肉！）
int l_queue_iterate(lua_State *L)
{
	int target_idx = stacktrace::find_table_by_method(L, "size");
	if (target_idx == 0) return 0;

	// frontポインタの現在地を取得
	lua_rawgeti(L, target_idx, 2); int front = lua_tointeger(L, -1); lua_pop(L, 1);

	// 👑 【ハッカーの逆襲】：Lua側でのクロージャ生成の摩擦を完全消去！
	// キュー本体（target_idx）と、開始インデックス（front）の2つのデータを背後に隠し持った（Upvalue）
	// C++ネイティブ最速のイテレーター関数をその場で1発デプロイ！
	lua_pushvalue(L, target_idx);
	lua_pushinteger(L, front);
	lua_pushcclosure(L, l_queue_iterator_next, 2); // 2つのUpvalueを密結合して返す

	return 1;
}

} // namespace util_queue
