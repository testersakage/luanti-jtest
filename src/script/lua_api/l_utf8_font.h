#pragma once

#include "lua_api/l_internal.h"

class LuaUTF8Font {
public:
    // scripting_client.cpp などの初期化時に呼ばれる関数
    static void Initialize(lua_State *L, int top);

private:
    // utf8.font_width(text)
    static int l_utf8_font_width(lua_State *L);
    
    // utf8.font_wrap(text, max_width)
    static int l_utf8_font_wrap(lua_State *L);

    // utf8.font_truncate(lua_State *L)
    static int l_utf8_font_truncate(lua_State *L);

};
