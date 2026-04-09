#pragma once
#include "lua_api/l_base.h"

class ModApiUnicode : public ModApiBase {
public:
    // Lua に API を登録する
    static void Initialize(lua_State *L, int top);

    //
    // UTF-8 decoding API（本体）
    //
    static int l_utf8toCodepoint(lua_State *L);

    //
    // Lua5.3 utf8.* 互換 API（予約・空実装）
    //
    static int l_utf8_len(lua_State *L);
    static int l_utf8_sub(lua_State *L);
    static int l_utf8_codes(lua_State *L);
};
