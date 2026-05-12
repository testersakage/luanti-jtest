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

	// --- 統合：utf8wrap の実体となる 4つの関数 ---

	// w21. 文字列の物理表示幅(pixel)を計算 (w01+w11)
	unsigned int get_text_width(const std::string &text, int han_w = 6, int zen_w = 12);

	// w22. 指定した物理幅(pixel)に収まるように安全にカット (w02+w13)
	std::string truncate_text(const std::string &text, unsigned int max_px, int han_w = 6, int zen_w = 12);

	// w23. 手動改行を考慮しない指定幅で自動改行し、行リストを返す (w03+w14)
	std::vector<std::string> wrap_text(const std::string &text, unsigned int max_px, int han_w = 6, int zen_w = 12);

	// w24. 手動改行を考慮しつつ指定幅で自動改行し、行リストを返す (w03+w12+w14)
	std::vector<std::string> generate_lines(const std::string &text, unsigned int max_px, int han_w = 6, int zen_w = 12);

/*
// --- 以下、統合前のラッパー関数 w01~03, w11~14---

// --- 以下、看板用ラッパー関数 (デフォルトは全角12pxフォント基準) ---

	// w01. 看板上の物理表示幅(pixel)を計算する (半角=6px, 全角=12px)
	int get_total_pixel_width(const std::string &s, int han_w = 6, int zen_w = 12);

	// w02. 指定した物理幅(pixel)に収まるように安全にカットする
	std::string truncate_to_pixel_width(const std::string &s, int max_px, int han_w = 6, int zen_w = 12);

	// w03. 指定した物理幅(pixel)で自動改行し、行ごとのリストを返す
	std::vector<std::string> get_lines(const std::string &s, int max_px, int han_w = 6, int zen_w = 12);

// --- 以下、看板用ラッパー関数 (utf8_fontengineからの移設) ---

	// w11. 幅を測る（旧 get_total_pixel_width をこれに統合可能）
	unsigned int get_text_width(const std::string &text, int han_w = 6, int zen_w = 12);

	// w12. 改行を挿入する（元の wrapText）
	std::vector<std::string> wrap_text(const std::string &text, unsigned int max_px, int han_w = 6, int zen_w = 12);

	// w13. 切り詰める（旧 truncate_to_pixel_width をこれに統合）
	std::string truncate_text(const std::string &text, unsigned int max_px, int han_w = 6, int zen_w = 12);

	// w14. 行リスト生成（旧 get_lines と統合。これが一番「知恵」が詰まった本尊になります）
	std::vector<std::string> generate_lines(const std::string &text, unsigned int max_px, int han_w = 6, int zen_w = 12);
*/
} // namespace utf8_53
