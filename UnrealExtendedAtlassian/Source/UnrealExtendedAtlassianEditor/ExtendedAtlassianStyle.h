// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSlateStyleSet;
class ISlateStyle;

/** Plugin-wide Backlot design tokens and Slate styles. */
class FExtendedAtlassianStyle
{
public:
	static void Register();
	static void Unregister();

	static const ISlateStyle& Get();
	static FName GetStyleSetName();
	static FLinearColor Color(const FName& Name);
	static FLinearColor FromHex(const TCHAR* Hex);

	/**
	 * Named geometry generated from the frozen HTML. Feature widgets must read every
	 * authored size through this accessor instead of repeating a numeric literal.
	 */
	static float Metric(const FName& Name);

private:
	static TSharedPtr<FSlateStyleSet> StyleInstance;
};
