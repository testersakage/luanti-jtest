#define SDL_MAIN_HANDLED
#include "sdl2_font.h"
#include <SDL2/SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "log.h"
//#include <iostream> // これを追加 debug表示用
namespace sdl2_font {

static FT_Library ft_library = nullptr;
static FT_Face ft_face = nullptr;
static int last_ft_error = 0; // ★これを追加！
static u32 last_char_advance = 0; // 歩幅を記録する箱
static std::string last_attempted_path = "";

u32 get_last_char_advance() { return last_char_advance; }

void* get_library_ptr() { return (void*)ft_library; }
void* get_face_ptr()    { return (void*)ft_face; }
int get_last_error() { return last_ft_error; }
std::string get_last_path() { return last_attempted_path; }

// ★ 職人の備忘録：現在ロード中の設定をメモしておく
static std::string loaded_path = "";
static unsigned int loaded_size = 0;
static int loaded_index = -1;

// --- 外部から現在の状態を自白させるための関数 ---
//std::string get_last_path()  { return last_attempted_path; }
unsigned int get_last_size() { return loaded_size; }
int get_last_index()         { return loaded_index; }


bool init(const std::string &font_path, unsigned int font_size, int requested_index)
{
	// 1. パスと要求を自白させる
	actionstream << "SDL2Font: [TRACE] init called. Path: " << font_path 
	             << " Size: " << font_size << " Requested Index: " << requested_index << std::endl;
	last_attempted_path = font_path;

	// 2. FreeType ライブラリの初期化 (シングルトン的な扱い)
	if (ft_library == nullptr) {
		FT_Error err = FT_Init_FreeType(&ft_library);
		if (err) {
			last_ft_error = (int)err;
			errorstream << "SDL2Font: Could not init FreeType library, err=" << last_ft_error << std::endl;
			return false;
		}
	}

	// 3. フォントの入れ替え（古いのがあれば捨てる）
	if (ft_face) {
		FT_Done_Face(ft_face);
		ft_face = nullptr;
	}

	// --- 職人の安全インデックス判定 ---
	// まず0番で読み込んで、含まれているフォント数を確認する
	FT_Face temp_face;
	if (FT_New_Face(ft_library, font_path.c_str(), 0, &temp_face)) {
		errorstream << "SDL2Font: FAILED to load font file at " << font_path << std::endl;
		return false;
	}
	int num_faces = temp_face->num_faces;
	FT_Done_Face(temp_face);

	// 複数あって、かつ範囲内ならリクエストを採用。それ以外は0。
	int final_index = (num_faces > 1 && requested_index >= 0 && requested_index < num_faces) 
	                  ? requested_index : 0;

	// 本番の読み込み
	FT_Error err = FT_New_Face(ft_library, font_path.c_str(), final_index, &ft_face);
	if (err) {
		last_ft_error = (int)err;
		errorstream << "SDL2Font: FAILED to load face at index " << final_index << std::endl;
		return false;
	}

	// 4. 文字マップとサイズの設定
	FT_Select_Charmap(ft_face, FT_ENCODING_UNICODE);
	FT_Set_Pixel_Sizes(ft_face, 0, font_size);
	
	// --- 職人の詳細自白ログ ---
	FT_Int major, minor, patch;
	FT_Library_Version(ft_library, &major, &minor, &patch);

	actionstream << "SDL2Font: === FT BACKEND READY ===" << std::endl;
	actionstream << "SDL2Font: FreeType Version: " << major << "." << minor << "." << patch << std::endl;
	actionstream << "SDL2Font: Family Name: " << (ft_face->family_name ? ft_face->family_name : "Unknown") << std::endl;
	actionstream << "SDL2Font: Style Name:  " << (ft_face->style_name ? ft_face->style_name : "Unknown") << std::endl;
	actionstream << "SDL2Font: Face Index:  " << final_index << " / " << num_faces << std::endl;
	actionstream << "SDL2Font: Pixel Size:  " << font_size << std::endl;
	actionstream << "SDL2Font: ==========================" << std::endl;

	// ★ 成功した瞬間に、今回の設定を「備忘録」に書き込む！
//	loaded_path  = font_path;
	loaded_size  = font_size;
	loaded_index = final_index; // 実際に確定したインデックス

	return true;
}

bool render_to_buffer(uint32_t code, unsigned char* dest, int dest_w, int dest_h, bool antialias, unsigned int font_size, unsigned int baseline) 
{
	if (!ft_face) return false;

	// 1. AAの有無でロードモードを変える
	// FT_LOAD_MONOCHROME はパキパキのドット絵モード
	FT_Int32 load_flags = antialias ? FT_LOAD_RENDER : (FT_LOAD_TARGET_MONO | FT_LOAD_RENDER);
	
	if (FT_Load_Char(ft_face, code, load_flags))
		return false;

	// FreeTypeから歩幅（64分分1ピクセル単位）を奪い取って、ピクセル単位に直す
	last_char_advance = ft_face->glyph->advance.x >> 6;

	// もし advance が 0 なら、最低限の幅を無理やり入れる（保険）
	if (last_char_advance == 0) {
		last_char_advance = (code <= 0xFF) ? 6 : 12;
	}

	FT_Bitmap &bitmap = ft_face->glyph->bitmap;

	// 2. 転写ロジック
	int start_y = (int)baseline - ft_face->glyph->bitmap_top;
	int start_x = ft_face->glyph->bitmap_left;

	for (int y = 0; y < (int)bitmap.rows; y++) {
		for (int x = 0; x < (int)bitmap.width; x++) {
			int ty = start_y + y;
			int tx = start_x + x;

			if (tx < 0 || tx >= dest_w || ty < 0 || ty >= dest_h) continue;

			unsigned char alpha = 0;
			if (antialias) {
				// グレー階調をそのままアルファに
				alpha = bitmap.buffer[y * bitmap.pitch + x];
			} else {
				// MONOCHROMEモードは1ビットずつデータが入っているので解析が必要
				unsigned char byte = bitmap.buffer[y * bitmap.pitch + (x >> 3)];
				alpha = (byte & (0x80 >> (x & 7))) ? 255 : 0;
			}

			if (alpha == 0) continue;

			int idx = (ty * dest_w + tx) * 4;
			dest[idx + 0] = 0; // R
			dest[idx + 1] = 0; // G
			dest[idx + 2] = 0; // B
			dest[idx + 3] = alpha;
		}
	}
	return true;
}

void cleanup()
{
	if (ft_face) {
		FT_Done_Face(ft_face);
		ft_face = nullptr;
	}
	if (ft_library) {
		FT_Done_FreeType(ft_library);
		ft_library = nullptr;
	}
	SDL_Quit();
}

} // namespace
