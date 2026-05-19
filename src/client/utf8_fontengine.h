// src/client/utf8_fontengine.h
#pragma once

// compile switch
#ifndef UTF8_ATLAS
    #define UTF8_ATLAS 0
#endif

#ifndef UTF8_SDL2_ATLAS
    #define UTF8_SDL2_ATLAS 1
#endif

#ifndef UTF8_SDL2_FREETYPE
    #define UTF8_SDL2_FREETYPE 0
#endif

#include "irrlichttypes.h"
#include <IImage.h>
#include <string>
#include <vector>
#include <map>    // 追加
#include <list>


// combine命令パース用構造体
enum class UTF8EngineType {
	STD, // Atlas(STD)版
	EX,  // Atlas(EX)版
	FT // FreeType版
};

// EX/FT共用 レンダリングに必要な情報を格納
struct RenderTask {
	UTF8EngineType engine_type = UTF8EngineType::FT; // デフォルトはFT
	u32 sign_width = 0;
	u32 sign_height = 0;
	u32 start_x = 0;
	u32 start_y = 0;
	video::SColor color = video::SColor(255, 0, 0, 0); // デフォルト黒
	std::vector<int> codes;	// EX/FT用 コードポイント列
	std::string raw_text;	// ST用 通常の文字列
};

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

#if UTF8_ATLAS
class UTF8FontAtlas; // 独立したクラス
#endif

class UTF8FontEngine {
public:
	UTF8FontEngine();  // 追加：コンストラクタの宣言
	~UTF8FontEngine(); // 追加：デストラクタの宣言


#if UTF8_ATLAS
	// imagesource.cpp [utf8combine 命令で実行されるレンダリング関数
	static void renderUtf8Combine(void *dest_img_ptr, const std::string &command);
#endif

#if UTF8_SDL2_ATLAS
	// imagesource.cpp [utf8combineex 命令で実行されるレンダリング関数
	static void renderutf8combineex(void *dest_img_ptr, const std::string &command);
#endif

#if UTF8_SDL2_FREETYPE
	// imagesource.cpp [utf8combineft 命令で実行されるレンダリング関数
	static void renderutf8combineft(video::IImage *baseimg, const std::string &spec);

	// FreeType キャッシュ制御API用関数
	static u32 getMaxCacheSize();
	static u32 getCacheCount();
	static void setMaxCacheSize(u32 max_size);
	static void clearCache();
#endif

private:
	// render関数のパース処理担当
	static RenderTask parseUtf8Spec(const std::string &spec);

#if UTF8_ATLAS
	// ST Atlas Cache
	// utf8_fontatlas へ移動
#endif

#if UTF8_SDL2_ATLAS
	// EX Atlas Cache
	// utf8_fontatlas へ移動
#endif

#if UTF8_SDL2_FREETYPE
	// FreeType Cahe制御関数
	static FTCachedGlyph* getOrCacheGlyph(u32 code, void* face_ptr, u32 load_flags);

	// FreeType Caheメモリ 64bit長で fontsize+code を鍵とする
	static std::map<u64, FTCachedGlyph> m_glyph_cache;

	// Cache制御 FIFO BYPASS MAX
	static std::list<u64> m_cache_order;    // FIFO管理用のリスト
	static bool m_cache_zero_bypass;        // Cache処理をスキップするフラグ
	static size_t m_max_cache_size;         // Cacheの最大容量設定
#endif
};
