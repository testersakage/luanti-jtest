// src/mcl/core/util_table.cpp
#include "util_table.h"

namespace util_table {

	//  1. table.update(t, ...) のC++最速ネイティブ実装
	int l_table_update(lua_State *L)
	{
		int top = lua_gettop(L);
		if (top < 1) return 0;
	
		// 1. 第1引数（ベーステーブル t）が本当にテーブルであるか厳格に改札
		luaL_checktype(L, 1, LUA_TTABLE); 

		// 2. 可変長引数（2番目〜最後のテーブルまで）を安全にループ走査
		for (int i = 2; i <= top; i++) {
			if (lua_istable(L, i)) {
				lua_pushnil(L); // lua_next のための最初の空キーを1個積む

				//  【絶対正義のスタック維持】：
				//    lua_next が周る間、常に [-2] = key, [-1] = value の絶対規律をキープ！
				while (lua_next(L, i) != 0) {

					// スタックの奥深くにある key と value の生コピーを、
					//    インデックス（絶対座標）で名指しで安全に最上部へ積み増す！
					lua_pushvalue(L, -2); // [-3]=key, [-2]=value, [-1]=key_copy
					lua_pushvalue(L, -2); // [-4]=key, [-3]=value, [-2]=key_copy, [-1]=value_copy

					// 執行：t[key_copy] = value_copy （実行後、コピーされた2個が自動でスタックから消滅）
					lua_settable(L, 1);    

					// 大修正】元の value だけをポップ！これでスタック最上部に「元のkey」だけがハだかで1個残り、
					//    次の lua_next へ1ナノ秒の狂いもなく安全にバケツリレーされて大調和通過します！！！
					lua_pop(L, 1);        
				}
			}
		}

		//  1番目の部屋に乗っているベーステーブル t をそのままスタックの戻り値として最速出荷。
		lua_pushvalue(L, 1); 
		return 1;
	}


	// 2. 【table.update_deep】：C++内部再帰による入れ子構造の安全ディープマージ
	// 内部の処理スタックを壊さないよう、再帰呼び出しのインデックスを完全固定制御
	void table_update_deep_recursive(lua_State *L, int target_table_idx, int source_table_idx)
	{
		lua_pushnil(L);
		while (lua_next(L, source_table_idx) != 0) {
			// スタック状態: [-2] = key, [-1] = value (v)
			
			// t[key] がすでにテーブルであり、かつ現在の v もテーブルであるか検証
			lua_pushvalue(L, -2); // keyのコピーを積む
			lua_gettable(L, target_table_idx); // t[key] を取得 ➔ [-1] = t[key]
			
			if (lua_istable(L, -1) && lua_istable(L, -2)) {
				// 両方テーブルなら、スタックの最上部2つを対象に再帰を執行
				// 現在のスタックの絶対位置を逆算して渡す
				int t_sub = lua_gettop(L);     // t[key]
				int s_sub = t_sub - 1;         // v
				table_update_deep_recursive(L, t_sub, s_sub);
				lua_pop(L, 1); // t[key] をポップして戻す
			}
			else {
				// それ以外なら、通常の代入処理を実行
				lua_pop(L, 1); // t[key] のチェック用残渣を一度捨てる
				
				lua_pushvalue(L, -2); // key_copy
				lua_pushvalue(L, -2); // value_copy
				lua_settable(L, target_table_idx); // t[key_copy] = value_copy
			}
			lua_pop(L, 1); // 元の value をポップして次へ
		}
	}

	int l_table_update_deep(lua_State *L)
	{
		int top = lua_gettop(L);
		if (top < 1) return 0;

		luaL_checktype(L, 1, LUA_TTABLE);

		for (int i = 2; i <= top; i++) {
			if (lua_istable(L, i)) {
				table_update_deep_recursive(L, 1, i);
			}
		}
		lua_pushvalue(L, 1);
		return 1;
	}

	// 3. 【table.keyset】：フィルタリング関数付きの超高速キー抽出エンジン
	// table.insert の多重テーブル捏造テロを 100% 門前払い消去する
	int l_table_keyset(lua_State *L)
	{
		luaL_checktype(L, 1, LUA_TTABLE); // 第1引数: 対象テーブル t
		bool has_filter = (lua_gettop(L) >= 2 && lua_isfunction(L, 2)); // 第2引数: フィルタ関数 f (任意)

		lua_newtable(L); // 戻り値となる成果物配列 ks ➔ インデックスは [top]
		int result_idx = lua_gettop(L);

		// 🛡️ 【最重要防衛】：元の第1引数テーブル（t）にメタテーブルが存在するかチェック
		if (lua_getmetatable(L, 1)) {
			// メタテーブルが存在した場合、現在作成した新規テーブル（result_idx）へガチッと結合！
			lua_setmetatable(L, result_idx); 
		}

		int count = 1;
		lua_pushnil(L);
		while (lua_next(L, 1) != 0) {
			bool insert_ok = true;
			if (has_filter) {
				lua_pushvalue(L, 2);  
				lua_pushvalue(L, -3); 
				lua_pushvalue(L, -3); 
				if (lua_pcall(L, 2, 1, 0) == 0) {
					insert_ok = lua_toboolean(L, -1);
					lua_pop(L, 1); 
				} else {
					insert_ok = false;
					lua_pop(L, 1); 
				}
			}

			if (insert_ok) {
				lua_pushvalue(L, -2); 
				lua_rawseti(L, result_idx, count++); 
			}
			lua_pop(L, 1); 
		}

		return 1; // 成果物テーブル ks のみがスタックに乗った状態で返却
	}

/*
// ❌ 2. table.reverse(t) のC++最速ネイティブ実装
int l_table_reverse(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	int n = lua_objlen(L, 1); // 配列の長さをネイティブ取得 (#t)
	
	int a = 1;
	int b = n;
	while (a < b) {
		lua_rawgeti(L, 1, a); // t[a]
		lua_rawgeti(L, 1, b); // t[b]
		
		lua_rawseti(L, 1, a); // t[a] = t[b]
		lua_rawseti(L, 1, b); // t[b] = t[a]
		
		a++;
		b--;
	}
	return 0; // 戻り値なし
}
*/

/*
// ❌ 3. table.max_index(t) のC++最速ネイティブ実装
int l_table_max_index(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	int max_idx = 0;
	
	lua_pushnil(L);
	while (lua_next(L, 1) != 0) {
		if (lua_isnumber(L, -2)) {
			int k = lua_tointeger(L, -2);
			if (k > max_idx) max_idx = k;
		}
		lua_pop(L, 1); // valueをポップ
	}
	lua_pushinteger(L, max_idx);
	return 1;
}
*/
} // namespace util_table
