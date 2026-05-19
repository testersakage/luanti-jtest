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
## 🚀 Usage & Integration
This engine aims to provide state-of-the-art rendering quality while maximizing the utilization of the vast array of existing sign mod assets available in Luanti.
## 1. Leveraging Existing Assets (e.g., signs_lib)
This engine officially supports the GNU Unifont font assets from the widely used signs_lib framework.

* Automated Asset Discovery (EX Engine Only): If signs_lib is installed in your environment, the included sample mod automatically locates its texture paths and registers them with the EX engine.
* Transparency Remaster: Renders legacy "black-background, white-text" assets as high-quality transparent pixel fonts instantly, without requiring manual image editing or preprocessing.

## 2. Getting Started with Sample Mods
Enable your preferred engine style by activating any of the three sample mods included in this repository (mod_utf8sign_sample, mod_utf8signex_sample, or mod_utf8signft_sample).

* ContentDB Ready: Pre-configured to automatically resolve asset paths even when downloaded and installed via ContentDB or alternative remote repositories.
* Dynamic JSON Profiling: Allows developers to add entirely new font atlas definitions simply by dropping in a configuration JSON file, completely eliminating the need to modify or recompile C++ core source code.

## ⚠️ Important Compatibility Notes

* Asset-Only Integration: While this engine borrows and utilizes font images (assets) from frameworks like signs_lib, it does not automatically override or upgrade the native rendering logic of sign nodes or entities provided by those third-party mods.
* Mod Modification Requirements: To upgrade existing signs from other mods to use the high-definition EX Atlas engine, their underlying Lua codebase must be modified to output texture commands in the [utf8combineex:... format.
* Recommended Deployment: For the best possible experience, it is highly recommended to use the included sample mods or develop new mods designed specifically around this unified font engine framework.

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
