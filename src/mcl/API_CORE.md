------------------------------
## 目次
* [top](README.md)
* [CORE](API_CORE.md)
* [ENTITIES](API_ENTITIES.md)
------------------------------
## Luanti C++ Native API Infrastructure Specification (dev4mcl)
本ディレクトリは、Mineclonia v0.120.0以降（Luanti 5.15.2以降）の中枢演算およびマルチスレッド環境下におけるボトルネックを高速化・安定化するために実装された、ネイティブC++ API群（計40関数）の実体仕様書である。
## 1. 共通設計規律 (Core Infrastructure Rules)

* 実名常駐・動的内部リレー規律: Lua側のグローバル関数名義およびオブジェクトメソッドは0手目（Modロード時）から100%存在保証（実名常駐）させ、実行時C++窓口（mclcapi）の有無を検知して動的にフォールバックを切り替える。非同期（Emerge-0等）での nil 即死を回避する。
* 引数直撃一本釣り規律: C++窓口での無駄なスタック走査や型検品ループを排除し、固定されたインデックス位置から luaL_check... を用いて最速でデータを回収する。
* 生ポインタの隔離保護: 空間メタデータ（MetaDataRef）やインベントリの生ポインタ操作など、非同期境界で切断リスクのある実務はLua側にホールドさせ、C++側は純粋な幾何学・算術計算・文字列トリミングに特化させる。

------------------------------
## 2. 開通APIマトリクス (40 APIs Registered)
## mcl_util/environment (チェストインベントリ・空間走査)

* mclcapi.native_get_double_container_neighbor_pos / native_get_eligible_transfer_item_slot / native_drop_items_from_meta_container
* ホッパーやチェスト、ラージチェストのアイテム移動時に発生する近隣チェストの座標逆算、移動可能スロットの自動選別、およびコンテナ破壊時のアイテム一斉放出処理。
* mclcapi.native_get_pointed_thing / native_traverse_tower / native_traverse_tower_group
* プレイヤーの視線（レイキャスト）判定、および特殊ノード・タワー構造の空間再帰走査のネイティブ化。
* mclcapi.native_replace_node_vm / native_bulk_set_node_vm / native_circle_bulk_set_node_vm
* VoxelManip（ボクセルマップデータ配列）に対する、特定ノードの高速一斉置換、バルク書き込み、および円形範囲への一斉スタンプ処理。

## mcl_util/item (ツール耐久値算術)

* mclcapi.calculate_durability / mclcapi.use_item_durability
* ツールや武器を使用・被弾した際、エンチャント（耐久力等）の確率補正を計算し、残り耐久数値を減算・返却する高速処理。

## mcl_util/misc (一般ユーティリティ)

* mclcapi.native_generate_uuid
* 個体（Mobなど）識別用の一意なUUID文字列の最速生成。
* mclcapi.native_get_nodepos
* 浮動小数点の個体座標から、マップのブロック整数座標（X, Y, Z）への一本釣り変換。
* mclcapi.native_calculate_knockback
* PvPおよびMob戦闘時、被弾側の速度ベクトル（Velocity）に加算すべきノックバック距離と方向の幾何学計算。

## mcl_util/object (回転行列変換)

* mclcapi.native_rotation_to_irrlicht
* プレイヤーの首の振り（オイラー角 Yaw/Pitch/Roll）から、Irrlicht 3次元 ZYX 回転行列への最速変換。

## mcl_util/shape (3次元衝突箱演算要塞)

* mclcapi.native_decompose_aabbs
* 渡された複数の衝突箱を、重なりのない直方体グリッド（AABBs）へと分解する最重量の幾何学心臓部。
* mclcapi.native_region_op / native_region_evaluate / native_any_occupied_p / native_region_volume / native_region_equal_p / native_region_walk / native_region_simplify / native_region_select_face / native_region_intersect_p
* shape.lua 由来の立体積計算（volume）、同一性チェック（equal_p）、簡素化（simplify）、および交差判定（intersect_p）にいたるまで、生Luaで回すと何万回もの二分探索ループでガベージ（GC）を撒き散らす幾何学ロジックをC++連続メモリで処理。

## mcl_util/table (高速配列・テーブル操作)

* mclcapi.native_table_update / mclcapi.native_table_update_deep / mclcapi.native_table_keyset
* テーブルのシャローコピー（上書きマージ）、ディープコピー（多重深度マージ）、および全キー（Key）の最速インデックス抽出。

## tga_encoder (画像エンコード・地図幾何学)

* mclcapi.native_tga_encode
* 地図生成時の重厚な2次元マトリクスを1重のフラット数値配列（0xRRGGBBAA）として回収し、C++連続メモリからTGAバイナリをパック出荷。

## flowlib (流体物理ベクトル)

* mclcapi.native_quick_flow
* 水流に浸かっている個体に対し、毎フレーム東西南北4方向の近隣水位・落差をスキャンし、単位流速ベクトルを算出。

## mcl_liquids (流体ロード時LBMチェック)

* mclcapi.native_does_sl_need_update / mclcapi.native_does_fl_need_update
* 水源（Source）および流動水（Flowing）のチャンクロード時、下および東西南北の周囲5方向への拡散余地・水位サポート関係、および無限水源化条件（count_sources >= 2）を高速検品。エリア移動時のフリーズを消滅。

## mcl_damage (PvP精密パルス安定化)

* mclcapi.native_from_mt
* プレイヤーが被弾するすべての瞬間（メインスレッド）に駆動。Luantiエンジン由来の原因テーブルから、_mcl_ で始まる隠し変数を std::strncmp による高速ポインタ走査でトリミング切り出し。Luaヒープの断片化（ガベージ）を根絶。

## mcl_explosions (爆風幾何学・ダメージ算術)

* mclcapi.native_compute_sphere_rays / mclcapi.native_calculate_impact
* 爆発時の球形視線ベクトルの高速生成、および距離と被曝露出度からのダメージ値・インパクト倍率の最速算出。

## mcl_worlds (ディメンション境界・時空管制)

* mclcapi.native_worlds_is_in_void / mclcapi.native_worlds_y_to_layer / mclcapi.native_worlds_pos_to_dimension / mclcapi.native_worlds_layer_to_y / mclcapi.native_worlds_tick_chunk_inhabited_time
* 奈落（Void）判定、Y軸高度から各ワールド（主世界、ネザー、エンド）のレイヤー変換、座標所属ディメンションの逆算、およびチャンクごとの累積プレイヤー滞在時間のミリ秒刻み同期。

------------------------------
