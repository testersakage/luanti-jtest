------------------------------
## Luanti Unicode Modernization Project (V3 Engine)
このリポジトリは、Luanti のテキスト描画システムを Unicode（特に日本語および多言語）に完全対応させ、Minecraft と同等、あるいはそれ以上の描画品質を実現するためのフォークです。
従来の Lua による低速な画像合成を廃止し、C++ エンジン側に直接「Unicode Glyph API」を実装することで、圧倒的なパフォーマンスと美しさを両立しています。
------------------------------
<img width=600, height=225, src="https://github.com/testersakage/luanti-jtest/blob/master/screenshots/samplesign.png"></img>
## 🔧 このフォークの革新的な機能

## 1. 14px の聖域 (Precision Rendering)

* 14px ユニット設計: 従来の 12px 規格を刷新し、上下に余裕を持たせた 14px 高アトラス を採用。
* ベースライン固定: 11px 目をベースラインに固定することで、キリル文字の下部や括弧のハミ出しを 1px の狂いもなく完璧に収容します。

## 2. C++/Lua 同期 API (SignManager)

* 共通の物差し: minetest.utf8sign API を通じて、サーバー（Lua）とクライアント（C++）が全く同じ文字幅データを共有。
* 黄金律の徹底: ASCII/半角カナを 6px、日本語・多言語を 12px と判定するロジックにより、オンライン環境での表示ズレを根絶しました。

## 3. ハイブリッド・ワードラップ (Intelligent Wrap)

* 物理限界の死守: スペースによる単語改行を優先しつつ、看板の物理幅を超えた場合は文字単位で強制改行。ロシア語などの長い単語も、一文字も漏らさず看板に収めます。

## 4. 超高速アトラス・プロセッサ

* V3 判定エンジン: 読み込んだアトラス画像（PNG）の高さから、12px版か14px版かを自動判別。リソースの解像度に合わせて動的に切り出し範囲を最適化します。

------------------------------

## 🛠️ ビルド方法 (How to Build)  
このフォークは Windows 上の **MSYS2 CLANG64** 環境でビルドと動作確認を行っています。

1. **環境準備**: [MSYS2](https://msys2.org) をインストールし、CLANG64 環境をセットアップしてください。  
2. **依存ライブラリの導入**: Luanti のビルドに必要なパッケージ一式を導入します。  
3. **ビルドの実行**:  
   ```bash
   cmake . -B build -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"  
   cmake --build build -j$(nproc)  


   1. 起動: build/bin/luanti.exe が生成されます。  

------------------------------
## 🚀 導入と利用例  

このエンジンを利用するには、**フォントアトラス（PNG画像）** の準備が必要です。

### 1. フォントアトラスの生成  
本エンジン専用のアトラス生成ツール [**font2unimg**](https://github.com/testersakage/font2unimg) を使用して、お好みの TrueType フォントから PNG 画像を生成してください。
*   **推奨設定**: `line_height: 14` (14px高) で生成することで、本エンジンの真価を発揮します。
*   生成した画像は、各 Mod の指定フォルダ（例: `textures/`）に配置してください。

### 2. mod_utf8sign_sample (同梱サンプル)
Minetest Game の標準看板を近代化するサンプル Mod を提供しています。

-- 1. まず C++ 側に看板の規格を教える  
```lua
minetest.utf8sign.set_config({  
    width = 115,  
    line_height = 14,  
    max_lines = 4,  
    char_w = 5  
})  
```

-- 2. utf8combine 命令で描画（自動的に生成したアトラスが参照されます）  
```lua
local texture = "[utf8combine:115x82:15,14@000000=こんにちは、世界！]"
```

------------------------------
## 📘 English Overview
This fork modernizes the Luanti text rendering system for full Unicode support. By implementing a high-performance C++ API, it eliminates the limitations of Lua-based rendering.
## Key Features:

* 14px Canvas Layout: Optimized for 14px high glyphs with an 11px baseline to prevent character clipping (essential for Cyrillic, Greek, and Japanese).
* Synchronized Layout Engine: A shared "SignManager" ensures 1:1 pixel accuracy between Server (Lua) and Client (C++).
* Hybrid Word Wrap: Prioritizes word boundaries but enforces character-level wrapping at the 115px physical limit to ensure no text loss.
* Auto-Detect Atlas Engine: Automatically scales rendering based on the loaded PNG atlas height (supports both 12px legacy and 14px modern assets).

------------------------------
## 🧪 動作確認済環境

* Luanti: このフォーク (5.16.0-dev)
* Sample Mod: mod_utf8sign_sample (MTG/Mineclonia対応版)
* Mineclonia: mineclonia-jtest

------------------------------
## 🧪 開発環境
* Windows 10 pro x64
* MSYS2 CLANG64

------------------------------
## 📝 補足
AI（Copilot/Gemini）と人間の協力により、正確で強固なコードベースを構築しました。
------------------------------

