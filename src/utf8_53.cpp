// src/utf8_53.cpp
#include "utf8_53.h"

namespace utf8_53 {

// 1. 次のUTF-8文字を解析するコアロジック
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

// 2. utf8.len(s, i, j) 用のカウント
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

// 3. utf8.char(...) 用の変換
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
    // 1. ASCII と制御文字、ラテン文字などは 1列
    if (cp < 0x1100) return 1;

    // 2. CJK全角文字の主要範囲
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

} // namespace utf8_53
