// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncIdLookup.h"
#include "Engine/GameInstance.h"


UEPFAsyncIdLookup* UEPFAsyncIdLookup::FromSteamIDs(UObject* WorldContext, const TArray<FString>& SteamIds)
{
	auto* Action = NewObject<UEPFAsyncIdLookup>();
	Action->Method = Steam;
	Action->Ids = SteamIds;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

UEPFAsyncIdLookup* UEPFAsyncIdLookup::FromCustomIDs(UObject* WorldContext, const TArray<FString>& CustomIds)
{
	auto* Action = NewObject<UEPFAsyncIdLookup>();
	Action->Method = Custom;
	Action->Ids = CustomIds;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

UEPFAsyncIdLookup* UEPFAsyncIdLookup::FromXboxLiveIDs(UObject* WorldContext, const TArray<FString>& XboxLiveIds)
{
	auto* Action = NewObject<UEPFAsyncIdLookup>();
	Action->Method = Xbox;
	Action->Ids = XboxLiveIds;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

UEPFAsyncIdLookup* UEPFAsyncIdLookup::FromPSNAccountIDs(UObject* WorldContext, const TArray<FString>& PsnAccountIds)
{
	auto* Action = NewObject<UEPFAsyncIdLookup>();
	Action->Method = PSN;
	Action->Ids = PsnAccountIds;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncIdLookup::Activate()
{
	auto* Sub = GetEPFSubsystemFromContext<UEPFIdLookupSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("ID Lookup subsystem not available"))); SetReadyToDestroy(); return; }
	if (Ids.Num() == 0) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("No IDs provided"))); SetReadyToDestroy(); return; }

	Sub->OnIdLookupComplete.AddDynamic(this, &UEPFAsyncIdLookup::HandleComplete);

	switch (Method)
	{
	case Steam: Sub->GetPlayFabIDsFromSteamIDs(Ids); break;
	case Custom: Sub->GetPlayFabIDsFromCustomIDs(Ids); break;
	case Xbox: Sub->GetPlayFabIDsFromXboxLiveIDs(Ids); break;
	case PSN: Sub->GetPlayFabIDsFromPSNAccountIDs(Ids); break;
	}
}

void UEPFAsyncIdLookup::HandleComplete(const FEPFResult& Result, const TArray<FEPFIdMapping>& Mappings)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFIdLookupSubsystem>(WorldContext.Get()))
		Sub->OnIdLookupComplete.RemoveDynamic(this, &UEPFAsyncIdLookup::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Mappings) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("ID lookup failed")));
	SetReadyToDestroy();
}
