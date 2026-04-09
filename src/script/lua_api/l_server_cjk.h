// l_server_cjk.h
#pragma once
#include "lua_api/l_base.h"

class ModApiServerCJK : public ModApiBase
{
public:
    static void Initialize(lua_State *L, int top);

    static int l_utf8_to_codepoints(lua_State *L);

    // Lua5.3 互換 API（空実装）
    static int l_utf8_len(lua_State *L);
    static int l_utf8_sub(lua_State *L);
    static int l_utf8_codes(lua_State *L);
};
