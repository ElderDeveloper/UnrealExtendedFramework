// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Private/USqlite/ESQLUsqliteTypes.h"

class FESQLUsqliteBuilder
{
public:
	static bool BuildProjectRoot(
		const FString& ProjectRoot,
		const FString& PreferredOutputDbPath,
		FString& OutResolvedDatabasePath,
		FString& OutError);

	static bool BuildProject(
		const FESQLUsqliteProject& Project,
		const FString& ProjectRoot,
		const FString& PreferredOutputDbPath,
		FString& OutResolvedDatabasePath,
		FString& OutError);
};