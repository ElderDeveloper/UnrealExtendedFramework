// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSlateStyleSet;
class ISlateStyle;

/**
 * Text styles for the document viewer.
 *
 * Built from the engine's default typefaces rather than font assets, so the plugin needs no
 * content and drops into any project. Style names here are the tag names used in rich-text markup
 * ("Bold", "Italic", "Code", "Strike"), so they must stay in sync with FExtendedAtlassianMarkup.
 */
class FExtendedAtlassianDocumentStyle
{
public:
	static void Register();
	static void Unregister();

	static const ISlateStyle& Get();
	static FName GetStyleSetName();

private:
	static TSharedPtr<FSlateStyleSet> StyleInstance;
};
