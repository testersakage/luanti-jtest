/*
 * l_server_cjk.cpp
 * Server-side Unicode / CJK API
 */

#include "l_server_cjk.h"
#include "cjk_common.h"

#include "lua_api/l_internal.h"
#include "lua_api/l_base.h"

/*--------------------------------------------------------------
 * Lua bindings
 *--------------------------------------------------------------*/

// utf8_to_codepoints(text)
int ModApiServerCJK::l_utf8_to_codepoints(lua_State *L)
{
    NO_MAP_LOCK_REQUIRED;

    std::string text = luaL_checkstring(L, 1);
    std::vector<uint32_t> cps = cjk::utf8_to_codepoints(text);

    lua_newtable(L);
    int i = 1;
    for (uint32_t cp : cps) {
        lua_pushinteger(L, cp);
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

// utf8_len(text)
int ModApiServerCJK::l_utf8_len(lua_State *L)
{
    NO_MAP_LOCK_REQUIRED;

    std::string text = luaL_checkstring(L, 1);
    lua_pushinteger(L, cjk::length(text));
    return 1;
}

// utf8_sub(text, start, end)
int ModApiServerCJK::l_utf8_sub(lua_State *L)
{
    NO_MAP_LOCK_REQUIRED;

    // 未実装（必要なら後で実装）
    lua_pushstring(L, "");
    return 1;
}

// utf8_codes(text)
int ModApiServerCJK::l_utf8_codes(lua_State *L)
{
    NO_MAP_LOCK_REQUIRED;

    // 未実装（必要なら後で実装）
    lua_newtable(L);
    return 1;
}

/*--------------------------------------------------------------
 * Registration
 *--------------------------------------------------------------*/

void ModApiServerCJK::Initialize(lua_State *L, int top)
{
    luaL_Reg reg[] = {
        {"utf8_to_codepoints", l_utf8_to_codepoints},
        {"utf8_len",           l_utf8_len},
        {"utf8_sub",           l_utf8_sub},
        {"utf8_codes",         l_utf8_codes},
        {NULL, NULL},
    };

    luaL_register(L, "core.unicode", reg);
    lua_pop(L, 1);
}
