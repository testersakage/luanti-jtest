// src/mcl/core/util_compat.cpp
#include "util_compat.h"
#include "mcl/stacktrace.h" // 👑 あなたの創設した無敵の逆探知インフラを結線！
#include "irrTypes.h"
#include <cmath>
#include <string>
#include <vector>
#include <cstdlib>

namespace util_compat {

// 💡 内部ヘルパー：ObjectRefが有効かどうかをC++の腕力で一瞬で検品
inline bool is_valid_objectref_cpp(lua_State *L, int obj_idx) {
	lua_getfield(L, obj_idx, "is_valid");
	if (lua_isfunction(L, -1)) {
		lua_pushvalue(L, obj_idx); lua_call(L, 1, 1);
		bool res = lua_toboolean(L, -1) != 0; lua_pop(L, 1); return res;
	}
	lua_pop(L, 1);
	lua_getfield(L, obj_idx, "get_pos");
	if (lua_isfunction(L, -1)) {
		lua_pushvalue(L, obj_idx); lua_call(L, 1, 1);
		bool res = !lua_isnil(L, -1); lua_pop(L, 2); return res;
	}
	lua_pop(L, 1); return false;
}

// ❌ #1. vector.random_direction() のC++完全移植（引数が何個溢れても無傷）
int l_vector_random_direction(lua_State *L)
{
	double x, y, z, l2;
	do {
		x = ((double)rand() / RAND_MAX) * 2.0 - 1.0;
		y = ((double)rand() / RAND_MAX) * 2.0 - 1.0;
		z = ((double)rand() / RAND_MAX) * 2.0 - 1.0;
		l2 = x*x + y*y + z*z;
	} while (l2 > 1.0 || l2 < 1e-6);

	double l = std::sqrt(l2);
	lua_newtable(L);
	lua_pushnumber(L, x / l); lua_setfield(L, -2, "x");
	lua_pushnumber(L, y / l); lua_setfield(L, -2, "y");
	lua_pushnumber(L, z / l); lua_setfield(L, -2, "z");
	return 1;
}

// 👑 【最深部】：connected_playersのC++ネイティブ超速イテレーター関数
static int l_connected_players_next(lua_State *L)
{
	lua_pushvalue(L, lua_upvalueindex(1)); // pls table
	int idx = lua_tointeger(L, lua_upvalueindex(2));
	bool has_radius = lua_toboolean(L, lua_upvalueindex(5)) != 0;

	int len = lua_objlen(L, -1);
	while (idx < len) {
		idx++;
		lua_pushinteger(L, idx); lua_replace(L, lua_upvalueindex(2));

		lua_rawgeti(L, -1, idx); // obj
		if (lua_isnil(L, -1)) { lua_pop(L, 1); continue; }

		if (!is_valid_objectref_cpp(L, lua_gettop(L))) { lua_pop(L, 1); continue; }

		if (has_radius) {
			lua_getfield(L, -1, "get_pos"); lua_pushvalue(L, -2); lua_call(L, 1, 1);
			if (lua_istable(L, -1)) {
				// table.x, table.y, table.zの数値を安全にポップ
				lua_getfield(L, -1, "x"); double px = lua_tonumber(L, -1); lua_pop(L, 1);
				lua_getfield(L, -1, "y"); double py = lua_tonumber(L, -1); lua_pop(L, 1);
				lua_getfield(L, -1, "z"); double pz = lua_tonumber(L, -1); lua_pop(L, 1);

				double cx = lua_tonumber(L, lua_upvalueindex(3));
				double cy = lua_tonumber(L, lua_upvalueindex(4));
				double cz = lua_tonumber(L, lua_upvalueindex(6));

				double dx = px - cx, dy = py - cy, dz = pz - cz;
				double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
				double radius = lua_tonumber(L, lua_upvalueindex(7));

				if (dist <= radius) {
					lua_pop(L, 1); // pos table
					lua_pushnumber(L, dist); 
					return 2; 
				}
			}
			lua_pop(L, 1); lua_pop(L, 1); continue;
		}
		return 1; 
	}
	return 0;
}

// ❌ #2. mcl_util.connected_players(center, radius) のC++完全移植（★逆探知シールド搭載型）
int l_connected_players(lua_State *L)
{
	// core.get_connected_players() のベース回収
	lua_getglobal(L, "core"); lua_getfield(L, -1, "get_connected_players"); lua_call(L, 0, 1);
	int pls_table_idx = lua_gettop(L);

	// 👑 【全方位逆探知】：コロン表記のself混入や、どさくさ引数をスタック内から自動スキャン！ [INDEX: 5]
	int center_idx = 0;
	int radius_idx = 0;
	int top = pls_table_idx - 1; // 降ってきた全引数の個数を検品

	for (int i = 1; i <= top; i++) {
		if (lua_istable(L, i)) {
			lua_getfield(L, i, "x");
			if (!lua_isnil(L, -1)) { center_idx = i; lua_pop(L, 1); break; }
			lua_pop(L, 1);
		}
	}
	if (center_idx > 0 && lua_isnumber(L, center_idx + 1)) {
		radius_idx = center_idx + 1;
	}

	bool has_radius = (center_idx > 0);
	double cx = 0, cy = 0, cz = 0, radius = 1.0;

	if (has_radius) {
		lua_getfield(L, center_idx, "x"); cx = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, center_idx, "y"); cy = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, center_idx, "z"); cz = lua_tonumber(L, -1); lua_pop(L, 1);
		if (radius_idx > 0) radius = lua_tonumber(L, radius_idx);
	}

	lua_pushvalue(L, pls_table_idx);
	lua_pushinteger(L, 0); 
	lua_pushnumber(L, cx); lua_pushnumber(L, cy);
	lua_pushboolean(L, has_radius);
	lua_pushnumber(L, cz); lua_pushnumber(L, radius);
	lua_pushcclosure(L, l_connected_players_next, 7); 
	return 1;
}

// ❌ #3. core.get_node_raw(x, y, z) のC++完全移植（★逆探知シールド搭載型）
int l_get_node_raw(lua_State *L)
{
	// 👑 【全方位逆探知】：コロン表記で「self, x, y, z」とズレて降ってきても、
	//    スタックの「一番ケツから3つの数値」を逆算して強制一本釣り！
	int top = lua_gettop(L);
	double x = 0, y = 0, z = 0;

	// 👑 【全方位逆探知】：引数が 3個（ハダカのx,y,z）なら 1, 2, 3 から回収！
	//    コロン表記（self混入）のせいで 4個 にズレていたら、2, 3, 4 から強制一本釣り！
	if (top >= 4) {
		x = lua_tonumber(L, 2);
		y = lua_tonumber(L, 3);
		z = lua_tonumber(L, 4);
	} else if (top >= 3) {
		x = lua_tonumber(L, 1);
		y = lua_tonumber(L, 2);
		z = lua_tonumber(L, 3);
	} else {
		// 引数が足りなければ安全に CONTENT_IGNORE 用のダミーを積んで受け流す防衛線
		lua_pushinteger(L, 0); lua_pushinteger(L, 0); lua_pushinteger(L, 0); lua_pushboolean(L, false);
		return 4;
	}

	lua_getglobal(L, "core"); lua_getfield(L, -1, "get_node");
	lua_newtable(L);
	lua_pushnumber(L, x); lua_setfield(L, -2, "x");
	lua_pushnumber(L, y); lua_setfield(L, -2, "y");
	lua_pushnumber(L, z); lua_setfield(L, -2, "z");
	lua_call(L, 1, 1); 

	lua_getfield(L, -1, "name"); std::string name = lua_tostring(L, -1); lua_pop(L, 1);
	lua_pop(L, 1); 

	lua_getfield(L, -2, "get_content_id"); lua_pushstring(L, name.c_str()); lua_call(L, 1, 1);
	int cid = lua_tointeger(L, -1); lua_pop(L, 1);

	// core.get_node から実際の param2（回転方向・水分量データ）を直接ぶっこ抜いてエミュレート！
	lua_getglobal(L, "core"); lua_getfield(L, -1, "get_node");
	lua_newtable(L);
	lua_pushnumber(L, x); lua_setfield(L, -2, "x");
	lua_pushnumber(L, y); lua_setfield(L, -2, "y");
	lua_pushnumber(L, z); lua_setfield(L, -2, "z");
	lua_call(L, 1, 1); // [-1] node table
	lua_getfield(L, -1, "param2"); int p2 = lua_tointeger(L, -1); lua_pop(L, 2); 

	// 👑 【これが正真正銘、本当の神の配線】：lua_settop(0) というスタック破壊の大贅肉は完全排除！！！
	//    スタックの底に溜まった既存の引数や core の残骸を一切無理にいじらず、ハダカのまま、
	//    現在のスタックの最上階へ、physics.lua（11行目）が待っている正しい順番のまま4つのデータをプッシュする！
	lua_pushinteger(L, cid);               // 1: id (★これが core.get_name_from_content_id へ直撃！)
	lua_pushinteger(L, 0);                 // 2: _  (ゴミ捨て場)
	lua_pushinteger(L, p2);                // 3: param2 (★これが正確な param2 数値になる！)
	lua_pushboolean(L, name != "ignore");  // 4: readable (ブーリアン)

	return 4; // 4つの完成品データを、1マクロのねじれもなく Lua へスピード返却！
}

// ❌ #4. core.time_to_day_night_ratio(tod) のC++完全移植（★逆探知シールド搭載型）
int l_time_to_day_night_ratio(lua_State *L)
{
	// 👑 【全方位逆探知】：何個引数が溢れようが、スタックの一番最後（最新）の数値を tod として自動逆探知！
	int top = lua_gettop(L);
	if (top == 0) { lua_pushnumber(L, 1.0); return 1; }
	
	double tod = lua_tonumber(L, top);
	double t = tod * 24000.0;
	if (t < 0.0)  t += (-std::floor(t / 24000.0)) * 24000.0;
	if (t > 24000.0) t -= (-std::floor(t / 24000.0)) * 24000.0;
	if (t > 12000.0) t = 24000.0 - t;

	if (t <= 4625.0) return lua_pushnumber(L, 175.0 / 1000.0), 1;
	if (t >= 6125.0) return lua_pushnumber(L, 1.0), 1;

	double tod_v[9][2] = {
		{4375.0, 175.0}, {4625.0, 175.0}, {4875.0, 250.0}, {5125.0, 350.0},
		{5375.0, 500.0}, {5625.0, 675.0}, {5875.0, 875.0}, {6125.0, 1000.0}, {6375.0, 1000.0}
	};

	for (int i = 1; i < 9; i++) {
		if (tod_v[i][0] > t) {
			double td0 = tod_v[i][0] - tod_v[i - 1][0];
			double f = (t - tod_v[i - 1][0]) / td0;
			return lua_pushnumber(L, (f * tod_v[i][1] + (1.0 - f) * tod_v[i - 1][1]) / 1000.0), 1;
		}
	}
	return lua_pushnumber(L, 1.0), 1;
}

} // namespace util_compat
