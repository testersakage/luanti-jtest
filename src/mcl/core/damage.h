// src/mcl/core/util_damage.h （★新規作成・1マクロの死角もない最速ダメージヘッダー定義）
#pragma once
#include <lua.hpp>
#include <unordered_map>
#include <string>

namespace damage {

	// 👑 戦闘中・持続ダメージ中は完全無音のメモリ処理！DISK I/Oを完全絶滅させるハッシュマップ
	extern std::unordered_map<std::string, float> g_player_health;
	extern std::unordered_map<std::string, bool> g_health_loaded;

	// 👑 【3大最速筋肉】：見る以外のスタック操作を一切しない絶対規律窓口群
	int l_damage_calculate_modifier(lua_State *L);
	int l_damage_tick_health(lua_State *L);
	int l_damage_sync_to_engine(lua_State *L);
	int l_damage_bulk_save_all(lua_State *L);

	// 👑 データ保護インフラ：C++メモリのHPデータを ModStorage へ強制ダンプセーブ / ロードする関数
	void save_health_to_storage(lua_State *L, const std::string& name, float hp);
	float load_health_from_storage(lua_State *L, const std::string& name);
}
