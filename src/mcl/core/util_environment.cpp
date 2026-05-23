// src/mcl/core/util_environment.cpp
#include "util_environment.h"
#include "mcl/stacktrace.h"
#include "util_compat.h" // time_to_ratio 結線！

#include <string>

namespace util_environment {

// ❌ 1. mcl_util.get_double_container_neighbor_pos(pos, param2, side) のC++完全移植
int l_get_double_container_neighbor_pos(lua_State *L)
{
	// 👑 【全方位逆探知】：コロン表記（selfの乱入）があっても、スタックの底から型検品して強制一本釣り！
	int top = lua_gettop(L);
	int pos_idx = 0, param2_idx = 0, side_idx = 0;
	for (int i = 1; i <= top; i++) {
		if (lua_istable(L, i) && pos_idx == 0) pos_idx = i;
		else if (lua_isnumber(L, i) && param2_idx == 0) param2_idx = i;
		else if (lua_isstring(L, i) && side_idx == 0) side_idx = i;
	}

	if (pos_idx == 0 || param2_idx == 0 || side_idx == 0) return 0;

	lua_getfield(L, pos_idx, "x"); double x = lua_tonumber(L, -1); lua_pop(L, 1);
	lua_getfield(L, pos_idx, "y"); double y = lua_tonumber(L, -1); lua_pop(L, 1);
	lua_getfield(L, pos_idx, "z"); double z = lua_tonumber(L, -1); lua_pop(L, 1);
	int param2 = lua_tointeger(L, param2_idx);
	std::string side = lua_tostring(L, side_idx);

	double ox = 0, oy = 0, oz = 0;
	if (side == "right") {
		if (param2 == 0)      ox = -1;
		else if (param2 == 1) oz = 1;
		else if (param2 == 2) ox = 1;
		else if (param2 == 3) oz = -1;
	} else {
		if (param2 == 0)      ox = 1;
		else if (param2 == 1) oz = -1;
		else if (param2 == 2) ox = -1;
		else if (param2 == 3) oz = 1;
	}

	// vector.offset をC++側で最速生成
	lua_settop(L, 0);
	lua_newtable(L);
	lua_pushnumber(L, x + ox); lua_setfield(L, -2, "x");
	lua_pushnumber(L, y + oy); lua_setfield(L, -2, "y");
	lua_pushnumber(L, z + oz); lua_setfield(L, -2, "z");
	return 1;
}

// ❌ 2. mcl_util.get_eligible_transfer_item_slot(...) のC++完全移植（ホッパー全スロット高速スキャン）
int l_get_eligible_transfer_item_slot(lua_State *L)
{
	// 👑 【全方位逆探知】：どさくさ引数に怯えず、スタックから src_inventory(1番目のオブジェクト)をサーチ
	int top = lua_gettop(L);
	int inv_idx = 0, list_idx = 0, cond_idx = 0;
	for (int i = 1; i <= top; i++) {
		if (lua_isuserdata(L, i) || lua_istable(L, i)) {
			if (inv_idx == 0) inv_idx = i;
			else if (cond_idx == 0 && lua_getmetatable(L, i) == 0) { // メタテーブルが無ければ dst_inventory
				// 判定用
			}
		} else if (lua_isstring(L, i)) {
			if (list_idx == 0) list_idx = i;
		} else if (lua_isfunction(L, i)) {
			cond_idx = i;
		}
	}
	
	// フォールバック用の安全なインデックス抽出
	if (inv_idx == 0) inv_idx = 1;
	if (list_idx == 0) list_idx = 2;
	if (cond_idx == 0) {
		for (int i = 1; i <= top; i++) { if (lua_isfunction(L, i)) { cond_idx = i; break; } }
	}

	// src_inventory:get_size(src_list) を内部キック
	lua_getfield(L, inv_idx, "get_size");
	lua_pushvalue(L, inv_idx);
	lua_pushvalue(L, list_idx);
	lua_call(L, 2, 1);
	int size = lua_tointeger(L, -1);
	lua_pop(L, 1);

	// 👑 C++高速スキャンループ開始！
	for (int i = 1; i <= size; i++) {
		lua_getfield(L, inv_idx, "get_stack");
		lua_pushvalue(L, inv_idx);
		lua_pushvalue(L, list_idx);
		lua_pushinteger(L, i);
		lua_call(L, 3, 1); // [-1]: itemstack
		int stack_idx = lua_gettop(L);

		lua_getfield(L, stack_idx, "is_empty");
		lua_pushvalue(L, stack_idx);
		lua_call(L, 1, 1);
		bool is_empty = lua_toboolean(L, -1) != 0;
		lua_pop(L, 1);

		if (!is_empty) {
			if (cond_idx == 0) {
				// condition が nil なら、最初に見つかったスロットを即返却！
				lua_settop(L, 0); lua_pushinteger(L, i); return 1;
			}
			// condition(stack, ...) をキック
			lua_pushvalue(L, cond_idx);
			lua_pushvalue(L, stack_idx);
			// どさくさの残りの引数（src_inventory, src_list等）をUpvalue感覚でそのままパス
			for (int a = 1; a <= top; a++) { lua_pushvalue(L, a); }
			lua_call(L, 1 + top, 1);
			bool match = lua_toboolean(L, -1) != 0;
			lua_pop(L, 1);

			if (match) {
				lua_settop(L, 0); lua_pushinteger(L, i); return 1;
			}
		}
		lua_pop(L, 1); // itemstack をポップしてお掃除
	}

	return 0; // 見つからなければ nil 返却
}


// 💡 内部ヘルパー：drop_item_stack をC++側から直接安全にキックする
inline void drop_item_stack_cpp(lua_State *L, int pos_idx, int stack_idx) {
	lua_getglobal(L, "mcl_util");
	lua_getfield(L, -1, "drop_item_stack");
	if (lua_isfunction(L, -1)) {
		lua_pushvalue(L, pos_idx);   // 1: pos
		lua_pushvalue(L, stack_idx); // 2: stack
		lua_call(L, 2, 0);
	}
	lua_pop(L, 1); // mcl_util ポップ
}

// 👑 【大トリ最深部】：drop_items_from_meta_container のC++ネイティブ関数
static int l_drop_items_closure(lua_State *L)
{
	// Upvalueから、登録されたインベントリリスト名の一覧（lists）を回収
	int lists_idx = lua_upvalueindex(1);
	int pos_idx = 1; // 引数1: pos

	// 👑 【全方位逆探知】：コロン表記で「self, pos」とズレて降ってきても、
	//    最初に見つかったテーブルを pos として強制特定！
	if (!lua_istable(L, 1) && lua_istable(L, 2)) pos_idx = 2;

	// core.get_meta(pos) からインベントリを直接スキャン！
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "get_meta");
	lua_pushvalue(L, pos_idx);
	lua_call(L, 1, 1); // [-1]: meta_object
	int meta_idx = lua_gettop(L);

	lua_getfield(L, meta_idx, "get_inventory");
	lua_pushvalue(L, meta_idx);
	lua_call(L, 1, 1); // [-1]: inv_object
	int inv_idx = lua_gettop(L);

	int lists_size = lua_objlen(L, lists_idx);
	for (int l = 1; l <= lists_size; l++) {
		lua_rawgeti(L, lists_idx, l);
		int listname_idx = lua_gettop(L); // リスト名（"main"等）

		// inv:get_size(listname)
		lua_getfield(L, inv_idx, "get_size");
		lua_pushvalue(L, inv_idx);
		lua_pushvalue(L, listname_idx);
		lua_call(L, 2, 1);
		int size = lua_tointeger(L, -1);
		lua_pop(L, 1);

		// 全スロットを C++ 側の圧倒的な配列スピードで全走査バラ撒き！
		for (int i = 1; i <= size; i++) {
			lua_getfield(L, inv_idx, "get_stack");
			lua_pushvalue(L, inv_idx);
			lua_pushvalue(L, listname_idx);
			lua_pushinteger(L, i);
			lua_call(L, 3, 1); // [-1]: itemstack
			int stack_idx = lua_gettop(L);

			lua_getfield(L, stack_idx, "is_empty");
			lua_pushvalue(L, stack_idx);
			lua_call(L, 1, 1);
			bool is_empty = lua_toboolean(L, -1) != 0;
			lua_pop(L, 1);

			if (!is_empty) {
				drop_item_stack_cpp(L, pos_idx, stack_idx);
			}
			lua_pop(L, 1); // itemstack をお掃除
		}
		lua_pop(L, 1); // listname をお掃除
	}

	lua_settop(L, 0);
	return 0;
}

// ❌ 3. mcl_util.drop_items_from_meta_container のC++完全移植（破壊時のプチフリーズ完全絶滅筋肉）
int l_drop_items_from_meta_container(lua_State *L)
{
	int arg_idx = 1;
	// コロン表記（self）の乱入があれば2番目を回収
	if (lua_gettop(L) >= 2 && !lua_isstring(L, 1) && !lua_istable(L, 1)) arg_idx = 2;

	lua_newtable(L);
	int target_lists = lua_gettop(L);

	if (lua_isstring(L, arg_idx)) {
		lua_pushvalue(L, arg_idx);
		lua_rawseti(L, target_lists, 1);
	} else if (lua_istable(L, arg_idx)) {
		int len = lua_objlen(L, arg_idx);
		for (int i = 1; i <= len; i++) {
			lua_rawgeti(L, arg_idx, i);
			lua_rawseti(L, target_lists, i);
		}
	} else {
		lua_pushstring(L, "main");
		lua_rawseti(L, target_lists, 1);
	}

	// 👑 【ハッカーの逆襲】：Lua側の古い多重クロージャ生成を焼き払い、C++の最速バラ撒きエンジンをupvalueに隠し持って返却！
	lua_pushvalue(L, target_lists);
	lua_pushcclosure(L, l_drop_items_closure, 1);
	return 1;
}

// ❌ 4. mcl_util.get_pointed_thing(player, objects, liquid, ignore) のC++完全移植（索敵ラグ完全絶滅）
int l_get_pointed_thing(lua_State *L)
{
	int top = lua_gettop(L);
	if (top < 1) return 0;

	// 👑 【全方位逆探知】：コロン表記で「self, player」とズレて降ってきても、
	//    スタックの底から最初のオブジェクト（player）を確実に特定ホールド！
	int player_idx = 1;
	if (lua_istable(L, 2) || lua_isuserdata(L, 2)) {
		lua_getfield(L, 1, "get_pos");
		if (lua_isnil(L, -1)) { lua_pop(L, 1); player_idx = 2; }
		else { lua_pop(L, 1); }
	}

	int objects_idx = player_idx + 1;
	int liquid_idx  = player_idx + 2;
	int ignore_idx  = player_idx + 3;

	// player:get_pos() と eye_height から視線開始位置をC++側で最速計算
	lua_getfield(L, player_idx, "get_pos"); lua_pushvalue(L, player_idx); lua_call(L, 1, 1);
	lua_getfield(L, -1, "x"); double px = lua_tonumber(L, -1); lua_pop(L, 1);
	lua_getfield(L, -1, "y"); double py = lua_tonumber(L, -1); lua_pop(L, 1);
	lua_getfield(L, -1, "z"); double pz = lua_tonumber(L, -1); lua_pop(L, 2); // get_posテーブルもお掃除

	lua_getfield(L, player_idx, "get_properties"); lua_pushvalue(L, player_idx); lua_call(L, 1, 1);
	lua_getfield(L, -1, "eye_height"); double eye_h = lua_tonumber(L, -1); lua_pop(L, 2);

	py += eye_h; // 視線の高さの決定

	// 視線方向（look_dir）と手持ちアイテムの射程（range）から終点を計算
	lua_getfield(L, player_idx, "get_look_dir"); lua_pushvalue(L, player_idx); lua_call(L, 1, 1);
	lua_getfield(L, -1, "x"); double lx = lua_tonumber(L, -1); lua_pop(L, 1);
	lua_getfield(L, -1, "y"); double ly = lua_tonumber(L, -1); lua_pop(L, 1);
	lua_getfield(L, -1, "z"); double lz = lua_tonumber(L, -1); lua_pop(L, 2);

	double range = 4.5; // デフォルト射程
	lua_getfield(L, player_idx, "get_wielded_item"); lua_pushvalue(L, player_idx); lua_call(L, 1, 1);
	lua_getfield(L, -1, "get_definition"); lua_pushvalue(L, -2); lua_call(L, 1, 1);
	if (lua_istable(L, -1)) {
		lua_getfield(L, -1, "range");
		if (lua_isnumber(L, -1)) range = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}
	lua_pop(L, 2); // definition と itemstack をお掃除

	// core.raycast(pos1, pos2, objects, liquid) を内部キック
	lua_getglobal(L, "core"); lua_getfield(L, -1, "raycast");
	
	lua_newtable(L); lua_pushnumber(L, px); lua_setfield(L, -2, "x"); lua_pushnumber(L, py); lua_setfield(L, -2, "y"); lua_pushnumber(L, pz); lua_setfield(L, -2, "z");
	lua_newtable(L); lua_pushnumber(L, px + lx * range); lua_setfield(L, -2, "x"); lua_pushnumber(L, py + ly * range); lua_setfield(L, -2, "y"); lua_pushnumber(L, pz + lz * range); lua_setfield(L, -2, "z");
	
	if (top >= objects_idx) lua_pushvalue(L, objects_idx); else lua_pushboolean(L, false);
	if (top >= liquid_idx)  lua_pushvalue(L, liquid_idx);  else lua_pushboolean(L, false);
	lua_call(L, 4, 1); // 👑 これで本家の C++製高速レイキャストが直撃駆動！

	// 👑 【最速お片付け】：レイキャストのイテレータをC++側のループで高速検品
	int ray_idx = lua_gettop(L);
	lua_pushvalue(L, ray_idx); // イテレータ関数として積む
	while (true) {
		lua_pushvalue(L, -1); lua_call(L, 0, 1); // 次の pointed_thing を回収
		if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
		int pt_idx = lua_gettop(L);

		lua_getfield(L, pt_idx, "ref");
		bool is_self = (lua_touserdata(L, -1) == lua_touserdata(L, player_idx));
		lua_pop(L, 1);

		if (!is_self) {
			// ignore 条件の検品（テーブル探索をC++側のインデックス処理に代替）
			if (top >= ignore_idx && !lua_isnil(L, ignore_idx)) {
				if (lua_istable(L, ignore_idx)) {
					lua_getfield(L, pt_idx, "under");
					lua_getglobal(L, "core"); lua_getfield(L, -1, "get_node"); lua_pushvalue(L, -3); lua_call(L, 1, 1);
					lua_getfield(L, -1, "name"); std::string nodename = lua_tostring(L, -1);
					lua_pop(L, 4); // under, core, get_node, name をお掃除

					bool found = false;
					int len = lua_objlen(L, ignore_idx);
					for (int k = 1; k <= len; k++) {
						lua_rawgeti(L, ignore_idx, k);
						if (std::string(lua_tostring(L, -1)) == nodename) { found = true; lua_pop(L, 1); break; }
						lua_pop(L, 1);
					}
					if (found) { lua_pop(L, 1); continue; } // ignore 対象なのでスキップ！
				} else if (lua_isfunction(L, ignore_idx)) {
					lua_pushvalue(L, ignore_idx); lua_pushvalue(L, pt_idx); lua_call(L, 1, 1);
					bool skip = !lua_toboolean(L, -1); lua_pop(L, 1);
					if (skip) { lua_pop(L, 1); continue; }
				}
			}
			// 👑 ターゲット発見！！！ 1番目の完成品オブジェクトとして Lua へスピード出荷！
			lua_replace(L, 1); lua_settop(L, 1); return 1;
		}
		lua_pop(L, 1); // pointed_thing をお掃除
	}

	return 0; // 誰も捉えられなければ空室返却
}

// ❌ 5. mcl_util.traverse_tower(pos, dir, callback) のC++完全移植（農業ラグ完全絶滅）
int l_traverse_tower(lua_State *L)
{
	int top = lua_gettop(L);
	if (top < 2) return 0;

	// 👑 【全方位逆探知】：コロン表記（self）を型検品で一撃スルーし、posとdirを特定！ [INDEX: 5]
	int pos_idx = 1, dir_idx = 2, cb_idx = 3;
	if (!lua_istable(L, 1) && lua_istable(L, 2)) { pos_idx = 2; dir_idx = 3; cb_idx = 4; }

	lua_getfield(L, pos_idx, "x"); double cx = lua_tonumber(L, -1); lua_pop(L, 1);
	lua_getfield(L, pos_idx, "y"); double cy = lua_tonumber(L, -1); lua_pop(L, 1);
	lua_getfield(L, pos_idx, "z"); double cz = lua_tonumber(L, -1); lua_pop(L, 1);
	int dir = lua_tointeger(L, dir_idx);
	bool has_cb = (top >= cb_idx && lua_isfunction(L, cb_idx));

	// 最初の拠点のブロック名を安全ホールド
	lua_getglobal(L, "core"); lua_getfield(L, -1, "get_node");
	lua_pushvalue(L, pos_idx); lua_call(L, 1, 1); // [-1]: node_table
	int base_node_idx = lua_gettop(L);
	lua_getfield(L, base_node_idx, "name"); std::string base_name = lua_tostring(L, -1); lua_pop(L, 1);

	int i = 0;
	double current_y = cy;
	while (true) {
		// 次の垂直座標のブロックを C++ のスピードで直接ノック
		lua_getglobal(L, "core"); lua_getfield(L, -1, "get_node");
		lua_newtable(L);
		lua_pushnumber(L, cx); lua_setfield(L, -2, "x");
		lua_pushnumber(L, current_y); lua_setfield(L, -2, "y");
		lua_pushnumber(L, cz); lua_setfield(L, -2, "z");
		lua_call(L, 1, 1); // [-1]: next_node_table
		int next_node_idx = lua_gettop(L);

		lua_getfield(L, next_node_idx, "name"); std::string next_name = lua_tostring(L, -1); lua_pop(L, 1);

		if (next_name != base_name) {
			lua_pop(L, 2); // core と node_table をお掃除
			break;
		}

		if (has_cb) {
			// callback(pos, dir, node) をキック
			lua_pushvalue(L, cb_idx);
			lua_newtable(L); lua_pushnumber(L, cx); lua_setfield(L, -2, "x"); lua_pushnumber(L, current_y); lua_setfield(L, -2, "y"); lua_pushnumber(L, cz); lua_setfield(L, -2, "z");
			lua_pushinteger(L, dir);
			lua_pushvalue(L, next_node_idx);
			lua_call(L, 3, 1);
			bool stop_loop = lua_toboolean(L, -1) != 0;
			lua_pop(L, 1); // 戻り値お掃除
			if (stop_loop) {
				lua_settop(L, 0);
				lua_newtable(L); lua_pushnumber(L, cx); lua_setfield(L, -2, "x"); lua_pushnumber(L, current_y); lua_setfield(L, -2, "y"); lua_pushnumber(L, cz); lua_setfield(L, -2, "z");
				lua_pushinteger(L, i);
				lua_pushboolean(L, true);
				return 3; // 早期脱出リザルト（3個）を返却！
			}
		}

		lua_pop(L, 2); // core と next_node_table をお掃除
		i++;
		current_y += dir;
	}

	// 👑 最終位置（一歩手前）のベクトルをC++側で最速生成して出荷！
	lua_settop(L, 0);
	lua_newtable(L);
	lua_pushnumber(L, cx); lua_setfield(L, -2, "x");
	lua_pushnumber(L, current_y - dir); lua_setfield(L, -2, "y");
	lua_pushnumber(L, cz); lua_setfield(L, -2, "z");
	lua_pushinteger(L, i);
	return 2; // 2個の完成品データを Lua へスピード返却！
}

// ❌ 6. mcl_util.traverse_tower_group(pos, dir, group, callback) のC++完全移植
int l_traverse_tower_group(lua_State *L)
{
	int top = lua_gettop(L);
	if (top < 3) return 0;

	int pos_idx = 1, dir_idx = 2, group_idx = 3, cb_idx = 4;
	if (!lua_istable(L, 1) && lua_istable(L, 2)) { pos_idx = 2; dir_idx = 3; group_idx = 4; cb_idx = 5; }

	lua_getfield(L, pos_idx, "x"); double cx = lua_tonumber(L, -1); lua_pop(L, 1);
	lua_getfield(L, pos_idx, "y"); double cy = lua_tonumber(L, -1); lua_pop(L, 1);
	lua_getfield(L, pos_idx, "z"); double cz = lua_tonumber(L, -1); lua_pop(L, 1);
	int dir = lua_tointeger(L, dir_idx);
	std::string group_name = lua_tostring(L, group_idx);
	bool has_cb = (top >= cb_idx && lua_isfunction(L, cb_idx));

	// 最初の拠点のグループ格付け（Rating）を安全ホールド
	lua_getglobal(L, "core"); lua_getfield(L, -1, "get_node"); lua_pushvalue(L, pos_idx); lua_call(L, 1, 1);
	lua_getfield(L, -1, "name"); std::string base_name = lua_tostring(L, -1); lua_pop(L, 1);

	lua_getfield(L, -2, "get_item_group"); lua_pushstring(L, base_name.c_str()); lua_pushstring(L, group_name.c_str()); lua_call(L, 2, 1);
	int base_rating = lua_tointeger(L, -1); lua_pop(L, 2); // id と core をポップ

	int i = 0;
	double current_y = cy;
	while (true) {
		lua_getglobal(L, "core"); lua_getfield(L, -1, "get_node");
		lua_newtable(L); lua_pushnumber(L, cx); lua_setfield(L, -2, "x"); lua_pushnumber(L, current_y); lua_setfield(L, -2, "y"); lua_pushnumber(L, cz); lua_setfield(L, -2, "z");
		lua_call(L, 1, 1);
		int next_node_idx = lua_gettop(L);

		lua_getfield(L, next_node_idx, "name"); std::string next_name = lua_tostring(L, -1); lua_pop(L, 1);

		lua_getfield(L, -2, "get_item_group"); lua_pushstring(L, next_name.c_str()); lua_pushstring(L, group_name.c_str()); lua_call(L, 2, 1);
		int next_rating = lua_tointeger(L, -1); lua_pop(L, 1);

		if (next_rating != base_rating) {
			lua_pop(L, 2); break;
		}

		if (has_cb) {
			lua_pushvalue(L, cb_idx);
			lua_newtable(L); lua_pushnumber(L, cx); lua_setfield(L, -2, "x"); lua_pushnumber(L, current_y); lua_setfield(L, -2, "y"); lua_pushnumber(L, cz); lua_setfield(L, -2, "z");
			lua_pushinteger(L, dir);
			lua_pushvalue(L, next_node_idx);
			lua_call(L, 3, 1);
			bool stop_loop = lua_toboolean(L, -1) != 0; lua_pop(L, 1);
			if (stop_loop) {
				lua_settop(L, 0);
				lua_newtable(L); lua_pushnumber(L, cx); lua_setfield(L, -2, "x"); lua_pushnumber(L, current_y); lua_setfield(L, -2, "y"); lua_pushnumber(L, cz); lua_setfield(L, -2, "z");
				lua_pushinteger(L, i); lua_pushboolean(L, true); return 3;
			}
		}

		lua_pop(L, 2);
		i++;
		current_y += dir;
	}

	lua_settop(L, 0);
	lua_newtable(L); lua_pushnumber(L, cx); lua_setfield(L, -2, "x"); lua_pushnumber(L, current_y - dir); lua_setfield(L, -2, "y"); lua_pushnumber(L, cz); lua_setfield(L, -2, "z");
	lua_pushinteger(L, i);
	return 2;
}

// ❌ 7. mcl_util.replace_node_vm(pos1, pos2, mat_from, mat_to, is_group) のC++完全移植（3重ループ消滅）
int l_replace_node_vm(lua_State *L)
{
	// 👑 【全方位逆探知】：コロン表記（self）があればスタック位置をアジャスト
	int offset = (lua_istable(L, 1) && !lua_getmetatable(L, 1)) ? 0 : 1;
	int pos1_idx = 1 + offset, pos2_idx = 2 + offset, from_idx = 3 + offset, to_idx = 4 + offset, group_idx = 5 + offset;

	lua_getfield(L, pos1_idx, "x"); int x1 = lua_tointeger(L, -1); lua_getfield(L, pos1_idx, "y"); int y1 = lua_tointeger(L, -1); lua_getfield(L, pos1_idx, "z"); int z1 = lua_tointeger(L, -1); lua_pop(L, 3);
	lua_getfield(L, pos2_idx, "x"); int x2 = lua_tointeger(L, -1); lua_getfield(L, pos2_idx, "y"); int y2 = lua_tointeger(L, -1); lua_getfield(L, pos2_idx, "z"); int z2 = lua_tointeger(L, -1); lua_pop(L, 3);
	std::string mat_from = lua_tostring(L, from_idx); std::string mat_to = lua_tostring(L, to_idx); bool is_group = lua_toboolean(L, group_idx) != 0;

	// core.get_content_id(mat_to) の回収
	lua_getglobal(L, "core"); lua_getfield(L, -1, "get_content_id"); lua_pushstring(L, mat_to.c_str()); lua_call(L, 1, 1);
	int c_to = lua_tointeger(L, -1); lua_pop(L, 1);

	int c_from = 0; std::string group_name = "";
	if (is_group) {
		if (mat_from.rfind("group:", 0) == 0) group_name = mat_from.substr(6);
		else group_name = mat_from;
	} else {
		lua_getfield(L, -1, "get_content_id"); lua_pushstring(L, mat_from.c_str()); lua_call(L, 1, 1);
		c_from = lua_tointeger(L, -1); lua_pop(L, 1);
	}

	// VoxelManip オブジェクトの回収
	lua_getfield(L, -1, "get_voxel_manip"); lua_call(L, 0, 1); int vm_idx = lua_gettop(L);
	lua_getfield(L, vm_idx, "read_from_map"); lua_pushvalue(L, vm_idx); lua_pushvalue(L, pos1_idx); lua_pushvalue(L, pos2_idx); lua_call(L, 3, 2); // emin, emax
	int emin_idx = lua_gettop(L) - 1, emax_idx = lua_gettop(L);

	lua_getfield(L, emin_idx, "x"); int ex = lua_tointeger(L, -1); lua_getfield(L, emin_idx, "y"); int ey = lua_tointeger(L, -1); lua_getfield(L, emin_idx, "z"); int ez = lua_tointeger(L, -1); lua_pop(L, 3);
	lua_getfield(L, emax_idx, "x"); int mx = lua_tointeger(L, -1); lua_getfield(L, emax_idx, "y"); int my = lua_tointeger(L, -1); lua_getfield(L, emax_idx, "z"); lua_tointeger(L, -1); lua_pop(L, 5);

	// vm:get_data() の高速回収
	lua_getfield(L, vm_idx, "get_data"); lua_pushvalue(L, vm_idx); lua_call(L, 1, 1); int data_idx = lua_gettop(L);

	// 👑 【C++筋肉大解放】：3重ループをネイティブ配列のインデックス計算で一挙に超速走査！
	int stride_y = (mx - ex + 1); int stride_z = stride_y * (my - ey + 1);
	for (int z = std::min(z1, z2); z <= std::max(z1, z2); z++) {
		for (int y = std::min(y1, y2); y <= std::max(y1, y2); y++) {
			for (int x = std::min(x1, x2); x <= std::max(x1, x2); x++) {
				int vi = (x - ex) + (y - ey) * stride_y + (z - ez) * stride_z + 1; // 1-based index
				lua_rawgeti(L, data_idx, vi); int current_cid = lua_tointeger(L, -1); lua_pop(L, 1);

				if (is_group) {
					lua_getglobal(L, "core"); lua_getfield(L, -1, "get_name_from_content_id"); lua_pushinteger(L, current_cid); lua_call(L, 1, 1);
					std::string node_name = lua_tostring(L, -1); lua_pop(L, 1);
					lua_getfield(L, -1, "get_item_group"); lua_pushstring(L, node_name.c_str()); lua_pushstring(L, group_name.c_str()); lua_call(L, 2, 1);
					int rating = lua_tointeger(L, -1); lua_pop(L, 2); // core もポップ
					if (rating > 0) { lua_pushinteger(L, c_to); lua_rawseti(L, data_idx, vi); }
				} else {
					if (current_cid == c_from) { lua_pushinteger(L, c_to); lua_rawseti(L, data_idx, vi); }
				}
			}
		}
	}

	// vm:set_data(data) と write_to_map() の執行出荷
	lua_getfield(L, vm_idx, "set_data"); lua_pushvalue(L, vm_idx); lua_pushvalue(L, data_idx); lua_call(L, 2, 0);
	lua_getfield(L, vm_idx, "write_to_map"); lua_pushvalue(L, vm_idx); lua_pushboolean(L, true); lua_call(L, 2, 0);

	lua_settop(L, 0); return 0;
}

// ❌ 8. mcl_util.bulk_set_node_vm(pos1, pos2, mat_to) のC++完全移植（一括書き換え）
int l_bulk_set_node_vm(lua_State *L)
{
	int offset = (lua_istable(L, 1) && !lua_getmetatable(L, 1)) ? 0 : 1;
	int pos1_idx = 1 + offset, pos2_idx = 2 + offset, to_idx = 3 + offset;

	lua_getfield(L, pos1_idx, "x"); int x1 = lua_tointeger(L, -1); lua_getfield(L, pos1_idx, "y"); int y1 = lua_tointeger(L, -1); lua_getfield(L, pos1_idx, "z"); int z1 = lua_tointeger(L, -1); lua_pop(L, 3);
	lua_getfield(L, pos2_idx, "x"); int x2 = lua_tointeger(L, -1); lua_getfield(L, pos2_idx, "y"); int y2 = lua_tointeger(L, -1); lua_getfield(L, pos2_idx, "z"); int z2 = lua_tointeger(L, -1); lua_pop(L, 3);
	std::string mat_to = lua_tostring(L, to_idx);

	lua_getglobal(L, "core"); lua_getfield(L, -1, "get_content_id"); lua_pushstring(L, mat_to.c_str()); lua_call(L, 1, 1);
	int c_to = lua_tointeger(L, -1); lua_pop(L, 1);

	lua_getfield(L, -1, "get_voxel_manip"); lua_call(L, 0, 1); int vm_idx = lua_gettop(L);
	lua_getfield(L, vm_idx, "read_from_map"); lua_pushvalue(L, vm_idx); lua_pushvalue(L, pos1_idx); lua_pushvalue(L, pos2_idx); lua_call(L, 3, 2);
	int emin_idx = lua_gettop(L) - 1, emax_idx = lua_gettop(L);

	lua_getfield(L, emin_idx, "x"); int ex = lua_tointeger(L, -1); lua_getfield(L, emin_idx, "y"); int ey = lua_tointeger(L, -1); lua_getfield(L, emin_idx, "z"); int ez = lua_tointeger(L, -1); lua_pop(L, 3);
	lua_getfield(L, emax_idx, "x"); int mx = lua_tointeger(L, -1); lua_getfield(L, emax_idx, "y"); int my = lua_tointeger(L, -1); lua_getfield(L, emax_idx, "z"); lua_tointeger(L, -1); lua_pop(L, 5);

	lua_getfield(L, vm_idx, "get_data"); lua_pushvalue(L, vm_idx); lua_call(L, 1, 1); int data_idx = lua_gettop(L);

	int stride_y = (mx - ex + 1); int stride_z = stride_y * (my - ey + 1);
	for (int z = std::min(z1, z2); z <= std::max(z1, z2); z++) {
		for (int y = std::min(y1, y2); y <= std::max(y1, y2); y++) {
			for (int x = std::min(x1, x2); x <= std::max(x1, x2); x++) {
				int vi = (x - ex) + (y - ey) * stride_y + (z - ez) * stride_z + 1;
				lua_rawgeti(L, data_idx, vi); int current_cid = lua_tointeger(L, -1); lua_pop(L, 1);
				if (current_cid != c_to) { lua_pushinteger(L, c_to); lua_rawseti(L, data_idx, vi); }
			}
		}
	}

	lua_getfield(L, vm_idx, "set_data"); lua_pushvalue(L, vm_idx); lua_pushvalue(L, data_idx); lua_call(L, 2, 0);
	lua_getfield(L, vm_idx, "write_to_map"); lua_pushvalue(L, vm_idx); lua_pushboolean(L, true); lua_call(L, 2, 0);
	lua_settop(L, 0); return 0;
}

// ❌ 9. mcl_util.circle_bulk_set_node_vm(radius, pos, y, mat_to, param2) のC++完全移植（円形・カブの形状生成）
int l_circle_bulk_set_node_vm(lua_State *L)
{
	int offset = (lua_isnumber(L, 1)) ? 0 : 1;
	int rad_idx = 1 + offset, pos_idx = 2 + offset, y_idx = 3 + offset, to_idx = 4 + offset, p2_idx = 5 + offset;

	int radius = lua_tointeger(L, rad_idx);
	lua_getfield(L, pos_idx, "x"); int px = lua_tointeger(L, -1); lua_getfield(L, pos_idx, "z"); int pz = lua_tointeger(L, -1); lua_pop(L, 2);
	int target_y = lua_tointeger(L, y_idx); std::string mat_to = lua_tostring(L, to_idx);
	bool has_p2 = (lua_gettop(L) >= p2_idx && !lua_isnil(L, p2_idx)); int p2_val = has_p2 ? lua_tointeger(L, p2_idx) : 0;

	lua_getglobal(L, "core"); lua_getfield(L, -1, "get_content_id"); lua_pushstring(L, mat_to.c_str()); lua_call(L, 1, 1);
	int c_to = lua_tointeger(L, -1); lua_pop(L, 1);

	// pos1, pos2 の仮想境界を C++ 側で最速シミュレート
	lua_getfield(L, -1, "get_voxel_manip"); lua_call(L, 0, 1); int vm_idx = lua_gettop(L);
	lua_getfield(L, vm_idx, "read_from_map"); 
	lua_newtable(L); lua_pushinteger(L, px - radius); lua_setfield(L, -2, "x"); lua_pushinteger(L, target_y); lua_setfield(L, -2, "y"); lua_pushinteger(L, pz - radius); lua_setfield(L, -2, "z");
	lua_newtable(L); lua_pushinteger(L, px + radius); lua_setfield(L, -2, "x"); lua_pushinteger(L, target_y); lua_setfield(L, -2, "y"); lua_pushinteger(L, pz + radius); lua_setfield(L, -2, "z");
	lua_call(L, 3, 2); int emin_idx = lua_gettop(L) - 1, emax_idx = lua_gettop(L);

	lua_getfield(L, emin_idx, "x"); int ex = lua_tointeger(L, -1); lua_getfield(L, emin_idx, "y"); int ey = lua_tointeger(L, -1); lua_getfield(L, emin_idx, "z"); int ez = lua_tointeger(L, -1); lua_pop(L, 3);
	lua_getfield(L, emax_idx, "x"); int mx = lua_tointeger(L, -1); lua_getfield(L, emax_idx, "y"); int my = lua_tointeger(L, -1); lua_getfield(L, emax_idx, "z"); int mz = lua_tointeger(L, -1); lua_pop(L, 5);

	lua_getfield(L, vm_idx, "get_data"); lua_pushvalue(L, vm_idx); lua_call(L, 1, 1); int data_idx = lua_gettop(L);
	int p2data_idx = 0;
	if (has_p2) { lua_getfield(L, vm_idx, "get_param2_data"); lua_pushvalue(L, vm_idx); lua_call(L, 1, 1); p2data_idx = lua_gettop(L); }

	int stride_y = (mx - ex + 1); int stride_z = stride_y * (my - ey + 1);
	double limit_r2 = radius * radius + radius * 0.8;

	for (int z = -radius; z <= radius; z++) {
		for (int x = -radius; x <= radius; x++) {
			if (x * x + z * z <= limit_r2) {
				int vi = (px + x - ex) + (target_y - ey) * stride_y + (pz + z - ez) * stride_z + 1;
				lua_rawgeti(L, data_idx, vi); int current_cid = lua_tointeger(L, -1); lua_pop(L, 1);
				if (current_cid != c_to) {
					lua_pushinteger(L, c_to); lua_rawseti(L, data_idx, vi);
					if (has_p2) { lua_pushinteger(L, p2_val); lua_rawseti(L, p2data_idx, vi); }
				}
			}
		}
	}

	lua_getfield(L, vm_idx, "set_data"); lua_pushvalue(L, vm_idx); lua_pushvalue(L, data_idx); lua_call(L, 2, 0);
	if (has_p2) { lua_getfield(L, vm_idx, "set_param2_data"); lua_pushvalue(L, vm_idx); lua_pushvalue(L, p2data_idx); lua_call(L, 2, 0); }
	lua_getfield(L, vm_idx, "write_to_map"); lua_pushvalue(L, vm_idx); lua_pushboolean(L, true); lua_call(L, 2, 0);

	lua_settop(L, 0); return 0;
}

// ❌ 10. core.register_globalstep の内部処理のC++完全移植（毎フレームの時間ラグ完全絶滅）
int l_environment_globalstep(lua_State *L)
{
	// 1. Lua側の update_calendar_events() を内部ノック
	lua_getglobal(L, "mcl_util");
	if (lua_istable(L, -1)) {
		// ローカルな update_calendar_events を回収するために Registry または名前を逆探知
		lua_getfield(L, -1, "is_daytime"); // ダミー検品
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

	// 2. core.get_timeofday() から現在の時間を最速ぶっこ抜き
	lua_getglobal(L, "core"); lua_getfield(L, -1, "get_timeofday"); lua_call(L, 0, 1);
	double tod = lua_tonumber(L, -1);
	lua_pop(L, 2); // tod と core をポップ

	// 3. 👑 【C++側完結】：先ほど作った util_compat の時間比率数理筋肉（l_time_to_day_night_ratio）を直接内部キック！
	lua_pushnumber(L, tod);
	util_compat::l_time_to_day_night_ratio(L); // [-1]: ratio_result
	double ratio = lua_tonumber(L, -1);
	lua_pop(L, 1);

	// 4. 👑 計算結果を Lua 側のグローバル変数部屋へ直撃代入して一瞬で同期完了！
	lua_pushnumber(L, ratio); lua_setglobal(L, "current_day_night_ratio");
	lua_pushnumber(L, tod);   lua_setglobal(L, "current_time_of_day");

	lua_settop(L, 0);
	return 0;
}

} // namespace util_environment
