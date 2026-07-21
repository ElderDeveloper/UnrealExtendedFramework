// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestManager.h"

#include "Engine/Engine.h"
#include "EGQuestConstants.h"
#include "IEGQuestPluginModule.h"

#include "Engine/ObjectLibrary.h"
#include "Interfaces/IPluginManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "EGQuestConstants.h"
#include "EGQuestEventCustom.h"
#include "EGQuestGraph.h"
#include "EGQuestHelper.h"
#include "EGQuestTextArgumentCustom.h"
#include "Logging/EGQuestLogger.h"

bool UEGQuestManager::bCalledLoadAllQuestsIntoMemory = false;

int32 UEGQuestManager::LoadAllQuestsIntoMemory(bool bAsync)
{
	bCalledLoadAllQuestsIntoMemory = true;
	// The library stays rooted so the loaded quest graphs keep a strong referencer between calls;
	// otherwise GC could collect them and GetAllQuestsFromMemory would return a shrinking set.
	static TStrongObjectPtr<UObjectLibrary> QuestLibrary;
	if (!QuestLibrary.IsValid())
	{
		QuestLibrary.Reset(UObjectLibrary::CreateLibrary(UEGQuestGraph::StaticClass(), false, GIsEditor));
	}
	TArray<FString> Paths{TEXT("/Game")};
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(QUEST_SYSTEM_HOST_PLUGIN_NAME.ToString()); Plugin.IsValid())
	{
		FString Path = Plugin->GetMountedAssetPath(); Path.RemoveFromEnd(TEXT("/")); Paths.Add(Path);
	}
	const int32 Count = QuestLibrary->LoadAssetDataFromPaths(Paths, !bAsync);
	QuestLibrary->LoadAssetsFromAssetData();

	// Primary asset ids derive from the asset name: name collisions would alias save identifiers.
	TMap<FPrimaryAssetId, UEGQuestGraph*> SeenAssetIds;
	for (UEGQuestGraph* Quest : GetAllQuestsFromMemory())
	{
		const FPrimaryAssetId AssetId = Quest->GetPrimaryAssetId();
		if (UEGQuestGraph** Existing = SeenAssetIds.Find(AssetId))
		{
			FEGQuestLogger::Get().Errorf(
				TEXT("Duplicate quest primary asset id '%s': '%s' and '%s'. Rename one - save data and asset lookups cannot distinguish them."),
				*AssetId.ToString(), *(*Existing)->GetPathName(), *Quest->GetPathName());
		}
		else
		{
			SeenAssetIds.Add(AssetId, Quest);
		}
	}
	return Count;
}

TArray<UEGQuestGraph*> UEGQuestManager::GetAllQuestsFromMemory()
{
#if WITH_EDITOR
	if (!bCalledLoadAllQuestsIntoMemory) LoadAllQuestsIntoMemory(false);
#endif
	TArray<UEGQuestGraph*> Result;
	for (TObjectIterator<UEGQuestGraph> It; It; ++It) if (IsValid(*It)) Result.Add(*It);
	return Result;
}

TArray<UEGQuestGraph*> UEGQuestManager::GetQuestsWithDuplicateGUIDs()
{
	TArray<UEGQuestGraph*> Result; TSet<FGuid> Seen;
	for (UEGQuestGraph* Quest : GetAllQuestsFromMemory())
	{
		if (Seen.Contains(Quest->GetGUID())) Result.Add(Quest); else Seen.Add(Quest->GetGUID());
	}
	return Result;
}

UWorld* UEGQuestManager::GetQuestWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}

	UWorld* FirstGameWorld = nullptr;
	int32 GameWorldCount = 0;
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (UWorld* World = WorldContext.World())
		{
			if (World->IsGameWorld())
			{
				if (!FirstGameWorld)
				{
					FirstGameWorld = World;
				}
				++GameWorldCount;
			}
		}
	}

	// Multiple game worlds means PIE with a listen server and/or clients in one process:
	// "the first game world" is a guess and may belong to the wrong peer. Outer-chain world
	// resolution (component-owned contexts) is unaffected; only ownerless contexts land here.
	if (GameWorldCount > 1)
	{
		FEGQuestLogger::Get().Warningf(
			TEXT("GetQuestWorld - %d game worlds are active (PIE multiplayer?); returning the first one ('%s'). Give quest contexts a world-owned outer to avoid this ambiguity."),
			GameWorldCount, *FirstGameWorld->GetName());
	}
	return FirstGameWorld;
}

bool UEGQuestManager::IsObjectACustomEvent(const UObject* Object) { return FEGQuestHelper::IsObjectAChildOf(Object, UEGQuestEventCustom::StaticClass()); }
bool UEGQuestManager::IsObjectACustomTextArgument(const UObject* Object) { return FEGQuestHelper::IsObjectAChildOf(Object, UEGQuestTextArgumentCustom::StaticClass()); }
