// src/mcl/entities/mobs_pathfinding.cpp （★通常時特化可変・実験インフラ確定版）

#include "mobs_pathfinding.h"
#include "mcl/stacktrace.h"
#include <lua.hpp>
#include <unordered_map>
#include <queue>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

namespace entities {

	struct GWPNode {
		int16_t x, y, z;
		double g, h, f;
		uint32_t parent_hash;
		bool covered;
	};

	// 👑 【時空の砂時計】：チャットコマンド /pathfinding_speed から上書きされる、通常追跡時の標準周期（デフォルト: 0.5秒）
	static double g_mob_pathfind_tick_interval = 0.5;

	// 💎 【決定版】：チャット欄からの無法な数値を水際で100%調教・完全ガード！！！
	int l_mobs_set_pathfinding_speed_native(lua_State *L)
	{
		if (lua_isnumber(L, 1)) {
			double input_val = lua_tonumber(L, 1);

			// 🛡️ 【絶対規律】：下限を0.01秒（10ms）、上限を5.0秒に強制ホールド！！！
			//     これにより /pathfinding_speed 0 などの即死テロが打ち込まれても、
			//     システムは安全な限界値へと全自動で引き算・お片付けを執行します。
			if (input_val < 0.01) {
				input_val = 0.01;
			} else if (input_val > 5.0) {
				input_val = 5.0;
			}

			g_mob_pathfind_tick_interval = input_val;
		}
		
		lua_pushnumber(L, g_mob_pathfind_tick_interval);
		return 1;
	}

	inline uint32_t hash_pos_no_jit(int16_t x, int16_t y, int16_t z, int16_t min_x, int16_t min_y, int16_t min_z) {
		return static_cast<uint32_t>((x - min_x) * 65536 + (y - min_y) * 256 + (z - min_z));
	}

	inline uint32_t hash_pos_with_jit(int16_t x, int16_t y, int16_t z, int16_t min_x, int16_t min_y, int16_t min_z) {
		return static_cast<uint32_t>(((x - min_x) << 16) + ((y - min_y) << 8) + (z - min_z));
	}

	struct NodeComparator {
		bool operator()(const GWPNode& a, const GWPNode& b) const {
			return (a.f > b.f);
		}
	};

	int l_mobs_register_villager_native(lua_State *L) { if (lua_gettop(L) > 0) { lua_pushvalue(L, 1); return 1; } return 0; }
	int l_mobs_check_poi_valid_native(lua_State *L) { if (lua_gettop(L) > 0) { lua_pushvalue(L, 1); return 1; } lua_pushboolean(L, false); return 1; }
	int l_mobs_filter_trades_native(lua_State *L) { if (lua_gettop(L) > 0) { lua_pushvalue(L, 1); return 1; } return 0; }

	// 💎 4. 【大開通】：native_pathfind (通常時のみ可変実験・A*強奪完全体)
	int l_mobs_find_path_native(lua_State *L)
	{
		int current_top = lua_gettop(L);
		if (!lua_istable(L, 2)) {
			lua_pushboolean(L, false); lua_pushnumber(L, 0.0); return 2;
		}

		int16_t min_x = 0, min_y = 0, min_z = 0;
		lua_getfield(L, 2, "minpos");
		if (lua_istable(L, -1)) {
			lua_getfield(L, -1, "x"); min_x = static_cast<int16_t>(lua_tonumber(L, -1)); lua_pop(L, 1);
			lua_getfield(L, -1, "y"); min_y = static_cast<int16_t>(lua_tonumber(L, -1)); lua_pop(L, 1);
			lua_getfield(L, -1, "z"); min_z = static_cast<int16_t>(lua_tonumber(L, -1)); lua_pop(L, 1);
		}
		lua_pop(L, 1);

		int range = 48;
		double tolerance = 0.0;
		lua_getfield(L, 2, "range"); if (lua_isnumber(L, -1)) range = static_cast<int>(lua_tonumber(L, -1)); lua_pop(L, 1);
		lua_getfield(L, 2, "tolerance"); if (lua_isnumber(L, -1)) tolerance = lua_tonumber(L, -1); lua_pop(L, 1);

		int16_t start_x = min_x + range;
		int16_t start_y = min_y + range;
		int16_t start_z = min_z + range;

		lua_getglobal(L, "core");
		lua_getfield(L, -1, "global_exists");
		lua_pushstring(L, "jit");
		lua_call(L, 1, 1);
		bool is_jit = lua_toboolean(L, -1);
		lua_pop(L, 2);

		int16_t target_x = start_x, target_y = start_y, target_z = start_z;
		int best_m_dist = 999999;
		int target_idx_in_lua = 1;

		lua_getfield(L, 2, "targets");
		if (lua_istable(L, -1)) {
			int len = lua_objlen(L, -1);
			for (int t = 1; t <= len; t++) {
				lua_rawgeti(L, -1, t);
				if (lua_istable(L, -1)) {
					lua_getfield(L, -1, "x"); int16_t tx = static_cast<int16_t>(lua_tonumber(L, -1)); lua_pop(L, 1);
					lua_getfield(L, -1, "y"); int16_t ty = static_cast<int16_t>(lua_tonumber(L, -1)); lua_pop(L, 1);
					lua_getfield(L, -1, "z"); int16_t tz = static_cast<int16_t>(lua_tonumber(L, -1)); lua_pop(L, 1);

					int d = std::abs(start_x - tx) + std::abs(start_y - ty) + std::abs(start_z - tz);
					if (d < best_m_dist) {
						best_m_dist = d;
						target_x = tx; target_y = ty; target_z = tz;
						target_idx_in_lua = t; 
					}
				}
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);

		auto calculate_hash = [=](int16_t x, int16_t y, int16_t z) -> uint32_t {
			return is_jit ? hash_pos_with_jit(x, y, z, min_x, min_y, min_z)
			              : hash_pos_no_jit(x, y, z, min_x, min_y, min_z);
		};

		std::priority_queue<GWPNode, std::vector<GWPNode>, NodeComparator> open_set;
		std::unordered_map<uint32_t, GWPNode> close_set;

		uint32_t start_hash = calculate_hash(start_x, start_y, start_z);
		GWPNode start_node = { start_x, start_y, start_z, 0.0, 0.0, 0.0, 0, false };
		start_node.h = std::abs(start_x - target_x) + std::abs(start_y - target_y) + std::abs(start_z - target_z);
		start_node.f = start_node.h;

		open_set.push(start_node);
		close_set[start_hash] = start_node;

		int dx[] = {-1, 1, 0, 0, 0, 0};
		int dy[] = {0, 0, -1, 1, 0, 0};
		int dz[] = {0, 0, 0, 0, -1, 1};

		uint32_t last_node_hash = start_hash;
		int max_nodes_limit = range * 16;
		int iterations = 0;
		bool path_found = false;

		while (!open_set.empty() && iterations++ < max_nodes_limit) {
			GWPNode curr = open_set.top();
			open_set.pop();

			uint32_t curr_hash = calculate_hash(curr.x, curr.y, curr.z);
			close_set[curr_hash].covered = true;
			last_node_hash = curr_hash;

			if (std::abs(curr.x - target_x) + std::abs(curr.y - target_y) + std::abs(curr.z - target_z) <= tolerance) {
				path_found = true;
				break;
			}

			for (int d = 0; d < 6; d++) {
				int16_t nx = curr.x + dx[d];
				int16_t ny = curr.y + dy[d];
				int16_t nz = curr.z + dz[d];
				uint32_t next_hash = calculate_hash(nx, ny, nz);

				if (std::abs(nx - start_x) > range || std::abs(nz - start_z) > range) continue;

				double move_dist = std::sqrt(dx[d]*dx[d] + dy[d]*dy[d] + dz[d]*dz[d]);
				double new_g = curr.g + move_dist;

				if (close_set.find(next_hash) == close_set.end() || new_g < close_set[next_hash].g) {
					GWPNode neighbor = { nx, ny, nz, new_g, 0.0, 0.0, curr_hash, false };
					neighbor.h = (std::abs(nx - target_x) + std::abs(ny - target_y) + std::abs(nz - target_z)) * 1.5;
					neighbor.f = neighbor.g + neighbor.h;

					close_set[next_hash] = neighbor;
					open_set.push(neighbor);
				}
			}
		}

		lua_getfield(L, 2, "nodes");
		if (lua_istable(L, -1)) {
			int nodes_idx = lua_gettop(L);
			uint32_t trace_hash = last_node_hash;

			while (trace_hash != 0 && trace_hash != start_hash) {
				auto it = close_set.find(trace_hash);
				if (it == close_set.end()) break;

				lua_rawgeti(L, nodes_idx, trace_hash);
				if (!lua_istable(L, -1)) {
					lua_pop(L, 1); lua_newtable(L);
					lua_pushnumber(L, it->second.x); lua_setfield(L, -2, "x");
					lua_pushnumber(L, it->second.y); lua_setfield(L, -2, "y");
					lua_pushnumber(L, it->second.z); lua_setfield(L, -2, "z");
					lua_pushvalue(L, -1); lua_rawseti(L, nodes_idx, trace_hash);
				}

				if (it->second.parent_hash != 0) {
					lua_rawgeti(L, nodes_idx, it->second.parent_hash);
					if (!lua_istable(L, -1)) {
						lua_pop(L, 1); lua_newtable(L);
						auto pit = close_set.find(it->second.parent_hash);
						if (pit != close_set.end()) {
							lua_pushnumber(L, pit->second.x); lua_setfield(L, -2, "x");
							lua_pushnumber(L, pit->second.y); lua_setfield(L, -2, "y");
							lua_pushnumber(L, pit->second.z); lua_setfield(L, -2, "z");
						}
						lua_pushvalue(L, -1); lua_rawseti(L, nodes_idx, it->second.parent_hash);
					}
					lua_setfield(L, -2, "referrer");
				}
				lua_pop(L, 1);
				trace_hash = it->second.parent_hash;
			}

			lua_rawgeti(L, nodes_idx, last_node_hash);
			if (lua_istable(L, -1)) {
				lua_getfield(L, 2, "targets");
				if (lua_istable(L, -1)) {
					lua_rawgeti(L, -1, target_idx_in_lua); 
					if (lua_istable(L, -1)) {
						lua_pushvalue(L, -3); 
						lua_setfield(L, -2, "best_node"); 
						lua_pushnumber(L, 0.0); lua_setfield(L, -2, "best_distance");
					}
					lua_pop(L, 1);
				}
				lua_pop(L, 1);
			}
			lua_pop(L, 1); 

			if (path_found) {
				lua_getfield(L, 2, "arrivals");
				if (lua_istable(L, -1)) {
					int arr_idx = lua_gettop(L);
					lua_getfield(L, 2, "targets");
					if (lua_istable(L, -1)) {
						lua_rawgeti(L, -1, target_idx_in_lua); 
						if (lua_istable(L, -1)) {
							lua_pushvalue(L, -1); 
							lua_rawseti(L, arr_idx, 1); 
						}
						lua_pop(L, 1);
					}
					lua_pop(L, 1);
				}
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1); 

		// ─── 👑 【これにて最終確定：通常時だけをピンポイントに可変実験するインフラ】 ───
		double final_tick_interval = g_mob_pathfind_tick_interval; // 👈 通常時はコマンドの数値を100%反映（標準0.5）

		if (!path_found) {
			// 🛡️ 密集・迷子時の緊急修正ステップ（0.05秒）は、安全のためにシステム固定ホールド！
			final_tick_interval = 0.05; 
		} 
		else if (start_node.h > 20.0) {
			// 🛡️ 遠距離の省エネステップ（2.0秒）も、サーバー負荷削減のためにシステム固定ホールド！
			final_tick_interval = 2.0;
		}

		if (path_found || iterations >= max_nodes_limit) {
			lua_pushboolean(L, true); 
		} else {
			lua_pushboolean(L, false); 
		}
		
		lua_pushnumber(L, final_tick_interval); // 👈 🏆 完璧な実験仕様パルスを出荷！！！

		return 2; 
	}
}
