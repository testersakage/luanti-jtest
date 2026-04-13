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

    // 【Pack】コードポイントを UTF-8 バイト列に変換して追加 (lua utf8.char 相当)
    void push_char(std::string &res, int code_point);

    // 【Unpack】文字列をコードポイントの配列に分解
    std::vector<int> to_codepoints(const std::string &s);

    // --- Unicode EAW (East Asian Width) & Layout Logic ---

    // コードポイントの論理幅を返す (半角=1, 全角=2)
    int get_char_width(int cp);

    // 文字列全体の論理幅の合計を返す (lua utf8.width 相当)
    int get_string_width(const std::string &s);

    // --- Modern Signboard Logic (12px Atlas Specification) ---

    // 看板上の物理表示幅(pixel)を計算する (半角=6px, 全角=12px)
    int get_total_pixel_width(const std::string &s);

    // 指定した物理幅(pixel)に収まるように安全にカットする
    std::string truncate_to_pixel_width(const std::string &s, int max_px);

	    // 指定した物理幅(pixel)で自動改行し、行ごとのリストを返す
    std::vector<std::string> get_lines(const std::string &s, int max_px);

} // namespace utf8_53
