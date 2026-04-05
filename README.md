# Luanti Unicode Fork

このリポジトリは、Luanti を Unicode（特に日本語）対応にするための個人フォークです。  
C++ 側に Unicode グリフ API を追加し、Mineclonia での日本語表示を実現することを目的としています。

---

## 🔧 このフォークで行っている主な変更

- Unicode グリフを扱うための C++ API の追加  
- Lua 側で UTF-8 → Unicode コードポイント変換を行う前提での API 設計  
- Unicode グリフ PNG（12×12）を利用したテクスチャ生成の準備

---

## 🔧 このフォークで追加した機能の利用例

- Mineclonia（Lua）で入力された文字列を UTF-8 → Unicode コードポイントに変換し、  
  Luanti Unicode API で処理された結果を看板に表示する

---

## 📁 リポジトリ構成

- `README.md` — このフォークの説明（あなたの README）
- `README.upstream.md` — 本家 Luanti の README（そのまま保存）
- `src/` — Luanti C++ コード（Unicode API を追加）
- `games/` — 使用しない（Mineclonia は外部フォルダで管理）
- `textures/` — Unicode グリフ PNG を配置予定

---

## 📘 本家 README

このプロジェクトは Luanti のフォークです。  
本家の README は以下に保存しています：

👉 **[README.upstream.md](README.upstream.md)**

---

## 🚀 開発の目的

Minecraft のように **看板に日本語を表示できる環境**を Luanti + Mineclonia で実現するために、  
以下の課題を解決します：

- Luanti のフォントレンダリングが ASCII 前提  
- Unicode グリフ API が存在しない  
- Mineclonia の看板描画が ASCII 固定  
- UTF-8 を扱う仕組みが Lua 側にない  

これらを C++ と Lua の両面から拡張します。

---

## 🧪 動作確認環境

- Luanti（このフォーク）
- Mineclonia（corburg 版を別フォルダに clone）

---

## 📝 補足

- コード生成に Copilot を利用しています。
