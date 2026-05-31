// src/script/lua_api/l_mcl_core_server.cpp
#include "l_mcl_core_server.h"
#include "mcl/core/damage.h"
#include "mcl/core/explosions.h"
#include "mcl/core/flowlib.h"
#include "mcl/core/util_environment.h"
#include "mcl/core/util_item.h"
#include "mcl/core/util_misc.h"
#include "mcl/core/util_object.h"
#include "mcl/core/util_shape.h"
#include "mcl/core/util_table.h"
#include "mcl/core/worlds.h"
#include "mcl/core/tga_encoder.h"
#include "mcl/entities/burning.h"
#include "mcl/entities/mobs.h"
//#include "mcl/entities/mobs.h"
//#include "mcl/entities/mobs_combat.h"
//#include "mcl/entities/mobs_pathfinding.h"
#include "log.h" //  【厳置】errorstream ログ直結マクロ

namespace l_mcl_core_server {

void bind_mainthread_CORE(lua_State *L) {
	lua_getglobal(L, "mcl_util");
	if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_newtable(L); lua_pushvalue(L, -1); lua_setglobal(L, "mcl_util"); }
	int util_idx = lua_gettop(L);

	// サーバー側のみ
	lua_pushcfunction(L, util_item::l_calculate_durability);  lua_setfield(L, util_idx, "calculate_durability"); 

	lua_pop(L, 1); // mcl_utilポップ

	lua_getglobal(L, "mclcapi");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		lua_newtable(L); 
		lua_pushvalue(L, -1);
		lua_setglobal(L, "mclcapi"); // _G.mclcapi = {} を最初から一発創世！
	}
	int core_idx = lua_gettop(L); // 基準となるテーブルの部屋番号を固定

	// CORE/tga_encoder/init.lua  1 porting lua to c++
	lua_pushcfunction(L, tga_encoder::l_tga_encode); lua_setfield(L, core_idx, "native_tga_encode");

	// util/enviroment.lua 14 function / 9 porting lua to c++
	lua_pushcfunction(L, util_environment::l_get_double_container_neighbor_pos); lua_setfield(L, core_idx, "native_get_double_container_neighbor_pos");
	lua_pushcfunction(L, util_environment::l_get_eligible_transfer_item_slot);    lua_setfield(L, core_idx, "native_get_eligible_transfer_item_slot");
	lua_pushcfunction(L, util_environment::l_drop_items_from_meta_container);    lua_setfield(L, core_idx, "native_drop_items_from_meta_container");
	lua_pushcfunction(L, util_environment::l_get_pointed_thing);               lua_setfield(L, core_idx, "native_get_pointed_thing");
	lua_pushcfunction(L, util_environment::l_traverse_tower);                  lua_setfield(L, core_idx, "native_traverse_tower");
	lua_pushcfunction(L, util_environment::l_traverse_tower_group);            lua_setfield(L, core_idx, "native_traverse_tower_group");
	lua_pushcfunction(L, util_environment::l_replace_node_vm);                 lua_setfield(L, core_idx, "native_replace_node_vm");
	lua_pushcfunction(L, util_environment::l_bulk_set_node_vm);                lua_setfield(L, core_idx, "native_bulk_set_node_vm");
	lua_pushcfunction(L, util_environment::l_circle_bulk_set_node_vm);          lua_setfield(L, core_idx, "native_circle_bulk_set_node_vm");
	// 変数参照
//	lua_pushcfunction(L, util_environment::l_environment_globalstep);          lua_setfield(L, core_idx, "var_environment_globalstep");

	// util/item.lua 5 function / 1 porting lua to c++
//	lua_pushcfunction(L, util_item::l_get_burntime);          lua_setfield(L, core_idx, "get_burntime");
//	lua_pushcfunction(L, util_item::l_is_fuel);                lua_setfield(L, core_idx, "is_fuel");
	lua_pushcfunction(L, util_item::l_calculate_durability);  lua_setfield(L, core_idx, "calculate_durability"); 
	lua_pushcfunction(L, util_item::l_use_item_durability);    lua_setfield(L, core_idx, "use_item_durability");
//	lua_pushcfunction(L, util_item::l_is_item_or_in_group);   lua_setfield(L, core_idx, "is_item_or_in_group"); 

	// util/misc.lua 17 function / 3 porting lua to c++
	lua_pushcfunction(L, util_misc::l_generate_uuid);         lua_setfield(L, core_idx, "native_generate_uuid");
	lua_pushcfunction(L, util_misc::l_get_nodepos);           lua_setfield(L, core_idx, "native_get_nodepos");
	lua_pushcfunction(L, util_misc::l_calculate_knockback);   lua_setfield(L, core_idx, "native_calculate_knockback");

	// util/object.lua 21 function / 6 porting lua to c++
//	lua_pushcfunction(L, util_object::l_props_changed);         lua_setfield(L, core_idx, "native_props_changed");
//	lua_pushcfunction(L, util_object::l_get_object_center);     lua_setfield(L, core_idx, "native_get_object_center");
//	lua_pushcfunction(L, util_object::l_target_eye_height);     lua_setfield(L, core_idx, "native_target_eye_height");
//	lua_pushcfunction(L, util_object::l_target_eye_pos);        lua_setfield(L, core_idx, "native_target_eye_pos");
//	lua_pushcfunction(L, util_object::l_set_bone_position);     lua_setfield(L, core_idx, "native_set_bone_position");
	lua_pushcfunction(L, util_object::l_rotation_to_irrlicht);   lua_setfield(L, core_idx, "native_rotation_to_irrlicht");
/*
	// damage/init.lua  3 poring lua to c++
	lua_pushcfunction(L, damage::l_damage_calculate_modifier); lua_setfield(L, core_idx, "native_damage_calculate_modifier");
	lua_pushcfunction(L, damage::l_damage_tick_health);        lua_setfield(L, core_idx, "native_damage_tick_health");
	lua_pushcfunction(L, damage::l_damage_sync_to_engine);     lua_setfield(L, core_idx, "native_damage_sync_to_engine");
	// save interval API
	lua_pushcfunction(L, damage::l_damage_bulk_save_all); lua_setfield(L, core_idx, "native_damage_bulk_save_all");
*/
	// damage/init.lua  1 poring lua to c++
	lua_pushcfunction(L, damage::l_native_from_mt);		lua_setfield(L, core_idx, "native_from_mt");

	// explosions/init.lua  2 poring lua to c++
	lua_pushcfunction(L, explosions::l_native_compute_sphere_rays); lua_setfield(L, core_idx, "native_compute_sphere_rays");
	lua_pushcfunction(L, explosions::l_native_calculate_impact);    lua_setfield(L, core_idx, "native_calculate_impact");

	// flowlib/init.lua  1 poring lua to c++
	lua_pushcfunction(L, flowlib::l_native_quick_flow);         	lua_setfield(L, core_idx, "native_quick_flow");
	// mcl_liquids/init.lua  2 poring lua to c++
	lua_pushcfunction(L, flowlib::l_native_does_sl_need_update);	lua_setfield(L, core_idx, "native_does_sl_need_update");
	lua_pushcfunction(L, flowlib::l_native_does_fl_need_update);	lua_setfield(L, core_idx, "native_does_fl_need_update");

	// worlds/init.lua  5 poring lua to c++
	lua_pushcfunction(L, worlds::l_worlds_is_in_void);                 lua_setfield(L, core_idx, "native_worlds_is_in_void");
	lua_pushcfunction(L, worlds::l_worlds_y_to_layer);                 lua_setfield(L, core_idx, "native_worlds_y_to_layer");
	lua_pushcfunction(L, worlds::l_worlds_pos_to_dimension);           lua_setfield(L, core_idx, "native_worlds_pos_to_dimension");
	lua_pushcfunction(L, worlds::l_worlds_layer_to_y);                 lua_setfield(L, core_idx, "native_worlds_layer_to_y");
	lua_pushcfunction(L, worlds::l_worlds_tick_chunk_inhabited_time); lua_setfield(L, core_idx, "native_worlds_tick_chunk_inhabited_time");

	lua_pop(L, 1); // mcl_util テーブルをお片付け
}

void bind_mainthread_ENTITIES(lua_State *L) {

	lua_getglobal(L, "mclcapi");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		lua_newtable(L); 
		lua_pushvalue(L, -1);
		lua_setglobal(L, "mclcapi"); // _G.mclapi = {} を最初から一発創世！
	}
	int ent_idx = lua_gettop(L); // 基準となるテーブルの部屋番号を固定

	// ENTITIES/mcl_burning/init.lua + api.lua
	lua_pushcfunction(L, burning::l_native_check_burning_environment);		lua_setfield(L, ent_idx, "native_check_burning_environment");

	// ENTITIES/mcl_mobs/api.lua
	lua_pushcfunction(L, mobs::l_native_update_mob_timers);			lua_setfield(L, ent_idx, "native_update_mob_timers");
	lua_pushcfunction(L, mobs::l_native_mob_environment_scan);		lua_setfield(L, ent_idx, "native_mob_environment_scan");
	// ENTITIES/mcl_mobs/pathfinder.lua
	lua_pushcfunction(L, mobs::l_native_gwp_compute_path);			lua_setfield(L, ent_idx, "native_gwp_compute_path");

/*
	// ENTITIES/mcl_mobs/init.lua
	lua_pushcfunction(L, entities::l_mobs_register_villager_native); lua_setfield(L, ent_idx, "native_register_villager");
	lua_pushcfunction(L, entities::l_mobs_check_poi_valid_native);   lua_setfield(L, ent_idx, "native_check_poi_valid");
	lua_pushcfunction(L, entities::l_mobs_filter_trades_native);     lua_setfield(L, ent_idx, "native_filter_trades");

	// ENTITIES/mcl_mobs/combat.lua
	lua_pushcfunction(L, entities::l_mobs_find_nearest_target_native); lua_setfield(L, ent_idx, "native_find_nearest_target");

	// ENTITIES/mcl_mobs/pathfinding.lua
	lua_pushcfunction(L, entities::l_mobs_find_path_native);         lua_setfield(L, ent_idx, "native_pathfind");
	lua_pushcfunction(L, entities::l_mobs_set_pathfinding_speed_native); 	lua_setfield(L, ent_idx, "native_set_pathfinding_speed");

	lua_pop(L, 1); // mcl_util テーブルをお片付け
*/
}

void bind_multithread_CORE(lua_State *L) {
	lua_getglobal(L, "mclcapi");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		lua_newtable(L); 
		lua_pushvalue(L, -1);
		lua_setglobal(L, "mclcapi"); // _G.mclcapi = {} を最初から一発創世！
	}
	int mth_idx = lua_gettop(L); // 基準となるテーブルの部屋番号を固定

	// util/shape.lua  4? function / 10 porting lua to c++
	lua_pushcfunction(L, util_shape::l_decompose_aabbs);       lua_setfield(L, mth_idx, "native_decompose_aabbs");
	lua_pushcfunction(L, util_shape::l_region_op);             lua_setfield(L, mth_idx, "native_region_op");
	lua_pushcfunction(L, util_shape::l_region_evaluate);       lua_setfield(L, mth_idx, "native_region_evaluate");
	lua_pushcfunction(L, util_shape::l_any_occupied_p);        lua_setfield(L, mth_idx, "native_any_occupied_p");
	lua_pushcfunction(L, util_shape::l_region_volume);         lua_setfield(L, mth_idx, "native_region_volume");
	lua_pushcfunction(L, util_shape::l_region_equal_p);        lua_setfield(L, mth_idx, "native_region_equal_p");
	lua_pushcfunction(L, util_shape::l_region_walk);           lua_setfield(L, mth_idx, "native_region_walk");
	lua_pushcfunction(L, util_shape::l_region_simplify);       lua_setfield(L, mth_idx, "native_region_simplify");
	lua_pushcfunction(L, util_shape::l_region_select_face);    lua_setfield(L, mth_idx, "native_region_select_face");
	lua_pushcfunction(L, util_shape::l_region_intersect_p);    lua_setfield(L, mth_idx, "native_region_intersect_p");
	// 変数
//	lua_pushcfunction(L, util_shape::l_var_set_aabb_weight);   lua_setfield(L, mth_idx, "var_set_aabb_weight");

	// util/table.lua  14 function / 3 porting lua to c++
	lua_pushcfunction(L, util_table::l_table_update);             lua_setfield(L, mth_idx, "native_table_update");
	lua_pushcfunction(L, util_table::l_table_update_deep);        lua_setfield(L, mth_idx, "native_table_update_deep");
	lua_pushcfunction(L, util_table::l_table_keyset);             lua_setfield(L, mth_idx, "native_table_keyset");
/*
	// ENTITIES/mcl_mobs/combat.lua
	lua_pushcfunction(L, entities::l_mobs_find_nearest_target_native); lua_setfield(L, mth_idx, "native_find_nearest_target");

	// ENTITIES/mcl_mobs/pathfinding.lua
	lua_pushcfunction(L, entities::l_mobs_find_path_native);         lua_setfield(L, mth_idx, "native_pathfind");
	lua_pushcfunction(L, entities::l_mobs_set_pathfinding_speed_native); 	lua_setfield(L, mth_idx, "native_set_pathfinding_speed");
*/
	lua_pop(L, 1); // mcl_util テーブルをお片付け
}

//#define L_MCL_MAP_METATABLE "mcl_core_map_object"

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
	// Resister for Main thread only
	bind_mainthread_CORE(L);
	bind_mainthread_ENTITIES(L);
//	bind_mcl_util_mainthread(L);
	// Resister for Main thread and other
	// Resister for Emerge-0 thread src/script/cpp_api/s_mapgen.cpp
	// Resister for AsyncWorker thread src/script/cpp_api/s_async.cpp
	bind_multithread_CORE(L);
	actionstream << "[lua_api]: [mcl/mods/CORE]: Resister C++ API for server." << std::endl;
}

} // namespace l_mcl_core_server
