// src/mcl/math_common.h
#pragma once
#include <string>

namespace mcl_math {

	// 📐 3. 【ビットパック数理本体】：A1R5G5B5減色パック（スタック操作0）
	inline unsigned short pack_color_a1r5g5b5(unsigned char r, unsigned char g, unsigned char b) {
		return 32768 | ((((r * 31 + 127) / 255) & 0x1F) << 10) | ((((g * 31 + 127) / 255) & 0x1F) << 5) | (((b * 31 + 127) / 255) & 0x1F);
	}

	// ⚔️ 4. 【文字列トリミング数理本体】：前方一致高速切り出し（スタック操作0）
	bool trim_prefix(const char* str, const char* prefix, std::string& out_trimmed);
}
