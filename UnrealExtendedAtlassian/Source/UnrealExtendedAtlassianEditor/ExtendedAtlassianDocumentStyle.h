// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ISlateStyle;

/**
 * Compatibility facade for document widgets that predate the unified Backlot style.
 */
class FExtendedAtlassianDocumentStyle
{
public:
	static void Register();
	static void Unregister();

	static const ISlateStyle& Get();
	static FName GetStyleSetName();
};
