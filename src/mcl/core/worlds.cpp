// src/mcl/core/util_worlds.cpp

#include "worlds.h"
#include "mcl/stacktrace.h"
#include <lua.hpp>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <cmath>

namespace worlds {

	std::unordered_map<std::string, float> g_chunk_timers;
	bool g_timers_loaded = false;

	// 🛡️ 【お直し完了】：Lua 側の mcl_vars から正確なディメンション境界値（min/max）を安全に回収するC++内部関数
	double get_mcl_var(lua_State *L, const char *var_name, double fallback)
	{
		double result = fallback;
		lua_getglobal(L, "mcl_vars");
		if (lua_istable(L, -1)) {
			lua_getfield(L, -1, var_name);
			if (lua_isnumber(L, -1)) {
				result = lua_tonumber(L, -1);
			}
			lua_pop(L, 1);
		}
		lua_pop(L, 1); // mcl_vars ポップ（スタック規律を完全維持）
		return result;
	}

	std::string get_dimension_prefix(lua_State *L, double y)
	{
		double overworld_min = get_mcl_var(L, "mg_overworld_min", 0.0);
		double nether_min    = get_mcl_var(L, "mg_nether_min", -20000.0);
		double nether_max    = get_mcl_var(L, "mg_nether_max", -15000.0);
		double end_min       = get_mcl_var(L, "mg_end_min", 15000.0);
		double end_max       = get_mcl_var(L, "mg_end_max", 20000.0);

		if (y >= overworld_min) return "overworld_";
		if (y >= nether_min && y <= nether_max) return "nether_";
		if (y >= end_min && y <= end_max) return "theEnd_";
		return "theVoid_";
	}

	// 💎 1. native_worlds_is_in_void （誤爆完全消滅版）
	int l_worlds_is_in_void(lua_State *L)
	{
		int top = lua_gettop(L);
		int pos_idx = 0;

		for (int i = 1; i <= top; i++) {
			if (lua_istable(L, i)) { pos_idx = i; break; }
		}

		if (pos_idx == 0) {
			lua_pushboolean(L, false);
			lua_pushboolean(L, false);
			return 2;
		}

		lua_getfield(L, pos_idx, "y"); double y = lua_tonumber(L, -1); lua_pop(L, 1);

		// 👑 【Symmetry】：Lua側の本物の仕様値をその場で動的サルベージ回収！
		double ov_max = get_mcl_var(L, "mg_overworld_max", 31000.0);
		double ov_min = get_mcl_var(L, "mg_overworld_min", 0.0);
		double nt_max = get_mcl_var(L, "mg_nether_max", -15000.0);
		double nt_min = get_mcl_var(L, "mg_nether_min", -20000.0);
		double ed_max = get_mcl_var(L, "mg_end_max", 20000.0);
		double ed_min = get_mcl_var(L, "mg_end_min", 15000.0);

		// 本家 Lua版の不等号の規律と「1マクロの狂いもなく完全に完全一致」にカチ揃え！！！
		bool in_void = !((y < ov_max && y > ov_min) || (y < nt_max + 128.0 && y > nt_min) || (y < ed_max && y > ed_min));
		bool deadly = false;
		double deadly_tolerance = 64.0;

		if (in_void) {
			if (y < ov_min && y > ed_max) {
				deadly = (y < ov_min - deadly_tolerance);
			} else if (y < ed_min && y > nt_max + 128.0) {
				deadly = (y < ed_min - deadly_tolerance) && (y > nt_max + 128.0 + deadly_tolerance);
			} else if (y < nt_min) {
				deadly = (y < nt_min - deadly_tolerance);
			}
		}

		lua_pushboolean(L, in_void);
		lua_pushboolean(L, deadly);
		return 2;
	}

	// 💎 2. native_worlds_y_to_layer （動的境界値回収・完全対応版）
	int l_worlds_y_to_layer(lua_State *L)
	{
		if (lua_gettop(L) == 0 || !lua_isnumber(L, 1)) {
			lua_pushnil(L);
			lua_pushstring(L, "void");
			return 2;
		}
		double y = lua_tonumber(L, 1);

		// Lua 側の mcl_vars から本物の設計値をその場でサルベージ回収！
		double ov_min     = get_mcl_var(L, "mg_overworld_min", 0.0);
		double ov_min_old = get_mcl_var(L, "mg_overworld_min_old", 0.0);
		double nt_min     = get_mcl_var(L, "mg_nether_min", -20000.0);
		double nt_max     = get_mcl_var(L, "mg_nether_max", -15000.0);
		double ed_min     = get_mcl_var(L, "mg_end_min", 15000.0);
		double ed_max     = get_mcl_var(L, "mg_end_max", 20000.0);

		// 本家 Lua版の if 分岐条件と不等号の向きを 100% 完全に完全一致調和！
		if (y >= ov_min) {
			lua_pushnumber(L, y - ov_min_old);
			lua_pushstring(L, "overworld");
			return 2;
		} else if (y >= nt_min && y <= nt_max + 128.0) {
			lua_pushnumber(L, y - nt_min);
			lua_pushstring(L, "nether");
			return 2;
		} else if (y >= ed_min && y <= ed_max) {
			lua_pushnumber(L, y - ed_min);
			lua_pushstring(L, "end");
			return 2;
		}

		lua_pushnil(L);
		lua_pushstring(L, "void");
		return 2;
	}

	// 💎 3. native_worlds_pos_to_dimension （動的境界値回収・完全対応版）
	int l_worlds_pos_to_dimension(lua_State *L)
	{
		int top = lua_gettop(L);
		int pos_idx = 0;
		for (int i = 1; i <= top; i++) {
			if (lua_istable(L, i)) { pos_idx = i; break; }
		}

		if (pos_idx == 0) {
			lua_pushstring(L, "void");
			return 1;
		}

		lua_getfield(L, pos_idx, "y"); double y = lua_tonumber(L, -1); lua_pop(L, 1);

		// 本物の設計値を動的サルベージ
		double ov_min = get_mcl_var(L, "mg_overworld_min", 0.0);
		double nt_min = get_mcl_var(L, "mg_nether_min", -20000.0);
		double nt_max = get_mcl_var(L, "mg_nether_max", -15000.0);
		double ed_min = get_mcl_var(L, "mg_end_min", 15000.0);
		double ed_max = get_mcl_var(L, "mg_end_max", 20000.0);

		if (y >= ov_min) lua_pushstring(L, "overworld");
		else if (y >= nt_min && y <= nt_max + 128.0) lua_pushstring(L, "nether");
		else if (y >= ed_min && y <= ed_max) lua_pushstring(L, "end");
		else lua_pushstring(L, "void");

		return 1;
	}

	// 💎 4. native_worlds_layer_to_y （動的境界値回収・完全対応版）
	int l_worlds_layer_to_y(lua_State *L)
	{
		double layer = luaL_checknumber(L, 1);
		const char* dim = luaL_optstring(L, 2, "overworld");
		std::string mc_dimension(dim);

		// 本物の設計値を動的サルベージ
		double ov_min_old = get_mcl_var(L, "mg_overworld_min_old", 0.0);
		double nt_min     = get_mcl_var(L, "mg_nether_min", -20000.0);
		double ed_min     = get_mcl_var(L, "mg_end_min", 15000.0);

		if (mc_dimension == "overworld") {
			lua_pushnumber(L, layer + ov_min_old);
		} else if (mc_dimension == "nether") {
			lua_pushnumber(L, layer + nt_min);
		} else if (mc_dimension == "end") {
			lua_pushnumber(L, layer + ed_min);
		} else {
			lua_pushnumber(L, layer);
		}
		return 1;
	}

	// 💎 5. native_worlds_tick_chunk_inhabited_time
	int l_worlds_tick_chunk_inhabited_time(lua_State *L)
	{
		int top = lua_gettop(L);
		int pos_idx = 0; double dtime = 0.0;

		for (int i = 1; i <= top; i++) {
			if (lua_istable(L, i)) { if (pos_idx == 0) pos_idx = i; }
			else if (lua_isnumber(L, i)) { dtime = lua_tonumber(L, i); }
		}

		if (pos_idx == 0 || dtime <= 0.0) return 0;

		lua_getfield(L, pos_idx, "x"); double x = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, pos_idx, "y"); double y = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, pos_idx, "z"); double z = lua_tonumber(L, -1); lua_pop(L, 1);

		int chunk_x = static_cast<int>(std::floor(std::floor(x + 0.5) / 16.0));
		int chunk_z = static_cast<int>(std::floor(std::floor(z + 0.5) / 16.0));

		// 👑 引数に L を渡して、正確なプレフィックスを動的生成！
		std::string chunkstring = get_dimension_prefix(L, y) + std::to_string(chunk_x) + "," + std::to_string(chunk_z);
		
		g_chunk_timers[chunkstring] += static_cast<float>(dtime);

		return 0; 
	}

	// 👑 【データ保護装甲】：サーバー終了時に、C++メモリの蓄積データを一括してModStorageへ保存コミット
	void save_chunk_timers_to_storage(lua_State *L)
	{
		lua_getglobal(L, "core");
		if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }
		
		lua_getfield(L, -1, "get_mod_storage");
		if (!lua_isfunction(L, -1)) { lua_pop(L, 2); return; }

		lua_call(L, 0, 1); // core.get_mod_storage() の実体を回収
		if (!lua_istable(L, -1) && !lua_isuserdata(L, -1)) { lua_pop(L, 2); return; }

		// C++側のメモリマップを一気にループし、DISKへ一括ダンプ保存
		for (const auto& pair : g_chunk_timers) {
			lua_getfield(L, -1, "set_float");
			lua_pushvalue(L, -2); // storageオブジェクト
			lua_pushstring(L, pair.first.c_str());
			lua_pushnumber(L, pair.second);
			lua_call(L, 3, 0); // storage:set_float(key, val)
		}
		lua_pop(L, 2); // storage と core をポップ
	}
}
