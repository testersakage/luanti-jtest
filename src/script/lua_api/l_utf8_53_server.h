// src/script/lua_api/l_utf8_53_server.h
#pragma once

#include "lua_api/l_internal.h" // これが Lua 関連のヘッダーを一括で正しく読み込みます
#include "irrlichttypes.h"
#include <lua.h>
#include <lauxlib.h>

/**
 * Lua API: utf8 (Lua 5.3 互換ライブラリ)
 * 
 * Luanti サーバー環境に Lua 5.3 準拠の unicode 操作関数を登録するためのクラスです。
 * 実装本体は l_utf8_53_server.cpp に記述します。
 */
// class定義でエラー出たら以下(ModApiServer側)に修正
//#include "lua_api/l_base.h"
//class ModApiServerCJK : public ModApiBase
class LuaUTF8 {
public:
	/**
	 * Lua環境の初期化時に呼ばれ、"utf8" テーブルをグローバルに登録します。
	 * @param L Lua 状態
	 */
	static void Initialize(lua_State *L, int top = 0);

private:
	/**
	 * utf8.char(···)
	 * 引数のコードポイントを UTF-8 バイト列に変換して返します。
	 */
	static int l_utf8_char(lua_State *L);

	/**
	 * utf8.codes(s)
	 * for p, c in utf8.codes(s) do で使用するイテレータを返します。
	 */
	static int l_utf8_codes(lua_State *L);

	/**
	 * utf8.codepoint(s [, i [, j]])
	 * 指定範囲の文字のコードポイントを整数として返します。
	 */
	static int l_utf8_codepoint(lua_State *L);

	/**
	 * utf8.len(s [, i [, j]])
	 * 文字列の UTF-8 文字数を返します。
	 */
	static int l_utf8_len(lua_State *L);

	/**
	 * utf8.offset(s, n [, i])
	 * i から数えて n 文字目のバイト位置を返します。
	 */
	static int l_utf8_offset(lua_State *L);

	// Unicode EAW (East Asian Width) 

	static int l_utf8_eaw_width(lua_State *L);
	static int l_utf8_eaw_truncate(lua_State *L);

};
