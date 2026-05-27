// src/mcl/core/tga_encoder.h
#pragma once

#include <string>
#include <vector>
#include "irrlichttypes.h" // Luanti純正の u8, u16, u32 を使うため
#include <lua.hpp>         // Lua API窓口と直結するため

#define L_MCL_MAP_METATABLE "mcl_core_map_object"

namespace tga_encoder {

#pragma pack(push, 1)
	// ─── 📊 【TGA公式規格：18バイト・バイナリヘッダー構造体】 ───
	struct TGAHeader {
		u8  id_length        = 0;  // IDフィールドの長さ (通常は0)
		u8  color_map_type   = 0;  // カラーマップの有無 (通常は0: なし)
		u8  image_type       = 2;  // 画像タイプ (2: フルカラー非圧縮 RGB/RGBA)
		
		// カラーマップ仕様 (使わないので全て0)
		u16 color_map_first  = 0;
		u16 color_map_length = 0;
		u8  color_map_size   = 0;
		
		// 画像の配置情報
		u16 x_origin         = 0;  // X方向の開始位置 (左端: 0)
		u16 y_origin         = 0;  // Y方向の開始位置 (通常下端、上下反転フラグと連動)
		u16 width            = 0;  // 画像の横幅 (ピクセル数)
		u16 height           = 0;  // 画像の縦幅 (ピクセル数)
		u8  bits_per_pixel   = 32; // 1ピクセルの色深度 (32bit: RGBA / 24bit: RGB)
		u8  image_descriptor = 0;  // 画像ディスクリプタ (下4bit: アルファ深度8bit, 上2bit: 左上起点)
	};

	// ─── 🏆 【Luaオブジェクト魔術の完全解体：C++ネイティブ画像構造体】 ───
	struct Image {
		std::vector<u32> pixels;
		u16 width  = 0;
		u16 height = 0;

		Image() = default;
		Image(const std::vector<u32>& input_pixels, u16 w, u16 h)
			: pixels(input_pixels), width(w), height(h) {}
	};

	// ─── 🎨 【色空間・ビット深度翻訳の符号インフラ】 ───
	inline u8 get_pixel_depth(const std::string &format) {
		if (format == "Y8")         return 8;
		if (format == "A1R5G5B5")   return 16;
		if (format == "B8G8R8")     return 24;
		if (format == "B8G8R8A8")   return 32;
		return 32;
	}
#pragma pack(pop)

	int l_tga_encode(lua_State *L);
	void Initialize(lua_State *L, int top);
	
	// ─── ⚡ 【新設：パレット一括変換エンジンの公式目次宣言】 ───
	// 💡 指摘大感謝！これでパレットデータのエンコードも公式機能として開通します！
	void encode_colormap(std::vector<u8> &out_data, const std::vector<u32> &colormap, const std::string &color_format);

	// ─── ⚡ 【新設：RLE圧縮エンジンの公式目次宣言】 ───
	void encode_data_A1R5G5B5_rle(std::vector<u8> &out_data, const std::vector<u32> &pixels);

	// ─── ⚡ 【新設：24bit RLE圧縮エンジンの公式目次宣言】 ───
	void encode_data_B8G8R8_rle(std::vector<u8> &out_data, const std::vector<u32> &pixels);

	// ─── ⚡ 【新設：32bit RLE圧縮エンジンの公式目次宣言】 ───
	void encode_data_B8G8R8A8_rle(std::vector<u8> &out_data, const std::vector<u32> &pixels);

	// ─── 🔤 【Lua API 窓口関数（17個の関数の大トリ）】 ───
	int l_tga_encode(lua_State *L);

	// ─── 🏆 【LuaとC++を公式に結ぶ、心臓部の配線登録関数】 ───
	void Initialize(lua_State *L, int top);

} // namespace tga_encoder
