// src/mcl/core/flowlib.cpp
#include "flowlib.h"
#include <lua.hpp>
#include <cmath>
#include <cstring>

namespace flowlib {

	// 平方根の逆数テーブル（既存維持）
	static const double inv_roots[] = {
		1.0, 1.0, 0.70710678118655, 0.0, 0.5, 0.44721359549996, 0.0, 0.0, 0.35355339059327
	};

	// level_tab の数理規律をC++側で完全固定キャッシュ化（RANGE_SPREAD = 7 固定仕様）
	static const int level_tab_cpp[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 8 };

	// 🛡️ 内部ヘルパー：Lua側の get_liquid_level（274行目〜）をC++側で完全対称再現
	inline int get_liquid_level_cpp(int id, int param2, int c_source, int c_flowing)
	{
		if (id == c_source) {
			return 8;
		}
		if (id == c_flowing) {
			return (param2 & 0x08) ? 8 : (param2 & 0x07);
		}
		return -1; // 流体ではない
	}

	// 🛡️ 内部ヘルパー：core.get_node_raw をC++から安全かつ最速でノックする改札口
	inline void get_node_raw_cpp(lua_State *L, int x, int y, int z, int& cid, int& param2)
	{
		lua_getglobal(L, "core");
		lua_getfield(L, -1, "get_node_raw");
		
		// 🎯 【型検査の修正】: lua_getfield の戻り値ではなく、スタック最上部の型を lua_type で検品
		if (lua_type(L, -1) == LUA_TFUNCTION) {
			lua_pushinteger(L, x);
			lua_pushinteger(L, y);
			lua_pushinteger(L, z);
			lua_call(L, 3, 3); // ➔ 戻り値: cid, param1, param2
			cid = static_cast<int>(lua_tointeger(L, -3));
			param2 = static_cast<int>(lua_tointeger(L, -1));
			lua_pop(L, 3); // 戻り値お掃除
		} else {
			lua_pop(L, 1); // 叩けなかったnilポインタをお掃除
			cid = 0; param2 = 0;
		}
		lua_pop(L, 1); // coreお掃除
	}

	// 🛡️ 内部ヘルパー：floodable_tab（浸食可否）のグローバル状態をLuaスタックから1発検索
	inline bool is_floodable_cpp(lua_State *L, int cid)
	{
		// 窓口から環境として手渡されたテーブル（引数6）から直接ルックアップする
		lua_pushvalue(L, 6); // floodable_tab をスタック最上部へ
		lua_pushinteger(L, cid);
		lua_gettable(L, -2);
		bool res = lua_toboolean(L, -1);
		lua_pop(L, 2); // 残渣お掃除
		return res;
	}

	// 既存の l_native_quick_flow ロジック（中身は1文字も触らず完全原型維持）
	inline int quick_flow_logic_cpp(lua_State *L, int node_param2, const char* node_liquidtype, int test_x, int test_y, int test_z, int direction) {
		lua_getglobal(L, "core"); lua_getfield(L, -1, "get_node"); lua_newtable(L);
		lua_pushinteger(L, test_x); lua_setfield(L, -2, "x"); lua_pushinteger(L, test_y); lua_setfield(L, -2, "y"); lua_pushinteger(L, test_z); lua_setfield(L, -2, "z");
		lua_call(L, 1, 1); lua_getfield(L, -1, "name"); const char* test_name = lua_tostring(L, -1); lua_pop(L, 1);
		lua_getglobal(L, "core"); lua_getfield(L, -1, "registered_nodes"); lua_getfield(L, -1, test_name);
		if (lua_isnil(L, -1)) { lua_pop(L, 4); return 0; }
		lua_getfield(L, -1, "liquidtype"); const char* test_liquidtype = lua_isstring(L, -1) ? lua_tostring(L, -1) : ""; lua_pop(L, 1);
		lua_getfield(L, -3, "param2"); int test_param2 = lua_isnumber(L, -1) ? static_cast<int>(lua_tointeger(L, -1)) : 0; lua_pop(L, 1);
		int result = 0;
		if (std::strcmp(node_liquidtype, "source") == 0) { if (std::strcmp(test_liquidtype, "flowing") == 0) result = direction; }
		else if (std::strcmp(node_liquidtype, "flowing") == 0) {
			if (std::strcmp(test_liquidtype, "source") == 0) result = -direction;
			else if (std::strcmp(test_liquidtype, "flowing") == 0) {
				if (test_param2 < node_param2) result = ((node_param2 - test_param2) > 6) ? -direction : direction;
				else if (test_param2 > node_param2) result = ((test_param2 - node_param2) > 6) ? direction : -direction;
			}
		}
		lua_pop(L, 5); return result;
	}

	int l_native_quick_flow(lua_State *L) {
		luaL_checktype(L, 1, LUA_TTABLE); luaL_checktype(L, 2, LUA_TTABLE);
		lua_getfield(L, 1, "x"); int px = static_cast<int>(lua_tointeger(L, -1)); lua_pop(L, 1);
		lua_getfield(L, 1, "y"); int py = static_cast<int>(lua_tointeger(L, -1)); lua_pop(L, 1);
		lua_getfield(L, 1, "z"); int pz = static_cast<int>(lua_tointeger(L, -1)); lua_pop(L, 1);
		lua_getfield(L, 2, "name"); const char* node_name = lua_tostring(L, -1); lua_pop(L, 1);
		lua_getfield(L, 2, "param2"); int node_param2 = static_cast<int>(lua_tointeger(L, -1)); lua_pop(L, 1);
		lua_getglobal(L, "core"); lua_getfield(L, -1, "registered_nodes"); lua_getfield(L, -1, node_name);
		if (lua_isnil(L, -1)) {
			lua_pop(L, 3); lua_newtable(L); lua_pushnumber(L, 0); lua_setfield(L, -2, "x"); lua_pushnumber(L, 0); lua_setfield(L, -2, "y"); lua_pushnumber(L, 0); lua_setfield(L, -2, "z"); return 1;
		}
		lua_getfield(L, -1, "liquidtype"); const char* node_liquidtype = lua_isstring(L, -1) ? lua_tostring(L, -1) : ""; lua_pop(L, 4);
		int x_flow = quick_flow_logic_cpp(L, node_param2, node_liquidtype, px - 1, py, pz, -1) + quick_flow_logic_cpp(L, node_param2, node_liquidtype, px + 1, py, pz, 1);
		int z_flow = quick_flow_logic_cpp(L, node_param2, node_liquidtype, px, py, pz - 1, -1) + quick_flow_logic_cpp(L, node_param2, node_liquidtype, px, py, pz + 1, 1);
		int y_flow = 0; if (std::strcmp(node_liquidtype, "source") != 0) { y_flow = (node_param2 >= 8) ? 1 : 0; }
		int sum = x_flow * x_flow + z_flow * z_flow; double inv_r = (sum >= 0 && sum <= 8) ? inv_roots[sum] : 0.0;
		lua_newtable(L); lua_pushnumber(L, x_flow * inv_r); lua_setfield(L, -2, "x"); lua_pushnumber(L, -y_flow); lua_setfield(L, -2, "y"); lua_pushnumber(L, z_flow * inv_r); lua_setfield(L, -2, "z");
		return 1;
	}


	// 👑 3. 【native_does_sl_need_update】：水源（Source）用5方向拡散余地一斉高速スキャン
	int l_native_does_sl_need_update(lua_State *L)
	{
		int x = static_cast<int>(luaL_checkinteger(L, 1));
		int y = static_cast<int>(luaL_checkinteger(L, 2));
		int z = static_cast<int>(luaL_checkinteger(L, 3));
		int c_source = static_cast<int>(luaL_checkinteger(L, 4));
		int c_flowing = static_cast<int>(luaL_checkinteger(L, 5));
		// 引数6: floodable_tab テーブル

		int next_level = level_tab_cpp[8]; 
		int cid, p2, lvl; // 🎯 【修正】警告の原因となった不要な重複定義を統合お片付け

		// ① 下方向（y-1）のスキャン
		get_node_raw_cpp(L, x, y - 1, z, cid, p2);
		if (is_floodable_cpp(L, cid)) { lua_pushboolean(L, true); return 1; }
		lvl = get_liquid_level_cpp(cid, p2, c_source, c_flowing);
		if (lvl >= 0 && lvl < 8) { lua_pushboolean(L, true); return 1; }

		// ② 東西南北（4方向）への平面スキャン
		int dx[] = { -1, 1, 0, 0 };
		int dz[] = { 0, 0, -1, 1 };
		for (int i = 0; i < 4; i++) {
			get_node_raw_cpp(L, x + dx[i], y, z + dz[i], cid, p2);
			if (is_floodable_cpp(L, cid)) { lua_pushboolean(L, true); return 1; }
			lvl = get_liquid_level_cpp(cid, p2, c_source, c_flowing);
			if (lvl >= 0 && lvl < next_level) { lua_pushboolean(L, true); return 1; }
		}

		lua_pushboolean(L, false);
		return 1;
	}


	// 👑 4. 【native_does_fl_need_update】：流動水（Flowing）用5方向サポート一斉高速スキャン
	int l_native_does_fl_need_update(lua_State *L)
	{
		int x = static_cast<int>(luaL_checkinteger(L, 1));
		int y = static_cast<int>(luaL_checkinteger(L, 2));
		int z = static_cast<int>(luaL_checkinteger(L, 3));
		int c_source = static_cast<int>(luaL_checkinteger(L, 4));
		int c_flowing = static_cast<int>(luaL_checkinteger(L, 5));
		// 引数6: floodable_tab テーブル

		int cid, p2;

		// ① 下方向（y-1）のスキャン
		get_node_raw_cpp(L, x, y - 1, z, cid, p2);
		if (is_floodable_cpp(L, cid)) { lua_pushboolean(L, true); return 1; }
		int l101 = get_liquid_level_cpp(cid, p2, c_source, c_flowing);
		if (l101 >= 0 && l101 < 8) { lua_pushboolean(L, true); return 1; }

		// ② 自分自身の現在の水位を取得
		get_node_raw_cpp(L, x, y, z, cid, p2);
		int l111 = get_liquid_level_cpp(cid, p2, c_source, c_flowing);
		if (l111 < 0) { lua_pushboolean(L, false); return 1; } 

		int next_level = level_tab_cpp[l111];

		// ③ 東西南北（4方向）への平面スキャン、および無限水源化条件（count_sources）の同時検品
		int dx[] = { -1, 1, 0, 0 };
		int dz[] = { 0, 0, -1, 1 };
		int count_sources = 0;
		int neighbor_lvl[] = { -1, -1, -1, -1 };

		for (int i = 0; i < 4; i++) {
			get_node_raw_cpp(L, x + dx[i], y, z + dz[i], cid, p2);
			if (cid == c_source) count_sources++;

			if (is_floodable_cpp(L, cid)) { lua_pushboolean(L, true); return 1; }
			
			neighbor_lvl[i] = get_liquid_level_cpp(cid, p2, c_source, c_flowing);
			if (neighbor_lvl[i] >= 0 && neighbor_lvl[i] < next_level) { lua_pushboolean(L, true); return 1; }
		}

		if (count_sources >= 2) { lua_pushboolean(L, true); return 1; }

		for (int i = 0; i < 4; i++) {
			if (neighbor_lvl[i] >= 0 && neighbor_lvl[i] > l111) { lua_pushboolean(L, false); return 1; }
		}

		// ④ 上方向（y+1）からの水位サポートチェック
		get_node_raw_cpp(L, x, y + 1, z, cid, p2);
		int l121 = get_liquid_level_cpp(cid, p2, c_source, c_flowing);
		if (l121 >= 0 && l121 > 0) { lua_pushboolean(L, false); return 1; }

		lua_pushboolean(L, true);
		return 1;
	}

} // namespace flowlib
