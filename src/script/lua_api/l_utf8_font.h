// src/script/lua_api/l_utf8_font.h
#pragma once

#include "lua_api/l_internal.h"

class LuaUTF8Font {
public:
    // scripting_client.cpp 等の初期化時に呼ばれ、
    // Lua 側に 'mcl_utf8' (または utf8) テーブルを登録します。
    static void Initialize(lua_State *L, int top);

private:
    // 1. mcl_utf8.get_width(text) -> returns total pixel width (6px/12px basis)
    static int l_get_width(lua_State *L);
    
    // 2. mcl_utf8.truncate(text, max_px) -> returns truncated string
    static int l_truncate(lua_State *L);

    // 3. mcl_utf8.to_table(text) -> returns table of codepoints {123, 456, ...}
    // 【決定した逆変換】push_char の対となる機能
    static int l_to_table(lua_State *L);

    // ※ 必要であれば、以前の自動改行ロジックもここに統合可能です
    // 4. mcl_utf8.get_lines(text, max_px) -> returns table of strings
    static int l_get_lines(lua_State *L);
};
