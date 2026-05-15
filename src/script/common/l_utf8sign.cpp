// src/script/common/l_utf8sign.cpp
#include "l_utf8sign.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include "client/sdl2_font.h"  // sdl2_font の存在を教える
#include "settings.h"          // g_settings を使うため
#include "client/utf8_fontengine.h"   // UTF8FontEngine を使うため
#include "client/utf8_fontatlas.h"

#include "porting.h"      // porting::path_share 用
#include "filesys.h"      // fs::PathExists 用
#include "util/string.h"  // DIR_DELIM 用（あるいは get_separator() 等）
#include "settings.h"     // g_settings 用
#include "json/json.h"
#include "SDL2/SDL_image.h"
#include <fstream>
#include <sstream>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "common/c_internal.h"
#include "common/c_converter.h" // これも念のため追加
#include "log.h"


UTF8SignManager* UTF8SignManager::m_instance = nullptr;

UTF8SignManager* UTF8SignManager::getInstance() {
	if (!m_instance) m_instance = new UTF8SignManager();
	return m_instance;
}

#if UTF8_SDL2_ATLAS
// 登録 UTF8SignManager::registerAtlas(const AtlasDefinition &def, const std::string &path)
void UTF8SignManager::registerAtlas(const AtlasDefinition &def, const std::string &path)
{
	// 重複登録のチェック：すでに同じパスが名簿にあれば、何もしない
	for (const auto &existing : m_available_atlases) {
		if (existing.full_path == path)
			return;
	}

	ResolvedAtlas res = { def, path };
	m_available_atlases.push_back(res);

	actionstream << "UTF8SignManager: Registered Atlas Resource [" << def.mod_name 
	             << "] at " << path << std::endl;
}

// 選択 UTF8SignManager::selectAtlas(const std::string &id)
void UTF8SignManager::selectAtlas(const std::string &id)
{
	for (const auto &res : m_available_atlases) {
		if (res.def.mod_name == id) {
			// ─── 「設計図(def)」から現場用設定「this->ex」へすべての諸元を完全カプセル化 ───
			this->ex.ex_current_atlas_id = res.def.mod_name;
			this->ex.ex_sub_path         = res.def.sub_path;
			this->ex.ex_file_pattern     = res.def.file_pattern;
			this->ex.ex_grid_columns     = res.def.grid_columns;
			this->ex.ex_grid_size        = res.def.grid_size;
			this->ex.ex_alpha_reverse    = res.def.alpha_reverse;

			// 看板の基本歩幅と行間も、JSONのグリフ仕様（glyph_w / glyph_h）とダイレクトに連動
			this->ex.ex_char_w_zen       = res.def.glyph_w;
			this->ex.ex_char_w_han       = res.def.glyph_w / 2; // 自動で半分に
			this->ex.ex_line_height      = res.def.glyph_h;

			// ─── 大元インフラ（utf8_53）の例外ルール配列へ例外データを転送 ───
			utf8_53::g_half_width_ranges.clear();
			utf8_53::g_half_width_ranges = res.def.half_width_ranges;

			// ─── 現場用設定（this->ex）の例外配列へもルールを丸ごと同期コピー！ ───
			this->ex.ex_half_width_ranges.clear();
			this->ex.ex_half_width_ranges = res.def.half_width_ranges;
			
			actionstream << "UTF8SignManager: Active Atlas switched to [" << id << "]" 
			             << " with " << this->ex.ex_half_width_ranges.size() << " half-width rules." << std::endl;
			return;
		}
	}
	errorstream << "UTF8SignManager: Atlas ID [" << id << "] not found in Grimoire!" << std::endl;
}

// 読み込み UTF8SignManager::loadGrimoire(const std::string &override_path)
void UTF8SignManager::loadGrimoire(const std::string &override_path)
{
	// 二度読み防止
	static bool grimoire_loaded = false;
	if (grimoire_loaded) return;

	std::string grimoire_path = override_path;

	// 引数がない場合は、これまでの minetest.conf 経由のパスを作る
	if (grimoire_path.empty()) {
		std::string grimoire_name = g_settings->get("utf8_atlas_config");
		if (grimoire_name.empty()) return;
		grimoire_path = std::string(porting::path_share) + DIR_DELIM + "fonts" + DIR_DELIM + grimoire_name;
	}

	if (fs::PathExists(grimoire_path)) {
		// actionstream << "UTF8SignManager: Found Atlas config." << std::endl;
	} else {
		// あくまで警告に留め、動作は止めない
		infostream << "UTF8SignManager: Not found Atlas config." << std::endl;
		return;
	}

	std::ifstream ifs(grimoire_path);
	Json::Value root;
	Json::Reader reader;

	if (reader.parse(ifs, root)) {
		// actionstream << "UTF8SignManager: [Atlas Config Parsing] " << grimoire_path << std::endl;

		const Json::Value atlases = root["target_atlases"];
		for (u32 i = 0; i < atlases.size(); ++i) {
			const Json::Value &entry = atlases[i];
			
			AtlasDefinition def;
			def.mod_name      = entry.get("id", "unknown").asString();
			def.sub_path      = entry.get("path", "").asString();
			def.file_pattern  = entry.get("file_pattern", "uni%02x.png").asString();
			def.grid_columns  = entry.get("grid_columns", 32).asUInt();
			def.grid_size     = entry.get("grid_size", 14).asUInt();
			def.glyph_w       = entry.get("glyph_w", 12).asUInt();
			def.glyph_h       = entry.get("glyph_h", 14).asUInt();
			def.alpha_reverse = entry.get("alpha_reverse", true).asBool();

			// ─── 【消失前再現】JSONの half_width_ranges 配列を正確にパースして def へ格納 ───
			if (entry.isMember("half_width_ranges") && entry["half_width_ranges"].isArray()) {
				const Json::Value &ranges_json = entry["half_width_ranges"];
				for (u32 j = 0; j < ranges_json.size(); ++j) {
					const Json::Value &range_entry = ranges_json[j];
					std::string start_str = range_entry.get("start", "0").asString();
					std::string end_str   = range_entry.get("end", "0").asString();
					
					utf8_53::WidthRange range;
					try {
						// 16進数文字列を数値へ変換（"0x0400" -> 1024）
						range.start = static_cast<u32>(std::stoul(start_str, nullptr, 16));
						range.end   = static_cast<u32>(std::stoul(end_str, nullptr, 16));
						range.memo  = range_entry.get("_memo", "").asString(); 

						// ─── 安全ガード1. 整合性チェック ───
						// 開始コードが終了コードより大きい、またはどちらかが0の場合は不正データとして弾く
						if (range.start > range.end || range.start == 0 || range.end == 0) {
							throw std::runtime_error("Invalid code range (start > end or zero)");
						}

						// 完璧なデータだけを配列に格納
						def.half_width_ranges.push_back(range);

					} catch (const std::exception &e) {
						// ─── 安全ガード2. 犯人を名指しでログに残す（デバッグ用） ───
						// どの設定ファイルの、何番目の要素で、なぜコケたのかをエラーログに美しく焼き付けます
						errorstream << "UTF8SignManager: [Warning] Failed to parse 'half_width_ranges' in Atlas [" 
						            << def.mod_name << "] at index " << j 
						            << " (start: '" << start_str << "', end: '" << end_str << "'). "
						            << "Reason: " << e.what() << ". Skipping this rule." << std::endl;

						// ─── 安全ガード3. サイレント・スキップ（続行） ───
						// continue は不要（ループの末尾なので自動的に次の j のスキャンへ安全に進みます）
					}
				}
			}

			// ─── 【消失前再現】5.15.2の2重スラッシュ・癒着を完全に防ぐ一本道結合 ───
			// path_shareの末尾状況に左右されず、確実にフォルダー区切りを1本挟み込みます
			std::string full_path = std::string(porting::path_share) + "/" + def.sub_path;

			// チェック用のファイル名を file_pattern から生成 (0番目のページ)
			char check_file[512];
			snprintf(check_file, sizeof(check_file), def.file_pattern.c_str(), 0);

			if (fs::PathExists(full_path + DIR_DELIM + check_file)) {
				this->registerAtlas(def, full_path);

				//  最初に見つかったAtlasを「デフォルト」として自動指名
				if (this->ex.ex_current_atlas_id == "none" || this->ex.ex_current_atlas_id.empty()) {
					this->selectAtlas(def.mod_name);
				}
			}
		}
		grimoire_loaded = true;
	}
}

// 選択情報の取得 UTF8SignManager::getSelectedAtlas()
const ResolvedAtlas& UTF8SignManager::getSelectedAtlas() const {
	// 1. atlas構造体の中の ID で検索
	for (const auto &res : m_available_atlases) { // 名前を合わせる
		if (res.def.mod_name == ex.ex_current_atlas_id) {
			return res;
		}
	}
	// 2. 見つからなければ先頭を返す
	if (!m_available_atlases.empty()) {
		return m_available_atlases[0];
	}

	static ResolvedAtlas fallback;
	return fallback;
}
#endif

UTF8SignManager::UTF8SignManager() {

	// --- SDL2イメージ・インフラの初期化 (一回だけ実行) ---
#if (UTF8_SDL2_ATLAS || UTF8_SDL2_FREETYPE)
    int flags = IMG_INIT_PNG | IMG_INIT_JPG; // PNGとJPGを有効化
    if ((IMG_Init(flags) & flags) != flags) {
        errorstream << "UTF8SignManager: SDL_image initialization failed! " << IMG_GetError() << std::endl;
    } else {
        actionstream << "UTF8SignManager: SDL_image (PNG/JPG) ready." << std::endl;
    }
#endif

#if UTF8_ATLAS
	st.st_page_cache = 4;

	if (g_settings) {
		if (g_settings->exists("utf8_st_page_cache")) {
			u32 conf_val = g_settings->getU32("utf8_st_page_cache");
			// 「防波堤 (8)」で安全を確保
			if (conf_val > 8) conf_val = 8;
			st.st_page_cache = conf_val;
		}
	}

//	actionstream << "UTF8SignManager: Atlas Path: " 
//		<< st_atlas.st_atlas_path << std::endl;
	actionstream << "UTF8SignManager: ST Font cache initialized with page: " 
		<< st.st_page_cache << std::endl;
#endif

#if UTF8_SDL2_ATLAS
	ex.ex_char_cache = 64;

	if (g_settings) {
		if (g_settings->exists("utf8_ex_char_cache")) {
			u32 conf_val = g_settings->getU32("utf8_ex_char_cache");
			if (conf_val > 256) conf_val = 256;
			ex.ex_char_cache = conf_val;
		}
	}

	ex.ex_page_cache = 4;

	if (g_settings) {
		if (g_settings->exists("utf8_ex_page_cache")) {
			u32 conf_val = g_settings->getU32("utf8_ex_page_cache");
			if (conf_val > 8) conf_val = 8;
			ex.ex_page_cache = conf_val;
		}
	}

	actionstream << "UTF8SignManager: EX Font cache initialized with size: " 
		<< ex.ex_char_cache << " / page: " << ex.ex_page_cache << std::endl;
#endif

#if UTF8_SDL2_FREETYPE
	ft.cache_size = 256;
	if (g_settings) {
		if (g_settings->exists("utf8_ft_cache")) {
			u32 conf_val = g_settings->getU32("utf8_ft_cache");
			// 3. 「防波堤 (2048)」で安全を確保
			if (conf_val > 2048) conf_val = 2048;
			ft.cache_size = conf_val;
		}
	}

	actionstream << "UTF8SignManager: FT Font cache initialized with size: " 
		<< ft.cache_size << std::endl;
#endif
}


namespace l_utf8sign {

// minetest.utf8sign.set_config(table)
int l_set_config(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	auto &mgr = *UTF8SignManager::getInstance();

#if UTF8_ATLAS
	/*
	lua_getfield(L, 1, "st_atlas");
	if (lua_istable(L, -1)) {
		lua_getfield(L, -1, "st_sign_width");
		if (!lua_isnil(L, -1)) { // 型チェック関数を使う
			warningstream << "l_utf8sign: 'st_sign_width' in set_config is DEPRECATED and ignored. "
				<< "The width is now automatically determined by the texture spec "
				<< "(e.g., [utf8combine:WIDTHxHEIGHT:...)." << std::endl;
		}
		lua_pop(L, 1);
		mgr.st_atlas.st_char_w_han   = getintfield_default(L, -1, "st_char_w_han",   mgr.st_atlas.st_char_w_han);
		mgr.st_atlas.st_char_w_zen   = getintfield_default(L, -1, "st_char_w_zen",   mgr.st_atlas.st_char_w_zen);
		mgr.st_atlas.st_line_height  = getintfield_default(L, -1, "st_line_height",  mgr.st_atlas.st_line_height);
		lua_getfield(L, -1, "st_padding_x");
		if (!lua_isnil(L, -1)) {
			warningstream << "l_utf8sign: 'st_padding_x' in set_config is ignored. "
			<< "Specify combine position in texture string (e.g., :x,y@401400) instead." << std::endl;
		}
		lua_pop(L, 1);
		lua_getfield(L, -1, "st_padding_y");
		if (!lua_isnil(L, -1)) {
			warningstream << "l_utf8sign: 'st_padding_y' in set_config is ignored. "
			<< "Specify combine position in texture string (e.g., :x,y@401400) instead." << std::endl;
		}
		lua_pop(L, 1);
		mgr.st_atlas.st_max_lines = getintfield_default(L, -1, "st_max_lines", mgr.st_atlas.st_max_lines);
		lua_getfield(L, -1, "st_atlas_path");
		if (lua_isstring(L, -1)) {
			mgr.st_atlas.st_atlas_path = lua_tostring(L, -1);
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	*/
	// 2. st_atlas グループの取得（Luaから直通）
	lua_getfield(L, 1, "st");
	if (lua_istable(L, -1)) {
		mgr.st.st_char_w_han   = getintfield_default(L, -1, "st_char_w_han",   mgr.st.st_char_w_han);
		mgr.st.st_char_w_zen   = getintfield_default(L, -1, "st_char_w_zen",   mgr.st.st_char_w_zen);
		mgr.st.st_line_height  = getintfield_default(L, -1, "st_line_height",  mgr.st.st_line_height);
		mgr.st.st_max_lines    = getintfield_default(L, -1, "st_max_lines",    mgr.st.st_max_lines);
		
		// あなたが新設した新メンバー群を Lua から安全に吸い上げる
		mgr.st.st_grid_columns  = getintfield_default(L, -1, "st_grid_columns",  mgr.st.st_grid_columns);
		mgr.st.st_grid_size     = getintfield_default(L, -1, "st_grid_size",     mgr.st.st_grid_size);
		mgr.st.st_page_cache    = getintfield_default(L, -1, "st_page_cache",    mgr.st.st_page_cache);

		// 文字列の取得
		lua_getfield(L, -1, "st_atlas_path");
		if (lua_isstring(L, -1)) {
			mgr.st.st_atlas_path = lua_tostring(L, -1);
		}
		lua_pop(L, 1);

		// ブーリアン（フラグ）の取得
		lua_getfield(L, -1, "st_alpha_reverse");
		if (lua_isboolean(L, -1)) {
			mgr.st.st_alpha_reverse = lua_toboolean(L, -1);
		}
		lua_pop(L, 1);

		lua_getfield(L, -1, "uax_half_switch");
		if (lua_isboolean(L, -1)) {
			mgr.st.uax_half_switch = lua_toboolean(L, -1);
		}
		lua_pop(L, 1);

		// ─── 【職人のハック】スイッチが ON なら、大元インフラへ UAX #11 固定ルールを全自動強制注入！ ───
		if (mgr.st.uax_half_switch) {
			utf8_53::g_half_width_ranges.clear();
			utf8_53::g_half_width_ranges.push_back({0x0370, 0x03FF, "ST-ギリシャ文字"});
			utf8_53::g_half_width_ranges.push_back({0x0400, 0x04FF, "ST-キリル文字"});
		}
	}
	lua_pop(L, 1);
#endif

#if UTF8_SDL2_ATLAS
	// 1. ex_atlas グループの取得
	lua_getfield(L, 1, "ex");
	if (lua_istable(L, -1)) {
		
		// --- 【大掃除の目玉】Luaから「id = "〇〇"」が指定されたら、その場でアトラスを強制切り替え！ ---
		// これにより、ビューアーやジェネレーター画面の操作とC++インフラが完全にリアルタイム同期します
		lua_getfield(L, -1, "id");
		if (lua_isstring(L, -1)) {
			std::string target_id = lua_tostring(L, -1);
			mgr.selectAtlas(target_id); // 魔導書から全諸元とUAX #11ルールをConfigへ一発カプセル化！
		}
		lua_pop(L, 1);

		// --- 既存の非推奨項目の安全ガード ---
		lua_getfield(L, -1, "ex_sign_width");
		if (!lua_isnil(L, -1)) {
			warningstream << "l_utf8sign: 'ex_sign_width' in set_config is DEPRECATED and ignored. "
				<< "The width is now automatically determined by the texture spec "
				<< "(e.g., [utf8combineex:WIDTHxHEIGHT:...)." << std::endl;
		}
		lua_pop(L, 1);

		// 歩幅・行間のフォールバック（手動指定があれば受け付ける）
		mgr.ex.ex_char_w_han   = getintfield_default(L, -1, "ex_char_w_han",   mgr.ex.ex_char_w_han);
		mgr.ex.ex_char_w_zen   = getintfield_default(L, -1, "ex_char_w_zen",   mgr.ex.ex_char_w_zen);
		mgr.ex.ex_line_height  = getintfield_default(L, -1, "ex_line_height",  mgr.ex.ex_line_height);

		// パディング警告ガード
		lua_getfield(L, -1, "ex_padding_x");
		if (!lua_isnil(L, -1)) {
			warningstream << "l_utf8sign: 'ex_padding_x' in set_config is ignored. "
				<< "Specify combine position in texture string (e.g., :x,y@401400) instead." << std::endl;
		}
		lua_pop(L, 1);

		lua_getfield(L, -1, "ex_padding_y");
		if (!lua_isnil(L, -1)) {
			warningstream << "l_utf8sign: 'ex_padding_y' in set_config is ignored. "
				<< "Specify combine position in texture string (e.g., :x,y@401400) instead." << std::endl;
		}
		lua_pop(L, 1);

		mgr.ex.ex_max_lines = getintfield_default(L, -1, "ex_max_lines", mgr.ex.ex_max_lines);

		// アルファ反転に関する警告ガード（新ヘッダー名 ex_alpha_reverse への配線修正）
		lua_getfield(L, -1, "ex_alpha_reverse");
		if (!lua_isnil(L, -1)) {
			warningstream << "l_utf8sign: 'ex_alpha_reverse' cannot be set via set_config."
				<< " Please define it in your Atlas JSON." << std::endl;
		}
		lua_pop(L, 1);

		// json からの情報は基本set不可（魔導書絶対主義の維持）
		// キャッシュは専用API経由で
	}
	lua_pop(L, 1);
#endif

#if UTF8_SDL2_FREETYPE
	// 2. ft グループの取得
	lua_getfield(L, 1, "ft");
	if (lua_istable(L, -1)) {
		// 文字列の更新（ttf_nameなど）が必要な場合は lua_getfield して lua_tostring 
		lua_getfield(L, -1, "ttf_name");
		if (lua_isstring(L, -1)) {
			mgr.ft.ttf_name = lua_tostring(L, -1);
		}
		lua_pop(L, 1);
		mgr.ft.font_index  = getintfield_default(L, -1, "font_index",  mgr.ft.font_index);

		mgr.ft.font_size   = getintfield_default(L, -1, "font_size",   mgr.ft.font_size);
		mgr.ft.baseline_y  = getintfield_default(L, -1, "baseline_y",  mgr.ft.baseline_y);
		// default_color はLua側からは参照専用（設定不可）
		lua_getfield(L, -1, "default_color");
		if (!lua_isnil(L, -1)) { // 型チェック関数を使う
			warningstream << "l_utf8sign: 'default_color' in set_config is ignored. "
			<< "Specify color in texture string (e.g., @401400) instead." << std::endl;
		}
		lua_pop(L, 1);
		mgr.ft.antialias   = getboolfield_default(L, -1, "antialias",  mgr.ft.antialias);

		lua_getfield(L, -1, "ft_sign_width");
		if (!lua_isnil(L, -1)) { // 型チェック関数を使う
			warningstream << "l_utf8sign: 'ft_sign_width' in set_config is DEPRECATED and ignored. "
				<< "The width is now automatically determined by the texture spec "
				<< "(e.g., [utf8combineft:WIDTHxHEIGHT:...)." << std::endl;
		}
		lua_pop(L, 1);
		mgr.ft.ft_line_height = getintfield_default(L, -1, "ft_line_height", mgr.ft.ft_line_height);
		mgr.ft.ft_max_lines   = getintfield_default(L, -1, "ft_max_lines",   mgr.ft.ft_max_lines);
		mgr.ft.ft_char_w_han  = getintfield_default(L, -1, "ft_char_w_han",  mgr.ft.ft_char_w_han);
		mgr.ft.ft_char_w_zen  = getintfield_default(L, -1, "ft_char_w_zen",  mgr.ft.ft_char_w_zen);
		lua_getfield(L, -1, "ft_padding_x");
		if (!lua_isnil(L, -1)) {
			warningstream << "l_utf8sign: 'ft_padding_x' in set_config is ignored. "
			<< "Specify combine position in texture string (e.g., :x,y@401400) instead." << std::endl;
		}
		lua_pop(L, 1);
		lua_getfield(L, -1, "ft_padding_y");
		if (!lua_isnil(L, -1)) {
			warningstream << "l_utf8sign: 'ft_padding_y' in set_config is ignored. "
			<< "Specify combine position in texture string (e.g., :x,y@401400) instead." << std::endl;
		}
		lua_pop(L, 1);
		// --- 合成モードの取得 ---
		// Lua側で blend_mode = 1 と書けば ALPHA になる
		int b_mode = getintfield_default(L, -1, "blend_mode", (int)mgr.ft.blend_mode);
		mgr.ft.blend_mode = (SignBlendMode)b_mode;
	}
	lua_pop(L, 1);
#endif

	return 0;
}

// minetest.utf8sign.get_config()
int l_get_config(lua_State *L) {
	auto &mgr = *UTF8SignManager::getInstance();
	lua_newtable(L);
#if UTF8_ATLAS
	// ─── 【新設】st_atlas テーブルを作成して Lua 側へ大公開！ ───
	lua_newtable(L);

	setintfield(L, -1, "st_sign_width",    mgr.st.st_sign_width);
	setintfield(L, -1, "st_char_w_han",    mgr.st.st_char_w_han);
	setintfield(L, -1, "st_char_w_zen",    mgr.st.st_char_w_zen);
	setintfield(L, -1, "st_line_height",   mgr.st.st_line_height);
	setintfield(L, -1, "st_max_lines",     mgr.st.st_max_lines);
	setintfield(L, -1, "st_grid_columns",  mgr.st.st_grid_columns);
	setintfield(L, -1, "st_grid_size",     mgr.st.st_grid_size);
	setintfield(L, -1, "st_page_cache",    mgr.st.st_page_cache);

	lua_pushstring(L, mgr.st.st_atlas_path.c_str());
	lua_setfield(L, -2, "st_atlas_path");

	lua_pushboolean(L, mgr.st.st_alpha_reverse);
	lua_setfield(L, -2, "st_alpha_reverse");

	lua_pushboolean(L, mgr.st.uax_half_switch);
	lua_setfield(L, -2, "uax_half_switch");

	// 【これみよがし】スイッチが ON の場合、C++側が内部生成した固定ルールも Lua へ親切に教えてあげる
	lua_newtable(L);
	if (mgr.st.uax_half_switch) {
		// 1. ギリシャ文字の報告
		lua_newtable(L);
		lua_pushinteger(L, 0x0370); lua_setfield(L, -2, "start");
		lua_pushinteger(L, 0x03FF); lua_setfield(L, -2, "end");
		lua_pushstring(L, "ST-ギリシャ文字"); lua_setfield(L, -2, "memo");
		lua_rawseti(L, -2, 1);

		// 2. キリル文字の報告
		lua_newtable(L);
		lua_pushinteger(L, 0x0400); lua_setfield(L, -2, "start");
		lua_pushinteger(L, 0x04FF); lua_setfield(L, -2, "end");
		lua_pushstring(L, "ST-キリル文字"); lua_setfield(L, -2, "memo");
		lua_rawseti(L, -2, 2);
	}
	lua_setfield(L, -2, "half_width_ranges");

	// "st" というキーで大元テーブルにガチッと合体！
	lua_setfield(L, -2, "st");
#endif

#if UTF8_SDL2_ATLAS

	// ex_atlasテーブルを作成
	lua_newtable(L);
	
	// --- 既存のConfig項目 ---
	setintfield(L, -1, "ex_sign_width",   mgr.ex.ex_sign_width);
	setintfield(L, -1, "ex_char_w_han",   mgr.ex.ex_char_w_han);
	setintfield(L, -1, "ex_char_w_zen",   mgr.ex.ex_char_w_zen);
	setintfield(L, -1, "ex_line_height",  mgr.ex.ex_line_height);
	setintfield(L, -1, "ex_padding_x",    mgr.ex.ex_padding_x);
	setintfield(L, -1, "ex_padding_y",    mgr.ex.ex_padding_y);
	setintfield(L, -1, "ex_max_lines",    mgr.ex.ex_max_lines);

	// --- 最新のヘッダー仕様（ex_***）に合わせて正確にマッピング変更 ---
	lua_pushstring(L, mgr.ex.ex_current_atlas_id.c_str()); // ex_current_atlas_id へ修正
	lua_setfield(L, -2, "id"); 

	lua_pushboolean(L, mgr.ex.ex_alpha_reverse);           // ex_alpha_reverse へ修正
	lua_setfield(L, -2, "ex_alpha_reverse");

	setintfield(L, -1, "ex_grid_size",    mgr.ex.ex_grid_size); // ex_grid_size へ修正

	// 文字列の命名規則や現場の住所も、必要に応じてLuaへ報告可能
	lua_pushstring(L, mgr.ex.ex_file_pattern.c_str());
	lua_setfield(L, -2, "ex_file_pattern");
	
	setintfield(L, -1, "ex_grid_columns", mgr.ex.ex_grid_columns);

	// ─── 【新設】C++の例外ルール（ベクター）を Lua の配列テーブルへ全自動翻訳 ───
	lua_newtable(L); // 例外ルール格納用の配列テーブル

	int rule_index = 1;
	for (const auto &range : mgr.ex.ex_half_width_ranges) {
		lua_newtable(L); // 各ルール用の `{start=x, ["end"]=y}` テーブル

		lua_pushinteger(L, range.start);
		lua_setfield(L, -2, "start");

		lua_pushinteger(L, range.end);
		lua_setfield(L, -2, "end"); // Luaの予約語対策として文字列キーで安全にセット

		lua_pushstring(L, range.memo.c_str());
		lua_setfield(L, -2, "memo");

		// 完成した1つのルールテーブルを、大元の配列の [rule_index] 番目へ格納
		lua_rawseti(L, -2, rule_index);
		rule_index++;
	}
	
	// 完成した配列テーブルを、"half_width_ranges" という名前で "ex" グループに合体！
	lua_setfield(L, -2, "half_width_ranges");

	lua_setfield(L, -2, "ex");
#endif

#if UTF8_SDL2_FREETYPE

	// ftテーブルを作成
	lua_newtable(L);
	// --- 文字列（フォントパス） ---
	lua_pushstring(L, mgr.ft.ttf_name.c_str());
	lua_setfield(L, -2, "ttf_name");
	setintfield(L, -1, "font_index",  mgr.ft.font_index);

	setintfield(L, -1, "font_size",   mgr.ft.font_size);
	setintfield(L, -1, "baseline_y",  mgr.ft.baseline_y);
	// --- カラーテーブル（default_color） ---
	lua_newtable(L); // カラー用の小テーブル作成
	setintfield(L, -1, "r", mgr.ft.default_color.getRed());
	setintfield(L, -1, "g", mgr.ft.default_color.getGreen());
	setintfield(L, -1, "b", mgr.ft.default_color.getBlue());
	setintfield(L, -1, "a", mgr.ft.default_color.getAlpha());
	lua_setfield(L, -2, "default_color"); // ftテーブルに紐付け	
	setboolfield(L, -1, "antialias",  mgr.ft.antialias);
	// 現在の描画モードを判定して Lua に伝える
	FT_Face face = (FT_Face)sdl2_font::get_face_ptr();
	bool is_actually_gray = false;
	if (face && face->glyph) {
		// PixelModeが GRAY(2) ならアンチエイリアスが効いている
		is_actually_gray = (face->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);
	}
	setboolfield(L, -1, "actually_antialiased", is_actually_gray);

	// --- 基本項目 ---
	setintfield(L, -1, "ft_sign_width",  mgr.ft.ft_sign_width);
	setintfield(L, -1, "ft_line_height", mgr.ft.ft_line_height);
	setintfield(L, -1, "ft_max_lines",   mgr.ft.ft_max_lines);
	setintfield(L, -1, "ft_char_w_han",  mgr.ft.ft_char_w_han);
	setintfield(L, -1, "ft_char_w_zen",  mgr.ft.ft_char_w_zen);
	setintfield(L, -1, "ft_padding_x",   mgr.ft.ft_padding_x);
	setintfield(L, -1, "ft_padding_y",   mgr.ft.ft_padding_y);
	setintfield(L, -1, "blend_mode",  (int)mgr.ft.blend_mode);
	setintfield(L, -1, "cache_size",   mgr.ft.cache_size);
	lua_setfield(L, -2, "ft");

#endif

	return 1;
}

#if UTF8_ATLAS
//  Cache 使用量 表示
int l_st_get_page_cache(lua_State *L) {
	lua_pushinteger(L, UTF8FontAtlas::getPageCache());
	return 1;
}
#endif

#if UTF8_SDL2_ATLAS
// minetest.utf8sign.ex_load_atlas_config(path)
int l_ex_load_atlas_config(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	UTF8SignManager::getInstance()->loadGrimoire(std::string(path));
	return 0;
}

int l_ex_get_atlas_status(lua_State *L) {
	auto &mgr = *UTF8SignManager::getInstance();
	const ResolvedAtlas &active = mgr.getSelectedAtlas();

	lua_newtable(L);

	lua_pushstring(L, active.def.mod_name.c_str());
	lua_setfield(L, -2, "active_id");

	lua_pushstring(L, active.full_path.c_str());
	lua_setfield(L, -2, "path");

	lua_pushstring(L, active.def.file_pattern.c_str());
	lua_setfield(L, -2, "file_pattern");

	lua_pushinteger(L, active.def.grid_columns);
	lua_setfield(L, -2, "grid_columns");

	lua_pushinteger(L, active.def.grid_size);
	lua_setfield(L, -2, "grid_size");

	lua_pushinteger(L, active.def.glyph_w);
	lua_setfield(L, -2, "glyph_w");

	lua_pushinteger(L, active.def.glyph_h);
	lua_setfield(L, -2, "glyph_h");

	lua_pushboolean(L, active.def.alpha_reverse);
	lua_setfield(L, -2, "alpha_reverse");

	return 1;
}

//  Char Cache 使用量
int l_ex_get_char_cache(lua_State *L) {
	lua_pushinteger(L, UTF8FontAtlas::getCharCacheEx());
	return 1;
}

//  Cache_sizeの現在の設定値確認
int l_ex_get_page_cache(lua_State *L) {
	lua_pushinteger(L, UTF8FontAtlas::getPageCacheEx());
	return 1;
}
#endif

#if UTF8_SDL2_FREETYPE
//  Cache 使用量
int l_ft_get_cache_count(lua_State *L) {
	lua_pushinteger(L, UTF8FontEngine::getCacheCount());
	return 1;
}

//  Cache_sizeの現在の設定値確認
int l_ft_get_cache_size(lua_State *L) {
	u32 size = UTF8SignManager::getInstance()->ft.cache_size;
	lua_pushinteger(L, size);
	return 1;
}

//  Cache_sizeの大きさ変更（上限2048）
int l_ft_set_cache_size(lua_State *L) {
	u32 new_size = (u32)luaL_checkinteger(L, 1);
	if (new_size > 2048) new_size = 2048; // ★防波堤

	UTF8SignManager::getInstance()->ft.cache_size = new_size;
	return 0;
}

//  Cacheの消去
int l_ft_clear_cache(lua_State *L) {
	UTF8FontEngine::clearCache();
	return 0;
}

// minetest.utf8sign.ft_get_debug_status()
int l_ft_get_debug_status(lua_State *L) {
	auto &ft_cfg = UTF8SignManager::getInstance()->ft; 
	lua_newtable(L);
	lua_pushstring(L, ft_cfg.last_spec.c_str());
	lua_setfield(L, -2, "spec");
	lua_pushinteger(L, ft_cfg.last_codes_size);
	lua_setfield(L, -2, "codes_size");

	// sdl2_font から最新の「自白データ」を奪い取って Lua のテーブルに詰める
	lua_pushstring(L, sdl2_font::get_last_path().c_str());
	lua_setfield(L, -2, "cpp_path"); // これが Lua の status.cpp_path になる

	lua_pushinteger(L, sdl2_font::get_last_error());
	lua_setfield(L, -2, "last_error"); // これが Lua の status.last_error になる
	return 1;
}
#endif

// 共通の登録関数
void Initialize(lua_State *L, int top) {
	lua_newtable(L); // minetest.utf8sign テーブル

	// --- 共通設定 ---
	lua_pushcfunction(L, l_set_config);
	lua_setfield(L, -2, "set_config");
	lua_pushcfunction(L, l_get_config);
	lua_setfield(L, -2, "get_config");

#if UTF8_ATLAS
	// --- minetest.utf8sign.st サブテーブル ---
	lua_newtable(L);
	lua_pushcfunction(L, l_st_get_page_cache);
	lua_setfield(L, -2, "get_page_cache");
	lua_setfield(L, -2, "st");
#endif

#if UTF8_SDL2_ATLAS
	// --- minetest.utf8sign.ex サブテーブル ---
	lua_newtable(L);
	lua_pushcfunction(L, l_ex_load_atlas_config);
	lua_setfield(L, -2, "load_atlas_config");
	lua_pushcfunction(L, l_ex_get_atlas_status);
	lua_setfield(L, -2, "get_atlas_status");
	lua_pushcfunction(L, l_ex_get_char_cache);
	lua_setfield(L, -2, "get_char_cache");
	lua_pushcfunction(L, l_ex_get_page_cache);
	lua_setfield(L, -2, "get_page_cache");
	lua_setfield(L, -2, "ex");
#endif

#if UTF8_SDL2_FREETYPE
	// --- minetest.utf8sign.ft サブテーブル ---
	lua_newtable(L); 
	lua_pushcfunction(L, l_ft_get_cache_count);
	lua_setfield(L, -2, "get_cache_count");
	lua_pushcfunction(L, l_ft_get_cache_size);
	lua_setfield(L, -2, "get_cache_size");
	lua_pushcfunction(L, l_ft_set_cache_size);
	lua_setfield(L, -2, "set_cache_size");
	lua_pushcfunction(L, l_ft_clear_cache);
	lua_setfield(L, -2, "clear_cache");
	lua_pushcfunction(L, l_ft_get_debug_status); // FT用デバッグとしてここに配置
	lua_setfield(L, -2, "get_debug_status");
	lua_setfield(L, -2, "ft"); // utf8sign.ft として登録
#endif

	lua_setfield(L, top, "utf8sign");
}

} // namespace l_utf8sign
