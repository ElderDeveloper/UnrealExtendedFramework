// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FESQLContentExtractor
{
public:
	static bool EnsureExtracted(const FString& SourcePath, FString& OutExtractedPath, FString& OutError);
	static FString GetContentCacheRoot();
};