// src/utf8_53.cpp
#include "utf8_53.h"
#include <string>
#include <vector>
#include <sstream>

namespace utf8_53 {

// ─── 【消失前再現】JSONから流し込まれた地域差リストの「動的実体ベクター」 ───
std::vector<WidthRange> g_half_width_ranges;

// ─── 【消失前再現】独立させた判定ルーチンのみの関数（UAX #11 準拠） ───
float get_char_width_ratio(uint32_t codepoint)
{
	// A. 絶対半角領域（ASCII 0x20〜0x7E & 半角カナ 0xFF61〜0xFF9F）の固定ジャッジ
	if ((codepoint >= 0x0020 && codepoint <= 0x007E) ||
		(codepoint >= 0xFF61 && codepoint <= 0xFF9F)) {
		return 0.5f;
	}

	// B. 【魔導書連動】JSONで動的に更新された地域差（キリル・ギリシャ等）の範囲スキャン
	for (const auto &range : g_half_width_ranges) {
		if (codepoint >= range.start && codepoint <= range.end) {
			return 0.5f; // 指定範囲にヒットしたら半角（0.5倍）を返す
		}
	}

	return 1.0f; // どれにも当てはまらなければ等倍（全角ベース）
}

// ─── 【消失前再現】文字パースと幅情報をセットで返す最上流窓口 ───
GlyphInfo get_next_glyph_info(const std::string &utf8_text, size_t &pos)
{
	GlyphInfo glyph;
	int cp = 0;

	// 既存のパース関数を叩いてコードポイントを取得
	if (get_next_char(utf8_text, pos, cp)) {
		glyph.codepoint = static_cast<uint32_t>(cp);
		// 独立させた判定ルーチンを呼び出して幅の比率をセット
		glyph.width_ratio = get_char_width_ratio(glyph.codepoint);
	} else {
		// パース失敗時の安全ガード
		glyph.codepoint = 0;
		glyph.width_ratio = 1.0f;
		if (pos < utf8_text.length()) pos++; // 無限ループ防止
	}

	return glyph;
}

//  次のUTF-8文字を解析するコアロジック
bool get_next_char(const std::string &s, size_t &pos, int &code_point) {
	if (pos >= s.length()) return false;

	unsigned char c = (unsigned char)s[pos];
	size_t len = 0;

	if (c < 0x80) { // 1バイト (ASCII)
		code_point = c;
		len = 1;
	} else if ((c & 0xE0) == 0xC0) { // 2バイト
		code_point = c & 0x1F;
		len = 2;
	} else if ((c & 0xF0) == 0xE0) { // 3バイト
		code_point = c & 0x0F;
		len = 3;
	} else if ((c & 0xF8) == 0xF0) { // 4バイト
		code_point = c & 0x07;
		len = 4;
	} else {
		return false; // 不正な先頭バイト
	}

	if (pos + len > s.length()) return false; // バイト不足

	// 後続バイトのチェック (0x80-0xBF)
	for (size_t k = 1; k < len; ++k) {
		unsigned char next_c = (unsigned char)s[pos + k];
		if ((next_c & 0xC0) != 0x80) return false;
		code_point = (code_point << 6) | (next_c & 0x3F);
	}

	pos += len; // 次の文字の開始位置へ進める
	return true;
}

//  utf8.len(s, i, j) 用のカウント
int count_chars(const std::string &s, size_t i, size_t j, size_t &err_pos) {
	int count = 0;
	size_t pos = i;
	while (pos <= j && pos < s.length()) {
		int cp;
		size_t last_pos = pos;
		if (!get_next_char(s, pos, cp)) {
			err_pos = last_pos; // エラーが発生したバイト位置
			return -1;
		}
		count++;
	}
	return count;
}

//  utf8.char(...) 用の変換
void push_char(std::string &res, int cp) {
	if (cp < 0x80) {
		res += (char)cp;
	} else if (cp < 0x800) {
		res += (char)(0xC0 | (cp >> 6));
		res += (char)(0x80 | (cp & 0x3F));
	} else if (cp < 0x10000) {
		res += (char)(0xE0 | (cp >> 12));
		res += (char)(0x80 | ((cp >> 6) & 0x3F));
		res += (char)(0x80 | (cp & 0x3F));
	} else {
		res += (char)(0xF0 | (cp >> 18));
		res += (char)(0x80 | ((cp >> 12) & 0x3F));
		res += (char)(0x80 | ((cp >> 6) & 0x3F));
		res += (char)(0x80 | (cp & 0x3F));
	}
}

// Unicode EAW (East Asian Width) 
int get_char_width(int cp) {
	if (cp < 0x1100) return 1;
	if (cp >= 0xFF61 && cp <= 0xFF9F) return 1;

	//  CJK全角文字の主要範囲
	if ((cp >= 0x1100 && cp <= 0x11FF) || // ハングル
		(cp >= 0x2329 && cp <= 0x232A) || // 〈 〉
		(cp >= 0x2E80 && cp <= 0x2FDF) || // 部首
		(cp >= 0x3000 && cp <= 0x303F) || // 全角記号・句読点
		(cp >= 0x3040 && cp <= 0x309F) || // ひらがな
		(cp >= 0x30A0 && cp <= 0x30FF) || // カタカナ
		(cp >= 0x3100 && cp <= 0x312F) || // 注音
		(cp >= 0x3130 && cp <= 0x318F) || // ハングル互換
		(cp >= 0x31A0 && cp <= 0x31EF) || // 各種拡張
		(cp >= 0x3200 && cp <= 0x32FF) || // 囲み文字 (株)など
		(cp >= 0x3400 && cp <= 0x4DBF) || // 漢字拡張A
		(cp >= 0x4E00 && cp <= 0x9FFF) || // 常用漢字・統合漢字
		(cp >= 0xAC00 && cp <= 0xD7AF) || // ハングル音節
		(cp >= 0xF900 && cp <= 0xFAFF) || // 漢字互換
		(cp >= 0xFE10 && cp <= 0xFE1F) || // 縦書き形式
		(cp >= 0xFE30 && cp <= 0xFE6F) || // CJK互換
		(cp >= 0xFF01 && cp <= 0xFF60) || // 全角英数・記号
		(cp >= 0xFFE0 && cp <= 0xFFE6) || // 円・ポンド等の記号
		(cp >= 0x1F300 && cp <= 0x1F9FF) || // 絵文字
		(cp >= 0x20000 && cp <= 0x3FFFF))   // 漢字拡張B〜F・超多倍文字
	{
		return 2;
	}

	// デフォルトは 1列
	return 1;
}

int get_string_width(const std::string &s) {
	int total_width = 0;
	size_t pos = 0;
	int cp;
	while (get_next_char(s, pos, cp)) {
		total_width += get_char_width(cp);
	}
	return total_width;
}

//  push_char の逆：文字列をコードポイントの配列(vector)に分解する
std::vector<int> to_codepoints(const std::string &s) {
	std::vector<int> res;
	size_t pos = 0;
	int cp;
	while (get_next_char(s, pos, cp)) {
		res.push_back(cp);
	}
	return res;
}

// --- 統合：utf8wrap の実体となる 4つの関数 ---

// w21. 文字列の物理表示幅(pixel)を計算
unsigned int get_text_width(const std::string &text, int han_w, int zen_w) {
	if (text.empty()) return 0;
	unsigned int total_w = 0;
	size_t pos = 0;
	
	// 【消失前再現】動的判定（get_char_width_ratio）が 0.5 を返したら、強制的に han_w を適用
	while (pos < text.length()) {
		int cp;
		if (!get_next_char(text, pos, cp)) break;
		if (cp == '\n' || cp == '\r') continue;

		float ratio = get_char_width_ratio(static_cast<uint32_t>(cp));
		if (ratio == 0.5f) {
			total_w += han_w;
		} else {
			total_w += (get_char_width(cp) == 2 ? zen_w : han_w);
		}
	}
	return total_w;
}

// w22. 指定した物理幅(pixel)に収まるように安全にカット
std::string truncate_text(const std::string &text, unsigned int max_px, int han_w, int zen_w) {
	std::string result = "";
	int current_px = 0;
	size_t pos = 0;
	
	// 【消失前再現】動的例外範囲と完全同期
	while (pos < text.length()) {
		size_t last_pos = pos;
		int cp;
		if (!get_next_char(text, pos, cp)) break;
		if (cp == '\n' || cp == '\r') break;

		float ratio = get_char_width_ratio(static_cast<uint32_t>(cp));
		int char_w = (ratio == 0.5f) ? han_w : (get_char_width(cp) == 2 ? zen_w : han_w);
		
		if (current_px + char_w > (int)max_px) break;

		result.append(text.substr(last_pos, pos - last_pos));
		current_px += char_w;
	}
	return result;
}

// w23. 手動改行を考慮しない指定幅で自動改行し、行リストを返す
std::vector<std::string> wrap_text(const std::string &text, unsigned int max_px, int han_w, int zen_w) {
	std::vector<std::string> lines;
	std::string remaining = text;
	while (!remaining.empty()) {
		std::string line = truncate_text(remaining, max_px, han_w, zen_w);
		size_t consume_len = line.length();

		// ハイブリッド・ラップ処理（単語の途中切れ防止）
		if (consume_len < remaining.length()) {
			char next_char = remaining[consume_len];
			if (next_char != ' ' && next_char != '\n' && next_char != '\r') {
				size_t last_space = line.find_last_of(" ");
				if (last_space != std::string::npos && last_space > (line.length() * 0.6)) {
					line = line.substr(0, last_space);
					consume_len = last_space + 1;
				}
			}
		}
		if (line.empty() && !remaining.empty()) { // 1文字も入らない救済
			size_t pos = 0; int cp;
			if (get_next_char(remaining, pos, cp)) {
				line = remaining.substr(0, pos);
				consume_len = pos;
			} else break;
		}
		lines.push_back(line);
		remaining = (consume_len < remaining.length()) ? remaining.substr(consume_len) : "";
	}
	return lines;
}

// w24. 手動改行を考慮しつつ指定幅で自動改行し、行リストを返す
std::vector<std::string> generate_lines(const std::string &text, unsigned int max_px, int han_w, int zen_w) {
	std::vector<std::string> final_lines;
	std::stringstream ss(text);
	std::string segment;
	while (std::getline(ss, segment, '\n')) {
		std::vector<std::string> wrapped = wrap_text(segment, max_px, han_w, zen_w);
		if (wrapped.empty()) final_lines.push_back("");
		else final_lines.insert(final_lines.end(), wrapped.begin(), wrapped.end());
	}
	if (!text.empty() && text.back() == '\n') final_lines.push_back("");
	return final_lines;
}

} // namespace utf8_53
