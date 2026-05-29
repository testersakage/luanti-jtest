// src/mcl/entities/mobs.cpp
#include "mobs.h"
#include <lua.hpp>
#include <cmath>

namespace mobs {

	// 🛡️ 内部ヘルパー：core.get_node_raw を最速ノックして Content ID と param2 を一本釣り
	inline void get_node_raw_mobs(lua_State *L, int x, int y, int z, int& cid, int& param2)
	{
		lua_getglobal(L, "core");
		lua_getfield(L, -1, "get_node_raw");
		lua_pushinteger(L, x);
		lua_pushinteger(L, y);
		lua_pushinteger(L, z);
		lua_call(L, 3, 3); // ➔ 戻り値: cid, param1, param2
		cid = static_cast<int>(lua_tointeger(L, -3));
		param2 = static_cast<int>(lua_tointeger(L, -1));
		lua_pop(L, 4); // 戻り値3つとcoreをお掃除
	}

	// 👑 1. 【native_update_mob_timers】：タイマー配列のC++側一括バッチ減算
	int l_native_update_mob_timers(lua_State *L)
	{
		luaL_checktype(L, 1, LUA_TTABLE); // 引数1: self (_timersを内包するオブジェクト)
		double dtime = luaL_checknumber(L, 2);  // 引数2: dtime 数値

		lua_getfield(L, 1, "_timers");
		if (!lua_istable(L, -1)) {
			lua_pop(L, 1);
			return 0;
		}

		lua_getfield(L, 1, "_timers_fired");
		bool has_fired_table = lua_istable(L, -1);

		// _timers テーブル（[-2]の位置）を走査
		lua_pushnil(L);
		while (lua_next(L, -3) != 0) {
			// [-2] = key, [-1] = value (残り時間数値)
			if (lua_isnumber(L, -1)) {
				double new_val = lua_tonumber(L, -1) - dtime;
				
				// 減算結果を _timers へ上書き書き戻し
				lua_pushvalue(L, -2); // key複製
				lua_pushnumber(L, new_val);
				lua_settable(L, -5); // _timers テーブルへスタンプ

				// 同時に対象の _timers_fired[key] を nil 引き算消去
				if (has_fired_table) {
					lua_pushvalue(L, -2); // key複製
					lua_pushnil(L);
					lua_settable(L, -5); // _timers_fired テーブルへスタンプ
				}
			}
			lua_pop(L, 1); // valueを掃除して次のループへ
		}

		lua_pop(L, 2); // テーブル2つをお掃除
		return 0;
	}

	// 👑 2. 【native_mob_environment_scan】：足元2面・頭部1面の一斉高速回収改札
	int l_native_mob_environment_scan(lua_State *L)
	{
		luaL_checktype(L, 1, LUA_TTABLE); // 引数1: feet 座標テーブル
		luaL_checktype(L, 2, LUA_TTABLE); // 引数2: pos_head 座標テーブル

		// feet 座標回収
		lua_getfield(L, 1, "x"); int fx = static_cast<int>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);
		lua_getfield(L, 1, "y"); int fy = static_cast<int>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);
		lua_getfield(L, 1, "z"); int fz = static_cast<int>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);

		// pos_head 座標回収
		lua_getfield(L, 2, "x"); int hx = static_cast<int>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);
		lua_getfield(L, 2, "y"); int hy = static_cast<int>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);
		lua_getfield(L, 2, "z"); int hz = static_cast<int>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);

		int cid_in, p2_in, cid_on, p2_on, cid_head, p2_head;

		// ① standing_in (feet) の回収
		get_node_raw_mobs(L, fx, fy, fz, cid_in, p2_in);

		// ② standing_on (feet.y - 1) の回収
		get_node_raw_mobs(L, fx, fy - 1, fz, cid_on, p2_on);

		// ③ head_in (pos_head) の回収
		get_node_raw_mobs(L, hx, hy, hz, cid_head, p2_head);

		// 🛡️ 【スタック安全ガード】：6つの Content ID と param2 を一挙に出荷！
		lua_pushinteger(L, cid_in);   lua_pushinteger(L, p2_in);
		lua_pushinteger(L, cid_on);   lua_pushinteger(L, p2_on);
		lua_pushinteger(L, cid_head); lua_pushinteger(L, p2_head);

		return 6;
	}

} // namespace mobs
