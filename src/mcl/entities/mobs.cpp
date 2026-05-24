// src/mcl/entities/mobs.cpp

#include "mobs.h"
#include <lua.hpp>
#include <string>
#include <vector>

namespace entities {

	// 💎 1. native_register_villager （🛡️オウム返し・見るだけのスタック規律）
	//    起動時に他Modから投げ込まれる「 Profession, POI, Trades, Gifts 」を安全に検品！
	int l_mobs_register_villager_native(lua_State *L)
	{
		int top = lua_gettop(L);
		std::string profession_name = "";

		// 引数のテーブルの中から、 Profession 名（内部識別子）を安全に一本釣り覗き見
		for (int i = 1; i <= top; i++) {
			if (lua_istable(L, i)) {
				lua_getfield(L, i, "name");
				if (lua_isstring(L, -1)) {
					profession_name = lua_tostring(L, -1);
					lua_pop(L, 1);
					break;
				}
				lua_pop(L, 1);
			}
		}

		// 登録処理自体は現行の Lua 側のインフラへそのまま安全着陸（オウム返し）させるため、
		// 1番目の引数（Profession）をそのままリダイレクト出荷して Lua 側へバケツリレー！
		if (top > 0) {
			lua_pushvalue(L, 1);
			return 1;
		}
		return 0;
	}

	// 💎 2. native_check_poi_valid （🛡️オウム返し・毎フレームの get_node 割り込み用安全弁）
	int l_mobs_check_poi_valid_native(lua_State *L)
	{
		int top = lua_gettop(L);
		if (top > 0) {
			lua_pushvalue(L, 1); // 引数があればそのまま最速リダイレクト
			return 1;
		}
		lua_pushboolean(L, false);
		return 1;
	}

	// 💎 3. native_filter_trades （🛡️オウム返し・取引レシピの最速ルックアップ）
	int l_mobs_filter_trades_native(lua_State *L)
	{
		int top = lua_gettop(L);
		if (top > 0) {
			lua_pushvalue(L, 1);
			return 1;
		}
		return 0;
	}
}
