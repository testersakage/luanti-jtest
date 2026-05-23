// src/mcl/core/util_environment.h
#pragma once
#include <lua.hpp>

namespace util_environment {
	int l_get_double_container_neighbor_pos(lua_State *L);
	int l_get_eligible_transfer_item_slot(lua_State *L);
	int l_drop_items_from_meta_container(lua_State *L);
	int l_get_pointed_thing(lua_State *L);
	int l_traverse_tower(lua_State *L);
	int l_traverse_tower_group(lua_State *L);
	int l_replace_node_vm(lua_State *L);
	int l_bulk_set_node_vm(lua_State *L);
	int l_circle_bulk_set_node_vm(lua_State *L);
	int l_environment_globalstep(lua_State *L);
}
