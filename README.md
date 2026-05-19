------------------------------
## Luanti Unicode Modernization Project

[English Documentation (README_en.md)](./README_en.md)

このリポジトリは、Luanti のテキスト描画システムを Unicode（特に日本語および多言語）に完全対応させ、Minecraft と同等の描画品質を目指したフォークです。
従来の Lua による文字画像の合成を廃止。Luaからは初期設定と文字列またはcodepointを「Unicode Glyph API」を通してC++ エンジン側に送信するだけで画像合成が完了します。さらに SDL2 / SDL_image にも対応したレンダリングエンジンにより、あらゆるフォント規格を動的に読み込む柔軟性と圧倒的なパフォーマンスを両立しました。

<img width=600, height=225, src="https://github.com/testersakage/luanti-jtest/blob/master/screenshots/samplesign.png"></img>
## 🔧 このフォークの革新的な機能

### 0. ネティブ C++ UTF-8 Glyph API インフラ
* 従来の Lua レイヤーによるテキストパースや文字幅計算（`utf8` ループ）を廃止。C++ エンジンの最深部に直接ネイティブな Unicode 解析基盤を構築しました。

### 1. マルチバイト対応 C++ フォントアトラス・インフラ 
* 従来の Lua 側での処理を廃止。数万文字におよぶ日本語・CJK（日中韓）の膨大なフォントデータを C++ のネイティブメモリ空間で超高速に捌く、独自のフォントアトラス（Font Atlas）スライサーとキャッシュシステムを実装しました。

### 2. 超高速画像合成コア「[utf8combine]」専用レンダラー
* 文字列コマンドからダイレクトに1枚の透過看板テクスチャを爆速で合成・生成する、独自の **`[utf8combine]` (および `[utf8combineex]`,`[utf8combineft]`) 専用レンダラー** を新設しました。

### 3. SDL2 / SDL_image / SDL_ttf グラフィックインフラの統合 
* Luanti 標準の描画エンジン（Irrlicht）の制約を打ち破るため、世界標準のグラフィックライブラリである **SDL2、SDL_image、SDL_ttf** をシステム内部へ統合しました。(Standardを除く)

### 4. カプセル化対応型・C++/Lua双方向同期インフラ「UTF8SignManager」
* 内蔵された強力な SDL2 グラフィックインフラと大元レイアウト物差しを中央制御するため、単一の強固な管理中枢である **`UTF8SignManager`** を実装しました。

### 5. 動的定義エンジン「DDE (Dynamic Definition Engine)」 (EX Atlasのみ)
* 従来のフォントアトラス描画における「C++側へのサイズや規格のハードコード」を完全に撤廃。外部JSONファイル（設計図）やLua側のパラメータテーブルを読み込むことで、12px、14px、16pxなど、この世に存在するあらゆるグリッドサイズや画像命名規則のアトラスへ、リビルドなしで即座にアジャスト・完全駆動する **「動的定義エンジン (DDE)」** を確立しました。

## マルチエンジン・ハイブリッド構成
システム構成に応じて3つのエンジンを用意しました。

### 1. ST (Standard) Atlas 
* Irrlicht（Luanti標準）ベースの標準的なエンジンで、従来の資産を活かしつつ高速化を実現。

### 2. EX (Extended) Atlas 
* Standardを拡張する未来のAtlasエンジン。SDLとの統合に加えて、外部JSONファイル（設計図）を読み込むことで、16x16や12x14など、あらゆるグリッドサイズのアトラスに即座に対応する動的定義エンジン（DDE）を搭載。

### 3. FT (FreeType) 
* アトラス画像すら不要とする、SDL2_ttf / FreeType によるダイレクトレンダリング機能を搭載。TrueTypeフォント（TTF/OTF）をそのまま看板に映し出す究極の柔軟性を提供します。

## コンパイルオプション制御
* 各エンジンはビルド時のフラグ（#ifdef）で個別に有効化可能。環境や用途に合わせた最適なバイナリを作成できます。

------------------------------
## 🛠️ ビルド方法 (How to Build)
Windows 上の MSYS2 CLANG64 環境でビルドと動作確認を行っています。

   1. 依存ライブラリの導入:
   MSYS2ターミナルで以下を実行し、SDL2関連のパッケージを導入してください。
   
   pacman -S mingw-w64-clang-x86_64-SDL2_image
   
   2. ビルドの実行:
   
   cmake . -B build -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles" -DENABLE_UTF8_SDL2_ATLAS=ON
   cmake --build build -j$(nproc)
   
   
------------------------------
## 🚀 導入と利用例 (Usage)
本エンジンは、既存の膨大な看板Mod資産を最大限に活用しつつ、最新の描画クオリティを提供することを目的としています。
## 1. 既存Modアセット（signs_lib）の活用
本エンジンは、Luantiで最も普及している [signs_lib](https://github.com/minetest-mods/font_api) のフォントアセット（GNU Unifont形式）を公式にサポートしています。

* 素材の自動認識(EX版のみ): signs_lib がインストールされている環境であれば、同梱のサンプルModが自動的にそのテクスチャパスを検索し、EXエンジンへと登録します。
* 透過リマスター: 特殊な画像加工なしで、既存の「黒背景・白抜き」アセットを背景透過のドットフォントとして美しく表示します。

## 2. サンプルModによる導入 
リポジトリに同梱されている3つのサンプルmod(mod_utf8sign_sample, mod_utf8signex_sample, mod_utf8signft_sample)からお好みのものを有効化してください。

* ContentDB対応: ContentDB等からダウンロードしたアセットのパスを自動解決するように構成されています。
* JSONによる動的定義: C++コードを書き換えることなく、JSONファイルを置くだけで新しいフォントアトラスを自由に追加できます。

## ⚠️ 注意事項：既存Modとの互換性について

* アセット利用の制限: 本エンジンは signs_lib 等のフォント画像（アセット）を拝借して描画を行いますが、それら他製Modが提供する看板ノード（Entity/Node）そのものの描画ロジックを自動的に書き換えるものではありません。
* 看板Modの改造: 既存Modの看板で本エンジンの高精細描画（EX Atlas）を利用したい場合は、そのMod側のLuaコードを修正し、描画命令を [utf8combineex:... 形式へ書き換える必要があります。
* 推奨環境: 本エンジンの機能をフルに体験するには、同梱のサンプルMod、または本エンジンを前提に設計されたModと組み合わせて使用することを推奨します。

------------------------------
## 📘 English Overview
This fork modernizes the Luanti text rendering system for full Unicode support. The newly implemented V4 "EX" Engine leverages SDL2 and a Dynamic Definition Engine (DDE) to support any font atlas specification (16x16, 12x14, etc.) via external JSON profiles. It features high-precision glyph extraction, automatic alpha-channel generation for legacy assets, and pixel-perfect synchronization between Lua and C++.
------------------------------
## 📝 補足
AI(Gemini)と人間の協力により、強固なコードベースを構築しました。
------------------------------

