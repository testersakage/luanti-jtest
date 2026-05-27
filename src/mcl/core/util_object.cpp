// src/mcl/core/util_object.cpp
#include "util_object.h"
#include "mcl/stacktrace.h" // 
#include <cmath>
#include <string>
#include <algorithm>

namespace util_object {
/*
	// 💡 浮動小数点を少数第2位で超速丸めして比較するインライン
	inline bool close_enough_cpp(double a, double b) {
		return std::floor(a * 100.0 + 0.5) == std::floor(b * 100.0 + 0.5);
	}

	// ❌ 1. props_changed(props, oldprops) のC++完全移植（前半戦の成果）
	int l_props_changed(lua_State *L)
	{
		if (!lua_istable(L, 1) || !lua_istable(L, 2)) {
			lua_pushboolean(L, true);
			return 1;
		}
		bool changed = false;
		lua_pushnil(L);
		while (lua_next(L, 1) != 0) {
			lua_pushvalue(L, -2);
			lua_gettable(L, 2);
			if (lua_isnil(L, -1) || lua_type(L, -1) != lua_type(L, -2)) {
				changed = true;
				lua_pop(L, 2); break;
			}
			if (lua_isnumber(L, -2)) {
				if (!close_enough_cpp(lua_tonumber(L, -2), lua_tonumber(L, -1))) { changed = true; lua_pop(L, 2); break; }
			} else if (lua_isstring(L, -2)) {
				if (std::string(lua_tostring(L, -2)) != std::string(lua_tostring(L, -1))) { changed = true; lua_pop(L, 2); break; }
			} else if (lua_isboolean(L, -2)) {
				if (lua_toboolean(L, -2) != lua_toboolean(L, -1)) { changed = true; lua_pop(L, 2); break; }
			}
			lua_pop(L, 1); lua_pop(L, 1);
		}
		lua_settop(L, 1);
		lua_pushboolean(L, changed);
		return 1;
	}
*/

/*
	// ❌ 2. mcl_util.get_object_center(obj) のC++完全移植（爆発・当たり判定の中心点数理）
	int l_get_object_center(lua_State *L)
	{
		// 👑 コロン表記・引数の位置ズレを stacktrace レダーで一撃逆探知！
		int target_idx = stacktrace::find_table_by_method(L, "get_properties");
		if (target_idx == 0) return 0;

		// obj:get_properties().collisionbox 取得
		lua_getfield(L, target_idx, "get_properties");
		lua_pushvalue(L, target_idx);
		lua_call(L, 1, 1);
		lua_getfield(L, -1, "collisionbox");
	
		// ymin = collisionbox[2], ymax = collisionbox[5]
		lua_rawgeti(L, -1, 2); double ymin = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_rawgeti(L, -1, 5); double ymax = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_pop(L, 2); // collisionbox と properties テーブルをお片付け

		// obj:get_pos() 取得
		lua_getfield(L, target_idx, "get_pos");
		lua_pushvalue(L, target_idx);
		lua_call(L, 1, 1);
		lua_getfield(L, -1, "y"); double pos_y = lua_tonumber(L, -1); lua_pop(L, 1);

		// pos.y = pos.y + (ymax - ymin) / 2.0
		double center_y = pos_y + (ymax - ymin) / 2.0;
		lua_pushnumber(L, center_y);
		lua_setfield(L, -2, "y"); // テーブル内の y を上書き

		return 1; // 計算完了した {x, y, z} 座標テーブルを Lua へ返却！
	}
*/

/*
	// ❌ 3. mcl_util.target_eye_height(attack) のC++完全修正版（安全お片付け仕様）
	int l_target_eye_height(lua_State *L)
	{
		int target_idx = stacktrace::find_table_by_method(L, "get_luaentity");
		if (target_idx == 0) { lua_pushnumber(L, 0.0); return 1; }

		// 💡 現在のスタックのトップ（一番上の部屋番号）を記録しておく
		int initial_top = lua_gettop(L);

		lua_getfield(L, target_idx, "get_luaentity");
		lua_pushvalue(L, target_idx);
		lua_call(L, 1, 1);
	
		double eye_height = 0.0;
		if (lua_istable(L, -1)) {
			lua_getfield(L, -1, "is_mob");
			bool is_mob = lua_toboolean(L, -1) != 0;
			lua_pop(L, 1);
			if (is_mob) {
				lua_getfield(L, -1, "get_eye_height");
				lua_pushvalue(L, -2);
				lua_call(L, 1, 1);
				eye_height = lua_tonumber(L, -1);
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1); // luaentity

		if (eye_height == 0.0) {
			lua_getfield(L, target_idx, "get_properties");
			lua_pushvalue(L, target_idx);
			lua_call(L, 1, 1);
			lua_getfield(L, -1, "eye_height");
			eye_height = lua_tonumber(L, -1);
			lua_pop(L, 2);
		}

		// 👑 【絶対安全防衛線】：他の関数（l_target_eye_pos）が積んだデータを巻き添え破壊しないよう、
		//    自分がこの関数内で増やしたゴミ（時差の残骸）だけをピンポイントで引き算（ポップ）してお掃除！
		lua_settop(L, initial_top); 
	
		lua_pushnumber(L, eye_height); // 最上部に結果だけをスタンプ！
		return 1;
	}
*/

/*
	// ❌ 4. mcl_util.target_eye_pos のC++完全防衛版（Luaのどんな嫌がらせも笑顔でいなす王座）
	int l_target_eye_pos(lua_State *L)
	{
		int target_idx = stacktrace::find_table_by_method(L, "get_pos");

		// 🚨 【究極の引き算】：もし get_pos を持たない不正なデータや、ただのプレーンな座標テーブル {x,y,z} が直接投げ込まれていたら
		if (target_idx == 0) {
			// もし第1引数が直接 {x,y,z} テーブルだったら、その中身をそのまま安全にコピーして流用！
			if (lua_istable(L, 1)) {
				lua_newtable(L);
				lua_getfield(L, 1, "x"); lua_setfield(L, -2, "x");
				lua_getfield(L, 1, "y"); lua_setfield(L, -2, "y");
				lua_getfield(L, 1, "z"); lua_setfield(L, -2, "z");
				return 1; // 100%安全な有効テーブルを複製して返却！ 空振り（nil）を物理的に絶対起こさせない！
			}
			// 完全に中身が壊れているゴミなら、原点座標を捏造してでも nil 化を阻止してAIの自爆を防衛！
			lua_newtable(L);
			lua_pushnumber(L, 0.0); lua_setfield(L, -2, "x");
			lua_pushnumber(L, 0.0); lua_setfield(L, -2, "y");
			lua_pushnumber(L, 0.0); lua_setfield(L, -2, "z");
			return 1;
		}

		// 💡 正常なオブジェクトであれば、今まで通り最速のFPU計算を執行！
		lua_getfield(L, target_idx, "get_pos");
		lua_pushvalue(L, target_idx);
		lua_call(L, 1, 1); 
		int pos_table_idx = lua_gettop(L);

		lua_pushvalue(L, target_idx);
		l_target_eye_height(L);
		double height = lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, pos_table_idx, "y"); 
		double pos_y = lua_tonumber(L, -1); 
		lua_pop(L, 1);
	
		lua_pushnumber(L, pos_y + height);
		lua_setfield(L, pos_table_idx, "y");

		lua_replace(L, 1);
		lua_settop(L, 1);
		return 1; // 完璧に焼き上がった {x, y, z} テーブルを返却！
	}
*/

/*
	// ❌ 5. mcl_util.set_bone_position(obj, bone, pos, rot, scale) のC++完全移植（ボーン同期）
	int l_set_bone_position(lua_State *L)
	{
		int target_idx = stacktrace::find_table_by_method(L, "get_bone_override");
		if (target_idx == 0) return 0;

		std::string bone = luaL_checkstring(L, target_idx + 1);
	
		// 引数テーブル pos, rot, scale の位置を安全に特定
		int top = lua_gettop(L);
		int pos_idx = 0, rot_idx = 0, scale_idx = 0;
		int tbl_count = 0;
		for (int i = target_idx + 2; i <= top; i++) {
			if (lua_istable(L, i)) {
				tbl_count++;
				if (tbl_count == 1) pos_idx = i;
				if (tbl_count == 2) rot_idx = i;
				if (tbl_count == 3) scale_idx = i;
			}
		}

		// obj:get_bone_override(bone) をキック
		lua_getfield(L, target_idx, "get_bone_override");
		lua_pushvalue(L, target_idx);
		lua_pushstring(L, bone.c_str());
		lua_call(L, 2, 1); // [-1] ov table

		bool need_update = true; // アニメーション同期は常に安全側に倒して更新
		if (need_update) {
			// obj:set_bone_override(bone, { ... }) を執行！
			lua_getfield(L, target_idx, "set_bone_override");
			lua_pushvalue(L, target_idx);
			lua_pushstring(L, bone.c_str());
		
			lua_newtable(L); // 新しい引数テーブル
		
			if (pos_idx > 0) {
				lua_newtable(L); lua_pushvalue(L, pos_idx); lua_setfield(L, -2, "vec");
				lua_pushboolean(L, true); lua_setfield(L, -2, "absolute");
				lua_pushnumber(L, 0.1);   lua_setfield(L, -2, "interpolation");
				lua_setfield(L, -2, "position");
			}
			if (rot_idx > 0) {
				lua_newtable(L); 
				// ラジアン変換 math.rad を C++インライン執行 (v * M_PI / 180.0)
				lua_newtable(L);
				lua_getfield(L, rot_idx, "x"); lua_pushnumber(L, lua_tonumber(L, -1) * M_PI / 180.0); lua_setfield(L, -3, "x"); lua_pop(L, 1);
				lua_getfield(L, rot_idx, "y"); lua_pushnumber(L, lua_tonumber(L, -1) * M_PI / 180.0); lua_setfield(L, -3, "y"); lua_pop(L, 1);
				lua_getfield(L, rot_idx, "z"); lua_pushnumber(L, lua_tonumber(L, -1) * M_PI / 180.0); lua_setfield(L, -3, "z"); lua_pop(L, 1);
				lua_setfield(L, -2, "vec");
				lua_pushboolean(L, true); lua_setfield(L, -2, "absolute");
				lua_pushnumber(L, 0.1);   lua_setfield(L, -2, "interpolation");
				lua_setfield(L, -2, "rotation");
			}
			if (scale_idx > 0) {
				lua_newtable(L); lua_pushvalue(L, scale_idx); lua_setfield(L, -2, "vec");
				lua_pushboolean(L, true); lua_setfield(L, -2, "absolute");
				lua_pushnumber(L, 0.1);   lua_setfield(L, -2, "interpolation");
				lua_setfield(L, -2, "scale");
			}
			lua_call(L, 3, 0);
		}
		return 0;
	}
*/

	// 6. mcl_util.rotation_to_irrlicht(x, y, z) のC++完全移植（3Dマトリクス三角関数筋肉：後半戦の成果）
	int l_rotation_to_irrlicht(lua_State *L)
	{
		// 1. 引数の厳格な型検品（改札）
		double x = luaL_checknumber(L, 1); 
		double y = luaL_checknumber(L, 2); 
		double z = luaL_checknumber(L, 3);
	
		const double NINETY_DEG = M_PI / 2.0;
		double cx = std::cos(x), sx = std::sin(x);
		double cy = std::cos(y), sy = std::sin(y);
		double cz = std::cos(z), sz = std::sin(z);

		double m00 = cy * cz - sx * sy * sz;
		double m10 = cz * sx * sy + cy * sz;
		double m11 = cx * cz;
		double m12 = -cy * cz * sx + sx * sz;
		double m20 = -cx * sy; 
		double m21 = sx; 
		double m22 = cx * cy;

		double tx = 0.0, ty = 0.0, tz = 0.0;
	
		// 2.  【完全対称（Symmetry）分岐】：戻り値の個数を Lua 側のオリジナルと1ビットの狂いもなく完全同期！
		if (m20 < 1.0) {
			if (m20 > -1.0) { 
				ty = std::asin(m20); 
				tz = std::atan2(m10, m00); 
				tx = std::atan2(m21, m22); 
			
				// 元のLua（311行目付近）は、この正常ルートでは戻り値を返していないため、
				//    C++側も何もスタックに積まずに return 0; で正常終了させる。
				return 0; 
			}
			else { 
				ty = -NINETY_DEG; 
				tz = -std::atan2(-m12, m11); 
				tx = 0.0; 
			}
		} else { 
			ty = NINETY_DEG; 
			tz = std::atan2(-m12, m11); 
			tx = 0.0; 
		}

		// 【スタック安全ガード】：lua_settop を引き算（消去）し、安全に3つの例外戻り値のみを積む！
		lua_pushnumber(L, tx); 
		lua_pushnumber(L, ty); 
		lua_pushnumber(L, tz);
		return 3;
	}

} // namespace util_object
