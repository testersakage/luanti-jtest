// src/script/lua_api/l_utf8_53_client.h
#pragma once

#include "lua_api/l_internal.h"
#include <lua.h>
#include <lauxlib.h>

/**
 * Lua API: utf8wrap (クライアント環境用)
 * 
 * 看板の描画サイズや改行位置を、物理的なピクセル幅に基づいて
 * 制御するための便利なラッパー関数群を登録します。
 */
class LuaUTF8Client {
public:
	/**
	 * クライアント側Lua環境の初期化時に呼ばれ、
	 * "utf8wrap" テーブルをグローバルに登録します。
	 */
	static void Initialize(lua_State *L, int top = 0);

private:
	// --- utf8wrap テーブルの各窓口 (w21, w22, w24 の実体) ---

	// utf8wrap.width(s [, han_w, zen_w])
	static int l_utf8wrap_width(lua_State *L);
	
	// utf8wrap.truncate(s, max_px [, han_w, zen_w])
	static int l_utf8wrap_truncate(lua_State *L);

	// utf8wrap.lines(s, max_px [, han_w, zen_w])
	static int l_utf8wrap_wrap(lua_State *L);

	// utf8wrap.lines(s, max_px [, han_w, zen_w])
	static int l_utf8wrap_lines(lua_State *L);

	// (必要なら追加) 文字列をコードポイントのテーブルに分解
	static int l_utf8wrap_to_table(lua_State *L);
};
