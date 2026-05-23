// src/mcl/core/util_worlds.h
#pragma once
#include <lua.hpp>
#include <unordered_map>
#include <string>

namespace worlds { // 👑 正真正銘、worlds で完全大統一！ [INDEX: 5]

	// 実務中は完全無音のメモリハッシュマップ！DISK I/Oテロを物理的に完全消滅させる
	extern std::unordered_map<std::string, float> g_chunk_timers;
	extern bool g_timers_loaded;

	// 👑 【5大最速空間数理筋肉】：見る以外のスタック操作を一切しない絶対規律API窓口群
	int l_worlds_is_in_void(lua_State *L);
	int l_worlds_y_to_layer(lua_State *L);
	int l_worlds_pos_to_dimension(lua_State *L);
	int l_worlds_layer_to_y(lua_State *L);
	int l_worlds_tick_chunk_inhabited_time(lua_State *L);

	// 👑 サーバー終了時（または定期バックアップ時）に、C++メモリから ModStorage へ一括書き出しセーブする防衛インフラ
	void save_chunk_timers_to_storage(lua_State *L);
	void load_chunk_timers_from_storage(lua_State *L);
}
