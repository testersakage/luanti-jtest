// src/mcl/core/damage.cpp
#include "damage.h"
#include <lua.hpp>
#include <string>
#include <cstring>

namespace damage {

	// 👑 【PvPパルス安定筋肉】：l_native_from_mt
	int l_native_from_mt(lua_State *L)
	{
		// 1. 引数の型検査（第1引数: mt_reason テーブル）
		luaL_checktype(L, 1, LUA_TTABLE);

		// キャッシュチェック（mt_reason._mcl_cached_reason があれば即返却）
		lua_getfield(L, 1, "_mcl_cached_reason");
		if (!lua_isnil(L, -1)) {
			return 1; // キャッシュテーブルをそのまま出荷
		}
		lua_pop(L, 1);

		// mt_reason._mcl_reason の直接引き抜きチェック
		lua_getfield(L, 1, "_mcl_reason");
		if (lua_istable(L, -1)) {
			// mcl_damage.finish_reason とキャッシュ登録の実務は
			// 安全のため直後の Lua 側改札口へリレー委託するため、ここではテーブルをそのまま通過出荷
			return 1;
		}
		lua_pop(L, 1);

		// 2. 成果物となる mcl_reason テーブルを新規創世
		lua_newtable(L); // ➔ スタック位置: [-1] (mcl_reason)
		lua_pushstring(L, "generic");
		lua_setfield(L, -2, "type"); // 初期値型: type = "generic"

		// mt_reason.type の文字列回収
		lua_getfield(L, 1, "type");
		std::string type_str = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
		lua_pop(L, 1);

		// mt_reason._mcl_type の回収
		lua_getfield(L, 1, "_mcl_type");
		std::string mcl_type_str = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
		lua_pop(L, 1);

		// 🎯 本家の属性ジャッジ条件分岐を完全再現
		if (!mcl_type_str.empty()) {
			lua_pushstring(L, mcl_type_str.c_str());
			lua_setfield(L, -2, "type");
		}
		else if (type_str == "fall") {
			lua_pushstring(L, "fall");
			lua_setfield(L, -2, "type");
		}
		else if (type_str == "drown") {
			lua_pushstring(L, "drown");
			lua_setfield(L, -2, "type");
		}
		else if (type_str == "punch") {
			// punch の場合は object や方向などのテーブル状態の遺伝子リレーが必要なため、
			// 記号フラグ "CALL_PUNCH" をマークして出荷し、Lua側の mcl_damage.from_punch へ安全リレーさせる
			lua_pushstring(L, "punch");
			lua_setfield(L, -2, "type");
			lua_pushboolean(L, true);
			lua_setfield(L, -2, "_mcl_trigger_punch");
		}
		else if (type_str == "node_damage") {
			// 流体（火・溶岩）のグループチェック
			lua_getfield(L, 1, "node");
			if (lua_istable(L, -1)) {
				// core.get_item_group をノックして判定
				lua_getglobal(L, "core");
				lua_getfield(L, -1, "get_item_group");
				lua_pushvalue(L, -3); // node
				lua_pushstring(L, "fire");
				lua_call(L, 2, 1);
				if (lua_tointeger(L, -1) > 0) {
					lua_pushstring(L, "in_fire");
					lua_setfield(L, -4, "type");
				}
				lua_pop(L, 2); // 戻り値とcoreお掃除

				lua_getglobal(L, "core");
				lua_getfield(L, -1, "get_item_group");
				lua_pushvalue(L, -3); // node
				lua_pushstring(L, "lava");
				lua_call(L, 2, 1);
				if (lua_tointeger(L, -1) > 0) {
					lua_pushstring(L, "lava");
					lua_setfield(L, -4, "type");
				}
				lua_pop(L, 2);
			}
			lua_pop(L, 1); // nodeお掃除
		}

		// 🎯 【最凶の文字列検索ループの強奪】：
		//     mt_reason 内のすべての鍵（key）を走査し、"_mcl_" 前方一致を連続メモリで高速切り出しトリミング！
		lua_pushnil(L); // ➔ テーブル走査の初期キー
		while (lua_next(L, 1) != 0) {
			// [-2] = key, [-1] = value
			if (lua_type(L, -2) == LUA_TSTRING) {
				const char* key_cstr = lua_tostring(L, -2);
				if (std::strncmp(key_cstr, "_mcl_", 5) == 0) {
					// 部屋番号 6手目（_mcl_ の5文字を削ったケツ）から文字列をトリミング切り出し
					std::string new_key(key_cstr + 5);
					lua_pushstring(L, new_key.c_str());
					lua_pushvalue(L, -2); // value を複製
					lua_settable(L, -5); // mcl_reason テーブルへ格納
				}
			}
			lua_pop(L, 1); // value のみをお掃除して次のループへ（keyは生かす規律）
		}

		return 1; // 組み立て終えた mcl_reason を出荷！
	}

} // namespace damage
