// src/utf8_53.cpp
#include "utf8_53.h"

namespace utf8_53 {

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
	//  ASCII範囲 (0x00 - 0x7F) と 0xFF までの半角範囲は 1列(6px相当)
	//  ラテン文字なども 1列
	if (cp < 0x1100) return 1;
	// 2. 半角カタカナ範囲 (U+FF61 - U+FF9F) を「幅1」に指定
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

	// 以下、逆変換関数

//  push_char の逆：文字列をコードポイントの配列(vector)に分解する
std::vector<int> to_codepoints(const std::string &s) {
	std::vector<int> res;
	size_t pos = 0;
	int cp;
	// 既存の get_next_char を活用
	while (get_next_char(s, pos, cp)) {
		res.push_back(cp);
	}
	return res;
}

	// 以下、ラッパー関数

//  看板の物理幅（ピクセル）を計算する
// 12pxアトラス仕様：半角=6px / 全角=12px と判定
int get_total_pixel_width(const std::string &s) {
	int total_px = 0;
	size_t pos = 0;
	int cp;
	while (get_next_char(s, pos, cp)) {
		// get_char_width(cp) が 1なら6px、2なら12px
		total_px += (get_char_width(cp) == 2 ? 12 : 6);
	}
	return total_px;
}

//  指定したピクセル幅に収まるように安全にカットする
std::string truncate_to_pixel_width(const std::string &s, int max_px) {
	std::string res = "";
	int current_px = 0;
	size_t pos = 0;
	int cp;

	while (pos < s.length()) {
		size_t last_pos = pos;
		if (!get_next_char(s, pos, cp)) break;

		// ★【重要】改行コード (LF=10, CR=13) が来たら、この行の切り出しを終了する
		if (cp == '\n' || cp == '\r') {
			break; 
		}

		int w = (get_char_width(cp) == 2 ? 12 : 6);
		if (current_px + w > max_px) break;

		res.append(s.substr(last_pos, pos - last_pos));
		current_px += w;
	}
	return res;
}
	
//  文字列を指定範囲で切り出す＋自動改行
std::vector<std::string> get_lines(const std::string &s, int max_px) {
	std::vector<std::string> lines;
	std::string remaining = s;
	int line_count = 0; // Debug用

	// 呼び出し時の生文字列と設定幅を確認
	// infostream << "[get_lines_debug] START: max_px=" << max_px << " text=[" << s << "]" << std::endl;

	while (!remaining.empty()) {
		// 現在の行に収まる分を切り出す
		std::string line = truncate_to_pixel_width(remaining, max_px);

		// --- ここから修正 ---
		size_t consume_len = line.length();

		// truncateが止まった位置の「次」が改行コードかどうかを確認
		if (consume_len < remaining.length()) {
			if (remaining[consume_len] == '\n') {
				consume_len += 1; // 改行コードを「消費」するが、lineには含めない
			} else if (remaining[consume_len] == '\r') {
				consume_len += 1;
				// Windowsの \r\n 対策
				if (consume_len < remaining.length() && remaining[consume_len] == '\n') {
					consume_len += 1;
				}
			}
		}

		if (line.empty()) {
			// 1文字も入らない（1文字がmax_pxより大きい）場合は、強制的に1文字出す
			size_t pos = 0;
			int cp;
			if (get_next_char(remaining, pos, cp)) {
				line = remaining.substr(0, pos);
			} else {
				break; // 異常系
			}
		}

		// --- デバッグログ ---
		line_count++;
		// infostream << "[get_lines_debug] L" << line_count 
		//            << ": text=[" << line << "] len=" << line.length() 
		//            << " consumed=" << consume_len 
		//            << (found_newline ? " (NEWLINE_SKIP)" : "") << std::endl;

		lines.push_back(line);
		remaining = remaining.substr(consume_len);
	}
	return lines;
}

} // namespace utf8_53
