// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "COSOperator.h"

#ifdef _IRR_WINDOWS_API_
#include <windows.h>
#else
#include <cstring>
#include <unistd.h>
#ifndef _IRR_ANDROID_PLATFORM_
#include <sys/types.h>
#ifdef _IRR_OSX_PLATFORM_
#include <sys/sysctl.h>
#endif
#endif
#endif

// "SDL_version.h" for SDL_VERSION_ATLEAST
#ifdef _IRR_USE_SDL3_
	#include <SDL3/SDL_clipboard.h>
	#include <SDL3/SDL_version.h>
#else
	#include <SDL_clipboard.h>
	#include <SDL_version.h>
#endif

#include "fast_atof.h"

// constructor
COSOperator::COSOperator()
{}

COSOperator::~COSOperator()
{
#ifdef _IRR_COMPILE_WITH_SDL_DEVICE_
	SDL_free(ClipboardSelectionText);
	SDL_free(PrimarySelectionText);
#endif
}

//! copies text to the clipboard
void COSOperator::copyToClipboard(const c8 *text) const
{
	if (strlen(text) == 0)
		return;

#if defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
	SDL_SetClipboardText(text);
#endif
}

//! copies text to the primary selection
void COSOperator::copyToPrimarySelection(const c8 *text) const
{
	if (strlen(text) == 0)
		return;

#if defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
#if SDL_VERSION_ATLEAST(2, 25, 0)
	SDL_SetPrimarySelectionText(text);
#endif
#endif
}

//! gets text from the clipboard
const c8 *COSOperator::getTextFromClipboard() const
{
#if defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
	SDL_free(ClipboardSelectionText);
	ClipboardSelectionText = SDL_GetClipboardText();

#if defined(_WIN32)
	// --- パッチ：コピペ時のWindows IME文字化け一括修正（UTF-8版） ---
	// クリップボードから貼り付け（Ctrl+V）された生のUTF-8文字列をスキャンし、
	// 不格好な記号を、Luanti標準フォントが綺麗に全角描画できる正しいコードへ置換します。
	if (ClipboardSelectionText) {
		std::string str(ClipboardSelectionText);
		bool changed = false;

		// 1. ～ (Windowsの全角チルダ \xEF\xBD\x9E -> 綺麗な全角波ダッシュ \xE3\x80\x9C)
		size_t pos = 0;
		while ((pos = str.find("\xEF\xBD\x9E", pos)) != std::string::npos) {
			str.replace(pos, 3, "\xE3\x80\x9C");
			pos += 3;
			changed = true;
		}
		// 2. ∥ (双対符 \xEF\xBC\x82 -> \xE2\x88\xA5)
		pos = 0;
		while ((pos = str.find("\xEF\xBC\x82", pos)) != std::string::npos) {
			str.replace(pos, 3, "\xE2\x88\xA5");
			pos += 3;
			changed = true;
		}
		// 3. － (全角マイナス \xEF\xBC\x8D -> \xE2\x88\x92)
		pos = 0;
		while ((pos = str.find("\xEF\xBC\x8D", pos)) != std::string::npos) {
			str.replace(pos, 3, "\xE2\x88\x92");
			pos += 3;
			changed = true;
		}

		// もし文字列の置換が発生した場合、SDL側のメモリを安全に再確保して上書き
		if (changed) {
			SDL_free(ClipboardSelectionText);
			ClipboardSelectionText = SDL_strdup(str.c_str());
		}
	}
	// -----------------------------------------------------------------
#endif

	return ClipboardSelectionText;
#else

	return 0;
#endif
}

//! gets text from the primary selection
const c8 *COSOperator::getTextFromPrimarySelection() const
{
#if defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
#if SDL_VERSION_ATLEAST(2, 25, 0)
	SDL_free(PrimarySelectionText);
	PrimarySelectionText = SDL_GetPrimarySelectionText();
	return PrimarySelectionText;
#endif
	return 0;

#else

	return 0;
#endif
}
