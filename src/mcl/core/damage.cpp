// src/mcl/core/damage.cpp

#include "damage.h"
#include "worlds.h"
#include "mcl/stacktrace.h"
#include <lua.hpp>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <cmath>

namespace damage {

	// 👑 戦闘中・持続ダメージ中は完全無音のメモリテンポラリ空間！
	std::unordered_map<std::string, float> g_player_health;
	std::unordered_map<std::string, bool> g_health_loaded;

	// 🛡️ 内部関数：ModStorageから特定のプレイヤーの浮動小数点HPを安全にロード
	float load_health_from_storage(lua_State *L, const std::string& name)
	{
		float hp = 20.0f; // マイクラのデフォルト最大体力
		lua_getglobal(L, "core");
		if (lua_istable(L, -1)) {
			lua_getfield(L, -1, "get_mod_storage");
			if (lua_isfunction(L, -1)) {
				lua_call(L, 0, 1);
				if (lua_istable(L, -1) || lua_isuserdata(L, -1)) {
					lua_getfield(L, -1, "get_float");
					lua_pushvalue(L, -2); // storage
					std::string key = "mcl_health_" + name;
					lua_pushstring(L, key.c_str());
					lua_call(L, 2, 1);
					if (lua_isnumber(L, -1)) {
						float saved_hp = static_cast<float>(lua_tonumber(L, -1));
						if (saved_hp > 0.0f) hp = saved_hp;
					}
					lua_pop(L, 1);
				}
				lua_pop(L, 1);
			} else { lua_pop(L, 1); }
		}
		lua_pop(L, 1); // coreポップ
		return hp;
	}

	// 🛡️ 内部関数：C++メモリのHPをModStorage（物理ファイル）へ一括強制書き出しセーブ
	void save_health_to_storage(lua_State *L, const std::string& name, float hp)
	{
		lua_getglobal(L, "core");
		if (lua_istable(L, -1)) {
			lua_getfield(L, -1, "get_mod_storage");
			if (lua_isfunction(L, -1)) {
				lua_call(L, 0, 1);
				if (lua_istable(L, -1) || lua_isuserdata(L, -1)) {
					lua_getfield(L, -1, "set_float");
					lua_pushvalue(L, -2); // storage
					std::string key = "mcl_health_" + name;
					lua_pushstring(L, key.c_str());
					lua_pushnumber(L, hp);
					lua_call(L, 3, 0);
				}
				lua_pop(L, 1);
			} else { lua_pop(L, 1); }
		}
		lua_pop(L, 1);
	}

	// 💎 1. native_damage_calculate_modifier （FPU難易度倍率最速スキャン）
	int l_damage_calculate_modifier(lua_State *L)
	{
		int top = lua_gettop(L);
		double damage = 0.0;
		int reason_idx = 0;

		// 見るだけのスタック規律：スタックの中から数値（damage）とテーブル（reason）を一本釣り
		for (int i = 1; i <= top; i++) {
			if (lua_isnumber(L, i)) { damage = lua_tonumber(L, i); }
			else if (lua_istable(L, i)) { reason_idx = i; }
		}

		// mcl_vars.difficulty をC++側から動的回収（0:イージー, 1:ノーマル, 2:ハード, 3:ベリーハード等）
		double difficulty = 2.0; // デフォルトノーマルフォールバック
		lua_getglobal(L, "mcl_vars");
		if (lua_istable(L, -1)) {
			lua_getfield(L, -1, "difficulty");
			if (lua_isnumber(L, -1)) { difficulty = lua_tonumber(L, -1); }
			lua_pop(L, 1);
		}
		lua_pop(L, 1);

		// 本家 Lua版の難易度倍率ロジックをFPUレジスタ内で一瞬で完食
		if (reason_idx != 0) {
			lua_getfield(L, reason_idx, "flags");
			if (lua_istable(L, -1)) {
				lua_getfield(L, -1, "scales");
				bool scales = lua_toboolean(L, -1);
				lua_pop(L, 1);

				if (scales) {
					if (difficulty == 0.0) {
						lua_pop(L, 1); lua_pushnumber(L, 0.0); return 1;
					} else if (difficulty == 1.0) {
						damage = std::min(damage / 2.0 + 1.0, damage);
					} else if (difficulty == 3.0) {
						damage = damage * 1.5;
					}
				}
			}
			lua_pop(L, 1);
		}

		lua_pushnumber(L, damage);
		return 1;
	}

	// 💎 2. native_damage_tick_health （DISKテロ完全抹殺・メモリHP加減算処理）
	int l_damage_tick_health(lua_State *L)
	{
		int top = lua_gettop(L);
		int player_idx = 0;
		double amount = 0.0;
		bool is_heal = false;

		for (int i = 1; i <= top; i++) {
			if (lua_isuserdata(L, i) || lua_istable(L, i)) { player_idx = i; }
			else if (lua_isnumber(L, i)) { amount = lua_tonumber(L, i); }
			else if (lua_isboolean(L, i)) { is_heal = lua_toboolean(L, i); }
		}

		if (player_idx == 0) { lua_pushnumber(L, 20.0); return 1; }

		// プレイヤー名を回収
		lua_getfield(L, player_idx, "get_player_name");
		if (!lua_isfunction(L, -1)) { lua_pop(L, 1); lua_pushnumber(L, 20.0); return 1; }
		lua_pushvalue(L, player_idx);
		lua_call(L, 1, 1);
		std::string name = lua_tostring(L, -1);
		lua_pop(L, 1);

		// 初回アクセス時はファイルからロードしてC++メモリに常駐キャッシュ
		if (!g_health_loaded[name]) {
			g_player_health[name] = load_health_from_storage(L, name);
			g_health_loaded[name] = true;
		}

		float current_hp = g_player_health[name];

		if (is_heal) {
			// 回復処理：hp_maxを動的に回収して安全上限ガード
			double hp_max = 20.0;
			lua_getfield(L, player_idx, "get_properties");
			if (lua_isfunction(L, -1)) {
				lua_pushvalue(L, player_idx); lua_call(L, 1, 1);
				if (lua_istable(L, -1)) {
					lua_getfield(L, -1, "hp_max");
					if (lua_isnumber(L, -1)) { hp_max = lua_tonumber(L, -1); }
					lua_pop(L, 1);
				}
				lua_pop(L, 1);
			} else { lua_pop(L, 1); }

			current_hp = static_cast<float>(std::min(hp_max, current_hp + amount));
		} else {
			// ダメージ減算処理：math.max(0, health - amount)
			current_hp = static_cast<float>(std::max(0.0, current_hp - amount));
		}

		// C++側のメモリマップを無音更新（DISKアクセスは物理的に完全0化！）
		g_player_health[name] = current_hp;

		lua_pushnumber(L, current_hp);
		return 1;
	}

	// 💎 3. native_damage_sync_to_engine （join/die/サーバー終了時の2重装甲同期）
	int l_damage_sync_to_engine(lua_State *L)
	{
		int top = lua_gettop(L);
		int player_idx = 0;
		std::string mode = "sync"; // sync, join, die

		for (int i = 1; i <= top; i++) {
			if (lua_isuserdata(L, i) || lua_istable(L, i)) { player_idx = i; }
			else if (lua_isstring(L, i)) { mode = lua_tostring(L, i); }
		}

		if (player_idx == 0) return 0;

		lua_getfield(L, player_idx, "get_player_name");
		if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return 0; }
		lua_pushvalue(L, player_idx); lua_call(L, 1, 1);
		std::string name = lua_tostring(L, -1);
		lua_pop(L, 1);

		if (mode == "join") {
			// ログイン時はDISKから吸い上げてC++メモリを確定
			g_player_health[name] = load_health_from_storage(L, name);
			g_health_loaded[name] = true;
		} else if (mode == "die") {
			// 死亡時はC++メモリのHPを0リセットし、DISKへ即同期
			g_player_health[name] = 0.0f;
			save_health_to_storage(L, name, 0.0f);
		} else {
			// 通常同期：C++メモリの値をDISK（物理ファイル）へガチッとコミットセーブ
			if (g_health_loaded[name]) {
				save_health_to_storage(L, name, g_player_health[name]);
			}
		}
		return 0;
	}

	// 👑 💎 4. native_damage_bulk_save_all (総大将ディレクションによる究極の1行セーブAPI)
	//    Lua側から10秒おきに1回呼ばれるだけで、C++の超高速なメモリ走査（std::unordered_map）によって
	//    全プレイヤーのHPと、全チャンクの滞在時間のすべてを、一瞬で ModStorage へバルク同期（セーブ）する！
	int l_damage_bulk_save_all(lua_State *L)
	{
		// 1. まずはC++メモリにある全プレイヤーのHPを名指しで一括DISKコミット
		for (const auto& pair : g_player_health) {
			if (g_health_loaded[pair.first]) {
				save_health_to_storage(L, pair.first, pair.second);
			}
		}

		// 2. 続いて世界のworldsネームスペースにあるチャンクタイマーを一気に一括DISKコミット
		worlds::save_chunk_timers_to_storage(L);

		return 0; // スタックを1ドットも汚さずに無音で Lua へリターン！
	}

}
