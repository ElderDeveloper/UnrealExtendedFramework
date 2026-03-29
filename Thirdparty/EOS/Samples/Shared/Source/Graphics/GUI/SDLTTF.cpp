// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"

#ifdef EOS_DEMO_SDL

#include "DebugLog.h"
#include "SDLTTF.h"
#include "StringUtils.h"
#include <utf8.h>

FSDLTrueTypeFont::FSDLTrueTypeFont(const std::string& FontFileName, size_t Size)
{
	TTFFont = TTF_OpenFont(FontFileName.c_str(), int(Size));
	assert(TTFFont && "Could not load font!");
}

FSDLTrueTypeFont::~FSDLTrueTypeFont()
{
	if (TTFFont)
	{
		TTF_CloseFont(TTFFont);
		TTFFont = nullptr;
	}
}

bool FSDLTrueTypeFont::ContainsCharacter(wchar_t Character) const
{
	static_assert(sizeof(wchar_t) == sizeof(Uint16) || sizeof(wchar_t) == 4, "wchar_t's size is not supported!");
	assert(TTFFont);

	Uint16 Char;

	if (sizeof(wchar_t) == 2)
	{
		//we can cast directly
		Char = (Uint16)Character;
	}
	else if (sizeof(wchar_t) == 4)
	{
		//we have to cast to perform UTF32 -> UTF8 -> UTF16 casts

		std::wstring UTF32CharacterString(1, Character);
		std::string UTF8CharacterString;
		utf8::utf32to8(UTF32CharacterString.begin(), UTF32CharacterString.end(), back_inserter(UTF8CharacterString));

		std::vector<Uint16> UTF16CharacterString;
		utf8::utf8to16(UTF8CharacterString.begin(), UTF8CharacterString.end(), back_inserter(UTF16CharacterString));

		if (UTF16CharacterString.empty())
		{
			return false;
		}

		Char = UTF16CharacterString[0];
	}

	if (TTF_GlyphIsProvided(TTFFont, Char) == 0)
	{
		return false;
	}

	return true;
}

Vector2 FSDLTrueTypeFont::MeasureString(const std::wstring& Text) const
{
	assert(TTFFont);

	Vector2 ResVector(0.0f);
	int Width = 0, Height = 0;

	int Result = TTF_SizeUTF8(TTFFont, FStringUtils::Narrow(Text).c_str(), &Width, &Height);
	if (Result != 0)
	{
		//error
		FDebugLog::LogError(L"FSDLTrueTypeFont: Could not measure string size. Possibly missing character. String: %ls", Text.c_str());
		return ResVector;
	}

	ResVector.x = float(Width);
	ResVector.y = float(Height);
	return ResVector;
}

Vector2 FSDLTrueTypeFont::MeasureString(const std::string& Text) const
{
	assert(TTFFont);

	Vector2 ResVector(0.0f);
	int Width = 0, Height = 0;
	int Result = TTF_SizeText(TTFFont, Text.c_str(), &Width, &Height);
	if (Result != 0)
	{
		//error
		FDebugLog::LogError(L"FSDLTrueTypeFont: Could not measure string size. Possibly missing character. String: %ls", Text.c_str());
		return ResVector;
	}

	ResVector.x = float(Width);
	ResVector.y = float(Height);
	return ResVector;
}

FTexturePtr FSDLTrueTypeFont::RenderString(const std::wstring& Text, FColor TextColor)
{
	assert(TTFFont);

	if (Text.empty())
	{
		return nullptr;
	}

	SDL_Color SDLColor;
	SDLColor.r = Uint8(255 * TextColor.R);
	SDLColor.g = Uint8(255 * TextColor.G);
	SDLColor.b = Uint8(255 * TextColor.B);
	SDLColor.a = Uint8(255 * TextColor.A);

	SDL_Surface* RenderedString = TTF_RenderUTF8_Blended(TTFFont, FStringUtils::Narrow(Text).c_str(), SDLColor);
	if (!RenderedString)
	{
		FDebugLog::LogError(L"FSDLTrueTypeFont: Could not render string to surface: %ls", Text.c_str());
		return nullptr;
	}

	SDL_Surface* ConvertedSurface = SDL_ConvertSurfaceFormat(RenderedString, SDL_PIXELFORMAT_RGBA32, 0);
	SDL_FreeSurface(RenderedString);

	if(!ConvertedSurface)
	{
		FDebugLog::LogError(L"FSDLTrueTypeFont: Could not convert surface to right format.");
		return nullptr;
	}

	//Load texture
	FTexturePtr TextTexture = FTexture::LoadFromSurface(ConvertedSurface);
	SDL_FreeSurface(ConvertedSurface);

	return TextTexture;
}

#endif //EOS_DEMO_SDL
