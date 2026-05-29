// src/mcl/entities/burning.cpp
#include "burning.h"
#include <lua.hpp>
#include <algorithm>
#include <cmath>

namespace burning {

	// 🛡️ 内部ヘルパー：core.get_node_raw をC++から最速でノックして Content ID を一本釣り
	inline int get_node_cid_cpp(lua_State *L, int x, int y, int z)
	{
		lua_getglobal(L, "core");
		lua_getfield(L, -1, "get_node_raw");
		lua_pushinteger(L, x);
		lua_pushinteger(L, y);
		lua_pushinteger(L, z);
		lua_call(L, 3, 3); // ➔ 戻り値: cid, param1, param2
		int cid = static_cast<int>(lua_tointeger(L, -3));
		lua_pop(L, 4); // 戻り値3つとcoreをお掃除
		return cid;
	}

	// 🛡️ 内部ヘルパー：指定された Content ID が、Lua側の指定グループに属しているかを検証
	inline int get_item_group_cpp(lua_State *L, int cid, const char* group_name)
	{
		lua_getglobal(L, "core");
		lua_getfield(L, -1, "get_name_from_content_id");
		lua_pushinteger(L, cid);
		lua_call(L, 1, 1); // ➔ [-1]: node_name 文字列
		
		lua_getglobal(L, "core");
		lua_getfield(L, -1, "get_item_group");
		lua_pushvalue(L, -3); // node_name を複製して第一引数へ
		lua_pushstring(L, group_name);
		lua_call(L, 2, 1); // ➔ [-1]: group_value 数値
		
		int value = static_cast<int>(lua_tointeger(L, -1));
		lua_pop(L, 4); // node_name, core, get_item_group, value をお掃除
		return value;
	}

	// 👑 【炎上環境一斉スキャン筋肉】：l_native_check_burning_environment
	int l_native_check_burning_environment(lua_State *L)
	{
		// 1. 引数の厳格な改札（贅肉ループなしの直撃一本釣り）
		luaL_checktype(L, 1, LUA_TTABLE); // pos
		luaL_checktype(L, 2, LUA_TTABLE); // minp
		luaL_checktype(L, 3, LUA_TTABLE); // maxp

		// 個体の現在座標の回収
		lua_getfield(L, 1, "x"); double px = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, 1, "y"); double py = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, 1, "z"); double pz = lua_tonumber(L, -1); lua_pop(L, 1);

		// collisionbox の限界値を回収
		lua_getfield(L, 2, "x"); double min_x = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, 2, "y"); double min_y = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, 2, "z"); double min_z = lua_tonumber(L, -1); lua_pop(L, 1);

		lua_getfield(L, 3, "x"); double max_x = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, 3, "y"); double max_y = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, 3, "z"); double max_z = lua_tonumber(L, -1); lua_pop(L, 1);

		// 3次元空間スキャン用の整数型絶対座標の算出
		int start_x = static_cast<int>(std::floor(px + min_x));
		int start_y = static_cast<int>(std::floor(py + min_y));
		int start_z = static_cast<int>(std::floor(pz + min_z));

		int end_x = static_cast<int>(std::floor(px + max_x));
		int end_y = static_cast<int>(std::floor(py + max_y));
		int end_z = static_cast<int>(std::floor(pz + max_z));

		int max_burn_time = 0;
		bool puts_out_fire = false;

		// 🎯 【3重ループ強奪マージ】：
		//     LuaJITスタックを叩き直し、命名（get_node_cid_cpp）を完全対称に一本釣り結合！
		for (int y = start_y; y <= end_y; y++) {
			for (int z = start_z; z <= end_z; z++) {
				for (int x = start_x; x <= end_x; x++) {
					int cid = get_node_cid_cpp(L, x, y, z);
					
					// 消火ノード（group:puts_out_fire）のチェック
					if (get_item_group_cpp(L, cid, "puts_out_fire") > 0) {
						puts_out_fire = true;
						max_burn_time = 0;
						break;
					}

					// 炎上ノード（group:set_on_fire）のチェック
					int burn_value = get_item_group_cpp(L, cid, "set_on_fire");
					if (burn_value > max_burn_time) {
						max_burn_time = burn_value;
					}
				}
				if (puts_out_fire) break;
			}
			if (puts_out_fire) break;
		}

		// 2. 🛡️ 【スタック安全ガード】：2つの結果（消火フラグ、最大炎上時間）を出荷
		lua_pushboolean(L, puts_out_fire);
		lua_pushinteger(L, max_burn_time);
		return 2;
	}

} // namespace burning
