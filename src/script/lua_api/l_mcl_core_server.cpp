// src/script/lua_api/l_mcl_core_server.cpp
#include "l_mcl_core_server.h"
#include "mcl/core/damage.h"
#include "mcl/core/explosions.h"
#include "mcl/core/util_compat.h"
#include "mcl/core/util_environment.h"
#include "mcl/core/util_item.h"
#include "mcl/core/util_misc.h"
#include "mcl/core/util_object.h"
#include "mcl/core/util_queue.h"
#include "mcl/core/util_ringbuffer.h"
#include "mcl/core/util_shape.h"
#include "mcl/core/util_table.h"
#include "mcl/core/worlds.h"
#include "mcl/core/tga_encoder.h"
#include "log.h" //  【厳置】errorstream ログ直結マクロ

namespace l_mcl_core_server {

//   【メインスレッド ＆ 非同期 Async スレッド、双方で全く同じ筋肉を1発創世する無敵のヘルパー】
void bind_mcl_util_mainthread(lua_State *L) {
	lua_getglobal(L, "mcl_util");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		lua_newtable(L); 
		lua_pushvalue(L, -1);
		lua_setglobal(L, "mcl_util"); // _G.mcl_util = {} を最初から一発創世！
	}
	int util_idx = lua_gettop(L); // 基準となるテーブルの部屋番号を固定
	
	// util/compat.lua ? function / ? porting lua to c++
	lua_pushcfunction(L, util_compat::l_vector_random_direction); lua_setfield(L, util_idx, "random_direction");
	lua_pushcfunction(L, util_compat::l_connected_players);       lua_setfield(L, util_idx, "connected_players");
	lua_pushcfunction(L, util_compat::l_get_node_raw);           lua_setfield(L, util_idx, "get_node_raw");
	lua_pushcfunction(L, util_compat::l_time_to_day_night_ratio); lua_setfield(L, util_idx, "time_to_ratio");

	// util/enviroment.lua ? function / ? porting lua to c++
	lua_pushcfunction(L, util_environment::l_get_double_container_neighbor_pos); lua_setfield(L, util_idx, "get_double_container_neighbor_pos");
	lua_pushcfunction(L, util_environment::l_get_eligible_transfer_item_slot);    lua_setfield(L, util_idx, "get_eligible_transfer_item_slot");
	lua_pushcfunction(L, util_environment::l_drop_items_from_meta_container);    lua_setfield(L, util_idx, "drop_items_from_meta_container");
	lua_pushcfunction(L, util_environment::l_get_pointed_thing);               lua_setfield(L, util_idx, "get_pointed_thing");
	lua_pushcfunction(L, util_environment::l_traverse_tower);                  lua_setfield(L, util_idx, "traverse_tower");
	lua_pushcfunction(L, util_environment::l_traverse_tower_group);            lua_setfield(L, util_idx, "traverse_tower_group");
	lua_pushcfunction(L, util_environment::l_replace_node_vm);                 lua_setfield(L, util_idx, "replace_node_vm");
	lua_pushcfunction(L, util_environment::l_bulk_set_node_vm);                lua_setfield(L, util_idx, "bulk_set_node_vm");
	lua_pushcfunction(L, util_environment::l_circle_bulk_set_node_vm);          lua_setfield(L, util_idx, "circle_bulk_set_node_vm");
	lua_pushcfunction(L, util_environment::l_environment_globalstep);          lua_setfield(L, util_idx, "native_environment_globalstep");

	// util/item.lua 5 function / 5 porting lua to c++
	lua_pushcfunction(L, util_item::l_get_burntime);          lua_setfield(L, util_idx, "get_burntime");
	lua_pushcfunction(L, util_item::l_is_fuel);                lua_setfield(L, util_idx, "is_fuel");
	lua_pushcfunction(L, util_item::l_calculate_durability);  lua_setfield(L, util_idx, "calculate_durability"); 
	lua_pushcfunction(L, util_item::l_use_item_durability);    lua_setfield(L, util_idx, "use_item_durability");
	lua_pushcfunction(L, util_item::l_is_item_or_in_group);   lua_setfield(L, util_idx, "is_item_or_in_group"); 

	// util/misc.lua 17 function / 3 porting lua to c++
	lua_pushcfunction(L, util_misc::l_generate_uuid);         lua_setfield(L, util_idx, "generate_uuid");
	lua_pushcfunction(L, util_misc::l_get_nodepos);           lua_setfield(L, util_idx, "get_nodepos");
	lua_pushcfunction(L, util_misc::l_calculate_knockback);   lua_setfield(L, util_idx, "calculate_knockback");

	// util/object.lua 15 function / 6 porting lua to c++
	lua_pushcfunction(L, util_object::l_props_changed);         lua_setfield(L, util_idx, "props_changed");
	lua_pushcfunction(L, util_object::l_get_object_center);     lua_setfield(L, util_idx, "get_object_center");
	lua_pushcfunction(L, util_object::l_target_eye_height);     lua_setfield(L, util_idx, "target_eye_height");
	lua_pushcfunction(L, util_object::l_target_eye_pos);        lua_setfield(L, util_idx, "target_eye_pos");
	lua_pushcfunction(L, util_object::l_set_bone_position);     lua_setfield(L, util_idx, "set_bone_position");
	lua_pushcfunction(L, util_object::l_rotation_to_irrlicht);   lua_setfield(L, util_idx, "rotation_to_irrlicht");

	// util/queue.lua  5 function / 3 porting lua to c++
	lua_pushcfunction(L, util_queue::l_queue_enqueue);  lua_setfield(L, util_idx, "native_enqueue");
	lua_pushcfunction(L, util_queue::l_queue_dequeue);  lua_setfield(L, util_idx, "native_dequeue");
	lua_pushcfunction(L, util_queue::l_queue_peek);     lua_setfield(L, util_idx, "native_peek");
	lua_pushcfunction(L, util_queue::l_queue_size);     lua_setfield(L, util_idx, "native_queue_size");
	lua_pushcfunction(L, util_queue::l_queue_iterate);  lua_setfield(L, util_idx, "native_iterate");

	// util/ringbuffer.lua  7 function / 4 porting lua to c++
	lua_pushcfunction(L, util_ringbuffer::l_rb_insert);               lua_setfield(L, util_idx, "native_rb_insert");
	lua_pushcfunction(L, util_ringbuffer::l_rb_indexof);              lua_setfield(L, util_idx, "native_rb_indexof");
	lua_pushcfunction(L, util_ringbuffer::l_rb_insert_if_not_exists); lua_setfield(L, util_idx, "native_rb_insert_if_not_exists");
	lua_pushcfunction(L, util_ringbuffer::l_rb_serialize);            lua_setfield(L, util_idx, "native_rb_serialize");

	// worlds/init.lua  5 poring lua to c++
	lua_pushcfunction(L, worlds::l_worlds_is_in_void);                 lua_setfield(L, util_idx, "native_worlds_is_in_void");
	lua_pushcfunction(L, worlds::l_worlds_y_to_layer);                 lua_setfield(L, util_idx, "native_worlds_y_to_layer");
	lua_pushcfunction(L, worlds::l_worlds_pos_to_dimension);           lua_setfield(L, util_idx, "native_worlds_pos_to_dimension");
	lua_pushcfunction(L, worlds::l_worlds_layer_to_y);                 lua_setfield(L, util_idx, "native_worlds_layer_to_y");
	lua_pushcfunction(L, worlds::l_worlds_tick_chunk_inhabited_time); lua_setfield(L, util_idx, "native_worlds_tick_chunk_inhabited_time");

	// damage/init.lua  3 poring lua to c++
	lua_pushcfunction(L, damage::l_damage_calculate_modifier); lua_setfield(L, util_idx, "native_damage_calculate_modifier");
	lua_pushcfunction(L, damage::l_damage_tick_health);        lua_setfield(L, util_idx, "native_damage_tick_health");
	lua_pushcfunction(L, damage::l_damage_sync_to_engine);     lua_setfield(L, util_idx, "native_damage_sync_to_engine");
	// save interval API
	lua_pushcfunction(L, damage::l_damage_bulk_save_all); lua_setfield(L, util_idx, "native_damage_bulk_save_all");

	// explosions/init.lua  3 poring lua to c++
	lua_pushcfunction(L, explosions::l_explosions_raycast_sphere);   lua_setfield(L, util_idx, "native_explosions_raycast_sphere");
	lua_pushcfunction(L, explosions::l_explosions_calculate_damage); lua_setfield(L, util_idx, "native_explosions_calculate_damage");
	lua_pushcfunction(L, explosions::l_explosions_scorch_nodes);     lua_setfield(L, util_idx, "native_explosions_scorch_nodes");

	lua_pop(L, 1); // mcl_util テーブルをお片付け
}

void bind_mcl_util_multithread(lua_State *L) {
	lua_getglobal(L, "mcl_util");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		lua_newtable(L); 
		lua_pushvalue(L, -1);
		lua_setglobal(L, "mcl_util"); // _G.mcl_util = {} を最初から一発創世！
	}
	int util_idx = lua_gettop(L); // 基準となるテーブルの部屋番号を固定

	// util/shape.lua  4? function / 10 porting lua to c++
	lua_pushcfunction(L, util_shape::l_decompose_aabbs);       lua_setfield(L, util_idx, "native_decompose_aabbs");
	lua_pushcfunction(L, util_shape::l_region_op);             lua_setfield(L, util_idx, "native_region_op");
	lua_pushcfunction(L, util_shape::l_region_evaluate);       lua_setfield(L, util_idx, "native_region_evaluate");
	lua_pushcfunction(L, util_shape::l_any_occupied_p);        lua_setfield(L, util_idx, "native_any_occupied_p");
	lua_pushcfunction(L, util_shape::l_region_volume);         lua_setfield(L, util_idx, "native_region_volume");
	lua_pushcfunction(L, util_shape::l_region_equal_p);        lua_setfield(L, util_idx, "native_region_equal_p");
	lua_pushcfunction(L, util_shape::l_region_walk);           lua_setfield(L, util_idx, "native_region_walk");
	lua_pushcfunction(L, util_shape::l_region_simplify);       lua_setfield(L, util_idx, "native_region_simplify");
	lua_pushcfunction(L, util_shape::l_region_select_face);    lua_setfield(L, util_idx, "native_region_select_face");
	lua_pushcfunction(L, util_shape::l_region_intersect_p);    lua_setfield(L, util_idx, "native_intersect_p");

	// util/table.lua  ?? function / 3 porting lua to c++
	lua_pushcfunction(L, util_table::l_table_update);             lua_setfield(L, util_idx, "native_table_update");
	lua_pushcfunction(L, util_table::l_table_reverse);             lua_setfield(L, util_idx, "native_table_reverse");
	lua_pushcfunction(L, util_table::l_table_max_index);             lua_setfield(L, util_idx, "native_table_max_index");

	lua_pop(L, 1); // mcl_util テーブルをお片付け
}

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
//	actionstream << "[tga_encoder] Native C++ implementation successfully injected." << std::endl;
	lua_pop(L, 1);

	// Resister for Main thread only
	bind_mcl_util_mainthread(L);
	// Resister for Main thread and other
	// Resister for Emerge-0 thread src/script/script_api/s_mapgen.cpp
	// Resister for AsyncWorker thread src/script/script_api/s_async.cpp
	bind_mcl_util_multithread(L);
	actionstream << "[lua_api]: [mcl/mods/CORE]: Resister C++ API for server." << std::endl;
}

} // namespace l_mcl_core_server
