------------------------------
## UTF8Sign API Reference (Certified Edition)
## 1. utf8: Basic & Statistical API
文字列操作APIです。

* utf8.len(s): 文字数（長さ）を返します。
* utf8.offset(s, n, [i]): n番目の文字のバイト位置を返します。
* utf8.codepoint(s, [i], [j]): コードポイント（数値）を返します。
* utf8.char(...): 数値から文字列を作成します。
* utf8.codes(s): イテレータを返します。
* utf8.to_table(s): 1文字ずつのテーブルに分解します。
* utf8.charpattern: 1文字を抽出するためのLuaパターンです。

------------------------------
## 2. utf8 wrap: Layout & Width Analysis API
描画の「計画（レイアウト）」を立てるための解析用APIです。

* utf8.eaw_width(s): 東アジア文字幅（半角1/全角2）による合計幅を返します。
* utf8.get_width(s): 現在のフォント設定での物理ピクセル幅を返します。
* utf8.get_lines(s, max_w): 指定幅で改行位置を計算し、行ごとのテーブルを返します。
* utf8.to_codepoints(s): 文字列を内部処理用の数値（int）配列に変換します。
* utf8.truncate(s, max_w): 指定ピクセル幅で切り詰めます。
* utf8.eaw_truncate(s, max_eaw): 東アジア文字幅で切り詰めます。

------------------------------
## 3. sign: Engine & Atlas Management API
エンジンの動作設定と、キャッシュ制御を司るAPIです。
## Global Configuration

* minetest.utf8sign.get_config(): 現在の全エンジン共通設定を取得します。
* minetest.utf8sign.set_config(table): 設定（フォントパス、サイズ等）を更新します。

## Extended Atlas (EX) Administration

* minetest.utf8sign.ex.load_atlas_config(path): 外部JSONからAtlasプロファイルを登録します。
* minetest.utf8sign.ex.get_atlas_status(): 現在アクティブなAtlasの詳細（ID、パス、グリッド設定等）を返します。

## Cache Statistics

* minetest.utf8sign.st.get_page_cache(): [ST専用] 旧Atlasのキャッシュページ数を返します。

* minetest.utf8sign.ex.get_char_cache(): [EX専用] 現在のEX版グリフキャッシュ数を返します。
* minetest.utf8sign.ex.get_page_cache(): [EX専用] ロード済みのAtlasページ（画像）数を返します。

* minetest.utf8sign.ft.get_cache_size(): [FT専用] 現在のキャッシュ上限設定を返します。
* minetest.utf8sign.ft.get_cache_count(): [FT専用] 現在メモリにあるFreeTypeグリフ数を返します。
* minetest.utf8sign.ft.set_cache_size(size): [FT専用] キャッシュ上限を更新します。
* minetest.utf8sign.ft.clear_cache(): [FT専用] キャッシュを強制消去します。

------------------------------
## 4. Configuration Settings (minetest.conf)
Luanti起動時に読み込まれる、エンジンの動作を制御するための設定項目です。
## Atlas Engine (ST/EX) Settings

* utf8_st_atlas_path
[ST専用] Standard Atlas画像のパスとファイル名のフォーマットを指定します。
luanti.exe からの相対パスが使用可能です。
(例: ../mods/mod_utf8sign_sample/textures/unicode_page_%02x.png)
* utf8_ex_char_cache = 64
[EX専用] Extended Atlasの一次キャッシュ（切り出し済み文字データ）の最大保持数を設定します。
* utf8_ex_page_cache = 4
[EX専用] Extended Atlasの二次キャッシュ（ロード済みのページ画像ファイル）の最大保持数を設定します。

## FreeType Engine (FT) Settings

* utf8_font_path = ../fonts/NotoSansCJKjp-Regular.otf
看板（FT版）およびレイアウト計算に使用するフォントファイルを指定します。
* utf8_ft_cache = 256
[FT専用] FreeTypeエンジンのグリフキャッシュ（レンダリング済み文字データ）の最大保持数を設定します。

------------------------------
