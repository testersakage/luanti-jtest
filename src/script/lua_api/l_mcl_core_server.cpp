// src/script/lua_api/l_mcl_core_server.cpp
#include "l_mcl_core_server.h"
#include "mcl/core/tga_encoder.h"
#include "log.h" // 👈 【厳置】errorstream ログ直結マクロ

namespace l_mcl_core_server {

#define L_MCL_MAP_METATABLE "mcl_core_map_object"

// 🕵️‍♂️ 【公式残存】スタックの状況を 100% 確実にコンソールへスタンプする無敵のレーダー
void dump_lua_stack(lua_State *L, const std::string &marker)
{
	int top = lua_gettop(L);
	errorstream << "▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬" << std::endl;
	errorstream << "  [LUA_STACK_DUMP s] Marker: [" << marker << "] Stack Depth: " << top << std::endl;
	
	for (int i = 1; i <= top; i++) {
		int type = lua_type(L, i);
		errorstream << " -> Argument #" << i << " (Relative: " << (i - top - 1) << "): [" << lua_typename(L, type) << "]";
		if (type == LUA_TSTRING) {
			errorstream << " value = \"" << lua_tostring(L, i) << "\"";
		} else if (type == LUA_TNUMBER) {
			errorstream << " value = " << lua_tonumber(L, i);
		} else if (type == LUA_TBOOLEAN) {
			errorstream << " value = " << (lua_toboolean(L, i) ? "true" : "false");
		}
		errorstream << std::endl;
	}
	errorstream << "▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬" << std::endl;
}

void Initialize(lua_State *L, int top)
{
	lua_newtable(L); // tga_encoder = {}

	// ❌ 1. 【第一段階：tga_encoder.image(pixels) 関数】
	lua_pushcfunction(L, [](lua_State *L) -> int {
		// 🔍 【解析ログ：第1段階ド頭】
		dump_lua_stack(L, "tga_encoder.image() CALL START");

		luaL_checktype(L, 1, LUA_TTABLE); // pixelsテーブル存在検品

		lua_newtable(L); // [オブジェクト本体インスタンス]
		lua_pushvalue(L, 1);
		lua_setfield(L, -2, "pixels");

		// デフォルトサイズ（128x128）を自動補完
		lua_pushinteger(L, 128); lua_setfield(L, -2, "width");
		lua_pushinteger(L, 128); lua_setfield(L, -2, "height");

		// メタテーブル結合
		luaL_getmetatable(L, L_MCL_MAP_METATABLE);
		lua_setmetatable(L, -2);

		return 1; 
	});
	lua_setfield(L, -2, "image"); 

	lua_setglobal(L, "tga_encoder"); 

	// 👑 2. 【第二段階：メタテーブル側の :save(filename, props) 回路】
	luaL_newmetatable(L, L_MCL_MAP_METATABLE); 
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index"); 

	lua_pushcfunction(L, [](lua_State *L) -> int {
		// 🔍 【解析ログ：第2段階 :save() 呼び出しド頭の生スタック状況】
		dump_lua_stack(L, "image:save() CALL START");

		luaL_checktype(L, 1, LUA_TTABLE); // self
		std::string filename = luaL_checkstring(L, 2); // filename
		luaL_checktype(L, 3, LUA_TTABLE); // properties

		lua_getfield(L, 1, "width");  int width  = lua_tointeger(L, -1); lua_pop(L, 1);
		lua_getfield(L, 1, "height"); int height = lua_tointeger(L, -1); lua_pop(L, 1);
		lua_getfield(L, 1, "pixels"); // [-1] に2次元 [z][x] テーブルが乗る

		// 👑 【2次元マトリクス ➔ 1次元フラット平坦化ストリーム】
		lua_newtable(L); 
		int flat_table_idx = lua_gettop(L);
		
		u32 count = 1;
		for (int z = 1; z <= height; z++) {
			lua_rawgeti(L, -2, z); 
			if (lua_istable(L, -1)) {
				for (int x = 1; x <= width; x++) {
					lua_rawgeti(L, -1, x); 
					if (lua_istable(L, -1)) {
						lua_rawgeti(L, -1, 1); u8 r = lua_tointeger(L, -1); lua_pop(L, 1);
						lua_rawgeti(L, -1, 2); u8 g = lua_tointeger(L, -1); lua_pop(L, 1);
						lua_rawgeti(L, -1, 3); u8 b = lua_tointeger(L, -1); lua_pop(L, 1);
						u8 a = 255; 
						
						u32 argb = (r << 24) | (g << 16) | (b << 8) | a;
						lua_pushinteger(L, argb);
						lua_rawseti(L, flat_table_idx, count++);
					}
					lua_pop(L, 1);
				}
			}
			lua_pop(L, 1);
		}

		// 大元の tga_encoder.cpp（l_tga_encode）用にスタック引数を完全カチ揃え積み直し！
		lua_pushvalue(L, 2);              // 1: filename
		lua_pushinteger(L, width);        // 2: width
		lua_pushinteger(L, height);       // 3: height
		lua_pushvalue(L, flat_table_idx); // 4: 完璧にフラット解凍した1次元ピクセルテーブル
		lua_pushvalue(L, 3);              // 5: properties

		// 贅肉のお片付け
		lua_replace(L, 5); lua_replace(L, 4); lua_replace(L, 3); lua_replace(L, 2); lua_replace(L, 1);
		lua_settop(L, 5);

		// 🔍 【解析ログ：C++本体（tga_encoder.cpp）の胃袋へ直撃突っ込む直前の、カチ揃った絶対正義の5大引数】
		dump_lua_stack(L, "BEFORE KICK C++ NATIVE ENCODER");

		// 本体を直撃キック！
		return tga_encoder::l_tga_encode(L);
	});
	lua_setfield(L, -2, "save"); 
	lua_pop(L, 1);
}

} // namespace l_mcl_core_server
