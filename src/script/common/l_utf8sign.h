// src/script/common/l_utf8sign.h
#pragma once

#include "lua_api/l_base.h"
#include "irrlichttypes_bloated.h"
#include <string>
#include <vector>

/* Mineclonia mcl_signs/init.lua の定数設定
	SIGN_WIDTH = 115		看板の物理サイズ（縦横）
	LINE_LENGTH = 15		一行の最大文字数（半角）
	NUMBER_OF_LINES = 4		最大行数
	LINE_HEIGHT = 14		一行の高さ（行間込みのpx）
	CHAR_WIDTH = 5			一文字の横幅(px)
	SIGN_GLOW_INTENSITY = 14 看板文字の明るさ
	CR_CODEPOINT = utf8.codepoint("\r")		改行コードポイント
	WRAP_CODEPOINT = utf8.codepoint("‐")	ラップコードポイント
	DEFAULT_COLOR = "#000000"				初期設定色
*/

// アトラス(旧) + EX(拡張) 用
struct UTF8AtlasConfig {
	u32 sign_width = 115;
	u32 char_w = 12;
	u32 line_height = 14;
	u32 padding_x = 15;
	// 職人こだわりのEX項目
	u32 ex_max_lines = 4;
	u32 ex_char_w_base = 5;
};

// FreeType(新) 看板用 構造体
struct UTF8FTConfig {
	// Font File
	std::string ttf_name = "../fonts/NotoSansCJKjp-Regular.otf";
	int font_index = 0;

	// Font Setting
	u32 font_size = 16;
	u32 baseline_y = 14;
	video::SColor default_color = video::SColor(255, 0, 0, 0);
	bool antialias = false;

	// combine setting
	u32 sign_width = 115;
	u32 line_height = 18;
	u32 max_lines = 4;
	u32 char_w_base = 5;
	u32 padding_x = 0;
	u32 padding_y = 0;

	// 診断用
	std::string last_ft_error = "None";
	int last_codes_size = 0;
	std::string last_spec = "";
	std::string last_font_path = "";
};


// サーバー・クライアント両方のメモリに個別に存在する「物差し」
class UTF8SignManager {
public:

	static UTF8SignManager* getInstance();
//	UTF8SignConfig config;

	UTF8AtlasConfig atlas;
    UTF8FTConfig ft;

	// 文字列の幅を現在の設定（char_w_base等）から計算する共通ロジック
	u32 getTextWidth(const std::string &text);

private:
	UTF8SignManager() {}
	static UTF8SignManager *m_instance;
};

namespace l_utf8sign {
	// Lua API: minetest.utf8sign.set_config(table)
	int l_set_config(lua_State *L);

	// Lua API: minetest.utf8sign.get_config()
	int l_get_config(lua_State *L);

	// Lua API: minetest.utf8sign.get_text_size(text)
	int l_get_text_size(lua_State *L);

	// API登録用
	void Initialize(lua_State *L, int top);
}
