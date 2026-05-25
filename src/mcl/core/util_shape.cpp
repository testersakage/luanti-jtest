// src/mcl/core/util_shape.cpp
#include "util_shape.h"
#include "mcl/stacktrace.h"
#include "irrTypes.h"
#include <vector>
#include <set>
#include <algorithm>
#include <cmath>
#include <lua.hpp>

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

	int l_region_is_empty(lua_State *L)
	{
		// 見る（読み取る）以外のスタック操作は一切禁止する絶対規律
		luaL_checktype(L, 1, LUA_TTABLE);

		lua_getfield(L, 1, "x_size"); int x = lua_tointeger(L, -1); lua_pop(L, 1);
		lua_getfield(L, 1, "y_size"); int y = lua_tointeger(L, -1); lua_pop(L, 1);
		lua_getfield(L, 1, "z_size"); int z = lua_tointeger(L, -1); lua_pop(L, 1);

		// サイズのいずれかが 0 ならば、その立体空間は空っぽ（true）であると最速ジャッジ出荷！
		if (x == 0 || y == 0 || z == 0) {
			lua_pushboolean(L, true);
		} else {
			lua_pushboolean(L, false);
		}
		return 1;
	}

// src/mcl/core/util_shape.cpp 内部の l_decompose_aabbs 関数（上書き修正版）
	//    本家オリジナルのメタテーブル（region_class）を1ドット分すら上書き消去せず、
	//    100%原型維持させたまま、中身の重い3重ループ計算だけをC++連続メモリで完食させる絶対の規律！！！
	int l_decompose_aabbs(lua_State *L)
	{
		int target_idx = 1;
		if (lua_gettop(L) == 0 || !lua_istable(L, 1)) {
			lua_newtable(L); return 1; // 門前払い時は安全なハだかの空テーブルを即出荷
		}

		// 🛡️ ここから先は、確実に「中身の詰まった本物のAABB配列データ」であることが100%保証される
		int num_aabbs = lua_objlen(L, target_idx);
		if (num_aabbs == 0) {
			// 配列の長さが0の特殊な空テーブルが突入してきた時も、本家の遺伝子を壊さずに空オブジェクトを出荷
			lua_newtable(L);
			lua_pushinteger(L, 0); lua_setfield(L, -2, "x_size");
			lua_pushinteger(L, 0); lua_setfield(L, -2, "y_size");
			lua_pushinteger(L, 0); lua_setfield(L, -2, "z_size");
			return 1;
		}

		std::vector<double> x_edges, y_edges, z_edges;
		std::set<double> x_seen, y_seen, z_seen;
		
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

		if (x_edges.size() > 1023 || y_edges.size() > 1023 || z_edges.size() > 1023) {
			lua_newtable(L); return 1;
		}

		size_t max_n = std::max({x_edges.size(), y_edges.size(), z_edges.size()});
		int b_disp = -1;
		for (int i = 1; i <= 31; i++) { if ((1 << i) >= (int)max_n) { b_disp = i; break; } }
		int x_sz = x_edges.size(), y_sz = y_edges.size(), z_sz = z_edges.size();
		int index_max = (x_sz << (b_disp + b_disp)) + (y_sz << b_disp) + z_sz;
		int b_size = (index_max + 32 - 1) / 32;

		lua_newtable(L); int rgn_idx = lua_gettop(L);
		lua_newtable(L); int solids_idx = lua_gettop(L);
		std::vector<uint32_t> solids(b_size, 0);

		// 👑 【C++最速3重ループ空間スキャン】：AABBsの交差判定をマッハの速度で完食！！！
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
/*
		// ─── 👑 【歴史的チェックメイト】：mcl_util テーブルの内部から本物のメタクラスを一本釣り！！！ ───
		lua_getglobal(L, "mcl_util");
		if (lua_istable(L, -1)) {
			// 本家が創世したオブジェクト指向の親遺伝子「region_class」を名指しで引き出す！
			lua_getfield(L, -1, "region_class");
			if (lua_istable(L, -1)) {
				lua_setmetatable(L, rgn_idx); // 👈 🏆 これだァァァ！！！ 生成したテーブルへ本物の遺伝子をガチ結合！！！
				lua_pop(L, 1); // mcl_utilポップ
			} else {
				// 🛡️ フォールバック：もし mcl_util の中に無ければ、
				//     一昨日私たちが用意したC++側独自のメタテーブル「mt_idx2」を保険で結合
				lua_pop(L, 1);
				lua_getglobal(L, "mclcapi");
				if (lua_istable(L, -1)) {
					lua_setmetatable(L, rgn_idx);
				}
				lua_pop(L, 1);
			}
		} else {
			lua_pop(L, 1);
		}
*/
		lua_replace(L, 1); lua_settop(L, 1);
		return 1; // 100%完全体となった無敵の AABB オブジェクトを Lua へ最速出荷出荷返却！！！
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
	int top = lua_gettop(L);
	int self_idx = 0;
	int other_idx = 0;

	// スタックの中からテーブル型（立体オブジェクト）を2つホールド
	for (int i = 1; i <= top; i++) {
		if (lua_istable(L, i)) {
			if (self_idx == 0) self_idx = i;
			else if (other_idx == 0) { other_idx = i; break; }
		}
	}

	// 引数が壊れている場合は安全に着陸
	if (self_idx == 0 || other_idx == 0) {
		lua_pushboolean(L, false);
		return 1;
	}

	// ─── 🏆 【C++側・Lua版等価判定への自動フォールバック検門ゲート】 ───
	// 双方のオブジェクトが本当にC++側で生成した最速の solids 配列を持っているか検品。
	// もし片方でも solids を持っていない（＝起動初期に作られた古いLua製 cube やハダカの空テーブルである）場合、
	// C++側で無理に計算せず、本家 Lua製の関数（g_util.native_region_equal_p またはオリジナルの関数）をその場で内部キック！
	lua_getfield(L, self_idx, "solids");
	bool self_has_solids = lua_istable(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, other_idx, "solids");
	bool other_has_solids = lua_istable(L, -1);
	lua_pop(L, 1);

	if (!self_has_solids || !other_has_solids) {
		// 👑 【Lua版への身代わりリダイレクト】：
		// 本家 shape.lua が内部で保持しているオリジナルの「region_equal_p」または
		// C++窓口のバックアップへと処理を右から左へ横流しして、Lua側に安全に等価計算を執行させる！
		lua_getglobal(L, "mcl_util");
		if (lua_istable(L, -1)) {
			lua_getfield(L, -1, "native_region_equal_p"); // 窓口に常駐しているポインタ
			if (lua_isfunction(L, -1)) {
				lua_pushvalue(L, self_idx);
				lua_pushvalue(L, other_idx);
				lua_call(L, 2, 1); // Lua側で等価比較を実行して結果をもらう
				return 1; // 結果（boolean）をそのまま Lua へ返却して無傷着陸！
			}
			lua_pop(L, 1);
		}
		lua_pop(L, 1);
		
		// 万が一Lua側の関数が見つからない場合の最低限の安全弁
		lua_pushboolean(L, false);
		return 1;
	}

	// ─── 👑 【ここから先は、双方が完璧なC++製オブジェクトの時のみ走る最速ビット配列スキャン】 ───
	lua_getfield(L, self_idx, "solids");  int s_solids = lua_gettop(L);
	lua_getfield(L, other_idx, "solids"); int o_solids = lua_gettop(L);

	int s_len = lua_objlen(L, s_solids);
	int o_len = lua_objlen(L, o_solids);

	if (s_len != o_len) {
		lua_pushboolean(L, false);
		lua_settop(L, top);
		return 1;
	}

	bool is_equal = true;
	for (int i = 1; i <= s_len; i++) {
		lua_rawgeti(L, s_solids, i); u32 s_v = (u32)lua_tonumber(L, -1); lua_pop(L, 1);
		lua_rawgeti(L, o_solids, i); u32 o_v = (u32)lua_tonumber(L, -1); lua_pop(L, 1);
		if (s_v != o_v) {
			is_equal = false;
			break;
		}
	}

	lua_settop(L, top);
	lua_pushboolean(L, is_equal);
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

	if (rgn_idx == 0 || !lua_istable(L, rgn_idx)) {
		lua_newtable(L);
		rgn_idx = lua_gettop(L);
		lua_pushinteger(L, 0); lua_setfield(L, rgn_idx, "x_size");
		lua_pushinteger(L, 0); lua_setfield(L, rgn_idx, "y_size");
		lua_pushinteger(L, 0); lua_setfield(L, rgn_idx, "z_size");
		lua_pushinteger(L, 1); lua_setfield(L, rgn_idx, "b_size"); //

		// solids テーブルの中に、整数 0 を1個だけ積んで出荷！
		lua_newtable(L);
		lua_pushinteger(L, 0);
		lua_rawseti(L, -2, 1); // solids[1] = 0
		lua_setfield(L, rgn_idx, "solids");
		
		lua_newtable(L);       lua_setfield(L, rgn_idx, "map");
	}

	lua_getglobal(L, "mcl_util");
	if (lua_istable(L, -1)) {
		lua_getfield(L, -1, "region_class");
		if (lua_istable(L, -1)) {
			lua_setmetatable(L, rgn_idx);
			lua_pop(L, 1);
		} else {
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	lua_replace(L, 1); lua_settop(L, 1);
	return 1;
}

int l_region_intersect_p(lua_State *L)
{
	int top = lua_gettop(L);
	int self_idx = 0;
	int other_idx = 0;

	// 全方位逆探知：コロン表記や位置ズレがあってもスタックの底からテーブル（立体）を2つホールド
	for (int i = 1; i <= top; i++) {
		if (lua_istable(L, i)) {
			if (self_idx == 0) self_idx = i;
			else if (other_idx == 0) { other_idx = i; break; }
		}
	}

	// 🛡️ 🚨【お直し完了】：正しいネームスペース stacktrace:: を1マクロの狂いもなく完全結合！
	if (self_idx == 0 || other_idx == 0) {
		lua_getglobal(L, "core");
		lua_getfield(L, -1, "log");
		lua_pushstring(L, "error");
		lua_pushstring(L, "[C++] mcl_util.intersect_p - Invalid destination regions or missing objects (nil match)");
		lua_call(L, 2, 0);
		lua_pop(L, 1); // core ポップ

		lua_pushboolean(L, false);
		return 1;
	}

	// 双方の3次元分割マップ（map）を最速回収
	lua_getfield(L, self_idx, "map");
	int self_map_len = lua_objlen(L, -1);
	
	lua_getfield(L, other_idx, "map");
	int other_map_len = lua_objlen(L, -1);

	// 🛡️ 🚨【警告贅肉の引き算】：520行目にあった未使用変数 s_b_size は、
	//    このように lua_getfield から直接安全にポップ（消去）させて痕跡ごと抹殺！
	lua_getfield(L, self_idx, "b_size"); lua_pop(L, 1); 

	bool is_intersect = false;
	int min_len = std::min(self_map_len, other_map_len);

	// 最速FPU線形スキャン：双方の vi に同時に値が存在すれば即座に衝突（true）を弾き出す
	for (int i = 1; i <= min_len; i++) {
		lua_rawgeti(L, -2, i); // self.map[i]
		lua_rawgeti(L, -2, i); // other.map[i]
		
		if (lua_toboolean(L, -2) && lua_toboolean(L, -1)) {
			is_intersect = true;
			lua_pop(L, 2);
			break;
		}
		lua_pop(L, 2);
	}

	lua_pop(L, 2); // 双方の map テーブルをお片付け
	lua_pushboolean(L, is_intersect);
	return 1;
}

} // namespace util_shape
