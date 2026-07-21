// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef DXTK
#include "SpriteFont.h"
using FontPtr = std::shared_ptr<DirectX::SpriteFont>;
#elif defined(EOS_DEMO_SDL)
#include "SDLTTF.h"
using FontPtr = std::shared_ptr<FSDLTrueTypeFont>;
#else
class FDummyFont
{
public:
	FDummyFont() {};
	~FDummyFont() {};
	Vector2 MeasureString(const char* Str) { return Vector2(5.f, 5.f); };
	Vector2 MeasureString(const wchar_t* Str) { return Vector2(5.f, 5.f); };
};
using FontPtr = std::shared_ptr<FDummyFont>;
#endif // DXTK

/**
 * Font class
 */
class FFont
{
public:
	/** Constructor */
	FFont(const std::wstring& FontAssetFile, size_t SizeOptional = 0);

	/** Destructor */
	virtual ~FFont() {};

	/** Create */
	void Create();

	/** Release */
	void Release();

	/** Get Font */
	FontPtr GetFont();

	/* Test string to use for measurements.*/
	static const std::wstring& GetTestString() { return TestString; }

private:
	/** Asset File */
	std::wstring AssetFile;

	/** Texture or Font asset */
	FontPtr Font;

	/** Size (optional) */
	size_t Size;

	/* Test string to use for measurements.*/
	static std::wstring TestString;
};