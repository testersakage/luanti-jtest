------------------------------
## Luanti C++ Native API Infrastructure Specification (dev4mcl)
本ディレクトリは、Mineclonia v0.120.0以降（Luanti 5.15.2以降）の高負荷処理を高速化するためにC++ APIを追加する試みです。
## 1. 基本設計
* c++側: 以下の4大数理計算とそれに付随する処理をC++側にAPIとして実装します。
  1. 3次元の多重走査（3重ループ）
  2. グループ属性検品
  3. ビットパック・数値算術
  4. 文字列トリミング
* lua側: 高速化対象の関数にC++ APIへの引数の前処理と分岐を追加し、従来の処理をフォールバックとして利用する改造を行う。
------------------------------
## 2. C++ APIマトリクス
各ページを参照
* mineclonia/mods/
  * [CORE/](API_CORE.md)
  * [ENTITIES/](API_ENTITIES.md)
------------------------------
## ファイル配置
- src/
  - script/
    - cpp_api/
      - s_async.cpp - Emerge-0 スレッド登録用  
      - s_mapgen.cpp - mapgen スレッド登録用  
    - lua_api/
      - l_mcl_client.cpp - Main スレッド登録用(Client)
      - l_mcl_server.cpp - Main スレッド登録用(Server)
  - mcl/
    - CORE/
      - flowlib.cpp  
      - damage.cpp  
      - explosions.cpp  
      - util_environment.cpp  
      - util_item.cpp
      - util_misc.cpp
      - util_object.cpp
      - util_shape.cpp
      - util_table.cpp
      - liquids.cpp  
      - worlds.cpp  
      - tga_encoder.cpp  
    - ENTITIES/
      - burning.cpp
      - mobs.cpp
    - math_common.cpp - ビットパック・数値算術、文字列トリミング
    - spatial_common.cpp - 3次元多重走査、グループ属性検品
##  開発環境
Windows10 pro + MSYS2 CLANG64 で開発しています。
## Mineclonia側の変更
こちらのブランチにあります。
https://github.com/testersakage/mineclonia-jtest/tree/dev4cpp/mods
オリジナルとはリネーム等による差し替えだけで使えるようになります。従来のLuaでの処理はフォールバックに回り、c++ APIを優先的に利用するようになります。

------------------------------
