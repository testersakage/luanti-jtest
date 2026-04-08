/*
 * l_client_cjk.h
 * Lua API for CJK / Unicode functions (client-side only)
 */

#pragma once

#include "lua_api/l_base.h"

class ModApiClientCJK : public ModApiBase
{
public:
    // Register functions to Lua (client-side)
    static void InitializeClient(lua_State *L, int top);

    /*--------------------------------------------------------------
     * Lua-exposed functions
     *--------------------------------------------------------------*/
    // text → atlas glyph indices (12x12 bitmap font)
    static int l_text_to_atlas_glyphs(lua_State *L);

    // UTF-8 → codepoints
    static int l_utf8_to_codepoints(lua_State *L);

    // codepoints → atlas glyphs
    static int l_codepoints_to_atlas_glyphs(lua_State *L);

private:
    // text → TTF glyph indices (variable size)
    static int l_text_to_glyphs(lua_State *L);

    // codepoints → TTF glyphs
    static int l_codepoints_to_glyphs(lua_State *L);
};
