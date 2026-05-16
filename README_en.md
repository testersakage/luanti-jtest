------------------------------
## Luanti Unicode Modernization Project
Japanese Documentation (README.md)
------------------------------
## Overview
This repository is a fork of Luanti dedicated to fully supporting Unicode (especially Japanese and multilingual text). It aims to achieve rendering quality equal to or better than Minecraft.
The V4 "EX" Engine, integrated with SDL2 / SDL_image, introduces the Dynamic Definition Engine (DDE). This allows for flexible support of any font atlas specification (e.g., 16x16, 12x14) via external JSON profiles, removing the limitations of hardcoded grid sizes.

<img width=600, height=225, src="https://github.com/testersakage/luanti-jtest/blob/master/screenshots/samplesign.png"></img>

## 🔧 Core Innovations

### 0. Native C++ UTF-8 Glyph API Infrastructure
* Eliminated high-overhead Lua-layer text parsing and width calculations (`utf8` loops) entirely. Built a bare-metal, native Unicode parsing matrix directly inside the C++ engine core.

### 1. Multibyte-Ready C++ Font Atlas Infrastructure
* Permanently removed legacy Lua-side processing. Implemented a custom Font Atlas Slicing matrix and cache system capable of managing massive CJK (Chinese, Japanese, Korean) font datasets inside native memory space with microsecond efficiency.

### 2. High-Velocity Texture Compiling: "[utf8combine]" Dedicated Renderer
* Engineered an exclusive rendering slot for **`[utf8combine]` (as well as `[utf8combineex]` and `[utf8combineft]`)** to intercept text commands and dynamically synthesize transparent sign sheets at hardware speed.

### 3. Integrated SDL2 / SDL_image / SDL_ttf Graphics Suite
* Shattered the legacy rendering constraints of Luanti’s native driver (Irrlicht) by embedding the industry-standard media libraries—**SDL2, SDL_image, and SDL_ttf**—deeply into the core pipeline. (Excludes Standard)

### 4. Encapsulated C++/Lua Bi-Directional Sync Framework: "UTF8SignManager"
* Implemented a unified, single-instance management hub (**`UTF8SignManager`**) to centrally govern the embedded SDL2 multimedia components and underlying text layout rules.

### 5. Dynamic Definition Engine "DDE" (*EX Atlas only)
* Fully abolished structural hardcoding of font dimensions and asset specifications inside the C++ runtime. Establishes a flexible **Dynamic Definition Engine (DDE)** that adapts and drives any atlas grid matrix (12px, 14px, 16px, etc.) instantly on the fly by reading asynchronous JSON blueprints or raw script-layer config tables.

---

## 🚀 Multi-Engine Hybrid Configuration
Offers three distinctive rendering pipelines selectable via compilation states:

### 1. ST (Standard) Atlas 
* Powered by Luanti's default Irrlicht framework, preserving legacy asset backward-compatibility while significantly accelerating rendering workflows.

### 2. EX (Extended) Atlas 
* The next-generation atlas pipeline that expands upon the Standard configuration. Deeply integrated with the SDL matrix, featuring the Dynamic Definition Engine (DDE) to automatically parse any custom grid blueprint (16x16, 12x14, etc.) at runtime.

### 3. FT (FreeType) 
* A direct rasterization engine that completely bypasses texture sheet dependencies via SDL2_ttf and FreeType. Delivers ultimate typographical crispness by rendering high-resolution vector fonts (TTF/OTF) straight to sign nodes.

## ⚙️ Compilation Options
* Each standalone rendering mode is fully toggled via internal build flags (`#ifdef`), allowing deployment of highly optimized, customized binaries tailored for specialized environments.

------------------------------
## Usage & Integration
## 1. Leveraging Existing Assets (e.g., signs_lib)
This engine officially supports the GNU Unifont assets from the widely used [signs_lib](https://github.com/minetest-mods/font_api).

* Auto-Detection: The included sample mod automatically locates signs_lib textures and registers them with the EX engine.
* Transparency Remaster: Renders legacy assets as high-quality transparent fonts without manual image editing.

## 2. Getting Started with the Sample Mod
Enable mod_utf8signex_sample (included in this repository) to experience the EX engine immediately.

* ContentDB Ready: The init.lua is pre-configured to resolve asset paths automatically in various environments.
* DDE Reference: Includes JSON profiles for PixelMplus12 and signs_lib as practical implementation examples.

## ⚠️ Important Compatibility Notes

* Asset-Only Integration: While this engine can utilize font textures from other mods, it does not automatically upgrade the rendering of those mods' existing sign nodes/entities.
* Mod Modification: To use EX rendering on existing signs from other mods, their Lua code must be modified to use the [utf8combineex:... command.
* Recommended Use: For best results, use the included sample mod or develop new mods designed specifically for the EX Atlas engine.

------------------------------
## How to Build
This fork is built and tested on Windows using the MSYS2 CLANG64 environment.

   1. Prerequisites: Install MSYS2 and set up the CLANG64 environment.
   2. Dependencies: Install the required packages for Luanti and the SDL2_image library:
   
   pacman -S mingw-w64-clang-x86_64-SDL2_image
   
   3. Build Execution:
   
   cmake . -B build -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles" -DENABLE_UTF8_SDL2_ATLAS=ON
   cmake --build build -j$(nproc)
   
   
## Environment & Development

* Luanti: This fork (5.16.0-dev)
* OS: Windows 10 Pro x64 / MSYS2 CLANG64
* Note: This project was built through the collaboration of AI and human ingenuity to ensure a robust and modernized codebase.

------------------------------
