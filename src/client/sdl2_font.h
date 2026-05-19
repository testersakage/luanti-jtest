// src/client_sdl2_font.h
#pragma once

// compile switch
#ifndef UTF8_ATLAS
    #define UTF8_ATLAS 0
#endif

#ifndef UTF8_SDL2_ATLAS
    #define UTF8_SDL2_ATLAS 1
#endif

#ifndef UTF8_SDL2_FREETYPE
    #define UTF8_SDL2_FREETYPE 0
#endif

#include <string>
#include "irrlichttypes.h"
// ... 既存の宣言の下に ...
namespace sdl2_font { // sdl2 に統一！

	void* get_library_ptr();
	void* get_face_ptr();
	u32 get_last_char_advance();

	// フォントファイル情報の保持
	std::string get_last_path();
	unsigned int get_last_size();
	int get_last_index();

	// 今現在、実際にライブラリの電源を握って動かしている主の名前を返す
	std::string get_current_owner();

	/**
	 * フォントエンジンの初期化
	 * @param font_path TTF/TTCフォントへのパス
	 * @param font_size 描画サイズ（看板用なら12程度）
	 * @param requested_index フォント内のインデックス
	 * @param owner 【将来への伏線】呼び出し主の識別名（デフォルト値により、既存のFT側のコード修正は不要！）
	 * @return 成功ならtrue
	 */
	bool init(const std::string &font_path, unsigned int font_size, int requested_index, const std::string &owner = "FT_ENGINE");

	int get_last_error(); 

	/**
	 * リソースの解放
	 */
	void cleanup();
} // namespace sdl2_font
