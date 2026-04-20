// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "TableAssetEditor/ESQLTableEditorDatabaseRegistry.h"

#include "Core/ESQLDatabase.h"
#include "TableAsset/ESQLTableAsset.h"

FCriticalSection FESQLTableEditorDatabaseRegistry::RegistryMutex;
TMap<FObjectKey, TWeakPtr<FESQLDatabase>> FESQLTableEditorDatabaseRegistry::Registry;

TSharedPtr<FESQLDatabase> FESQLTableEditorDatabaseRegistry::Find(const UESQLTableAsset* TableAsset)
{
	if (!TableAsset)
	{
		return nullptr;
	}

	FScopeLock Lock(&RegistryMutex);
	const FObjectKey AssetKey(TableAsset);
	if (TWeakPtr<FESQLDatabase>* Found = Registry.Find(AssetKey))
	{
		if (TSharedPtr<FESQLDatabase> Database = Found->Pin())
		{
			return Database;
		}

		Registry.Remove(AssetKey);
	}

	return nullptr;
}

void FESQLTableEditorDatabaseRegistry::Set(const UESQLTableAsset* TableAsset, TSharedPtr<FESQLDatabase> Database)
{
	if (!TableAsset)
	{
		return;
	}

	FScopeLock Lock(&RegistryMutex);
	const FObjectKey AssetKey(TableAsset);
	if (Database.IsValid() && Database->IsOpen())
	{
		Registry.Add(AssetKey, Database);
	}
	else
	{
		Registry.Remove(AssetKey);
	}
}

void FESQLTableEditorDatabaseRegistry::Clear(const UESQLTableAsset* TableAsset)
{
	if (!TableAsset)
	{
		return;
	}

	FScopeLock Lock(&RegistryMutex);
	Registry.Remove(FObjectKey(TableAsset));
}

void FESQLTableEditorDatabaseRegistry::ClearIfMatches(const UESQLTableAsset* TableAsset, const TSharedPtr<FESQLDatabase>& Database)
{
	if (!TableAsset)
	{
		return;
	}

	FScopeLock Lock(&RegistryMutex);
	const FObjectKey AssetKey(TableAsset);
	if (TWeakPtr<FESQLDatabase>* Found = Registry.Find(AssetKey))
	{
		const TSharedPtr<FESQLDatabase> RegisteredDatabase = Found->Pin();
		if (!RegisteredDatabase.IsValid() || RegisteredDatabase == Database)
		{
			Registry.Remove(AssetKey);
		}
	}
}