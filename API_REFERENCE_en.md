------------------------------
## 📜 UTF8Sign API Reference (Certified Edition)
## 1. utf8: Basic & Statistical API
Provides fundamental UTF-8 string manipulation functions.

* utf8.len(s): Returns the number of characters (not bytes) in the string.
* utf8.offset(s, n, [i]): Returns the byte position of the n-th character.
* utf8.codepoint(s, [i], [j]): Returns the codepoints (numerical values) of the characters.
* utf8.char(...): Creates a UTF-8 string from multiple codepoints.
* utf8.codes(s): Returns an iterator for UTF-8 codepoints.
* utf8.to_table(s): Explodes a UTF-8 string into a table of individual characters.
* utf8.charpattern: The Lua pattern for matching a single UTF-8 character.

------------------------------
## 2. utf8 wrap: Layout & Width Analysis API
Used for layout planning and text analysis before rendering.

* utf8.eaw_width(s): Returns the total width based on East Asian Width (Half-width = 1, Full-width = 2).
* utf8.get_width(s): Returns the physical pixel width based on current font settings.
* utf8.get_lines(s, max_w): Splits a string into lines based on the specified pixel width. Returns a table of strings.
* utf8.to_codepoints(s): Converts a string into an array (table) of codepoint integers for internal processing.
* utf8.truncate(s, max_w): Truncates a string to fit within the specified pixel width.
* utf8.eaw_truncate(s, max_eaw): Truncates a string based on East Asian Width.

------------------------------
## 3. sign: Engine & Atlas Management API
Controls engine behavior, Atlas definitions, and provides highly efficient symmetric cache control statistics.
## 🌐 Global Configuration

* minetest.utf8sign.get_config(): Retrieves the current common settings for all engines (paths, sizes, etc.).
* minetest.utf8sign.set_config(table): Updates engine settings (e.g., font path, size).

## 🧭 Extended Atlas (EX) Administration

* minetest.utf8sign.ex.load_atlas_config(path): Registers an Atlas profile from an external JSON file.
* minetest.utf8sign.ex.get_atlas_status(): Returns details of the currently active Atlas (ID, path, grid settings, etc.).

## 👑 Cache Control APIs (v2.5.0 Finalized Standard)
The historical result of comprehensive code refactoring. Highly efficient cache control functions.
## 💾 Standard Atlas (ST) Cache Control

* minetest.utf8sign.st.get_page_cache(): [ST Only] Returns the number of cached pages for the Standard Atlas.
* minetest.utf8sign.st.set_cache_size(size): [ST Only] Updates the maximum page cache capacity.

## ⚡ Extended Atlas (EX) Cache Control

* minetest.utf8sign.ex.get_cache_size(): [EX Only] Returns the current maximum limits (max_chars, max_pages) via Multi-Return.
* minetest.utf8sign.ex.get_cache_count(): [EX Only] Returns the current stored counts (char_count, page_count) via Multi-Return.
* minetest.utf8sign.ex.set_cache_size(max_chars, max_pages): [EX Only] Updates both limits. Pass nil to leave specific limits unchanged (State Retention). Fully synchronized to the C++ core instantly.
* minetest.utf8sign.ex.clear_cache(clear_char, clear_page): [EX Only] Pinpoints and flushes specific cache rooms cleanly via boolean tokens (char, page).

## 🔤 FreeType Engine (FT) Cache Control

* minetest.utf8sign.ft.get_cache_size(): [FT Only] Returns the current maximum glyph cache capacity.
* minetest.utf8sign.ft.get_cache_count(): [FT Only] Returns the number of FreeType glyphs currently in memory.
* minetest.utf8sign.ft.set_cache_size(size): [FT Only] Updates the cache capacity limit. Automatically synchronized to C++ core.
* minetest.utf8sign.ft.clear_cache(): [FT Only] Flushes all FreeType glyph caches cleanly.

------------------------------
## 4. Configuration Settings (minetest.conf)
These settings control the behavior, safety thresholds, and parameters of the various font engines upon loading.
## 📦 Atlas Engine (ST/EX) Settings

* utf8_st_atlas_path: [ST Only] Specifies the path and filename format for Standard Atlas images. Relative paths from luanti.exe are supported. (Example: ../mods/mod_utf8sign_sample/textures/unicode_page_%02x.png)
* utf8_ex_max_chars = 64: [EX Only] Sets the maximum capacity for the primary glyph cache (individual character data). Protected by an absolute safety threshold (2048).
* utf8_ex_max_pages = 4: [EX Only] Sets the maximum capacity for the secondary page image cache. Protected by an absolute safety threshold (16).
* utf8_ex_atlas_config: [EX Only] Specifies the fallback profile configuration file (.json).

## 🔤 FreeType Engine (FT) Settings

* utf8_font_path = ../fonts/NotoSansCJKjp-Regular.otf: Specifies the default font file path used for FT-based signs and layout metrics calculations.
* utf8_ft_cache = 256: [FT Only] Sets the maximum capacity for the FreeType glyph cache. Protected by an absolute threshold (2048) to prevent out-of-memory crash loops.

