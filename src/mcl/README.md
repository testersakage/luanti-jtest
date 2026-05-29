------------------------------
## Luanti C++ Native API Infrastructure Specification (dev4mcl)
本ディレクトリは、Mineclonia v0.120.0以降（Luanti 5.15.2以降）の中枢演算およびマルチスレッド環境下におけるボトルネックを高速化・安定化するために実装された、ネイティブC++ API群の実体仕様書である。
## 1. 共通設計規律 (Core Infrastructure Rules)

* 実名常駐・動的内部リレー規律: Lua側のグローバル関数名義およびオブジェクトメソッドは0手目（Modロード時）から100%存在保証（実名常駐）させ、実行時C++窓口（mclcapi）の有無を検知して動的にフォールバックを切り替える。非同期（Emerge-0等）での nil 即死を回避する。
* 引数直撃一本釣り規律: C++窓口での無駄なスタック走査や型検品ループを排除し、固定されたインデックス位置から luaL_check... を用いて最速でデータを回収する。
* 生ポインタの隔離保護: 空間メタデータ（MetaDataRef）やインベントリの生ポインタ操作など、非同期境界で切断リスクのある実務はLua側にホールドさせ、C++側は純粋な幾何学・算術計算・文字列トリミングに特化させる。

------------------------------
## 2. 開通APIマトリクス

各ページを参照
* [CORE](API_CORE.md)
* [ENTITIES](API_ENTITIES.md)
------------------------------
## 3. コンパイルおよび配線手順 (Build & Binding)
## C++側のビルド追加 (src/CMakeLists.txt)

mcl/core/util_shape.cpp
mcl/core/explosions.cpp
mcl/core/flowlib.cpp
mcl/core/damage.cpp# (他、各コアモジュールの.cppをここに集約)

## 窓口一括結線 (src/script/lua_api/l_mcl_core_server.cpp)
bind_multithread_CORE 内で各 namespace の関数ポインタを mclcapi テーブルへフィールド展開する。

## Mineclonia側の変更
こちらのブランチにあります。
https://github.com/testersakage/mineclonia-jtest/tree/dev4cpp/mods
オリジナルとはリネーム等による差し替えだけで使えるようになります。従来のLuaでの処理はフォールバックに回り、c++ APIを優先的に利用するようになります。

------------------------------
