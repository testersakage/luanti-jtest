// src/mcl/entities/mobs.cpp
#include "mobs.h"
#include "irr_v3d.h" 
#include "log.h" // 🎯 【結線】：Luantiの純正C++ロガー（actionstream用）を目次インクルード！
#include <lua.hpp>
#include <cmath>
#include <vector>
#include <queue>
#include <unordered_map>
#include <string>
#include <algorithm>

namespace mobs {

	inline void get_node_raw_mobs(lua_State *L, int x, int y, int z, int& cid, int& param2)
	{
		lua_getglobal(L, "core");
		lua_getfield(L, -1, "get_node_raw");
		lua_pushinteger(L, x);
		lua_pushinteger(L, y);
		lua_pushinteger(L, z);
		lua_call(L, 3, 3);
		cid = static_cast<int>(lua_tointeger(L, -3));
		param2 = static_cast<int>(lua_tointeger(L, -1));
		lua_pop(L, 4);
	}

	inline std::string get_node_class_cpp(lua_State *L, int cid)
	{
		lua_getglobal(L, "mcl_mobs");
		lua_getfield(L, -1, "gwp_basic_node_classes");
		lua_pushinteger(L, cid);
		lua_gettable(L, -2);
		std::string node_class = lua_isstring(L, -1) ? lua_tostring(L, -1) : "OPEN";
		lua_pop(L, 3);
		return node_class;
	}

	int l_native_update_mob_timers(lua_State *L)
	{
		luaL_checktype(L, 1, LUA_TTABLE); 
		double dtime = luaL_checknumber(L, 2);

		lua_getfield(L, 1, "_timers");
		if (!lua_istable(L, -1)) {
			lua_pop(L, 1);
			return 0;
		}

		const int TIMERS_TABLE_INDEX = 3;

		struct TimerUpdate {
			std::string key;
			double new_value;
		};
		std::vector<TimerUpdate> updates;

		lua_pushnil(L);
		while (lua_next(L, TIMERS_TABLE_INDEX) != 0) {
			if (lua_isstring(L, -2) && lua_isnumber(L, -1)) {
				updates.push_back({lua_tostring(L, -2), lua_tonumber(L, -1) - dtime});
			}
			lua_pop(L, 1); 
		}

		for (const auto& up : updates) {
			lua_pushstring(L, up.key.c_str());
			lua_pushnumber(L, up.new_value);
			lua_settable(L, -3); 
		}

		lua_pop(L, 1); 
		return 0;
	}

	int l_native_mob_environment_scan(lua_State *L)
	{
		luaL_checktype(L, 1, LUA_TTABLE);
		luaL_checktype(L, 2, LUA_TTABLE);

		lua_getfield(L, 1, "x"); int fx = static_cast<int>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);
		lua_getfield(L, 1, "y"); int fy = static_cast<int>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);
		lua_getfield(L, 1, "z"); int fz = static_cast<int>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);

		lua_getfield(L, 2, "x"); int hx = static_cast<int>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);
		lua_getfield(L, 2, "y"); int hy = static_cast<int>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);
		lua_getfield(L, 2, "z"); int hz = static_cast<int>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);

		int cid_in, p2_in, cid_on, p2_on, cid_head, p2_head;

		get_node_raw_mobs(L, fx, fy, fz, cid_in, p2_in);
		get_node_raw_mobs(L, fx, fy - 1, fz, cid_on, p2_on);
		get_node_raw_mobs(L, hx, hy, hz, cid_head, p2_head);

		lua_pushinteger(L, cid_in);   lua_pushinteger(L, p2_in);
		lua_pushinteger(L, cid_on);   lua_pushinteger(L, p2_on);
		lua_pushinteger(L, cid_head); lua_pushinteger(L, p2_head);

		return 6;
	}

	struct AStarNode {
		s16 x, y, z;
		double g, h, f;
		v3s16 parent_pos; 
		bool has_parent;
		std::string node_class;
	};

	struct CompareNodes {
		bool operator()(const AStarNode* a, const AStarNode* b) {
			return a->f > b->f;
		}
	};

	int l_native_gwp_compute_path(lua_State *L)
	{
		// 1. 絶対インデックス部屋（1番と2番）から X,Y,Z を引き抜き
		lua_getfield(L, 1, "x"); s16 sx = static_cast<s16>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);
		lua_getfield(L, 1, "y"); s16 sy = static_cast<s16>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);
		lua_getfield(L, 1, "z"); s16 sz = static_cast<s16>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);

		lua_getfield(L, 2, "x"); s16 gx = static_cast<s16>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);
		lua_getfield(L, 2, "y"); s16 gy = static_cast<s16>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);
		lua_getfield(L, 2, "z"); s16 gz = static_cast<s16>(std::floor(lua_tonumber(L, -1))); lua_pop(L, 1);

		s16 range = static_cast<s16>(luaL_checkinteger(L, 3)); 
		double tolerance = luaL_checknumber(L, 4); 

		// 🎯【C++ネイティブ背面露出】：
		//    C++の胃袋へ突入した生の数値をそのままサーバーログ（debug.txt）へ直撃スタンプ！！！
		//    これにより「C++側が0,0,0を掴まされているのか、それともA*の内部で0,0,0へワープしているのか」
		//    非対称の悪夢の正確な座標位置が1秒で露出逮捕されます。
/*		actionstream << "======= [CPP_AABB_DEBUG] native_gwp_compute_path INVOCATION =======" << std::endl;
		actionstream << "   -> START_POS: " << sx << "," << sy << "," << sz << std::endl;
		actionstream << "   -> GOAL_POS : " << gx << "," << gy << "," << gz << std::endl;
		actionstream << "   -> PARAMS   : range=" << range << ", tolerance=" << tolerance << std::endl;
*/
		s16 min_x = sx - range, max_x = sx + range;
		s16 min_y = sy - range, max_y = sy + range; 
		s16 min_z = sz - range, max_z = sz + range;

		std::priority_queue<AStarNode*, std::vector<AStarNode*>, CompareNodes> open_set;
		std::unordered_map<std::string, AStarNode*> all_nodes; 

		auto get_node_key = [](s16 x, s16 y, s16 z) {
			return std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(z);
		};

		AStarNode* start_node = new AStarNode{sx, sy, sz, 0.0, 0.0, 0.0, {0,0,0}, false, "OPEN"};
		start_node->h = std::abs(sx - gx) + std::abs(sy - gy) + std::abs(sz - gz); 
		start_node->f = start_node->h;
		open_set.push(start_node);
		all_nodes[get_node_key(sx, sy, sz)] = start_node;

		AStarNode* goal_reached_node = nullptr;
		AStarNode* best_partial_node = start_node;
		double min_h_found = start_node->h;

		int nodes_processed = 0;
		int max_nodes_limit = range * 16; 

		while (!open_set.empty() && nodes_processed < max_nodes_limit) {
			AStarNode* current = open_set.top();
			open_set.pop();
			nodes_processed++;

			double current_h = std::abs(current->x - gx) + std::abs(current->y - gy) + std::abs(current->z - gz);
			if (current_h <= tolerance) {
				goal_reached_node = current;
				break;
			}

			if (current_h < min_h_found) {
				min_h_found = current_h;
				best_partial_node = current;
			}

			s16 dx[] = {1, -1, 0, 0};
			s16 dz[] = {0, 0, 1, -1};
			s16 dy_offsets[] = {0, 1, -1}; 

			for (int i = 0; i < 4; i++) {
				for (int j = 0; j < 3; j++) {
					s16 nx = current->x + dx[i];
					s16 ny = current->y + dy_offsets[j];
					s16 nz = current->z + dz[i];

					if (nx < min_x || nx > max_x || ny < min_y || ny > max_y || nz < min_z || nz > max_z) continue;

					int cid, p2;
					get_node_raw_mobs(L, nx, ny, nz, cid, p2);
					std::string n_class = get_node_class_cpp(L, cid);

					if (n_class == "OPEN" && dy_offsets[j] == 0) {
						int fall_dist = 0;
						while (ny > min_y && fall_dist < 4) { 
							ny--;
							fall_dist++;
							get_node_raw_mobs(L, nx, ny, nz, cid, p2);
							std::string drop_class = get_node_class_cpp(L, cid);
							if (drop_class != "OPEN") {
								n_class = drop_class;
								break;
							}
						}
					}

					if (n_class == "LAVA" || n_class == "IGNORE") continue;

					if (n_class == "WALKABLE") {
						ny = ny + 1;
						n_class = "OPEN";
					}

					double move_cost = 1.0 + (n_class == "WATER" ? 8.0 : 0.0); 
					if (dy_offsets[j] == 1) move_cost += 0.5; 

					double tentative_g = current->g + move_cost;

					std::string n_key = get_node_key(nx, ny, nz);
					if (all_nodes.count(n_key)) {
						AStarNode* neighbor = all_nodes[n_key];
						if (tentative_g < neighbor->g) {
							neighbor->g = tentative_g;
							neighbor->f = neighbor->g + neighbor->h;
							neighbor->parent_pos = {current->x, current->y, current->z};
							neighbor->has_parent = true;
							open_set.push(neighbor); 
						}
					} else {
						AStarNode* neighbor = new AStarNode{nx, ny, nz, tentative_g, 0.0, 0.0, {current->x, current->y, current->z}, true, n_class};
						neighbor->h = (std::abs(nx - gx) + std::abs(ny - gy) + std::abs(nz - gz)) * 1.5; 
						neighbor->f = neighbor->g + neighbor->h;
						open_set.push(neighbor);
						all_nodes[n_key] = neighbor;
					}
				}
			}
		}

		AStarNode* target_node = goal_reached_node ? goal_reached_node : best_partial_node;
		
		std::vector<AStarNode*> path_nodes;
		
		if (goal_reached_node) {
			while (target_node) {
				path_nodes.push_back(target_node);
				if (target_node->has_parent) {
					std::string p_key = get_node_key(target_node->parent_pos.X, target_node->parent_pos.Y, target_node->parent_pos.Z);
					target_node = all_nodes.count(p_key) ? all_nodes[p_key] : nullptr;
				} else {
					break;
				}
			}
		} else {
			if (target_node) {
				path_nodes.push_back(target_node);
			}
		}

		// 🎯【成果物の座標スキャン】：C++側が出荷するまさにその一歩目の実数座標も全量露出ログ！
/*		if (!path_nodes.empty()) {
			AStarNode* first_wp = path_nodes.back(); // 配列の最末尾＝最初の一歩
			actionstream << "   -> OUTPUT_NODE: x=" << first_wp->x << ", y=" << (first_wp->y - 0.5) << ", z=" << first_wp->z << " | total_nodes=" << path_nodes.size() << std::endl;
		}
*/
		lua_newtable(L); 
		int waypoint_idx = 1;

		for (auto it = path_nodes.begin(); it != path_nodes.end(); ++it) {
			AStarNode* n = *it;
			
			lua_newtable(L);
			lua_pushnumber(L, static_cast<double>(n->x)); lua_setfield(L, -2, "x");
			lua_pushnumber(L, static_cast<double>(n->y) - 0.5); lua_setfield(L, -2, "y");
			lua_pushnumber(L, static_cast<double>(n->z)); lua_setfield(L, -2, "z");
			
			lua_pushnumber(L, 0.0);  lua_setfield(L, -2, "x_offset");
			lua_pushnumber(L, -0.5); lua_setfield(L, -2, "y_offset"); 
			
			lua_rawseti(L, -2, waypoint_idx++);
		}

		for (auto& pair : all_nodes) {
			delete pair.second;
		}

		return 1;
	}

} // namespace mobs
