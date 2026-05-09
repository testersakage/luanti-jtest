------------------------------
## Luanti Unicode Modernization Project (V4 Engine "EX")
Japanese Documentation (README.md)
------------------------------
## Overview
This repository is a fork of Luanti dedicated to fully supporting Unicode (especially Japanese and multilingual text). It aims to achieve rendering quality equal to or better than Minecraft.
The V4 "EX" Engine, integrated with SDL2 / SDL_image, introduces the Dynamic Definition Engine (DDE). This allows for flexible support of any font atlas specification (e.g., 16x16, 12x14) via external JSON profiles, removing the limitations of hardcoded grid sizes.

<img width=600, height=225, src="https://github.com/testersakage/luanti-jtest/blob/master/screenshots/samplesign.png"></img>

## Innovative Features
## 1. Dynamic Definition Engine (DDE)

* Freedom from Specs: Supports various grid sizes through external JSON configuration files.
* Dynamic Profiles: Switch font sets via Lua scripts without the need to rebuild the engine.

## 2. SDL2 / SDL_image Integration

* Multi-format Support: Native support for PNG, JPG, and other formats via SDL_image.
* Alpha Reverse: Automatically converts legacy "white-on-black" assets into modern transparent fonts.
* Pixel-Perfect Precision: Implements cell_w logic to calculate exact spacing based on the physical dimensions of the atlas image.

## 3. Multi-Engine Hybrid Architecture

* Refined Standard Atlas: Improved stability for legacy 12px/14px assets using the Irrlicht-based engine.
* FreeType (FT) Engine: Direct rendering of TrueType (TTF) and OpenType (OTF) fonts for ultimate typographical flexibility.
* Build-time Control: All engines can be toggled via CMake flags (#ifdef control).

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
