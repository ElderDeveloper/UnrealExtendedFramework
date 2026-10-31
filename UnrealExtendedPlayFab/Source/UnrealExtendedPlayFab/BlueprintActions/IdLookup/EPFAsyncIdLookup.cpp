// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncIdLookup.h"


UEPFAsyncIdLookup* UEPFAsyncIdLookup::FromSteamIDs(UObject* WorldContext, const TArray<FString>& SteamIds)
{
	auto* Action = NewObject<UEPFAsyncIdLookup>();
	Action->Method = Steam;
	Action->Ids = SteamIds;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

UEPFAsyncIdLookup* UEPFAsyncIdLookup::FromCustomIDs(UObject* WorldContext, const TArray<FString>& CustomIds)
{
	auto* Action = NewObject<UEPFAsyncIdLookup>();
	Action->Method = Custom;
	Action->Ids = CustomIds;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

UEPFAsyncIdLookup* UEPFAsyncIdLookup::FromXboxLiveIDs(UObject* WorldContext, const TArray<FString>& XboxLiveIds)
{
	auto* Action = NewObject<UEPFAsyncIdLookup>();
	Action->Method = Xbox;
	Action->Ids = XboxLiveIds;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

UEPFAsyncIdLookup* UEPFAsyncIdLookup::FromPSNAccountIDs(UObject* WorldContext, const TArray<FString>& PsnAccountIds)
{
	auto* Action = NewObject<UEPFAsyncIdLookup>();
	Action->Method = PSN;
	Action->Ids = PsnAccountIds;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncIdLookup::ExecuteAction()
{
	auto* Sub = RequireGameInstanceSubsystem<UEPFIdLookupSubsystem>();
	if (!Sub) return;
	if (Ids.Num() == 0) { FailInvalidArgument(TEXT("No IDs provided")); return; }

	Sub->OnIdLookupComplete.AddDynamic(this, &UEPFAsyncIdLookup::HandleComplete);

	CallWithContext([&]
	{
		switch (Method)
		{
		case Steam: Sub->GetPlayFabIDsFromSteamIDs(Ids); break;
		case Custom: Sub->GetPlayFabIDsFromCustomIDs(Ids); break;
		case Xbox: Sub->GetPlayFabIDsFromXboxLiveIDs(Ids); break;
		case PSN: Sub->GetPlayFabIDsFromPSNAccountIDs(Ids); break;
		}
	});
}

void UEPFAsyncIdLookup::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFIdLookupSubsystem>())
		Sub->OnIdLookupComplete.RemoveDynamic(this, &UEPFAsyncIdLookup::HandleComplete);
}

void UEPFAsyncIdLookup::HandleComplete(const FEPFResult& Result, const TArray<FEPFIdMapping>& Mappings)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Mappings) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("ID lookup failed")));
}
