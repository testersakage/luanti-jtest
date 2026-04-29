// src/client/utf8_fontengine.h
#pragma once

#include "irrlichttypes.h"
#include <IImage.h>
#include <string>
#include <vector>
#include <map>    // 追加

struct RenderTask; 

// 生成されたピクセルデータと寸法をセットで保存する
struct FTCachedGlyph {
	std::vector<u8> bitmap; // 実際のピクセルデータ
	u32 width;			// 文字が描かれている有効なピクセル幅
	u32 rows;			// 文字が描かれている有効なピクセルの行数
	int pitch;			// 画像データの「一行が何バイトか」を表す
	int bitmap_left;	// (左余白): ペン先から、右に何ピクセル離れて描き始めるか。
	int bitmap_top;		// (上からの浮き): ベースラインから、上に何ピクセル突き出しているか。
	u32 advance;		//(文字送り): 次の文字のためにペン先を右に何ピクセル進めるか
	u8 pixel_mode;		// Mode (GRAY か MONO か)を記録
};

// ★ ここに「前方宣言」を追加
class UTF8FontAtlas; 

class UTF8FontEngine {
public:
	UTF8FontEngine();  // ★追加：コンストラクタの宣言
	~UTF8FontEngine(); // ★追加：デストラクタの宣言

	/**
	 * 指定したUTF-8文字列が、現在のフォント設定で
	 * 何ピクセルの幅になるかを返します。
	 */
	static unsigned int getTextWidth(const std::string &text);

	/**
	 * 指定したピクセル幅を超えないように、
	 * UTF-8文字列に改行(\n)を挿入、または分割したリストを返します。
	 */
	static std::vector<std::string> wrapText(const std::string &text, unsigned int max_pixel_width);

	/**
	 * 指定したピクセル幅に収まる最大長まで
	 * UTF-8文字列を切り詰めて返します。
	 */
	static std::string truncateText(const std::string &text, unsigned int max_pixel_width);

	/**
	 * 手動改行(\n)を考慮しつつ、指定幅で自動改行された行リストを生成します。
	 * mcl_signs の init.lua のロジックを C++ で再現したものです。
	 */
	static std::vector<std::string> generateLines(const std::string &text, unsigned int max_pixel_width);

	/**
	 * imagesource.cpp からの [utf8combine 命令を受け取り、看板をレンダリングします。
	 */
	static void renderUtf8Combine(void *dest_img_ptr, const std::string &command);

	/**
	 * imagesource.cpp からの [utf8combineft 命令を受け取り、看板をレンダリングします。
	*/
	static void renderutf8combineft(video::IImage *baseimg, const std::string &spec);

	/**
	 * フォントエンジンから指定した文字のグリフ（画像）を取得します。
	 * 仮想スクリーンバッファ方式により、フォントの種類を問わずキャプチャ可能です。
	 */
	static void* getGlyphImage(wchar_t c);
	
	/* FreeType Cache API */
	static u32 getCacheCount();
	static void clearCache();

private:
	// ★ ここに「倉庫番」を配属します
	static UTF8FontAtlas *m_atlas; 

	// render関数のパース処理担当
	static RenderTask parseUtf8Spec(const std::string &spec);

	// FreeType Cahe制御
	static FTCachedGlyph* getOrCacheGlyph(u32 code, void* face_ptr, u32 load_flags);

	// FreeType Caheメモリ 64bit長で fontsize+code を鍵とする
	static std::map<u64, FTCachedGlyph> m_glyph_cache;

};
