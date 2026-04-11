// src/utf8_53.h
#pragma once
#include <string>
#include <vector>

namespace utf8_53 {
    // 文字列のバイト位置 pos から次の文字の開始位置とコードポイントを取得
    // 戻り値: 成功したら true、不正なシーケンスなら false
    bool get_next_char(const std::string &s, size_t &pos, int &code_point);

    // 指定範囲 [i, j] の文字数をカウント
    // 戻り値: 文字数。不正なバイトがあれば -1 とその位置を返す
    int count_chars(const std::string &s, size_t i, size_t j, size_t &err_pos);

    // コードポイントを UTF-8 バイト列に変換して追加
    void push_char(std::string &res, int code_point);

    // Unicode EAW (East Asian Width) 

    // 【追加】コードポイントの幅を返す (1 or 2)
    int get_char_width(int cp);

    // 【追加】文字列全体の表示幅を合計して返す
    int get_string_width(const std::string &s);

}
