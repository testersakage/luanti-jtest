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

        // 改行コードで停止
        if (cp == '\n' || cp == '\r') {
            break; 
        }

        // ★【修正ポイント】物差しを getTextWidth と完全に同期させる
        // ASCII(0x7F以下) と 半角カナ(FF61-FF9F) 以外はすべて 12px (全角)
        int w = 12;
        if (cp <= 0x7F || (cp >= 0xFF61 && cp <= 0xFF9F)) {
            w = 6;
        }

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

    while (!remaining.empty()) {
        // 1. まず現在の幅(max_px)に収まる「物理的な限界」を測る
        std::string line = truncate_to_pixel_width(remaining, max_px);
        size_t consume_len = line.length();

        // --- ハイブリッド・ラップ処理 ---
        // もし物理限界が文字列の途中であり、かつその直後にスペースや改行がない場合
        if (consume_len < remaining.length()) {
            char next_char = remaining[consume_len];
            if (next_char != ' ' && next_char != '\n' && next_char != '\r') {
                // 単語の途中かもしれないので、直前のスペースを探す
                size_t last_space = line.find_last_of(" ");
                
                // スペースが見つかり、かつそれが極端に手前でない場合のみ、そこで改行する
                // (あまりに手前すぎると空白が目立つので、その場合は文字単位で切る「ハイブリッド」判断)
                if (last_space != std::string::npos && last_space > (line.length() * 0.6)) {
                    line = line.substr(0, last_space);
                    consume_len = last_space + 1; // スペースそのものは消費して消す
                }
            }
        }

        // --- 改行コードの処理 (既存ロジック) ---
        if (consume_len < remaining.length()) {
            if (remaining[consume_len] == '\n') {
                consume_len += 1;
            } else if (remaining[consume_len] == '\r') {
                consume_len += 1;
                if (consume_len < remaining.length() && remaining[consume_len] == '\n') {
                    consume_len += 1;
                }
            }
        }

        // 1文字も入らない場合の救済（巨大な全角文字など）
        if (line.empty() && !remaining.empty()) {
            size_t pos = 0;
            int cp;
            if (get_next_char(remaining, pos, cp)) {
                line = remaining.substr(0, pos);
                consume_len = pos;
            } else {
                break;
            }
        }

        lines.push_back(line);
        remaining = (consume_len < remaining.length()) ? remaining.substr(consume_len) : "";
    }
    return lines;
}

} // namespace utf8_53
