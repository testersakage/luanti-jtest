// src/mcl/core/explosions.cpp （★第9章 EXPLOSIONS・ネイティブ幾何学筋肉・製品版確定全文）

#include "explosions.h"
#include <lua.hpp>
#include <vector>
#include <set>
#include <algorithm>
#include <cmath>

namespace explosions {

	// 🛡️ 本家ハッシュ関数（core.hash_node_position）の数理をC++側で完全対称（Symmetry）に再現
	inline int64_t hash_node_position_cpp(int x, int y, int z) {
		return (int64_t)(x & 0xFFFF) | ((int64_t)(y & 0xFFFF) << 16) | ((int64_t)(z & 0xFFFF) << 32);
	}

	// 👑 1. 【native_compute_sphere_rays】：球形レイキャストベクトルの超高速精選生成
	//    本家Lua版のブルートフォース3重ループとテーブル多重捏造テロを、C++連続メモリで全消去！
	int l_native_compute_sphere_rays(lua_State *L)
	{
		// 1. 引数をインデックス直撃一本釣り回収（贅肉ループの完全引き算）
		double radius = luaL_checknumber(L, 1);
		if (radius <= 0.0) {
			lua_newtable(L);
			return 1;
		}

		int r = static_cast<int>(std::ceil(radius));
		std::vector<int64_t> seen_hashes;
		std::vector<std::vector<double>> sphere_rays;

		auto add_ray_cpp = [&](int x, int y, int z) {
			int64_t h = hash_node_position_cpp(x, y, z);
			if (std::find(seen_hashes.begin(), seen_hashes.end(), h) == seen_hashes.end()) {
				seen_hashes.push_back(h);
				double d = x * x + y * y + z * z;
				if (d > 0.0) {
					double len = std::sqrt(d);
					sphere_rays.push_back({x / len, y / len, z / len});
				}
			}
		};

		// 🎯 【完全対称数理】：本家Lua版（1行の横ズレも無い）の3連続対称走査アルゴリズムを完全再現！
		for (int y = -r; y <= r; y++) {
			for (int z = -r; z <= r; z++) {
				for (int x = -r; x <= 0; x++) {
					if (x * x + y * y + z * z <= radius * radius) {
						add_ray_cpp(x, y, z);
						add_ray_cpp(-x, y, z);
						break;
					}
				}
			}
		}

		for (int x = -r; x <= r; x++) {
			for (int z = -r; z <= r; z++) {
				for (int y = -r; y <= 0; y++) {
					if (x * x + y * y + z * z <= radius * radius) {
						add_ray_cpp(x, y, z);
						add_ray_cpp(x, -y, z);
						break;
					}
				}
			}
		}

		for (int x = -r; x <= r; x++) {
			for (int y = -r; y <= r; y++) {
				for (int z = -r; z <= 0; z++) {
					if (x * x + y * y + z * z <= radius * radius) {
						add_ray_cpp(x, y, z);
						add_ray_cpp(x, y, -z);
						break;
					}
				}
			}
		}

		// 2. 成果物テーブルを一括して Lua 宇宙へ高速出荷
		lua_newtable(L);
		int idx = 1;
		for (const auto& ray : sphere_rays) {
			lua_newtable(L);
			lua_pushnumber(L, ray[0]); lua_setfield(L, -2, "x");
			lua_pushnumber(L, ray[1]); lua_setfield(L, -2, "y");
			lua_pushnumber(L, ray[2]); lua_setfield(L, -2, "z");
			lua_rawseti(L, -2, idx++);
		}

		return 1;
	}

	// 👑 2. 【native_calculate_impact】：距離と露出（Exposure）から衝撃度（Impact）を計算する高精度FPU算術
	int l_native_calculate_impact(lua_State *L)
	{
		// 1. 引数をインデックスから型検品・直撃一本釣り回収
		double dist = luaL_checknumber(L, 1);
		double punch_radius = luaL_checknumber(L, 2);
		double exposure = luaL_checknumber(L, 3);
		double strength = luaL_checknumber(L, 4);

		// 2. マインクラフトの物理シミュレーション規律をFPUレジスタ内で最速計算
		double impact = (1.0 - dist / punch_radius) * exposure;
		if (impact < 0.0) impact = 0.0;

		double damage = std::floor((impact * impact + impact) * 7.0 * strength + 1.0);

		// 3. 成果物の数値2つ（ダメージ値、インパクト倍率）を出荷
		lua_pushnumber(L, damage);
		lua_pushnumber(L, impact);
		return 2;
	}

} // namespace explosions
