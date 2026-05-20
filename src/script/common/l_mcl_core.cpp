// src/script/common/l_mcl_core.cpp
#include "l_mcl_core.h"
#include "mcl/core/tga_encoder.h"

namespace l_mcl_core {

void Initialize(lua_State *L, int top)
{
	// ─── 🏆 【Lua版と100%完全同一名：グローバル「tga_encoder」の直接錬金】 ───
	// 1. まず、Luaの世界のグローバル空間に「tga_encoder = {}」を新規作成してスタックに乗せる
	lua_newtable(L); 

	// 2. その中に、さらにオブジェクト指向の器となる「image = {}」のサブテーブルを仕込む
	lua_newtable(L); 

	// 3. TGAエンコーダーのC++心臓部（tga_encode）を、この「image」テーブルの中に
	// 「save」という名前（Lua版と200%完全一致！）で公式バインド登録！
	// これにより、Lua側から「tga_encoder.image:save()」で叩かれた瞬間にC++へ直通します！
	tga_encoder::Initialize(L, lua_gettop(L)); // ※ tga_encoder.cpp側で "save" という名前で登録されます
	
	// 「image」テーブルを「tga_encoder」のフィールドとしてガチッと紐付け
	lua_setfield(L, -2, "image");

	// 4. 最後に、この完璧なC++製テーブルを、Luaの最上位グローバル（_G）空間へ
	// 「tga_encoder」という名前のまま、ダイレクトに上書き公式登録！
	lua_setglobal(L, "tga_encoder");
}

} // namespace l_mcl_core
