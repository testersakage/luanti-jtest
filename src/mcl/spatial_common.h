// src/mcl/spatial_common.h
#pragma once
#include <lua.hpp>
#include <string>

namespace mcl_spatial {
	
	// 🧱 1. 【空間走査数理本体】：指定座標の Content ID と param2 を一本釣り（将来の生筋肉直結ターゲット）
	void get_node_raw_cpp(lua_State *L, int x, int y, int z, int& cid, int& param2);

	// 🌊 2. 【グループ属性検品本体】：Content ID から探索クラス属性を逆算キャッシュルックアップ
	std::string get_node_class_cpp(lua_State *L, int cid);
}
