#include "lua_api/l_unicode.h"
#include "lua_api/l_internal.h"

// ------------------------------------------------------------
// UTF-8 decoding (移植版)
// ------------------------------------------------------------
int ModApiUnicode::l_utf8toCodepoint(lua_State *L)
{
    size_t len;
    const char *str = luaL_checklstring(L, 1, &len);

    const unsigned char *s = (const unsigned char *)str;
    size_t i = 0;

    lua_newtable(L);  // 結果の配列テーブル
    int out_index = 1;

    while (i < len) {
        uint32_t cp = 0;
        unsigned char c = s[i];

        if (c < 0x80) {
            cp = c;
            i += 1;
        }
        else if ((c >> 5) == 0x6) {
            if (i + 1 >= len) break;
            cp = ((c & 0x1F) << 6) | (s[i+1] & 0x3F);
            i += 2;
        }
        else if ((c >> 4) == 0xE) {
            if (i + 2 >= len) break;
            cp = ((c & 0x0F) << 12) |
                 ((s[i+1] & 0x3F) << 6) |
                 (s[i+2] & 0x3F);
            i += 3;
        }
        else if ((c >> 3) == 0x1E) {
            if (i + 3 >= len) break;
            cp = ((c & 0x07) << 18) |
                 ((s[i+1] & 0x3F) << 12) |
                 ((s[i+2] & 0x3F) << 6) |
                 (s[i+3] & 0x3F);
            i += 4;
        }
        else {
            cp = 0xFFFD;
            i += 1;
        }

        lua_pushinteger(L, cp);
        lua_rawseti(L, -2, out_index++);
    }

    return 1; // テーブルを返す
}

// ------------------------------------------------------------
// Lua5.3 utf8.* 予約（空実装）
// ------------------------------------------------------------
int ModApiUnicode::l_utf8_len(lua_State *L)
{
    lua_pushnil(L);
    return 1;
}

int ModApiUnicode::l_utf8_sub(lua_State *L)
{
    lua_pushnil(L);
    return 1;
}

int ModApiUnicode::l_utf8_codes(lua_State *L)
{
    lua_newtable(L);
    return 1;
}

// ------------------------------------------------------------
// API 登録
// ------------------------------------------------------------
#define API_FCT(name) \
    lua_pushcfunction(L, l_##name); \
    lua_setfield(L, unicode_table, #name)

void ModApiUnicode::Initialize(lua_State *L, int top)
{
    lua_newtable(L);                 // core.unicode テーブル作成
    int unicode_table = lua_gettop(L);

    API_FCT(utf8toCodepoint);

    // Lua5.3 互換 API の予約（空実装） ※絶対削除禁止
    API_FCT(utf8_len);
    API_FCT(utf8_sub);
    API_FCT(utf8_codes);

    lua_setfield(L, top, "unicode");
}
