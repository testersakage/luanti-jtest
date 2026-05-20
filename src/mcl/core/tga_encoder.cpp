// src/mcl/core/tga_encoder.cpp
#include "tga_encoder.h"
#include "log.h"
#include <fstream>
#include <vector>
#include <cmath>

namespace tga_encoder {

// 🔤 【旧75-128行】Luaの encode_colormap 関数を完全にC++ネイティブへ裏返し
void encode_colormap(std::vector<u8> &out_data, const std::vector<u32> &colormap, const std::string &color_format)
{
	if (colormap.empty()) return;

	if (color_format == "A1R5G5B5") {
		for (u32 color : colormap) {
			u8 r = (color >> 24) & 0xFF;
			u8 g = (color >> 16) & 0xFF;
			u8 b = (color >> 8)  & 0xFF;

			u16 r5 = (r * 31 + 127) / 255;
			u16 g5 = (g * 31 + 127) / 255;
			u16 b5 = (b * 31 + 127) / 255;

			u16 colorword = 32768 | (r5 << 10) | (g5 << 5) | b5;
			out_data.push_back(colorword & 0xFF);
			out_data.push_back((colorword >> 8) & 0xFF);
		}
	} 
	else if (color_format == "B8G8R8") {
		for (u32 color : colormap) {
			out_data.push_back((color >> 8)  & 0xFF); // B
			out_data.push_back((color >> 16) & 0xFF); // G
			out_data.push_back((color >> 24) & 0xFF); // R
		}
	} 
	else if (color_format == "B8G8R8A8") {
		for (u32 color : colormap) {
			out_data.push_back((color >> 8)  & 0xFF);  // B
			out_data.push_back((color >> 16) & 0xFF);  // G
			out_data.push_back((color >> 24) & 0xFF);  // R
			out_data.push_back(color & 0xFF);         // A
		}
	}
}

// 🔤 【旧284-370行】16bit A1R5G5B5 専用の超高速RLE圧縮エンジン
void encode_data_A1R5G5B5_rle(std::vector<u8> &out_data, const std::vector<u32> &pixels)
{
	if (pixels.empty()) return;
	u32 total = pixels.size();
	u32 i = 0;
	auto to_w16 = [](u32 color) -> u16 {
		u8 r = (color >> 24) & 0xFF; u8 g = (color >> 16) & 0xFF; u8 b = (color >> 8)  & 0xFF;
		return 32768 | (((r * 31 + 127) / 255) << 10) | (((g * 31 + 127) / 255) << 5) | ((b * 31 + 127) / 255);
	};
	std::vector<u16> raw_buffer;
	while (i < total) {
		u32 run_len = 1; u16 current_val = to_w16(pixels[i]);
		while (i + run_len < total && run_len < 128 && to_w16(pixels[i + run_len]) == current_val) run_len++;
		if (run_len > 1) {
			if (!raw_buffer.empty()) {
				out_data.push_back((u8)(raw_buffer.size() - 1));
				for (u16 r_val : raw_buffer) { out_data.push_back(r_val & 0xFF); out_data.push_back((r_val >> 8) & 0xFF); }
				raw_buffer.clear();
			}
			out_data.push_back((u8)(128 + run_len - 1));
			out_data.push_back(current_val & 0xFF); out_data.push_back((current_val >> 8) & 0xFF);
			i += run_len;
		} else {
			raw_buffer.push_back(current_val); i++;
			if (raw_buffer.size() == 128) {
				out_data.push_back(127);
				for (u16 r_val : raw_buffer) { out_data.push_back(r_val & 0xFF); out_data.push_back((r_val >> 8) & 0xFF); }
				raw_buffer.clear();
			}
		}
	}
	if (!raw_buffer.empty()) {
		out_data.push_back((u8)(raw_buffer.size() - 1));
		for (u16 r_val : raw_buffer) { out_data.push_back(r_val & 0xFF); out_data.push_back((r_val >> 8) & 0xFF); }
	}
}

// 🔤 【旧384-457行】24bit B8G8R8 専用の超高速RLE圧縮エンジン
void encode_data_B8G8R8_rle(std::vector<u8> &out_data, const std::vector<u32> &pixels)
{
	if (pixels.empty()) return;
	u32 total = pixels.size(); u32 i = 0;
	struct RGB { u8 b, g, r; };
	auto to_rgb = [](u32 color) -> RGB { return {(u8)((color >> 8) & 0xFF), (u8)((color >> 16) & 0xFF), (u8)((color >> 24) & 0xFF)}; };
	std::vector<RGB> raw_buffer;
	while (i < total) {
		u32 run_len = 1; RGB current_val = to_rgb(pixels[i]);
		while (i + run_len < total && run_len < 128) {
			RGB next_val = to_rgb(pixels[i + run_len]);
			if (next_val.b == current_val.b && next_val.g == current_val.g && next_val.r == current_val.r) run_len++; else break;
		}
		if (run_len > 1) {
			if (!raw_buffer.empty()) {
				out_data.push_back((u8)(raw_buffer.size() - 1));
				for (const auto &p : raw_buffer) { out_data.push_back(p.b); out_data.push_back(p.g); out_data.push_back(p.r); }
				raw_buffer.clear();
			}
			out_data.push_back((u8)(128 + run_len - 1));
			out_data.push_back(current_val.b); out_data.push_back(current_val.g); out_data.push_back(current_val.r);
			i += run_len;
		} else {
			raw_buffer.push_back(current_val); i++;
			if (raw_buffer.size() == 128) {
				out_data.push_back(127);
				for (const auto &p : raw_buffer) { out_data.push_back(p.b); out_data.push_back(p.g); out_data.push_back(p.r); }
				raw_buffer.clear();
			}
		}
	}
	if (!raw_buffer.empty()) {
		out_data.push_back((u8)(raw_buffer.size() - 1));
		for (const auto &p : raw_buffer) { out_data.push_back(p.b); out_data.push_back(p.g); out_data.push_back(p.r); }
	}
}

// 🔤 【旧471-546行】32bit B8G8R8A8 専用の超高速RLE圧縮エンジン
void encode_data_B8G8R8A8_rle(std::vector<u8> &out_data, const std::vector<u32> &pixels)
{
	if (pixels.empty()) return;
	u32 total = pixels.size(); u32 i = 0;
	struct RGBA { u8 b, g, r, a; };
	auto to_rgba = [](u32 color) -> RGBA { return {(u8)((color >> 8) & 0xFF), (u8)((color >> 16) & 0xFF), (u8)((color >> 24) & 0xFF), (u8)(color & 0xFF)}; };
	std::vector<RGBA> raw_buffer;
	while (i < total) {
		u32 run_len = 1; RGBA current_val = to_rgba(pixels[i]);
		while (i + run_len < total && run_len < 128) {
			RGBA next_val = to_rgba(pixels[i + run_len]);
			if (next_val.b == current_val.b && next_val.g == current_val.g && next_val.r == current_val.r && next_val.a == current_val.a) run_len++; else break;
		}
		if (run_len > 1) {
			if (!raw_buffer.empty()) {
				out_data.push_back((u8)(raw_buffer.size() - 1));
				for (const auto &p : raw_buffer) { out_data.push_back(p.b); out_data.push_back(p.g); out_data.push_back(p.r); out_data.push_back(p.a); }
				raw_buffer.clear();
			}
			out_data.push_back((u8)(128 + run_len - 1));
			out_data.push_back(current_val.b); out_data.push_back(current_val.g); out_data.push_back(current_val.r); out_data.push_back(current_val.a);
			i += run_len;
		} else {
			raw_buffer.push_back(current_val); i++;
			if (raw_buffer.size() == 128) {
				out_data.push_back(127);
				for (const auto &p : raw_buffer) { out_data.push_back(p.b); out_data.push_back(p.g); out_data.push_back(p.r); out_data.push_back(p.a); }
				raw_buffer.clear();
			}
		}
	}
	if (!raw_buffer.empty()) {
		out_data.push_back((u8)(raw_buffer.size() - 1));
		for (const auto &p : raw_buffer) { out_data.push_back(p.b); out_data.push_back(p.g); out_data.push_back(p.r); out_data.push_back(p.a); }
	}
}


// 🔤 【大トリ】Lua API 窓口関数
// Lua側から直接ファイルを焼き出す無敵の高速勝手口窓口
int l_tga_encode(lua_State *L)
{
	// ─── 👑 【究極の絶対防衛圏：スタック完全自動リビルド ＆ 改札回路】 ───
	int stack_count = lua_gettop(L);
	
	if (stack_count >= 1 && lua_istable(L, 1)) {
		// self テーブルの内部から幅、高さ、ピクセル配列を安全に引っこ抜く！
		lua_getfield(L, 1, "width");  
		lua_getfield(L, 1, "height"); 
		lua_getfield(L, 1, "pixels"); 

		if (stack_count < 2 || !lua_istable(L, 2)) {
			lua_newtable(L); 
		} else {
			lua_pushvalue(L, 2); 
		}

		if (stack_count >= 3 && lua_isstring(L, 3)) {
			lua_pushvalue(L, 3); 
		} else {
			lua_pushstring(L, "__MEMORY__"); 
		}

		lua_replace(L, 1); // 1: filename
		lua_replace(L, 5); // 5: properties
		lua_replace(L, 4); // 4: pixels
		lua_replace(L, 3); // 3: height
		lua_replace(L, 2); // 2: width

		lua_settop(L, 5);
	}

	// 💡 インデックス 1〜5 番固定で、ピュアに引数を一本釣り回収！
	std::string filename = luaL_checkstring(L, 1);
	u16 width            = (u16)luaL_checkinteger(L, 2);
	u16 height           = (u16)luaL_checkinteger(L, 3);
	luaL_checktype(L, 4, LUA_TTABLE); // ピクセルテーブル

	int prop_idx = 5;
	std::string color_format = "B8G8R8A8"; 
	std::string compression  = "RAW";      

	if (lua_istable(L, prop_idx)) {
		lua_getfield(L, prop_idx, "color_format");
		if (lua_isstring(L, -1)) color_format = lua_tostring(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, prop_idx, "compression");
		if (lua_isstring(L, -1)) compression = lua_tostring(L, -1);
		lua_pop(L, 1);
	}

	// 🎯 【18バイト公式ヘッダーの錬金】
	TGAHeader header;
	u8 has_colormap = 0;
	header.id_length        = 0;             
	header.color_map_type   = has_colormap ? 1 : 0; 
	header.image_type       = (compression == "RLE") ? 10 : 2; 

	if (color_format == "Y8" && compression == "RAW") {
		header.image_type = 3; 
	}

	header.x_origin         = 0;       
	header.y_origin         = 0;       
	header.width            = width;   
	header.height           = height;  
	header.bits_per_pixel   = get_pixel_depth(color_format);
//	header.image_descriptor = 8;       
	u8 alpha_bits = 0;
	if (header.bits_per_pixel == 32) {
		alpha_bits = 8; // 32bit（B8G8R8A8）ならアルファはジャスト8bit！
	} else if (header.bits_per_pixel == 16) {
		alpha_bits = 1; // 16bit（A1R5G5B5）ならアルファはジャスト1bit！ ➔ これでGIMPが大歓喜！
	}
	
	// 上位2bit（0x20）を立てて「画像は左上起点（Top-Left）」であることをIrrlichtの規格へ完全カチ揃え！
	header.image_descriptor = 0x20 | (alpha_bits & 0x0F); 

	header.color_map_first  = 0;
	header.color_map_length = 0; 
	header.color_map_size   = header.bits_per_pixel;

	// ピクセルデータの一括チャージ
	u32 total_pixels = (u32)width * (u32)height;
	std::vector<u8> buffer;
	buffer.reserve(total_pixels * 4); 

	if (compression == "RLE") {
		std::vector<u32> raw_pixels;
		raw_pixels.reserve(total_pixels);
		for (u32 i = 1; i <= total_pixels; i++) {
			// 👑 【お掃除箇所】古い base_idx 変数を引き算し、4番固定で安全直撃！
			lua_rawgeti(L, 4, i);
			raw_pixels.push_back((u32)lua_tointeger(L, -1));
			lua_pop(L, 1);
		}
		if (color_format == "B8G8R8A8")      encode_data_B8G8R8A8_rle(buffer, raw_pixels);
		else if (color_format == "B8G8R8")   encode_data_B8G8R8_rle(buffer, raw_pixels);
		else if (color_format == "A1R5G5B5") encode_data_A1R5G5B5_rle(buffer, raw_pixels);
	} 
	else {
		for (u32 i = 1; i <= total_pixels; i++) {
			// 👑 【お掃除箇所】古い base_idx 変数を引き算し、4番固定で安全直撃！
			lua_rawgeti(L, 4, i);
			u8 r = 255, g = 255, b = 255, a = 255;

			if (lua_isnumber(L, -1)) {
				u32 pixel_val = (u32)lua_tointeger(L, -1);
				r = (pixel_val >> 24) & 0xFF; g = (pixel_val >> 16) & 0xFF; b = (pixel_val >> 8) & 0xFF; a = pixel_val & 0xFF;
				if (color_format == "Y8") {
					buffer.push_back((u8)(pixel_val & 0xFF));
					lua_pop(L, 1); continue;
				}
			} 
			else if (lua_istable(L, -1)) {
				lua_rawgeti(L, -1, 1); r = (u8)lua_tointeger(L, -1); lua_pop(L, 1);
				lua_rawgeti(L, -1, 2); g = (u8)lua_tointeger(L, -1); lua_pop(L, 1);
				lua_rawgeti(L, -1, 3); b = (u8)lua_tointeger(L, -1); lua_pop(L, 1);
				lua_rawgeti(L, -1, 4); a = (u8)lua_tointeger(L, -1); lua_pop(L, 1);
			}

			if (color_format == "Y8") {
				buffer.push_back((u8)std::round(std::sqrt(0.299 * r * r + 0.587 * g * g + 0.114 * b * b)));
			} else if (color_format == "B8G8R8A8") {
				buffer.push_back(b); buffer.push_back(g); buffer.push_back(r); buffer.push_back(a);
			} else if (color_format == "B8G8R8") {
				buffer.push_back(b); buffer.push_back(g); buffer.push_back(r);
			} else if (color_format == "A1R5G5B5") {
				u16 w16 = 32768 | ((((r * 31 + 127) / 255) & 0x1F) << 10) | ((((g * 31 + 127) / 255) & 0x1F) << 5) | (((b * 31 + 127) / 255) & 0x1F);
				buffer.push_back(w16 & 0xFF); buffer.push_back((w16 >> 8) & 0xFF);
			}
			lua_pop(L, 1);
		}
	}

	// ─── 🏆 【仮想TGAメモリ ＆ ディスク物理ライトの自動勝手口分岐】 ───
	if (filename == "__MEMORY__") {
		std::string raw_binary(reinterpret_cast<const char*>(&header), sizeof(TGAHeader));
		raw_binary.append(reinterpret_cast<const char*>(buffer.data()), buffer.size());
		raw_binary.append("\x00\x00\x00\x00\x00\x00\x00\x00TRUEVISION-XFILE.\x00", 26);

		lua_pushlstring(L, raw_binary.data(), raw_binary.size()); 
		return 1; 
	} 

	std::ofstream file(filename, std::ios::binary);
	if (!file.is_open()) {
		errorstream << "TGAEncoder: FAILED to open file: " << filename << std::endl;
		lua_pushboolean(L, false);
		return 1;
	}
	file.write(reinterpret_cast<const char*>(&header), sizeof(TGAHeader));
	file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
	file.write("\x00\x00\x00\x00\x00\x00\x00\x00TRUEVISION-XFILE.\x00", 26);
	file.close();

	lua_pushboolean(L, true); 
	return 1;
}

void Initialize(lua_State *L, int top)
{
	lua_pushcfunction(L, l_tga_encode);
	lua_setfield(L, top, "l_tga_encode"); 
}

} // namespace tga_encoder
