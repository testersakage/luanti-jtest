// src/script/lua_api/l_utf8_53_server.cpp
#include "l_utf8_53_server.h"
#include "common/c_types.h"
#include "../utf8_53.h"

// class定義でエラー出たら以下(ModApiServer側)に修正
//#include "lua_api/l_internal.h"
//#include "lua_api/l_base.h"

// --- 補助関数: インデックス変換 ---
static size_t get_pos_arg(lua_State *L, int arg_idx, size_t len, size_t default_val)
{
    if (lua_isnoneornil(L, arg_idx))
        return default_val;

    lua_Integer pos = luaL_checkinteger(L, arg_idx);
    if (pos > 0) return (size_t)pos - 1;
    else if (pos < 0) {
        if ((size_t)(-pos) > len) return 0;
        return len + (size_t)pos;
    }
    return 0;
}

// --- 補助関数: utf8.codes 用のイテレータ本体 ---
static int iter_codes(lua_State *L)
{
    size_t len;
    const char *s_ptr = luaL_checklstring(L, 1, &len);
    std::string s(s_ptr, len);
    size_t pos = (size_t)lua_tointeger(L, 2); // 前回の終了位置（0始まり）

    int code_point;
    if (utf8_53::get_next_char(s, pos, code_point)) {
        // 次の開始位置(Luaは1始まりなので+1不要、posが既に次を指している)
        lua_pushinteger(L, pos); 
        lua_pushinteger(L, code_point);
        return 2;
    }
    
    return 0; // 終了
}

// --- Lua API 実装 ---

int LuaUTF8::l_utf8_char(lua_State *L) {
    int n = lua_gettop(L);
    std::string result;
    for (int i = 1; i <= n; i++) {
        utf8_53::push_char(result, (int)luaL_checkinteger(L, i));
    }
    lua_pushlstring(L, result.c_str(), result.length());
    return 1; 
}

int LuaUTF8::l_utf8_codes(lua_State *L) {
    luaL_checkstring(L, 1);
    lua_pushcfunction(L, iter_codes);
    lua_pushvalue(L, 1);
    lua_pushinteger(L, 0); // 初期位置
    return 3;
}

int LuaUTF8::l_utf8_codepoint(lua_State *L) {
    size_t len;
    const char *s_ptr = luaL_checklstring(L, 1, &len);
    std::string s(s_ptr, len);
    size_t i = get_pos_arg(L, 2, len, 0);
    size_t j = get_pos_arg(L, 3, len, i);

    int count = 0;
    size_t pos = i;
    while (pos <= j && pos < len) {
        int cp;
        if (!utf8_53::get_next_char(s, pos, cp))
            return luaL_error(L, "invalid UTF-8 code");
        lua_pushinteger(L, cp);
        count++;
    }
    return count; 
}

int LuaUTF8::l_utf8_len(lua_State *L)
{
    size_t byte_len;
    const char *s_ptr = luaL_checklstring(L, 1, &byte_len);
    std::string s(s_ptr, byte_len);

    size_t i = get_pos_arg(L, 2, byte_len, 0);
    size_t j = get_pos_arg(L, 3, byte_len, byte_len - 1);

    size_t err_pos = 0;
    int count = utf8_53::count_chars(s, i, j, err_pos);

    if (count >= 0) {
        lua_pushinteger(L, count);
        return 1;
    }
    lua_pushboolean(L, false);
    lua_pushinteger(L, err_pos + 1);
    return 2;
}

int LuaUTF8::l_utf8_offset(lua_State *L) {
    size_t len;
    const char *s_ptr = luaL_checklstring(L, 1, &len);
    std::string s(s_ptr, len);
    lua_Integer n = luaL_checkinteger(L, 2);
    size_t i = get_pos_arg(L, 3, len, (n >= 0) ? 0 : len);

    if (i > len) return luaL_error(L, "initial position out of string");

    if (n == 0) {
        while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--;
        lua_pushinteger(L, i + 1);
        return 1;
    }

    size_t pos = i;
    if (n > 0) {
        n--; 
        while (n > 0 && pos < len) {
            int cp;
            if (!utf8_53::get_next_char(s, pos, cp)) break;
            n--;
        }
    } else {
        // 負のnの処理：逆方向に文字の先頭バイトを探してスキャン
        while (n < 0 && pos > 0) {
            pos--; // 1バイト戻る
            // そのバイトが「後続バイト(10xxxxxx)」である間、さらに戻る
            while (pos > 0 && ((unsigned char)s[pos] & 0xC0) == 0x80) {
                pos--;
            }
            n++; // 1文字見つけたのでカウントアップ
        }
    }

    if (n == 0) lua_pushinteger(L, pos + 1);
    else lua_pushnil(L);
    return 1; 
}

/**
 * utf8.eaw_width(s)
 * East Asian Widthに基づく表示幅を返します。
 */
int LuaUTF8::l_utf8_eaw_width(lua_State *L)
{
    size_t len;
    const char *s_ptr = luaL_checklstring(L, 1, &len);
    std::string s(s_ptr, len);

    // 内部ロジック utf8_53 側は名前を変えずに呼び出す
    int total_width = utf8_53::get_string_width(s);
    lua_pushinteger(L, total_width);
    return 1;
}

/**
 * utf8.eaw_truncate(s, max_width)
 * 指定した表示幅で文字列を切り詰めます。
 */
int LuaUTF8::l_utf8_eaw_truncate(lua_State *L)
{
    size_t len;
    const char *s_ptr = luaL_checklstring(L, 1, &len);
    std::string s(s_ptr, len);
    int max_width = (int)luaL_checkinteger(L, 2);

    std::string res = "";
    int current_width = 0;
    size_t pos = 0;
    int cp;

    while (utf8_53::get_next_char(s, pos, cp)) {
        int w = utf8_53::get_char_width(cp);
        if (current_width + w > max_width)
            break;
        
        utf8_53::push_char(res, cp);
        current_width += w;
    }

    lua_pushlstring(L, res.c_str(), res.length());
    return 1;
}


// --- 登録リスト ---
// class定義でエラー出たら以下(ModApiServer側)に修正
//void ModApiServerCJK::Initialize(lua_State *L, int top)
void LuaUTF8::Initialize(lua_State *L, int top)
{
	static const luaL_Reg utf8_funcs[] = {
	    {"char",      LuaUTF8::l_utf8_char},
	    {"codes",     LuaUTF8::l_utf8_codes},
	    {"codepoint", LuaUTF8::l_utf8_codepoint},
	    {"len",       LuaUTF8::l_utf8_len},
	    {"offset",    LuaUTF8::l_utf8_offset},
	    // Lua側から見える名前を eaw_... に設定
	    {"eaw_width",    LuaUTF8::l_utf8_eaw_width},
	    {"eaw_truncate", LuaUTF8::l_utf8_eaw_truncate},	    {NULL, NULL}
	};

    lua_newtable(L);
    luaL_setfuncs(L, utf8_funcs, 0);
    lua_pushstring(L, "[\0-\x7F\xC2-\xF4][\x80-\xBF]*");
    lua_setfield(L, -2, "charpattern");

    // グローバルに "utf8" として公開
    lua_setglobal(L, "utf8");
}
