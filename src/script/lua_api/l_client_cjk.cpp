/*
 * l_client_cjk.cpp
 * Lua API for CJK / Unicode functions (client-side only)
 */

#include "l_client_cjk.h"
#include "client/client.h"
#include "client/client_unicode.h"

// Register functions to Lua
void ModApiClientCJK::Initialize(lua_State *L)
{
    // Register Lua functions
    API_FCT(text_to_atlas_glyphs);
    API_FCT(text_to_glyphs);
    API_FCT(utf8_to_codepoints);
    API_FCT(codepoints_to_atlas_glyphs);
    API_FCT(codepoints_to_glyphs);
}

/*--------------------------------------------------------------
 * Lua-exposed functions
 *--------------------------------------------------------------*/

int ModApiClientCJK::l_text_to_atlas_glyphs(lua_State *L)
{
    // 1. Lua 引数を取得
    const char *text = luaL_checkstring(L, 1);

    // 2. Client を取得
    Client *client = getClient(L);
    if (!client)
        return 0;

    // 3. ClientUnicode を取得
    ClientUnicode &unicode = client->getUnicode();

    // 4. 実際に atlas glyph を取得
    std::vector<int> glyphs = unicode.textToAtlasGlyphs(text);

    // 5. Lua テーブルとして返す
    lua_newtable(L);
    int i = 1;
    for (int g : glyphs) {
        lua_pushinteger(L, g);
        lua_rawseti(L, -2, i++);
    }

    return 1; // テーブルを返す
}


int ModApiClientCJK::l_text_to_glyphs(lua_State *L)
{
    // TODO: implement
    return 0;
}

int ModApiClientCJK::l_utf8_to_codepoints(lua_State *L)
{
    const char *text = luaL_checkstring(L, 1);

    Client *client = getClient(L);
    if (!client)
        return 0;

    ClientUnicode &unicode = client->getUnicode();
    std::vector<uint32_t> cps = unicode.utf8ToCodepoints(text);

    lua_newtable(L);
    int i = 1;
    for (uint32_t cp : cps) {
        lua_pushinteger(L, cp);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}


int ModApiClientCJK::l_codepoints_to_atlas_glyphs(lua_State *L)
{
    // 引数は Lua テーブル
    luaL_checktype(L, 1, LUA_TTABLE);

    Client *client = getClient(L);
    if (!client)
        return 0;

    ClientUnicode &unicode = client->getUnicode();

    // Lua テーブル → std::vector<uint32_t>
    std::vector<uint32_t> cps;
    size_t len = lua_rawlen(L, 1);
    for (size_t i = 1; i <= len; i++) {
        lua_rawgeti(L, 1, i);
        uint32_t cp = luaL_checkinteger(L, -1);
        lua_pop(L, 1);
        cps.push_back(cp);
    }

    // 変換
    std::vector<int> glyphs = unicode.codepointsToAtlasGlyphs(cps);

    // Lua テーブルで返す
    lua_newtable(L);
    int idx = 1;
    for (int g : glyphs) {
        lua_pushinteger(L, g);
        lua_rawseti(L, -2, idx++);
    }

    return 1;
}

