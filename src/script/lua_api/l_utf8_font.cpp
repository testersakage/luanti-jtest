#include "l_utf8_font.h"
#include "client/utf8_fontengine.h"

// これを追加！
#include "../../utf8_53.h" 
#include "irrlichttypes.h"

// utf8.font_width(text)
int LuaUTF8Font::l_utf8_font_width(lua_State *L)
{
    const char *text = luaL_checkstring(L, 1);
    
    // 先ほど作った C++ ロジックを呼び出す
    u32 width = UTF8FontEngine::getTextWidth(text);
    
    lua_pushinteger(L, width);
    return 1;
}

// utf8.font_wrap(text, max_width)
int LuaUTF8Font::l_utf8_font_wrap(lua_State *L)
{
    const char *text = luaL_checkstring(L, 1);
    u32 max_w = (u32)luaL_checkinteger(L, 2);
    
    std::vector<std::string> lines = UTF8FontEngine::wrapText(text, max_w);
    
    // Luaのテーブルとして返す
    lua_newtable(L);
    for (size_t i = 0; i < lines.size(); i++) {
        lua_pushstring(L, lines[i].c_str());
        lua_rawseti(L, -2, i + 1); // Luaは1始まり
    }
    return 1;
}

// l_utf8_font_truncate(lua_State *L)
int LuaUTF8Font::l_utf8_font_truncate(lua_State *L)
{
	const char *text = luaL_checkstring(L, 1);
	u32 max_w = (u32)luaL_checkinteger(L, 2);

	std::string result = UTF8FontEngine::truncateText(text, max_w);

	lua_pushstring(L, result.c_str());
	return 1;
}

void LuaUTF8Font::Initialize(lua_State *L, int top)
{
    // 既存の "utf8" テーブルを取得（なければ作成）
    lua_getglobal(L, "utf8");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "utf8");
    }

    // --- ここから独自拡張の登録 ---

    // utf8.font_width(text)
    lua_pushcfunction(L, l_utf8_font_width);
    lua_setfield(L, -2, "font_width");

    // utf8.font_wrap(text, max_px)
    lua_pushcfunction(L, l_utf8_font_wrap);
    lua_setfield(L, -2, "font_wrap");

    // utf8.font_truncate(text, max_px)
    lua_pushcfunction(L, l_utf8_font_truncate);
    lua_setfield(L, -2, "font_truncate");

    // utf8.font_truncate(text, max_px) をこれみよがしに追加
    lua_pushcfunction(L, l_utf8_font_truncate);
    lua_setfield(L, -2, "font_truncate");	

    lua_pop(L, 1); // utf8テーブルをポップ
}

