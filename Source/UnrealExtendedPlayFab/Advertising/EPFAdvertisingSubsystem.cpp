// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAdvertisingSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

void UEPFAdvertisingSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UEPFAdvertisingSubsystem::Deinitialize() { Super::Deinitialize(); }


void UEPFAdvertisingSubsystem::AttributeInstall(const FString& AdvertisingIdType, const FString& AdvertisingId)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(*AdvertisingIdType, AdvertisingId);

	SendPlayFabRequestDetailed(TEXT("/Client/AttributeInstall"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAdvertising — Install attributed"));
			OnInstallAttributed.Broadcast(Result);
		}));
}


void UEPFAdvertisingSubsystem::GetAdPlacements(const FString& AppId, const FString& Identifier)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("AppId"), AppId);

	TSharedRef<FJsonObject> IdObj = MakeShared<FJsonObject>();
	IdObj->SetStringField(TEXT("Name"), Identifier.IsEmpty() ? TEXT("default") : Identifier);
	Body->SetObjectField(TEXT("Identifier"), IdObj);

	SendPlayFabRequestDetailed(TEXT("/Client/GetAdPlacements"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			TArray<FEPFAdPlacement> Placements;
			if (Result.bSuccess && Response.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* PlArr = nullptr;
				if (Response->TryGetArrayField(TEXT("AdPlacements"), PlArr) && PlArr)
				{
					for (const auto& V : *PlArr)
					{
						const TSharedPtr<FJsonObject>* Obj = nullptr;
						if (V->TryGetObject(Obj) && Obj)
						{
							FEPFAdPlacement P;
							P.PlacementId = (*Obj)->GetStringField(TEXT("PlacementId"));
							P.PlacementName = (*Obj)->GetStringField(TEXT("PlacementName"));
							P.RewardAssetUrl = (*Obj)->GetStringField(TEXT("RewardAssetUrl"));
							P.RewardDescription = (*Obj)->GetStringField(TEXT("RewardDescription"));
							P.RewardId = (*Obj)->GetStringField(TEXT("RewardId"));
							P.RewardName = (*Obj)->GetStringField(TEXT("RewardName"));
							Placements.Add(P);
						}
					}
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAdvertising — %d ad placements"), Placements.Num());
			}
			OnAdPlacementsReceived.Broadcast(Result, Placements);
		}));
}


void UEPFAdvertisingSubsystem::ReportAdActivity(const FString& PlacementId, const FString& RewardId, const FString& Activity)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("PlacementId"), PlacementId);
	Body->SetStringField(TEXT("RewardId"), RewardId);
	Body->SetStringField(TEXT("Activity"), Activity);

	SendPlayFabRequestDetailed(TEXT("/Client/ReportAdActivity"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			OnAdActivityReported.Broadcast(Result);
		}));
}


void UEPFAdvertisingSubsystem::RewardAdActivity(const FString& PlacementId, const FString& RewardId)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("PlacementId"), PlacementId);
	Body->SetStringField(TEXT("RewardId"), RewardId);

	SendPlayFabRequestDetailed(TEXT("/Client/RewardAdActivity"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAdvertising — Ad activity rewarded"));
			OnAdActivityRewarded.Broadcast(Result);
		}));
}
