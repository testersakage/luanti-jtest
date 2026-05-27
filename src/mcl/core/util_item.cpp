// src/mcl/core/util_item.cpp
#include "util_item.h"
#include "mcl/stacktrace.h"
//#include "log.h" // debug.txt
#include <string>
#include <cmath>
#include <algorithm>

namespace util_item {

/*
	// ❌ 1. mcl_util.get_burntime(item) のC++完全移植（共通チェッカー結線）
int l_get_burntime(lua_State *L)
{
	// 👑 1番目がテーブル（ItemStack）なら、共通チェッカーでスタック内の正確な部屋番号を逆探知！
	int target_idx = 1;
	if (lua_istable(L, 1)) {
		target_idx = stacktrace::find_table_by_method(L, "get_name");
		if (target_idx == 0) target_idx = 1;
	}

	// 🕵️‍♂️ 正しいターゲットインデックスから引数（item文字列またはオブジェクト）を検品
	luaL_checkany(L, target_idx);

	// fuel_cache テーブルの代わりに、C++側から直接 core.get_craft_result を高速指名キック！
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "get_craft_result");
	
	lua_newtable(L); // input table
	lua_pushstring(L, "fuel"); lua_setfield(L, -2, "method");
	lua_pushinteger(L, 1);      lua_setfield(L, -2, "width");
	
	lua_newtable(L); // items table
	lua_pushvalue(L, target_idx); // 特定した正しい部屋のデータをコピー！
	lua_rawseti(L, -2, 1); // items[1] = item
	lua_setfield(L, -2, "items");

	lua_call(L, 1, 1); // core.get_craft_result を執行！ -> 戻り値 result テーブル
	
	lua_getfield(L, -1, "time"); // result.time
	double burntime = lua_tonumber(L, -1);
	
	lua_settop(L, 1); // スタックをお掃除
	lua_pushnumber(L, burntime);
	return 1;
}
*/

/*
// ❌ 2. mcl_util.is_fuel(item) のC++完全移植（共通チェッカー結線）
int l_is_fuel(lua_State *L)
{
	l_get_burntime(L); // 手前の関数をそのまま再利用キック（スタックごと引き継ぎ）
	double burntime = lua_tonumber(L, -1);
	lua_pushboolean(L, burntime != 0.0);
	return 1;
}
*/

	// ❌ 3. mcl_util.calculate_durability(itemstack) のC++完全移植（mcl_ttの got userdata を完全粉砕！）
	int l_calculate_durability(lua_State *L)
	{
		// 👑 【無敵の逆探知】 mcl_ttがどんなお行儀の悪い引数で呼ぼうが、ItemStackテーブルの部屋番号を一撃ロックオン！
		int target_idx = stacktrace::find_table_by_method(L, "get_name");
		if (target_idx == 0) {
			lua_pushnumber(L, 0.0);
			return 1;
		}

		// 1. 特定した部屋から itemstack:get_name() をキック
		lua_getfield(L, target_idx, "get_name");
		lua_pushvalue(L, target_idx);
		lua_call(L, 1, 1);
		std::string name = lua_tostring(L, -1);
		lua_pop(L, 1);

		// 2. mcl_enchanting.get_enchantment(itemstack, "unbreaking") をキック
		lua_getglobal(L, "mcl_enchanting");
		lua_getfield(L, -1, "get_enchantment");
		lua_pushvalue(L, target_idx);
		lua_pushstring(L, "unbreaking");
		lua_call(L, 2, 1);
		int unbreaking_level = lua_tointeger(L, -1);
		lua_pop(L, 2); // 戻り値とmcl_enchantingをポップ

		// 3. 各種アイテムグループを core.get_item_group から高速ネイティブ取得
		lua_getglobal(L, "core");
		lua_getfield(L, -1, "get_item_group");
	
		// armor_uses
		lua_pushvalue(L, -1); lua_pushstring(L, name.c_str()); lua_pushstring(L, "mcl_armor_uses"); lua_call(L, 2, 1);
		int armor_uses = lua_tointeger(L, -1); lua_pop(L, 1);
	
		// elytra
		lua_pushvalue(L, -1); lua_pushstring(L, name.c_str()); lua_pushstring(L, "elytra"); lua_call(L, 2, 1);
		int elytra = lua_tointeger(L, -1); lua_pop(L, 2); // get_item_group関数ポインタもお片付け

		double uses = 0.0;

		if (armor_uses > 0) {
			uses = armor_uses;
			if (unbreaking_level > 0) {
				if (elytra <= 0) {
					uses = uses / (0.6 + 0.4 / (unbreaking_level + 1.0));
				} else {
					uses = uses * (unbreaking_level + 1.0);
				}
			}
		} else {
			// def = itemstack:get_definition() 取得
			lua_getfield(L, target_idx, "get_definition");
			lua_pushvalue(L, target_idx);
			lua_call(L, 1, 1);
			if (lua_istable(L, -1)) {
				lua_getfield(L, -1, "_mcl_uses");
				if (lua_isnumber(L, -1)) {
					uses = lua_tonumber(L, -1);
					if (unbreaking_level > 0) {
						uses = uses * (unbreaking_level + 1.0);
					}
				}
				lua_pop(L, 1);
			}
			lua_pop(L, 1);

			if (uses == 0.0) {
				// itemstack:get_tool_capabilities() のフォールバック
				lua_getfield(L, target_idx, "get_tool_capabilities");
				lua_pushvalue(L, target_idx);
				lua_call(L, 1, 1);
				if (lua_istable(L, -1)) {
					lua_getfield(L, -1, "groupcaps");
					if (lua_istable(L, -1)) {
						lua_pushnil(L);
						if (lua_next(L, -2) != 0) { // next(groupcaps) をシミュレート
							if (lua_istable(L, -1)) {
								lua_getfield(L, -1, "uses");
								if (lua_isnumber(L, -1)) uses = lua_tonumber(L, -1);
								lua_pop(L, 1);
							}
							lua_pop(L, 2); // key と value をポップ
						}
					}
					lua_pop(L, 1);
				}
				lua_pop(L, 1);
			}
		}

		lua_settop(L, 1);
		lua_pushnumber(L, uses);
		return 1;
	}

	// ❌ 4. mcl_util.use_item_durability(itemstack, n) のC++完全移植（共通チェッカー結線）
	int l_use_item_durability(lua_State *L)
	{
		// 👑 共通マシーンで一撃逆探知！
		int target_idx = stacktrace::find_table_by_method(L, "get_name");
		if (target_idx == 0) return 0;

		// コロン表記のズレに備え、スタックの最後の部屋（top）から確実に整数 n を回収！
		int n = luaL_checkinteger(L, lua_gettop(L));

		if (n > 0) {
			lua_pushvalue(L, target_idx);
			l_calculate_durability(L); // 上の関数を直撃キック
			double uses = lua_tonumber(L, -1);
			lua_pop(L, 1);

			// itemstack:add_wear_by_uses(math.floor(uses / n)) をキック！
			lua_getfield(L, target_idx, "add_wear_by_uses");
			lua_pushvalue(L, target_idx);
			lua_pushinteger(L, std::floor(uses / n));
			lua_call(L, 2, 0);
		}
		return 0;
	}

/*
// ❌ 5. mcl_util.is_item_or_in_group(itemname, group_or_item) （朝一番に大成功した筋肉！）
int l_is_item_or_in_group(lua_State *L)
{
	std::string itemname = luaL_checkstring(L, 1);
	std::string group_or_item = luaL_checkstring(L, 2);

	if (group_or_item.rfind("group:", 0) == 0) {
		std::string group_name = group_or_item.substr(6);
		lua_getglobal(L, "core");
		lua_getfield(L, -1, "get_item_group");
		lua_pushstring(L, itemname.c_str());
		lua_pushstring(L, group_name.c_str());
		lua_call(L, 2, 1);
		int g = lua_tointeger(L, -1);
		if (g != 0) return 1;
		lua_pushboolean(L, false);
		return 1;
	}
	lua_pushboolean(L, itemname == group_or_item);
	return 1;
}
*/

} // namespace util_item
