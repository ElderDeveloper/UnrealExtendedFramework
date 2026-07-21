// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Texture.h"

#ifdef EOS_DEMO_SDL

class FSDLTrueTypeFont
{
public:
	FSDLTrueTypeFont(const std::string& FontFileName, size_t Size);
	~FSDLTrueTypeFont();

	FSDLTrueTypeFont(const FSDLTrueTypeFont&) = delete;
	FSDLTrueTypeFont& operator=(const FSDLTrueTypeFont&) = delete;

	bool ContainsCharacter(wchar_t Character) const;
	Vector2 MeasureString(const std::wstring& Text) const;
	Vector2 MeasureString(const std::string& Text) const;

	FTexturePtr RenderString(const std::wstring& Text, FColor TextColor = Color::Black);

	bool IsValid() const { return TTFFont != nullptr; }

private:
	TTF_Font* TTFFont = nullptr;
	FColor TextColor;
};

#endif //EOS_DEMO_SDL
