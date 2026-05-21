// src/mcl/core/util_ringbuffer.cpp
#include "util_ringbuffer.h"
#include "mcl/stacktrace.h" // 👑 あなたの創設した無敵の逆探知インフラを結合！
#include <string>

namespace util_ringbuffer {

// ❌ 1. rb:indexof(val) のC++完全移植
int l_rb_indexof(lua_State *L)
{
	int target_idx = stacktrace::find_table_by_method(L, "serialize");
	if (target_idx == 0) { lua_pushboolean(L, false); return 1; }

	int top = lua_gettop(L);
	luaL_checkany(L, top); // 検索対象の val

	lua_getfield(L, target_idx, "data"); // [-1] data table
	int data_idx = lua_gettop(L);

	int len = lua_objlen(L, data_idx);
	for (int i = 1; i <= len; i++) {
		lua_rawgeti(L, data_idx, i);
		if (lua_equal(L, top, -1)) {
			lua_pop(L, 2); // data table と要素をお片付け
			lua_pushinteger(L, i); // 見つかったインデックスを返却！
			return 1;
		}
		lua_pop(L, 1);
	}

	lua_pop(L, 1); // data table
	lua_pushboolean(L, false); // 見つからなければ false を返却
	return 1;
}

// ❌ 2. rb:serialize() のC++完全移植（悪魔の2連テーブル詰め替えをFPUスピードで一瞬で完食！）
int l_rb_serialize(lua_State *L)
{
	int target_idx = stacktrace::find_table_by_method(L, "serialize");
	if (target_idx == 0) return 0;

	// self.size と self.position を回収
	lua_getfield(L, target_idx, "size");     int size = lua_tointeger(L, -1);     lua_pop(L, 1);
	lua_getfield(L, target_idx, "position"); int position = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, target_idx, "data");     int data_idx = lua_gettop(L);

	int wrap = size - position;

	// 新しい平坦化テーブル（data）をC++の腕力でLuaスタック上に高速錬金！
	lua_newtable(L);
	int flat_table_idx = lua_gettop(L);

//	int offset = 0; // 未使用変数
	int count = 1;

	// 👑 A面： oldest entry (position + 1) から末尾（wrap分）までを一瞬でC++スタンプ！
	for (int i = 1; i <= wrap; i++) {
		lua_rawgeti(L, data_idx, position + i);
		if (!lua_isnil(L, -1)) {
			lua_rawseti(L, flat_table_idx, count++);
		} else {
			lua_pop(L, 1);
//			offset++;
		}
	}

	// 👑 B面： ド頭から現在の position までを一瞬でC++スタンプ！
	for (int i = 1; i <= position; i++) {
		lua_rawgeti(L, data_idx, i);
		if (!lua_isnil(L, -1)) {
			lua_rawseti(L, flat_table_idx, count++);
		} else {
			lua_pop(L, 1);
		}
	}

	// 👑 【究極のコア直結パススルー】 core.serialize(data) をこの場でダイレクトにキック！
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "serialize");
	lua_pushvalue(L, flat_table_idx);
	lua_call(L, 1, 1); // 焼き上がったシリアライズテキスト（文字列）を取得！

	lua_replace(L, 1);
	lua_settop(L, 1);
	return 1;
}

// ❌ 3. rb:insert(record) のC++完全移植
int l_rb_insert(lua_State *L)
{
	int target_idx = stacktrace::find_table_by_method(L, "serialize");
	if (target_idx == 0) return 0;

	int top = lua_gettop(L);
	luaL_checkany(L, top); // record

	lua_getfield(L, target_idx, "size");     int size = lua_tointeger(L, -1);     lua_pop(L, 1);
	lua_getfield(L, target_idx, "position"); int position = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, target_idx, "data");     int data_idx = lua_gettop(L);

	// self.position = self.position + 1
	position = position + 1;
	
	// self.data[self.position] = record
	lua_pushvalue(L, top);
	lua_rawseti(L, data_idx, position);

	// if self.position >= self.size then self.position = 0 end
	if (position >= size) {
		position = 0;
	}

	// 更新した position を戻す
	lua_pushinteger(L, position);
	lua_setfield(L, target_idx, "position");

	// auto_update_node_meta_key の自動保存処理をエミュレート
	lua_getfield(L, target_idx, "auto_update_node_meta_key");
	if (lua_isstring(L, -1)) {
		std::string meta_key = lua_tostring(L, -1);
		lua_pop(L, 1);

		if (meta_key.length() >= 16) {
			std::string hash_str = meta_key.substr(0, 16);
			std::string real_key = meta_key.substr(16);

			lua_getglobal(L, "core");
			
			// pos = core.get_position_from_hash(hash)
			lua_getfield(L, -1, "get_position_from_hash");
			lua_pushstring(L, hash_str.c_str());
			lua_call(L, 1, 1);

			// meta = core.get_meta(pos)
			lua_getfield(L, -3, "get_meta");
			lua_pushvalue(L, -2);
			lua_call(L, 1, 1);

			// meta:set_string(real_key, self:serialize())
			lua_getfield(L, -1, "set_string");
			lua_pushvalue(L, -2); // meta
			lua_pushstring(L, real_key.c_str());
			lua_pushvalue(L, target_idx);
			l_rb_serialize(L); // 内部キックで超速シリアライズ！
			lua_call(L, 3, 0);

			// node_meta_private の処理
			lua_getfield(L, target_idx, "node_meta_private");
			if (lua_toboolean(L, -1)) {
				lua_getfield(L, -3, "mark_as_private"); // meta:mark_as_private(real_key)
				lua_pushvalue(L, -4);
				lua_pushstring(L, real_key.c_str());
				lua_call(L, 2, 0);
			}
			lua_pop(L, 1);
		}
	} else {
		lua_pop(L, 1);
	}

	lua_settop(L, 0);
	return 0;
}

// ❌ 4. rb:insert_if_not_exists(record) のC++完全移植
int l_rb_insert_if_not_exists(lua_State *L)
{
	lua_pushvalue(L, lua_gettop(L));
	l_rb_indexof(L);
	bool exists = lua_toboolean(L, -1) != 0;
	lua_pop(L, 1);

	if (!exists) {
		lua_pushvalue(L, lua_gettop(L));
		l_rb_insert(L);
		lua_pushboolean(L, true);
		return 1;
	}

	lua_pushboolean(L, false);
	return 1;
}

} // namespace util_ringbuffer
