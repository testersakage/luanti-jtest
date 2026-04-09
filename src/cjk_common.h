// cjk_common.h
#pragma once
#include <string>
#include <vector>

namespace cjk {

// UTF-8 → コードポイント
std::vector<uint32_t> utf8_to_codepoints(const std::string &text);

// コードポイント → UTF-8
std::string codepoints_to_utf8(const std::vector<int> &cps);

// コードポイント数
size_t length(const std::string &text);

// 正規化（将来）
std::string normalize(const std::string &text, const std::string &form);

// Unicode カテゴリ（将来）
std::string category(int cp);

// グリフ名生成（AtlasGlyph / TrueType 共通）
std::string glyph_name(int cp);

} // namespace cjk
