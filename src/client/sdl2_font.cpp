// src/client/sdl2_font.cpp
#define SDL_MAIN_HANDLED
#include "sdl2_font.h"
#include <SDL2/SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "log.h"

#include "porting.h"      // porting::path_user を使うため
#include "filesys.h"      // fs::PathExists を使うため
#include "util/string.h"  // DIR_DELIM を使うため（環境によってはこちら）
#include "settings.h" // これで g_settings が使える
#include "content/subgames.h" // ゲームリスト取得
#include "script/common/l_utf8sign.h"
//#include <iostream> // これを追加 debug表示用


namespace sdl2_font {

	static FT_Library ft_library = nullptr;
	static FT_Face ft_face = nullptr;
	static int last_ft_error = 0; 
	static u32 last_char_advance = 0; 
	static std::string last_attempted_path = "";

	u32 get_last_char_advance() { return last_char_advance; }
	void* get_library_ptr() { return (void*)ft_library; }
	void* get_face_ptr()    { return (void*)ft_face; }
	int get_last_error() { return last_ft_error; }
	std::string get_last_path() { return last_attempted_path; }

	// ─── 🏆 【マルチフォント化を匂わせる、クリーンな絶対記憶インフラ】 ───
	static std::string s_last_owner = ""; // 最後に初期化した主の名前
	static std::string s_last_path  = ""; // 最後に初期化したフォントパス
	static unsigned int s_last_size = 0;  // 最後に初期化したフォントサイズ
	static int s_last_index         = -1; // 最後に初期化したインデックス

	// 外部から現在の主の状態を覗き見させるためのゲッター
	std::string get_current_owner() { return s_last_owner; }
	unsigned int get_last_size()   { return s_last_size; }
	int get_last_index()           { return s_last_index; }


// ─── 🔄 【進化版初期化関数：引数の最後に owner を残してロマンを完全定着】 ───
bool init(const std::string &font_path, unsigned int font_size, int requested_index, const std::string &owner)
{
	// ─── 🛡️ 【多重初期化・二重ロードを 100% 物理防衛する一本道ガード】 ───
	// 「主の名前」と「フォントの住所」と「サイズ」が前回と 1px も狂いなく同じなら、
	// すでに FreeType の顔（Face）はスタンバイ状態なので、重いロードを完全スルーして爆速リターン！
	if (ft_library != nullptr && s_last_owner == owner && s_last_path == font_path && s_last_size == font_size) {
		return true;
	}

	actionstream << "SDL2Font: [TRACE] init called by [" << owner << "]. Path: " << font_path 
	             << " Size: " << font_size << " Requested Index: " << requested_index << std::endl;
	last_attempted_path = font_path;

	// FreeType ライブラリの大元初期化 (シングルトン / TTF_WasInit 代替ガード)
	if (ft_library == nullptr) {
		FT_Error err = FT_Init_FreeType(&ft_library);
		if (err) {
			last_ft_error = (int)err;
			errorstream << "SDL2Font: Could not init FreeType library, err=" << last_ft_error << std::endl;
			return false;
		}
	}

	// 古いフォントを安全に解放
	if (ft_face) {
		FT_Done_Face(ft_face);
		ft_face = nullptr;
	}

	// 職人の安全インデックス判定 (0番で読み込んで含まれているフォント数を確認する)
	FT_Face temp_face;
	if (FT_New_Face(ft_library, font_path.c_str(), 0, &temp_face)) {
		errorstream << "SDL2Font: FAILED to load font file at " << font_path << std::endl;
		return false;
	}
	int num_faces = temp_face->num_faces;
	FT_Done_Face(temp_face);

	// 複数あって、かつ範囲内ならリクエストを採用。それ以外は0。
	int final_index = (num_faces > 1 && requested_index >= 0 && requested_index < num_faces) 
	                  ? requested_index : 0;

	// 本番の読み込み
	FT_Error err = FT_New_Face(ft_library, font_path.c_str(), final_index, &ft_face);
	if (err) {
		last_ft_error = (int)err;
		errorstream << "SDL2Font: FAILED to load face at index " << final_index << std::endl;
		return false;
	}

	// 文字マップとサイズの設定
	FT_Select_Charmap(ft_face, FT_ENCODING_UNICODE);
	FT_Set_Pixel_Sizes(ft_face, 0, font_size);
	
	// ─── 🎯 【聖なる所有権 ＆ スペックの確定メモ】 ───
	// ロードが完全に成功したため、現在の状態を脳内メモリへガチッと刻印！
	s_last_owner = owner;
	s_last_path  = font_path;
	s_last_size  = font_size;
	s_last_index = final_index; 

#if UTF8_SDL2_ATLAS
	// 外部Atlasが利用可能かの確認（先ほど大掃除した空文字列引数仕様へ完全対応！）
	UTF8SignManager::getInstance()->loadGrimoire("");
#endif

	return true;
}

void cleanup()
{
	if (ft_face) {
		FT_Done_Face(ft_face);
		ft_face = nullptr;
	}
	if (ft_library) {
		FT_Done_FreeType(ft_library);
		ft_library = nullptr;
	}
	s_last_owner = "";
	s_last_path  = "";
	s_last_size  = 0;
	s_last_index = -1;
	SDL_Quit();
}

} // namespace
