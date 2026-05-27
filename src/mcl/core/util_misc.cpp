// src/mcl/core/util_misc.cpp
#include "util_misc.h"
#include <string>
#include <random>
#include <ctime>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace util_misc {

	// ❌ 1. mcl_util.generate_uuid() のC++最速1パス実装（現状維持）
	int l_generate_uuid(lua_State *L)
	{
		static std::mt19937 gen(std::time(nullptr));
		std::uniform_int_distribution<> dis(0, 15);
		std::uniform_int_distribution<> dis2(8, 11);

		// 🛡️ 【評価順序の安全ガード】：コンパイラによる評価順のねじれを完全に防犯するため、
		//     31個の乱数値をあらかじめ配列にカプセル化（固定）する。
		int v[31];
		for (int i = 0; i < 31; i++) {
			if (i == 15) {
				v[i] = dis2(gen); // 'y' の位置（4ビット制限: 8〜11）
			} else {
				v[i] = dis(gen);  // 'x' の位置（0〜15）
			}
		}

		char uuid[37];
		std::snprintf(uuid, sizeof(uuid), 
			"%x%x%x%x%x%x%x%x-%x%x%x%x-4%x%x%x-%x%x%x%x-%x%x%x%x%x%x%x%x%x%x%x%x",
			v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7],
			v[8], v[9], v[10], v[11],
			v[12], v[13], v[14],
			v[15], v[16], v[17], v[18],
			v[19], v[20], v[21], v[22], v[23], v[24], v[25], v[26], v[27], v[28], v[29], v[30]
		);

		// 🛡️ 【スタック完全同期（Symmetry）】：Luaの string.gsub の2個の戻り値を完全模倣！
		lua_pushstring(L, uuid);     // 第1戻り値：UUID文字列
		lua_pushinteger(L, 31);      // 第2戻り値：置換個数定数（31）をスタックへ追加出荷
	
		return 2; // 2個の戻り値を確定返却
	}

	// ❌ 2. mcl_util.get_nodepos(pos) のC++完全移植（3次元座標の最速整数丸め込み筋肉）
	int l_get_nodepos(lua_State *L)
	{
		luaL_checktype(L, 1, LUA_TTABLE); // 引数1: pos {x, y, z}
	
		// テーブルから x, y, z を一瞬でダイレクト回収
		lua_getfield(L, 1, "x"); double x = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, 1, "y"); double y = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, 1, "z"); double z = lua_tonumber(L, -1); lua_pop(L, 1);

		// C++最速の std::floor(v + 0.5) 四捨五入をレジスタで執行！
		lua_newtable(L); // 戻り値用の新しいテーブル
		lua_pushinteger(L, static_cast<int>(std::floor(x + 0.5))); lua_setfield(L, -2, "x");
		lua_pushinteger(L, static_cast<int>(std::floor(y + 0.5))); lua_setfield(L, -2, "y");
		lua_pushinteger(L, static_cast<int>(std::floor(z + 0.5))); lua_setfield(L, -2, "z");

		return 1; // 丸め込み完了済みの {x,y,z} テーブルを Lua へ返却！
	}

	// ❌ 3. mcl_util.calculate_knockback(velocity, factor, resistance, standing, x, z) のC++完全移植（ノックバック物理演算）
	int l_calculate_knockback(lua_State *L)
	{
		// 🕵️‍♂️ 【引数検品】1: velocity(table), 2: factor(num), 3: resistance(num), 4: standing(bool), 5: x(num), 6: z(num)
		luaL_checktype(L, 1, LUA_TTABLE);
		double factor     = luaL_checknumber(L, 2);
		double resistance = luaL_checknumber(L, 3);
		bool standing     = lua_toboolean(L, 4) != 0;
		double k_x        = luaL_checknumber(L, 5);
		double k_z        = luaL_checknumber(L, 6);

		lua_getfield(L, 1, "x"); double v_x = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, 1, "y"); double v_y = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, 1, "z"); double v_z = lua_tonumber(L, -1); lua_pop(L, 1);

		// 計算ロジック: local factor = factor * (1.0 - math.min (1.0, resistance))
		factor = factor * (1.0 - std::min(1.0, resistance));

		lua_newtable(L); // 戻り値用ベクトルテーブル v

		if (factor <= 1.0e-5) {
			lua_pushnumber(L, 0); lua_setfield(L, -2, "x");
			lua_pushnumber(L, 0); lua_setfield(L, -2, "y");
			lua_pushnumber(L, 0); lua_setfield(L, -2, "z");
			return 1; // vector.zero()
		}

		// vector.normalize(vector.new(x, 0, z)) をC++のFPUで超速エミュレート
		double len = std::sqrt(k_x * k_x + k_z * k_z);
		double n_x = (len > 0.0) ? (k_x / len) : 0.0;
		double n_z = (len > 0.0) ? (k_z / len) : 0.0;

		// v.x = (velocity.x / 2 + (v.x * 20)) * 0.546
		double res_x = ((v_x / 2.0) + ((n_x * factor) * 20.0)) * 0.546;
		double res_z = ((v_z / 2.0) + ((n_z * factor) * 20.0)) * 0.546;

		// v.y = standing and (math.min (0.4 * 20, velocity.y / 2.0 + factor * 10)) or velocity.y
		double res_y = v_y;
		if (standing) {
			res_y = std::min(0.4 * 20.0, (v_y / 2.0) + (factor * 10.0));
		}

		lua_pushnumber(L, res_x); lua_setfield(L, -2, "x");
		lua_pushnumber(L, res_y); lua_setfield(L, -2, "y");
		lua_pushnumber(L, res_z); lua_setfield(L, -2, "z");

		return 1; // 計算完了した吹っ飛びベクトルテーブル v を Lua へ返却！
	}

} // namespace util_misc
