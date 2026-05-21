// src/mcl/core/util_shape.cpp
#include "util_shape.h"
#include "mcl/stacktrace.h"
#include "irrTypes.h"
#include <vector>
#include <set>
#include <algorithm>
#include <cmath>

namespace util_shape {

// 💡 二分探索ヘルパー
inline int bisect_cpp(const std::vector<double>& edges, double value) {
	auto it = std::lower_bound(edges.begin(), edges.end(), value);
	return std::distance(edges.begin(), it) + 1;
}

// 💡 3重ループ最深部用の超速ネイティブビット判定マシーン
inline bool is_occupied_p_cpp(const std::vector<u32>& solids, int disp, int x, int y, int z) {
	int index = (x << (disp + disp)) + (y << disp) + z;
	int idx = index / 32;
	int off = index & (32 - 1);
	if (idx >= 0 && idx < (int)solids.size()) {
		return (solids[idx] & (1 << off)) != 0;
	}
	return false;
}

// 💡 3重ループ最深部用の超速ネイティブビット書き込みマシーン
inline void mark_occupied_cpp(std::vector<u32>& solids, int disp, int x, int y, int z) {
	int index = (x << (disp + disp)) + (y << disp) + z;
	int idx = index / 32;
	int off = index & (32 - 1);
	if (idx >= 0 && idx < (int)solids.size()) {
		solids[idx] |= (1 << off);
	}
}

// 💡 優位値判定のC++インライン最速エミュレート
inline void get_dominating_values_cpp(int il, int ir, const std::vector<double>& l, const std::vector<double>& r, int l_max, int r_max, int& ldom, int& rdom) {
	if (il == l_max) { ldom = 0; rdom = (ir != r_max) ? ir : 0; }
	else if (ir == r_max) { ldom = (il != l_max) ? il : 0; rdom = 0; }
	else if (l[il - 1] < r[ir - 1]) { ldom = il; rdom = (ir != 1) ? ir - 1 : 0; }
	else if (r[ir - 1] < l[il - 1]) { ldom = (il != 1) ? il - 1 : 0; rdom = ir; }
	else { ldom = il; rdom = ir; }
}

// 📥 1. Luaのテーブルから solids 配列をC++側へ1パス回収する、全関数共通の公式インフラ！
static std::vector<u32> get_solids_from_lua(lua_State *L, int rgn_idx) {
	std::vector<u32> res;
	lua_getfield(L, rgn_idx, "solids");
	int len = lua_objlen(L, -1);
	for (int i = 1; i <= len; i++) {
		lua_rawgeti(L, -1, i);
		res.push_back(static_cast<u32>(lua_tointeger(L, -1)));
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	return res;
}

// 📥 2. Luaのテーブルからエッジ座標配列をC++側へ1パス回収する、全関数共通の公式インフラ！
static std::vector<double> get_edges_from_lua(lua_State *L, int rgn_idx, const char* name) {
	std::vector<double> res;
	lua_getfield(L, rgn_idx, name);
	int len = lua_objlen(L, -1);
	for (int i = 1; i <= len; i++) {
		lua_rawgeti(L, -1, i);
		res.push_back(lua_tonumber(L, -1));
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	return res;
}

// ヘルパー：テーブルから数値配列（エッジ）を高速回収
static std::vector<double> get_edges_cpp(lua_State *L, int rgn_idx, const char* name) {
	std::vector<double> res;
	lua_getfield(L, rgn_idx, name);
	int len = lua_objlen(L, -1);
	for (int i = 1; i <= len; i++) {
		lua_rawgeti(L, -1, i);
		res.push_back(lua_tonumber(L, -1));
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	return res;
}

// ヘルパー：テーブルからsolidsビット配列を高速回収
static std::vector<u32> get_solids_cpp(lua_State *L, int rgn_idx) {
	std::vector<u32> res;
	lua_getfield(L, rgn_idx, "solids");
	int len = lua_objlen(L, -1);
	for (int i = 1; i <= len; i++) {
		lua_rawgeti(L, -1, i); res.push_back(static_cast<u32>(lua_tointeger(L, -1))); lua_pop(L, 1);
	}
	lua_pop(L, 1);
	return res;
}

static void push_edges_cpp(lua_State *L, int rgn_idx, const std::vector<double>& src, const char* name) {
	lua_newtable(L);
	int e_idx = lua_gettop(L);
	for (size_t i = 0; i < src.size(); i++) {
		lua_pushnumber(L, src[i]);
		lua_rawseti(L, e_idx, i + 1);
	}
	lua_setfield(L, rgn_idx, name);
}

// ❌ 1. mcl_util.decompose_AABBs(aabbs) のC++完全移植
int l_decompose_aabbs(lua_State *L)
{
	int target_idx = stacktrace::find_table_by_method(L, "decompose_AABBs");
	if (target_idx == 0) target_idx = 1;
	luaL_checktype(L, target_idx, LUA_TTABLE);

	std::vector<double> x_edges, y_edges, z_edges;
	std::set<double> x_seen, y_seen, z_seen;
	int num_aabbs = lua_objlen(L, target_idx);
	
	for (int i = 1; i <= num_aabbs; i++) {
		lua_rawgeti(L, target_idx, i);
		if (lua_istable(L, -1)) {
			for (int j = 1; j <= 6; j++) {
				lua_rawgeti(L, -1, j); double v = lua_tonumber(L, -1); lua_pop(L, 1);
				if (j == 1 || j == 4) { if (x_seen.insert(v).second) x_edges.push_back(v); }
				if (j == 2 || j == 5) { if (y_seen.insert(v).second) y_edges.push_back(v); }
				if (j == 3 || j == 6) { if (z_seen.insert(v).second) z_edges.push_back(v); }
			}
		}
		lua_pop(L, 1);
	}
	std::sort(x_edges.begin(), x_edges.end());
	std::sort(y_edges.begin(), y_edges.end());
	std::sort(z_edges.begin(), z_edges.end());

	if (x_edges.size() > 1023 || y_edges.size() > 1023 || z_edges.size() > 1023) return 0;

	int max_n = std::max({x_edges.size(), y_edges.size(), z_edges.size()});
	int b_disp = -1;
	for (int i = 1; i <= 31; i++) { if ((1 << i) >= max_n) { b_disp = i; break; } }
	int x_sz = x_edges.size(), y_sz = y_edges.size(), z_sz = z_edges.size();
	int index_max = (x_sz << (b_disp + b_disp)) + (y_sz << b_disp) + z_sz;
	int b_size = (index_max + 32 - 1) / 32;

	lua_newtable(L); int rgn_idx = lua_gettop(L);
	lua_newtable(L); int solids_idx = lua_gettop(L);
	std::vector<u32> solids(b_size, 0);

	for (int i = 1; i <= num_aabbs; i++) {
		lua_rawgeti(L, target_idx, i);
		double a[7] = {0};
		for (int j = 1; j <= 6; j++) { lua_rawgeti(L, -1, j); a[j] = lua_tonumber(L, -1); lua_pop(L, 1); }
		lua_pop(L, 1);
		int x1 = bisect_cpp(x_edges, a[1]); int y1 = bisect_cpp(y_edges, a[2]); int z1 = bisect_cpp(z_edges, a[3]);
		int x2 = bisect_cpp(x_edges, a[4]); int y2 = bisect_cpp(y_edges, a[5]); int z2 = bisect_cpp(z_edges, a[6]);
		for (int x = x1; x <= x2 - 1; x++) {
			for (int y = y1; y <= y2 - 1; y++) {
				for (int z = z1; z <= z2 - 1; z++) {
					mark_occupied_cpp(solids, b_disp, x - 1, y - 1, z - 1);
				}
			}
		}
	}
	for (int i = 0; i < b_size; i++) { lua_pushinteger(L, solids[i]); lua_rawseti(L, solids_idx, i + 1); }
	lua_setfield(L, rgn_idx, "solids");
	auto push_edges = [&](const std::vector<double>& src, const char* name) {
		lua_newtable(L); int e_idx = lua_gettop(L);
		for (size_t i = 0; i < src.size(); i++) { lua_pushnumber(L, src[i]); lua_rawseti(L, e_idx, i + 1); }
		lua_setfield(L, rgn_idx, name);
	};
	push_edges(x_edges, "x_edges"); push_edges(y_edges, "y_edges"); push_edges(z_edges, "z_edges");
	lua_pushinteger(L, x_sz); lua_setfield(L, rgn_idx, "x_size");
	lua_pushinteger(L, y_sz); lua_setfield(L, rgn_idx, "y_size");
	lua_pushinteger(L, z_sz); lua_setfield(L, rgn_idx, "z_size");
	lua_pushinteger(L, b_size); lua_setfield(L, rgn_idx, "b_size");
	lua_pushinteger(L, b_disp); lua_setfield(L, rgn_idx, "b_disp");
	// 出荷直前のテーブルへ直接 region_class メタテーブルを結合させる
	lua_getglobal(L, "mcl_util");
	lua_getfield(L, -1, "region_class"); // グローバルに実在する本物の region_class メタを取得
	lua_setmetatable(L, rgn_idx);        // 生成した region テーブルへガチッと結合！
	lua_pop(L, 1);                       // mcl_util をポップしてお掃除

	lua_replace(L, 1); lua_settop(L, 1);
	return 1;
}

// ❌ 2. region_op(l, r, op_type) のC++完全移植
int l_region_op(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_checktype(L, 2, LUA_TTABLE);
	int op_type = luaL_checkinteger(L, 3);

	lua_getfield(L, 1, "x_size"); int l_x_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 2, "x_size"); int r_x_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 1, "b_disp"); int l_b_disp = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 2, "b_disp"); int r_b_disp = lua_tointeger(L, -1); lua_pop(L, 1);

	std::vector<double> l_x_edges = get_edges_cpp(L, 1, "x_edges"), l_y_edges = get_edges_cpp(L, 1, "y_edges"), l_z_edges = get_edges_cpp(L, 1, "z_edges");
	std::vector<double> r_x_edges = get_edges_cpp(L, 2, "x_edges"), r_y_edges = get_edges_cpp(L, 2, "y_edges"), r_z_edges = get_edges_cpp(L, 2, "z_edges");
	std::vector<u32> l_solids = get_solids_cpp(L, 1), r_solids = get_solids_cpp(L, 2);

	auto merge_edges = [](const std::vector<double>& l, const std::vector<double>& r) {
		std::vector<double> res; std::set<double> s(l.begin(), l.end()); s.insert(r.begin(), r.end());
		res.assign(s.begin(), s.end()); return res;
	};
	std::vector<double> x_edges = merge_edges(l_x_edges, r_x_edges);
	std::vector<double> y_edges = merge_edges(l_y_edges, r_y_edges);
	std::vector<double> z_edges = merge_edges(l_z_edges, r_z_edges);

	int x_size = x_edges.size(), y_size = y_edges.size(), z_size = z_edges.size();
	if (x_size > 1023 || y_size > 1023 || z_size > 1023) return 0;

	int max_n = std::max({x_size, y_size, z_size});
	int b_disp = -1;
	for (int i = 1; i <= 31; i++) { if ((1 << i) >= max_n) { b_disp = i; break; } }
	int index_max = (x_size << (b_disp + b_disp)) + (y_size << b_disp) + z_size;
	int b_size = (index_max + 32 - 1) / 32;

	std::vector<u32> solids(b_size, 0);
	int lx_max = l_x_size + 1, rx_max = r_x_size + 1;
	int lx = 1, rx = 1, x = 0;

	while (lx != lx_max || rx != rx_max) {
		int lx_old = lx, rx_old = rx;
		int ly_max = l_y_edges.size() + 1, ry_max = r_y_edges.size() + 1;
		int ly = 1, ry = 1;
		int lx_dom = 0, rx_dom = 0;
		get_dominating_values_cpp(lx, rx, l_x_edges, r_x_edges, lx_max, rx_max, lx_dom, rx_dom);
		int y = 0;

		while (ly != ly_max || ry != ry_max) {
			int ly_old = ly, ry_old = ry;
			int lz_max = l_z_edges.size() + 1, rz_max = r_z_edges.size() + 1;
			int lz = 1, rz = 1;
			int ly_dom = 0, ry_dom = 0;
			get_dominating_values_cpp(ly, ry, l_y_edges, r_y_edges, ly_max, ry_max, ly_dom, ry_dom);
			int z = 0;

			while (lz != lz_max || rz != rz_max) {
				int lz_old = lz, rz_old = rz;
				int lz_dom = 0, rz_dom = 0;
				get_dominating_values_cpp(lz, rz, l_z_edges, r_z_edges, lz_max, rz_max, lz_dom, rz_dom);

				bool l_on = lx_dom && ly_dom && lz_dom && is_occupied_p_cpp(l_solids, l_b_disp, lx_dom - 1, ly_dom - 1, lz_dom - 1);
				bool r_on = rx_dom && ry_dom && rz_dom && is_occupied_p_cpp(r_solids, r_b_disp, rx_dom - 1, ry_dom - 1, rz_dom - 1);

				bool op_res = false;
				if (op_type == 1)      op_res = (l_on || r_on);
				else if (op_type == 2) op_res = (l_on && r_on);
				else if (op_type == 3) op_res = (l_on && !r_on);
				else if (op_type == 4) op_res = (l_on != r_on);
				else if (op_type == 5) op_res = (r_on && !l_on);

				if (op_res) { mark_occupied_cpp(solids, b_disp, x, y, z); }
				if (lz != lz_max && (rz_old == rz_max || l_z_edges[lz_old - 1] <= r_z_edges[rz_old - 1])) lz++;
				if (rz != rz_max && (lz_old == lz_max || r_z_edges[rz_old - 1] <= l_z_edges[lz_old - 1])) rz++;
				z++;
			}
			if (ly != ly_max && (ry_old == ry_max || l_y_edges[ly_old - 1] <= r_y_edges[ry_old - 1])) ly++;
			if (ry != ry_max && (ly_old == ly_max || r_y_edges[ry_old - 1] <= l_y_edges[ly_old - 1])) ry++;
			y++;
		}
		if (lx != lx_max && (rx_old == rx_max || l_x_edges[lx_old - 1] <= r_x_edges[rx_old - 1])) lx++;
		if (rx != rx_max && (lx_old == lx_max || r_x_edges[rx_old - 1] <= l_x_edges[lx_old - 1])) rx++;
		x++;
	}

	lua_newtable(L); int rgn_idx = lua_gettop(L);
	lua_newtable(L); int solids_idx = lua_gettop(L);
	for (int i = 0; i < b_size; i++) { lua_pushinteger(L, solids[i]); lua_rawseti(L, solids_idx, i + 1); }
	lua_setfield(L, rgn_idx, "solids");
	push_edges_cpp(L, rgn_idx, x_edges, "x_edges");
	push_edges_cpp(L, rgn_idx, y_edges, "y_edges");
	push_edges_cpp(L, rgn_idx, z_edges, "z_edges");
	lua_pushinteger(L, x_size); lua_setfield(L, rgn_idx, "x_size");
	lua_pushinteger(L, y_size); lua_setfield(L, rgn_idx, "y_size");
	lua_pushinteger(L, z_size); lua_setfield(L, rgn_idx, "z_size");
	lua_pushinteger(L, b_size); lua_setfield(L, rgn_idx, "b_size");
	lua_pushinteger(L, b_disp); lua_setfield(L, rgn_idx, "b_disp");

	lua_getglobal(L, "mcl_util");
	lua_getfield(L, -1, "region_class"); 
	lua_setmetatable(L, rgn_idx);        
	lua_pop(L, 1);                       

	lua_replace(L, 1); lua_settop(L, 1);
	return 1;
}


// ❌ 3. region_evaluate(l, r, op_type) のC++完全移植
int l_region_evaluate(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_checktype(L, 2, LUA_TTABLE);
	int op_type = luaL_checkinteger(L, 3);

	lua_getfield(L, 1, "x_size"); int l_x_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 2, "x_size"); int r_x_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 1, "y_size"); int l_y_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 2, "y_size"); int r_y_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 1, "z_size"); int l_z_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 2, "z_size"); int r_z_size = lua_tointeger(L, -1); lua_pop(L, 1);

	if (l_x_size == 0 || l_y_size == 0 || l_z_size == 0) {
		bool r_empty = (r_x_size == 0 || r_y_size == 0 || r_z_size == 0);
		if (op_type == 1 || op_type == 4) { lua_pushboolean(L, !r_empty); return 1; }
		lua_pushboolean(L, false); return 1;
	}
	if (r_x_size == 0 || r_y_size == 0 || r_z_size == 0) {
		if (op_type == 1 || op_type == 4 || op_type == 3) { lua_pushboolean(L, true); return 1; }
		lua_pushboolean(L, false); return 1;
	}

	lua_getfield(L, 1, "b_disp"); int l_b_disp = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 2, "b_disp"); int r_b_disp = lua_tointeger(L, -1); lua_pop(L, 1);

	std::vector<double> l_x_edges = get_edges_cpp(L, 1, "x_edges"), l_y_edges = get_edges_cpp(L, 1, "y_edges"), l_z_edges = get_edges_cpp(L, 1, "z_edges");
	std::vector<double> r_x_edges = get_edges_cpp(L, 2, "x_edges"), r_y_edges = get_edges_cpp(L, 2, "y_edges"), r_z_edges = get_edges_cpp(L, 2, "z_edges");
	std::vector<u32> l_solids = get_solids_cpp(L, 1), r_solids = get_solids_cpp(L, 2);

	int lx_max = l_x_size + 1, rx_max = r_x_size + 1;
	int lx = 1, rx = 1;

	while (lx != lx_max || rx != rx_max) {
		int lx_old = lx, rx_old = rx;
		int ly_max = l_y_edges.size() + 1, ry_max = r_y_edges.size() + 1;
		int ly = 1, ry = 1;
		int lx_dom = 0, rx_dom = 0;
		get_dominating_values_cpp(lx, rx, l_x_edges, r_x_edges, lx_max, rx_max, lx_dom, rx_dom);

		while (ly != ly_max || ry != ry_max) {
			int ly_old = ly, ry_old = ry;
			int lz_max = l_z_edges.size() + 1, rz_max = r_z_edges.size() + 1;
			int lz = 1, rz = 1;
			int ly_dom = 0, ry_dom = 0;
			get_dominating_values_cpp(ly, ry, l_y_edges, r_y_edges, ly_max, ry_max, ly_dom, ry_dom);

			while (lz != lz_max || rz != rz_max) {
				int lz_old = lz, rz_old = rz;
				int lz_dom = 0, rz_dom = 0;
				get_dominating_values_cpp(lz, rz, l_z_edges, r_z_edges, lz_max, rz_max, lz_dom, rz_dom);

				bool l_on = lx_dom && ly_dom && lz_dom && is_occupied_p_cpp(l_solids, l_b_disp, lx_dom - 1, ly_dom - 1, lz_dom - 1);
				bool r_on = rx_dom && ry_dom && rz_dom && is_occupied_p_cpp(r_solids, r_b_disp, rx_dom - 1, ry_dom - 1, rz_dom - 1);

				bool op_res = false;
				if (op_type == 1)      op_res = (l_on || r_on);
				else if (op_type == 2) op_res = (l_on && r_on);
				else if (op_type == 3) op_res = (l_on && !r_on);
				else if (op_type == 4) op_res = (l_on != r_on);
				else if (op_type == 5) op_res = (r_on && !l_on);

				if (op_res) { lua_pushboolean(L, true); return 1; }
				if (lz != lz_max && (rz_old == rz_max || l_z_edges[lz_old - 1] <= r_z_edges[rz_old - 1])) lz++;
				if (rz != rz_max && (lz_old == lz_max || r_z_edges[rz_old - 1] <= l_z_edges[lz_old - 1])) rz++;
			}
			if (ly != ly_max && (ry_old == ry_max || l_y_edges[ly_old - 1] <= r_y_edges[ry_old - 1])) ly++;
			if (ry != ry_max && (ly_old == ly_max || r_y_edges[ry_old - 1] <= l_y_edges[ly_old - 1])) ry++;
		}
		if (lx != lx_max && (rx_old == rx_max || l_x_edges[lx_old - 1] <= r_x_edges[rx_old - 1])) lx++;
		if (rx != rx_max && (lx_old == lx_max || r_x_edges[rx_old - 1] <= l_x_edges[lx_old - 1])) rx++;
	}
	lua_pushboolean(L, false);
	return 1;
}

// ❌ 4. any_occupied_p(region) のC++完全移植
int l_any_occupied_p(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_getfield(L, 1, "x_size"); int x_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 1, "y_size"); int y_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 1, "z_size"); int z_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 1, "b_disp"); int b_disp = lua_tointeger(L, -1); lua_pop(L, 1);

	std::vector<u32> solids = get_solids_cpp(L, 1);
	for (int x = 0; x < x_size - 1; x++) {
		for (int y = 0; y < y_size - 1; y++) {
			for (int z = 0; z < z_size - 1; z++) {
				if (is_occupied_p_cpp(solids, b_disp, x, y, z)) { lua_pushboolean(L, true); return 1; }
			}
		}
	}
	lua_pushboolean(L, false);
	return 1;
}

struct PartAABB { int x1, y1, z1, x2, y2, z2; };

// 💡 600行目の find_cuboid をC++最速のポインタレジスタスキャンで完全エミュレート！
static bool find_cuboid_cpp(const std::vector<u32>& solids, int b_disp, const PartAABB& part, int& cx1, int& cy1, int& cz1, int& cx2, int& cy2, int& cz2)
{
	bool identified = false;
	int start_x = 0, start_y = 0, start_z = 0;

	for (int x = part.x1; x < part.x2; x++) {
		for (int y = part.y1; y < part.y2; y++) {
			for (int z = part.z1; z < part.z2; z++) {
				if (is_occupied_p_cpp(solids, b_disp, x, y, z)) {
					identified = true; start_x = x; start_y = y; start_z = z;
					break;
				}
			}
			if (identified) break;
		}
		if (identified) break;
	}

	if (!identified) return false;

	cx1 = start_x; cy1 = start_y; cz1 = start_z;

	int x_end = start_x + 1;
	while (x_end < part.x2) {
		if (!is_occupied_p_cpp(solids, b_disp, x_end, start_y, start_z)) break;
		x_end++;
	}

	int y_end = start_y + 1;
	while (y_end < part.y2) {
		bool done = false;
		for (int x_test = start_x; x_test < x_end; x_test++) {
			if (!is_occupied_p_cpp(solids, b_disp, x_test, y_end, start_z)) { done = true; break; }
		}
		if (done) break;
		y_end++;
	}

	int z_end = start_z + 1;
	while (z_end < part.z2) {
		bool done = false;
		for (int y_test = start_y; y_test < y_end; y_test++) {
			for (int x_test = start_x; x_test < x_end; x_test++) {
				if (!is_occupied_p_cpp(solids, b_disp, x_test, y_test, z_end)) { done = true; break; }
			}
			if (done) break;
		}
		if (done) break;
		z_end++;
	}

	cx2 = x_end; cy2 = y_end; cz2 = z_end;
	return true;
}

// ❌ 5. region_class:volume() のC++完全移植
int l_region_volume(lua_State *L)
{
	int target_idx = stacktrace::find_table_by_method(L, "volume");
	if (target_idx == 0) target_idx = 1;

	lua_getfield(L, target_idx, "x_size"); int x_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, target_idx, "y_size"); int y_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, target_idx, "z_size"); int z_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, target_idx, "b_disp"); int b_disp = lua_tointeger(L, -1); lua_pop(L, 1);

	std::vector<double> x_edges = get_edges_cpp(L, target_idx, "x_edges"), y_edges = get_edges_cpp(L, target_idx, "y_edges"), z_edges = get_edges_cpp(L, target_idx, "z_edges");
	std::vector<u32> solids = get_solids_cpp(L, target_idx);

	double volume = 0.0;
	for (int x = 1; x < x_size; x++) {
		double dx = x_edges[x] - x_edges[x - 1];
		for (int y = 1; y < y_size; y++) {
			double dy = y_edges[y] - y_edges[y - 1];
			for (int z = 1; z < z_size; z++) {
				double dz = z_edges[z] - z_edges[z - 1];
				if (is_occupied_p_cpp(solids, b_disp, x - 1, y - 1, z - 1)) { volume += dx * dy * dz; }
			}
		}
	}
	lua_pushnumber(L, volume);
	return 1;
}

// ❌ 6. region_class:equal_p(other) のC++完全移植（実体消失からの完全大救済筋肉！）
int l_region_equal_p(lua_State *L)
{
	// 👑 コロン表記・引数の位置ズレを stacktrace レーダーで一撃逆探知！
	int self_idx = stacktrace::find_table_by_method(L, "equal_p");
	if (self_idx == 0) return 0;
	int other_idx = (self_idx == 1) ? 2 : 3; // 2番目の引数を特定

	lua_getfield(L, self_idx, "x_size");  int s_x = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, other_idx, "x_size"); int o_x = lua_tointeger(L, -1); lua_pop(L, 1);

	// 泥臭い空っぽの境界条件（Punt if empty）を一瞬で片付ける！
	if (s_x == 0) {
		lua_pushboolean(L, o_x == 0);
		return 1;
	} else if (o_x == 0) {
		lua_pushboolean(L, false);
		return 1;
	}

	// 🚀 【高速パス】： solids 配列のサイズと中身のビットを100% C++最速ストレートダイレクト比較！
	lua_getfield(L, self_idx, "b_size"); int s_b_size = lua_tointeger(L, -1); lua_pop(L, 1);
	std::vector<u32> s_solids = get_solids_from_lua(L, self_idx);
	std::vector<u32> o_solids = get_solids_from_lua(L, other_idx);
	
	if (s_solids.size() == o_solids.size()) {
		bool match = true;
		for (size_t i = 0; i < s_solids.size(); i++) {
			if (s_solids[i] != o_solids[i]) { match = false; break; }
		}
		if (match) { lua_pushboolean(L, true); return 1; }
	}

	// 🚀 【低速パス】： 3重ループによる精細ビットトラバース比較
	lua_getfield(L, self_idx, "b_disp");  int s_b_disp = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, other_idx, "b_disp"); int o_b_disp = lua_tointeger(L, -1); lua_pop(L, 1);

	std::vector<double> s_x_edges = get_edges_from_lua(L, self_idx, "x_edges");
	std::vector<double> s_y_edges = get_edges_from_lua(L, self_idx, "y_edges");
	std::vector<double> s_z_edges = get_edges_from_lua(L, self_idx, "z_edges");

	std::vector<double> o_x_edges = get_edges_from_lua(L, other_idx, "x_edges");
	std::vector<double> o_y_edges = get_edges_from_lua(L, other_idx, "y_edges");
	std::vector<double> o_z_edges = get_edges_from_lua(L, other_idx, "z_edges");

	auto merge_edges = [](const std::vector<double>& l, const std::vector<double>& r) {
		std::vector<double> res; std::set<double> s(l.begin(), l.end()); s.insert(r.begin(), r.end());
		res.assign(s.begin(), s.end()); return res;
	};
	std::vector<double> common_x = merge_edges(s_x_edges, o_x_edges);
	std::vector<double> common_y = merge_edges(s_y_edges, o_y_edges);
	std::vector<double> common_z = merge_edges(s_z_edges, o_z_edges);

	for (size_t x = 1; x < common_x.size(); x++) {
		for (size_t y = 1; y < common_y.size(); y++) {
			for (size_t z = 1; z < common_z.size(); z++) {
				bool s_on = is_occupied_p_cpp(s_solids, s_b_disp, x - 1, y - 1, z - 1);
				bool o_on = is_occupied_p_cpp(o_solids, o_b_disp, x - 1, y - 1, z - 1);
				if (s_on != o_on) {
					lua_pushboolean(L, false);
					return 1;
				}
			}
		}
	}

	lua_pushboolean(L, true);
	return 1;
}

// ❌ 7. region_walk(region, fn, data) ➔ 👑 【深層の魔王・3次元空間再結合木エンジンのC++完全移植】
int l_region_walk(lua_State *L)
{
	// 🕵️‍♂️ 【引数検品】1: region(table), 2: fn(function), 3: data(any)
	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	lua_getfield(L, 1, "x_size"); int x_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 1, "y_size"); int y_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 1, "z_size"); int z_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 1, "b_disp"); int b_disp = lua_tointeger(L, -1); lua_pop(L, 1);

	if (x_size == 0 || y_size == 0 || z_size == 0) { lua_pushinteger(L, 0); return 1; }

	auto get_edges = [&](int rgn_idx, const char* name) {
		std::vector<double> res; lua_getfield(L, rgn_idx, name); int len = lua_objlen(L, -1);
		for (int i = 1; i <= len; i++) { lua_rawgeti(L, -1, i); res.push_back(lua_tonumber(L, -1)); lua_pop(L, 1); }
		lua_pop(L, 1); return res;
	};
	std::vector<double> x_edges = get_edges(1, "x_edges"), y_edges = get_edges(1, "y_edges"), z_edges = get_edges(1, "z_edges");
	std::vector<u32> solids = get_solids_from_lua(L, 1);

	// 👑 【メモリゴミ完全消去】：Lua製のキューの代わりに、C++の std::vector スタックで高速処理！
	std::vector<PartAABB> queue;
	queue.push_back({0, 0, 0, x_size, y_size, z_size});

	size_t q_head = 0;
	while (q_head < queue.size()) {
		PartAABB next_val = queue[q_head++];
		int cx1 = 0, cy1 = 0, cz1 = 0, cx2 = 0, cy2 = 0, cz2 = 0;

		if (find_cuboid_cpp(solids, b_disp, next_val, cx1, cy1, cz1, cx2, cy2, cz2)) {
			// 回収した立方体を、実寸の AABB テーブル {x1,y1,z1,x2,y2,z2} へ復元！
			lua_pushvalue(L, 2); // fn をスタックへ積む
			
			lua_newtable(L);
			lua_pushnumber(L, x_edges[cx1]); lua_rawseti(L, -2, 1);
			lua_pushnumber(L, y_edges[cy1]); lua_rawseti(L, -2, 2);
			lua_pushnumber(L, z_edges[cz1]); lua_rawseti(L, -2, 3);
			lua_pushnumber(L, x_edges[cx2]); lua_rawseti(L, -2, 4);
			lua_pushnumber(L, y_edges[cy2]); lua_rawseti(L, -2, 5);
			lua_pushnumber(L, z_edges[cz2]); lua_rawseti(L, -2, 6);

			lua_pushvalue(L, 3); // data
			lua_call(L, 2, 1);   // fn(aabb, data) を実行！
			
			int ret = lua_tointeger(L, -1); lua_pop(L, 1);
			if (ret == 1) { lua_pushinteger(L, 1); return 1; }

			// 👑 【動的空間分割】：余った空間の残骸（6分割パーツ）を、C++側で一瞬でスタックへ積み上げる！
			if (cx1 > next_val.x1) queue.push_back({next_val.x1, next_val.y1, next_val.z1, cx1, next_val.y2, next_val.z2});
			if (cx2 < next_val.x2) queue.push_back({cx2, next_val.y1, next_val.z1, next_val.x2, next_val.y2, next_val.z2});
			if (cy1 > next_val.y1) queue.push_back({cx1, next_val.y1, next_val.z1, cx2, cy1, next_val.z2});
			if (cy2 < next_val.y2) queue.push_back({cx1, cy2, next_val.z1, cx2, next_val.y2, next_val.z2});
			if (cz1 > next_val.z1) queue.push_back({cx1, cy1, next_val.z1, cx2, cy2, cz1});
		}
	}

	lua_pushinteger(L, 0);
	return 1;
}

// 💡 816行目の edge_redundant_p をC++最速レジスタループで完全エミュレート！
static bool edge_redundant_p_cpp(const std::vector<u32>& solids, int b_disp, int x_pos, int other_0_end, int other_1_end, int axis_mode)
{
	for (int y_pos = 1; y_pos <= other_0_end; y_pos++) {
		for (int z_pos = 1; z_pos <= other_1_end; z_pos++) {
			bool prev_state = false;
			bool state = false;

			// axis_mode: 1=X軸検証, 2=Y軸検証, 3=Z軸検証
			if (axis_mode == 1) {
				state = is_occupied_p_cpp(solids, b_disp, x_pos - 1, y_pos - 1, z_pos - 1);
				if (x_pos > 1) prev_state = is_occupied_p_cpp(solids, b_disp, x_pos - 2, y_pos - 1, z_pos - 1);
			} else if (axis_mode == 2) {
				state = is_occupied_p_cpp(solids, b_disp, y_pos - 1, x_pos - 1, z_pos - 1);
				if (x_pos > 1) prev_state = is_occupied_p_cpp(solids, b_disp, y_pos - 1, x_pos - 2, z_pos - 1);
			} else {
				state = is_occupied_p_cpp(solids, b_disp, y_pos - 1, z_pos - 1, x_pos - 1);
				if (x_pos > 1) prev_state = is_occupied_p_cpp(solids, b_disp, y_pos - 1, z_pos - 1, x_pos - 2);
			}

			if (prev_state != state) return false;
		}
	}
	return true;
}

// ❌ 6. region_class:simplify() のC++完全移植（3次元トポロジー平滑化エンジンのC++完全乗っ取り）
int l_region_simplify(lua_State *L)
{
	// 👑 コロン表記の引数ズレを stacktrace レーダーで一撃逆探知！
	int target_idx = stacktrace::find_table_by_method(L, "simplify");
	if (target_idx == 0) target_idx = 1;

	lua_getfield(L, target_idx, "x_size"); int x_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, target_idx, "y_size"); int y_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, target_idx, "z_size"); int z_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, target_idx, "b_disp"); int rgn_b_disp = lua_tointeger(L, -1); lua_pop(L, 1);

	auto get_edges = [&](int rgn_idx, const char* name) {
		std::vector<double> res; lua_getfield(L, rgn_idx, name); int len = lua_objlen(L, -1);
		for (int i = 1; i <= len; i++) { lua_rawgeti(L, -1, i); res.push_back(lua_tonumber(L, -1)); lua_pop(L, 1); }
		lua_pop(L, 1); return res;
	};
	std::vector<double> r_x_edges = get_edges(target_idx, "x_edges"), r_y_edges = get_edges(target_idx, "y_edges"), r_z_edges = get_edges(target_idx, "z_edges");
	std::vector<u32> r_solids = get_solids_from_lua(L, target_idx);

	std::vector<double> x_edges, y_edges, z_edges;

	// 👑 X, Y, Z の境界エッジ間引きスキャンをC++の高速レジスタ演算で1パス処理！
	for (int x = 1; x <= x_size; x++) { if (!edge_redundant_p_cpp(r_solids, rgn_b_disp, x, y_size, z_size, 1)) x_edges.push_back(r_x_edges[x - 1]); }
	for (int y = 1; y <= y_size; y++) { if (!edge_redundant_p_cpp(r_solids, rgn_b_disp, y, x_size, z_size, 2)) y_edges.push_back(r_y_edges[y - 1]); }
	for (int z = 1; z <= z_size; z++) { if (!edge_redundant_p_cpp(r_solids, rgn_b_disp, z, x_size, y_size, 3)) z_edges.push_back(r_z_edges[z - 1]); }

	int new_x_sz = x_edges.size(), new_y_sz = y_edges.size(), new_z_sz = z_edges.size();

	int max_n = std::max({new_x_sz, new_y_sz, new_z_sz});
	int disp = -1;
	for (int i = 1; i <= 31; i++) { if ((1 << i) >= max_n) { disp = i; break; } }
	int index_max = (new_x_sz << (disp + disp)) + (new_y_sz << disp) + new_z_sz;
	int b_size = (index_max + 32 - 1) / 32;

	std::vector<u32> solids(b_size, 0);

	// 👑 【3重ループ内・3連二分探索の完全破壊】：C++の std::lower_bound で一瞬でビット再マッピング！
	for (size_t x = 0; x < x_edges.size(); x++) {
		int src_x = bisect_cpp(r_x_edges, x_edges[x]);
		for (size_t y = 0; y < y_edges.size(); y++) {
			int src_y = bisect_cpp(r_y_edges, y_edges[y]);
			for (size_t z = 0; z < z_edges.size(); z++) {
				int src_z = bisect_cpp(r_z_edges, z_edges[z]);

				if (is_occupied_p_cpp(r_solids, rgn_b_disp, src_x - 1, src_y - 1, src_z - 1)) {
					mark_occupied_cpp(solids, disp, x, y, z);
				}
			}
		}
	}

	// 焼き上がったクリーンな新しい Region テーブルを Lua へ一括詰め戻しデプロイ！
	lua_newtable(L); int rgn_idx = lua_gettop(L);
	lua_newtable(L); int solids_idx = lua_gettop(L);
	for (int i = 0; i < b_size; i++) { lua_pushinteger(L, solids[i]); lua_rawseti(L, solids_idx, i + 1); }
	lua_setfield(L, rgn_idx, "solids");

	auto push_edges = [&](const std::vector<double>& src, const char* name) {
		lua_newtable(L); int e_idx = lua_gettop(L);
		for (size_t i = 0; i < src.size(); i++) { lua_pushnumber(L, src[i]); lua_rawseti(L, e_idx, i + 1); }
		lua_setfield(L, rgn_idx, name);
	};
	push_edges(x_edges, "x_edges"); push_edges(y_edges, "y_edges"); push_edges(z_edges, "z_edges");

	lua_pushinteger(L, new_x_sz); lua_setfield(L, rgn_idx, "x_size");
	lua_pushinteger(L, new_y_sz); lua_setfield(L, rgn_idx, "y_size");
	lua_pushinteger(L, new_z_sz); lua_setfield(L, rgn_idx, "z_size");
	lua_pushinteger(L, b_size); lua_setfield(L, rgn_idx, "b_size");
	lua_pushinteger(L, disp); lua_setfield(L, rgn_idx, "b_disp");

	lua_getglobal(L, "core"); // メタテーブル結合用にガワを合わせる
	lua_replace(L, 1); lua_settop(L, 1);
	return 1;
}

// ❌ 7. region_class:select_face(normal_axis, pos) ➔ 👑 【大トリ・3D断面切り出しエンジンのC++完全移植】
int l_region_select_face(lua_State *L)
{
	int target_idx = stacktrace::find_table_by_method(L, "select_face");
	if (target_idx == 0) target_idx = 1;

	std::string normal_axis = luaL_checkstring(L, target_idx + 1);
	double pos = luaL_checknumber(L, target_idx + 2);

	lua_getfield(L, target_idx, "x_size"); int x_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, target_idx, "y_size"); int y_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, target_idx, "z_size"); int z_size = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, target_idx, "b_disp"); int rgn_b_disp = lua_tointeger(L, -1); lua_pop(L, 1);

	// 👑 お直し1： 既存のget_edges_from_luaヘルパーを使って、エッジ配列（材料）を高速回収
	std::vector<double> r_x_edges = get_edges_from_lua(L, target_idx, "x_edges");
	std::vector<double> r_y_edges = get_edges_from_lua(L, target_idx, "y_edges");
	std::vector<double> r_z_edges = get_edges_from_lua(L, target_idx, "z_edges");
	
	// 👑 お直し2： 共通インフラ名（get_solids_from_lua）へと正しく置換完了！
	std::vector<u32> r_solids = get_solids_from_lua(L, target_idx);

	std::vector<double> m_edges, a_edges, b_edges;
	int m_size = 0, a_size = 0, b_size_val = 0; // 💡 重複を避けるため b_size_val へ改名！
	int axis_mode = 1;

	if (normal_axis == "x") {
		m_edges = r_x_edges; m_size = x_size; a_edges = r_y_edges; a_size = y_size; b_edges = r_z_edges; b_size_val = z_size; axis_mode = 1;
	} else if (normal_axis == "y") {
		m_edges = r_y_edges; m_size = y_size; a_edges = r_x_edges; a_size = x_size; b_edges = r_z_edges; b_size_val = z_size; axis_mode = 2;
	} else {
		m_edges = r_z_edges; m_size = z_size; a_edges = r_x_edges; a_size = x_size; b_edges = r_y_edges; b_size_val = y_size; axis_mode = 3;
	}

	int basis = bisect_cpp(m_edges, pos);
	if (basis == 0 || basis > m_size) {
		lua_getglobal(L, "mcl_util"); lua_getfield(L, -1, "empty_region"); lua_replace(L, 1); lua_settop(L, 1);
		return 1;
	}

	int basis_other = (m_edges[basis - 1] == pos && basis != 1) ? basis - 1 : basis;

	std::vector<double> out_x_edges, out_y_edges, out_z_edges;
	double min_p = std::min(pos, -pos), max_p = std::max(pos, -pos);

	if (axis_mode == 1) {
		out_x_edges = {min_p, max_p}; out_y_edges = a_edges; out_z_edges = b_edges;
	} else if (axis_mode == 2) {
		out_y_edges = {min_p, max_p}; out_x_edges = a_edges; out_z_edges = b_edges;
	} else {
		out_z_edges = {min_p, max_p}; out_x_edges = a_edges; out_y_edges = b_edges;
	}

	int n_x_sz = out_x_edges.size(), n_y_sz = out_y_edges.size(), n_z_sz = out_z_edges.size();
	int max_n = std::max({n_x_sz, n_y_sz, n_z_sz});
	int disp = -1;
	for (int i = 1; i <= 31; i++) { if ((1 << i) >= max_n) { disp = i; break; } }
	int index_max = (n_x_sz << (disp + disp)) + (n_y_sz << disp) + n_z_sz;
	
	// 👑 お直し3： 新しい断面ビットセットのサイズ（二二重定義エラーの完全駆逐！）
	int n_b_size = (index_max + 32 - 1) / 32; 

	std::vector<u32> solids(n_b_size, 0);

	// 2重ループによる断面ビットスタンプ
	for (int b1 = 0; b1 < b_size_val; b1++) { // 💡 改名した b_size_val を使用！
		for (int a1 = 0; a1 < a_size; a1++) {
			bool on1 = false, on2 = false;
			if (axis_mode == 1) {
				on1 = is_occupied_p_cpp(r_solids, rgn_b_disp, basis - 1, a1, b1);
				on2 = is_occupied_p_cpp(r_solids, rgn_b_disp, basis_other - 1, a1, b1);
			} else if (axis_mode == 2) {
				on1 = is_occupied_p_cpp(r_solids, rgn_b_disp, a1, basis - 1, b1);
				on2 = is_occupied_p_cpp(r_solids, rgn_b_disp, a1, basis_other - 1, b1);
			} else {
				on1 = is_occupied_p_cpp(r_solids, rgn_b_disp, a1, b1, basis - 1);
				on2 = is_occupied_p_cpp(r_solids, rgn_b_disp, a1, b1, basis_other - 1);
			}

			if (on1 || on2) {
				if (axis_mode == 1)      mark_occupied_cpp(solids, disp, 0, a1, b1);
				else if (axis_mode == 2) mark_occupied_cpp(solids, disp, a1, 0, b1);
				else                     mark_occupied_cpp(solids, disp, a1, b1, 0);
			}
		}
	}

	lua_newtable(L); int rgn_idx = lua_gettop(L);
	lua_newtable(L); int solids_idx = lua_gettop(L);
	for (int i = 0; i < n_b_size; i++) { lua_pushinteger(L, solids[i]); lua_rawseti(L, solids_idx, i + 1); }
	lua_setfield(L, rgn_idx, "solids");

	auto push_edges = [&](const std::vector<double>& src, const char* name) {
		lua_newtable(L); int e_idx = lua_gettop(L);
		for (size_t i = 0; i < src.size(); i++) { lua_pushnumber(L, src[i]); lua_rawseti(L, e_idx, i + 1); }
		lua_setfield(L, rgn_idx, name);
	};
	push_edges(out_x_edges, "x_edges"); push_edges(out_y_edges, "y_edges"); push_edges(out_z_edges, "z_edges");

	lua_pushinteger(L, n_x_sz); lua_setfield(L, rgn_idx, "x_size");
	lua_pushinteger(L, n_y_sz); lua_setfield(L, rgn_idx, "y_size");
	lua_pushinteger(L, n_z_sz); lua_setfield(L, rgn_idx, "z_size");
	lua_pushinteger(L, n_b_size); lua_setfield(L, rgn_idx, "b_size");
	lua_pushinteger(L, disp); lua_setfield(L, rgn_idx, "b_disp");

	lua_getglobal(L, "mcl_util");
	lua_getfield(L, -1, "region_class"); 
	lua_setmetatable(L, rgn_idx);        
	lua_pop(L, 1);  

	lua_getglobal(L, "core"); lua_replace(L, 1); lua_settop(L, 1);
	return 1;
}

} // namespace util_shape
