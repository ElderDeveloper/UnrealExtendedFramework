// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"

class FESQLDatabase;
class UESQLTableAsset;

class FESQLTableEditorDatabaseRegistry
{
public:
	static TSharedPtr<FESQLDatabase> Find(const UESQLTableAsset* TableAsset);
	static void Set(const UESQLTableAsset* TableAsset, TSharedPtr<FESQLDatabase> Database);
	static void Clear(const UESQLTableAsset* TableAsset);
	static void ClearIfMatches(const UESQLTableAsset* TableAsset, const TSharedPtr<FESQLDatabase>& Database);

private:
	static FCriticalSection RegistryMutex;
	static TMap<FObjectKey, TWeakPtr<FESQLDatabase>> Registry;
};