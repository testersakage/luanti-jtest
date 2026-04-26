// src/client_sdl2_font.h
#pragma once

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

/**
 * フォントエンジンの初期化
 * @param font_path TTF/TTCフォントへのパス
 * @param font_size 描画サイズ（看板用なら12程度）
 * @return 成功ならtrue
 */
bool init(const std::string &font_path, unsigned int font_size, int requested_index);
//bool init(const std::string &font_path, unsigned int font_size);
	int get_last_error(); 
//	std::string get_last_path(); 

/**
 * 指定した文字(コードポイント)をRGBAバッファへ描画する
 * @param code Unicodeコードポイント
 * @param dest 書き込み先のRGBAバッファ
 * @param dest_w バッファの幅
 * @param dest_h バッファの高さ
 * @return 成功ならtrue
 */
bool render_to_buffer(uint32_t code, unsigned char* dest, int dest_w, int dest_h, bool antialias, unsigned int font_size, unsigned int baseline);
//bool render_to_buffer(uint32_t code, unsigned char* dest, int dest_w, int dest_h, bool antialias, unsigned int font_size, unsigned int baseline);

/**
 * リソースの解放
 */
void cleanup();

} // namespace sdl2_font
