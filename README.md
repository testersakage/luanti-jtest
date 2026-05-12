------------------------------
## Luanti Unicode Modernization Project (V4 Engine "EX")

[English Documentation (README_en.md)](./README_en.md)

このリポジトリは、Luanti のテキスト描画システムを Unicode（特に日本語および多言語）に完全対応させ、Minecraft と同等、あるいはそれ以上の描画品質を実現するためのフォークです。
従来の Lua による低速な画像合成を完全に廃止。C++ エンジン側に直接「Unicode Glyph API」を実装し、さらに SDL2 / SDL_image を統合した 「EX Atlas エンジン」 により、あらゆるフォント規格を動的に飲み込む柔軟性と圧倒的なパフォーマンスを両立しました。

<img width=600, height=225, src="https://github.com/testersakage/luanti-jtest/blob/master/screenshots/samplesign.png"></img>
## 🔧 このフォークの革新的な機能

## 1. 動的定義エンジン "DDE" (Dynamic Definition Engine) [NEW]

* 規格からの解放: 12pxや14pxといった固定規格を廃止。外部JSONファイル（設計図）を読み込むことで、16x16や12x14など、あらゆるグリッドサイズのアトラスに即座に対応します。
* プロファイル切り替え: Mod側からJSONを指定するだけで、リビルドなしで看板のフォントセットを動的に切り替え可能です。

## 2. SDL2 / SDL_image による高精細レンダリング [NEW]

* マルチフォーマット対応: SDL_imageの導入により、PNG/JPG等の様々な画像形式をサポート。
* 透過錬金術 (Alpha Reverse): 背景が黒塗りの古いアセットでも、エンジン側でアルファチャンネルを反転・生成し、最新の透過看板として蘇らせます。
* ピクセルパーフェクト: 画像の実サイズから1ピクセルあたりの歩幅を逆算する cell_w ロジックを搭載。1pxの狂いもない完璧な文字間隔を実現しました。

## 3. C++/Lua 同期インフラ (SignManager)

* 共通の物差し: minetest.utf8sign API を通じて、サーバー（Lua）とクライアント（C++）が全く同じ文字幅データを共有。オンライン環境での表示ズレを根絶しました。
* 高度なレイアウト解析: 東アジア文字幅（EAW）に対応し、半角を1、全角を2として正確に判定。プロフェッショナルなワードラップを提供します。

## 4. マルチエンジン・ハイブリッド構成 [UPDATED]

* 旧Atlas (Standard) の洗練: 前回リリースしたIrrlicht（Luanti標準）ベースのエンジンも継続サポート。V4の知恵をフィードバックし、12px/14px判定の安定性をさらに向上させました。
* FreeType (FT) エンジンの内蔵: アトラス画像すら不要とする、SDL2_ttf / FreeType によるダイレクトレンダリング機能も搭載。TrueTypeフォント（TTF/OTF）をそのまま看板に映し出す究極の柔軟性を提供します。
* コンパイルオプション制御: 各エンジンはビルド時のフラグ（#ifdef）で個別に有効化可能。環境や用途に合わせた最適なバイナリを作成できます。

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

* 素材の自動認識: signs_lib がインストールされている環境であれば、同梱のサンプルModが自動的にそのテクスチャパスを検索し、EXエンジンへと登録します。
* 透過リマスター: 特殊な画像加工なしで、既存の「黒背景・白抜き」アセットを背景透過のドットフォントとして美しく表示します。

## 2. サンプルModによる導入 (mod_utf8signex_sample)
リポジトリに同梱されている mod_utf8signex_sample を mods/ フォルダに配置して有効化してください。

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
AIと人間の協力により、強固なコードベースを構築しました。
------------------------------

