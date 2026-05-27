// src/mcl/core/tga_encoder.cpp （★第6章 TGA・カラーマップ＆RLE＆仮想メモリ完全合体・製品版確定全文）

#include "tga_encoder.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <fstream>
#include "log.h"

namespace tga_encoder {

	// ─── 📊 【新設実体：パレット一括変換（カラーマップ）エンコード筋肉】 ───
	//     本家オリジナルの Lua 側の 77行目〜のカラーマップ実務を C++ のレジスタ演算で 0ms 完食！！！ [INDEX: 5]
	void encode_colormap(std::vector<u8> &out_data, const std::vector<u32> &colormap, const std::string &color_format) {
		if (colormap.empty()) return;

		if (color_format == "A1R5G5B5") {
			for (u32 p : colormap) {
				u8 r = (p >> 24) & 0xFF, g = (p >> 16) & 0xFF, b = (p >> 8) & 0xFF;
				u16 w16 = 32768 | ((((r * 31 + 127) / 255) & 0x1F) << 10) | ((((g * 31 + 127) / 255) & 0x1F) << 5) | (((b * 31 + 127) / 255) & 0x1F);
				out_data.push_back(w16 & 0xFF); out_data.push_back((w16 >> 8) & 0xFF);
			}
		}
		else if (color_format == "B8G8R8") {
			for (u32 p : colormap) {
				u8 r = (p >> 24) & 0xFF, g = (p >> 16) & 0xFF, b = (p >> 8) & 0xFF;
				out_data.push_back(b); out_data.push_back(g); out_data.push_back(r);
			}
		}
		else if (color_format == "B8G8R8A8") {
			for (u32 p : colormap) {
				u8 r = (p >> 24) & 0xFF, g = (p >> 16) & 0xFF, b = (p >> 8) & 0xFF, a = p & 0xFF;
				out_data.push_back(b); out_data.push_back(g); out_data.push_back(r); out_data.push_back(a);
			}
		}
	}

	// ─── ⚡ 【RLE圧縮：A1R5G5B5（16bit）最速パック筋肉】 ───
	void encode_data_A1R5G5B5_rle(std::vector<u8> &out_data, const std::vector<u32> &pixels) {
		size_t i = 0;
		while (i < pixels.size()) {
			size_t run_len = 1;
			while (i + run_len < pixels.size() && run_len < 128 && pixels[i] == pixels[i + run_len]) { run_len++; }
			if (run_len > 1) {
				out_data.push_back(static_cast<u8>(128 + run_len - 1));
				u32 p = pixels[i];
				u8 r = (p >> 24) & 0xFF, g = (p >> 16) & 0xFF, b = (p >> 8) & 0xFF;
				u16 w16 = 32768 | ((((r * 31 + 127) / 255) & 0x1F) << 10) | ((((g * 31 + 127) / 255) & 0x1F) << 5) | (((b * 31 + 127) / 255) & 0x1F);
				out_data.push_back(w16 & 0xFF); out_data.push_back((w16 >> 8) & 0xFF);
				i += run_len;
			} else {
				size_t raw_len = 1;
				while (i + raw_len < pixels.size() && raw_len < 128) {
					if (i + raw_len + 1 < pixels.size() && pixels[i + raw_len] == pixels[i + raw_len + 1]) break;
					raw_len++;
				}
				out_data.push_back(static_cast<u8>(raw_len - 1));
				for (size_t k = 0; k < raw_len; k++) {
					u32 p = pixels[i + k];
					u8 r = (p >> 24) & 0xFF, g = (p >> 16) & 0xFF, b = (p >> 8) & 0xFF;
					u16 w16 = 32768 | ((((r * 31 + 127) / 255) & 0x1F) << 10) | ((((g * 31 + 127) / 255) & 0x1F) << 5) | (((b * 31 + 127) / 255) & 0x1F);
					out_data.push_back(w16 & 0xFF); out_data.push_back((w16 >> 8) & 0xFF);
				}
				i += raw_len;
			}
		}
	}

	// ─── ⚡ 【RLE圧縮：B8G8R8（24bit）最速パック筋肉】 ───
	void encode_data_B8G8R8_rle(std::vector<u8> &out_data, const std::vector<u32> &pixels) {
		size_t i = 0;
		while (i < pixels.size()) {
			size_t run_len = 1;
			while (i + run_len < pixels.size() && run_len < 128 && (pixels[i] & 0xFFFFFF00) == (pixels[i + run_len] & 0xFFFFFF00)) { run_len++; }
			if (run_len > 1) {
				out_data.push_back(static_cast<u8>(128 + run_len - 1));
				u32 p = pixels[i];
				out_data.push_back((p >> 8) & 0xFF); out_data.push_back((p >> 16) & 0xFF); out_data.push_back((p >> 24) & 0xFF);
				i += run_len;
			} else {
				size_t raw_len = 1;
				while (i + raw_len < pixels.size() && raw_len < 128) {
					if (i + raw_len + 1 < pixels.size() && (pixels[i + raw_len] & 0xFFFFFF00) == (pixels[i + raw_len + 1] & 0xFFFFFF00)) break;
					raw_len++;
				}
				out_data.push_back(static_cast<u8>(raw_len - 1));
				for (size_t k = 0; k < raw_len; k++) {
					u32 p = pixels[i + k];
					out_data.push_back((p >> 8) & 0xFF); out_data.push_back((p >> 16) & 0xFF); out_data.push_back((p >> 24) & 0xFF);
				}
				i += raw_len;
			}
		}
	}

	// ─── ⚡ 【RLE圧縮：B8G8R8A8（32bit）最速パック筋肉】 ───
	void encode_data_B8G8R8A8_rle(std::vector<u8> &out_data, const std::vector<u32> &pixels) {
		size_t i = 0;
		while (i < pixels.size()) {
			size_t run_len = 1;
			while (i + run_len < pixels.size() && run_len < 128 && pixels[i] == pixels[i + run_len]) { run_len++; }
			if (run_len > 2) { 
				out_data.push_back(static_cast<u8>(128 + run_len - 1));
				u32 p = pixels[i];
				out_data.push_back((p >> 8) & 0xFF); out_data.push_back((p >> 16) & 0xFF); out_data.push_back((p >> 24) & 0xFF); out_data.push_back(p & 0xFF);
				i += run_len;
			} else {
				size_t raw_len = 1;
				while (i + raw_len < pixels.size() && raw_len < 128) {
					if (i + run_len + 1 < pixels.size() && pixels[i + raw_len] == pixels[i + raw_len + 1]) break;
					raw_len++;
				}
				out_data.push_back(static_cast<u8>(raw_len - 1));
				for (size_t k = 0; k < raw_len; k++) {
					u32 p = pixels[i + k];
					out_data.push_back((p >> 8) & 0xFF); out_data.push_back((p >> 16) & 0xFF); out_data.push_back((p >> 24) & 0xFF); out_data.push_back(p & 0xFF);
				}
				i += raw_len;
			}
		}
	}

	// ─── 🏆 【引数完全フラット・スタック無傷ガード・カラーマップ・RLE・仮想メモリ完全対応】 ───
	int l_tga_encode(lua_State *L)
	{
		std::string filename     = luaL_checkstring(L, 1);
		u16 width                = (u16)luaL_checkinteger(L, 2);
		u16 height               = (u16)luaL_checkinteger(L, 3);
		luaL_checktype(L, 4, LUA_TTABLE); 
		std::string color_format = luaL_checkstring(L, 5);
		std::string compression  = luaL_checkstring(L, 6);

		u32 total_pixels = (u32)width * (u32)height;
		std::vector<u32> raw_pixels;
		raw_pixels.reserve(total_pixels);

		for (u32 i = 1; i <= total_pixels; i++) {
			lua_rawgeti(L, 4, i);
			raw_pixels.push_back((u32)lua_tointeger(L, -1));
			lua_pop(L, 1);
		}

		// 🎨 properties.colormap が紛れ込んでいるか検品・回収（無ければ空） [INDEX: 1]
		std::vector<u32> colormap_pixels;
		if (lua_istable(L, 7)) { // 7番目の隠し引数として放流
			int cmap_len = lua_objlen(L, 7);
			colormap_pixels.reserve(cmap_len);
			for (int i = 1; i <= cmap_len; i++) {
				lua_rawgeti(L, 7, i);
				colormap_pixels.push_back((u32)lua_tointeger(L, -1));
				lua_pop(L, 1);
			}
		}

		TGAHeader header;
		header.id_length        = 0;             
		header.color_map_type   = colormap_pixels.empty() ? 0 : 1; 
		header.image_type       = (compression == "RLE") ? 10 : (colormap_pixels.empty() ? 2 : 1); 

		if (color_format == "Y8" && compression == "RAW") { header.image_type = 3; }

		header.x_origin         = 0;       
		header.y_origin         = 0;       
		header.width            = width;   
		header.height           = height;  
		header.bits_per_pixel   = get_pixel_depth(color_format);

		u8 alpha_bits = (header.bits_per_pixel == 32) ? 8 : ((header.bits_per_pixel == 16) ? 1 : 0);
		header.image_descriptor = 0x20 | (alpha_bits & 0x0F); 

		header.color_map_first  = 0;
		header.color_map_length = static_cast<u16>(colormap_pixels.size()); 
		header.color_map_size   = colormap_pixels.empty() ? 0 : get_pixel_depth(color_format);

		std::vector<u8> buffer;
		buffer.reserve(total_pixels * 4 + colormap_pixels.size() * 4); 

		// 🏗️ 【パレットの一括パックチャージ】：ヘッダーの直後へカラーマップをカチ流し込み！！！ [INDEX: 5]
		if (!colormap_pixels.empty()) {
			encode_colormap(buffer, colormap_pixels, color_format);
		}

		// 🏗️ 【最速C++ FPUレジスタ圧縮ストリームパック】
		if (compression == "RLE") {
			if (color_format == "B8G8R8A8")      encode_data_B8G8R8A8_rle(buffer, raw_pixels);
			else if (color_format == "B8G8R8")   encode_data_B8G8R8_rle(buffer, raw_pixels);
			else if (color_format == "A1R5G5B5") encode_data_A1R5G5B5_rle(buffer, raw_pixels);
		} 
		else {
			for (u32 pixel_val : raw_pixels) {
				u8 r = (pixel_val >> 24) & 0xFF; u8 g = (pixel_val >> 16) & 0xFF;
				u8 b = (pixel_val >> 8) & 0xFF;  u8 a = pixel_val & 0xFF;

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
			}
		}

		// ─── 🏆 【仮想TGAメモリ自動勝手口分岐】 ─── [INDEX: 5]
		if (filename == "__MEMORY__") {
			std::string raw_binary(reinterpret_cast<const char*>(&header), sizeof(TGAHeader));
			raw_binary.append(reinterpret_cast<const char*>(buffer.data()), buffer.size());
			raw_binary.append("\x00\x00\x00\x00\x00\x00\x00\x00TRUEVISION-XFILE.\x00", 26);

			lua_pushlstring(L, raw_binary.data(), raw_binary.size()); 
			return 1; 
		} 

		// ─── 🏆 【ディスク物理ライトの一撃ライト】 ───
		std::ofstream file(filename, std::ios::binary);
		if (!file.is_open()) {
			errorstream << "TGAEncoder: FAILED to open file: " << filename << std::endl;
			lua_pushboolean(L, false); return 1;
		}
		file.write(reinterpret_cast<const char*>(&header), sizeof(TGAHeader));
		file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
		file.write("\x00\x00\x00\x00\x00\x00\x00\x00TRUEVISION-XFILE.\x00", 26);
		file.close();

		lua_pushboolean(L, true); 
		return 1;
	}

	void Initialize(lua_State *L, int top) {
		lua_pushcfunction(L, l_tga_encode);
		lua_setfield(L, top, "native_tga_encode"); 
	}

} // namespace tga_encoder
