// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFPlayerProfileSubsystem.h"
#include "Shared/EPFSettings.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

namespace
{
	FString GetOptionalStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
	{
		if (!Object.IsValid())
		{
			return FString();
		}

		FString Value;
		return Object->TryGetStringField(FieldName, Value) ? Value : FString();
	}
}

void UEPFPlayerProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFPlayerProfileSubsystem::Deinitialize()
{
	CachedProfile = FEPFPlayerProfile();
	Super::Deinitialize();
}


// ── Get Player Profile ───────────────────────────────────────────────────────

void UEPFPlayerProfileSubsystem::GetPlayerProfile(const FString& PlayFabId)
{
	// Read constraints from project settings (Project Settings → Extended PlayFab → Profile Constraints)
	const FEPFProfileConstraints& Constraints = UEPFSettings::Get()->ProfileConstraints;
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	if (!PlayFabId.IsEmpty())
	{
		Body->SetStringField(TEXT("PlayFabId"), PlayFabId);
	}

	// Build ProfileConstraints only from the fields the caller explicitly opted into.
	// Sending a field that your title's Client Profile Constraints does not allow
	// results in error 1303 (RequestViewConstraintParamsNotAllowed).
	TSharedPtr<FJsonObject> ProfileConstraints = MakeShared<FJsonObject>();
	ProfileConstraints->SetBoolField(TEXT("ShowDisplayName"),                    Constraints.bShowDisplayName);
	ProfileConstraints->SetBoolField(TEXT("ShowAvatarUrl"),                      Constraints.bShowAvatarUrl);
	ProfileConstraints->SetBoolField(TEXT("ShowCreated"),                        Constraints.bShowCreated);
	ProfileConstraints->SetBoolField(TEXT("ShowLastLogin"),                      Constraints.bShowLastLogin);
	ProfileConstraints->SetBoolField(TEXT("ShowStatistics"),                     Constraints.bShowStatistics);
	ProfileConstraints->SetBoolField(TEXT("ShowLinkedAccounts"),                 Constraints.bShowLinkedAccounts);
	ProfileConstraints->SetBoolField(TEXT("ShowOrigination"),                    Constraints.bShowOrigination);
	ProfileConstraints->SetBoolField(TEXT("ShowBannedUntil"),                    Constraints.bShowBannedUntil);
	ProfileConstraints->SetBoolField(TEXT("ShowCampaignAttributions"),           Constraints.bShowCampaignAttributions);
	ProfileConstraints->SetBoolField(TEXT("ShowPushNotificationRegistrations"),  Constraints.bShowPushNotificationRegistrations);
	ProfileConstraints->SetBoolField(TEXT("ShowContactEmailAddresses"),          Constraints.bShowContactEmailAddresses);
	ProfileConstraints->SetBoolField(TEXT("ShowTags"),                           Constraints.bShowTags);
	ProfileConstraints->SetBoolField(TEXT("ShowLocations"),                      Constraints.bShowLocations);
	ProfileConstraints->SetBoolField(TEXT("ShowMemberships"),                    Constraints.bShowMemberships);
	Body->SetObjectField(TEXT("ProfileConstraints"), ProfileConstraints);

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetPlayerProfile"),
		Body,
		EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			FEPFPlayerProfile Profile;
			if (Result.bSuccess && Response.IsValid())
			{
				const TSharedPtr<FJsonObject>* ProfileObj = nullptr;
				if (Response->TryGetObjectField(TEXT("PlayerProfile"), ProfileObj) && ProfileObj)
				{
					Profile.PlayFabId = GetOptionalStringField(*ProfileObj, TEXT("PlayerId"));
					Profile.DisplayName = GetOptionalStringField(*ProfileObj, TEXT("DisplayName"));
					Profile.AvatarUrl = GetOptionalStringField(*ProfileObj, TEXT("AvatarUrl"));
					Profile.Created = GetOptionalStringField(*ProfileObj, TEXT("Created"));
					Profile.LastLogin = GetOptionalStringField(*ProfileObj, TEXT("LastLogin"));

					// Linked accounts
					const TArray<TSharedPtr<FJsonValue>>* AccountsArr = nullptr;
					if ((*ProfileObj)->TryGetArrayField(TEXT("LinkedAccounts"), AccountsArr) && AccountsArr)
					{
						for (const auto& AccVal : *AccountsArr)
						{
							const TSharedPtr<FJsonObject>* AccObj = nullptr;
							if (AccVal->TryGetObject(AccObj) && AccObj)
							{
								const FString Platform = GetOptionalStringField(*AccObj, TEXT("Platform"));
								if (!Platform.IsEmpty())
								{
									Profile.LinkedAccounts.Add(Platform);
								}
							}
						}
					}

					// Statistics
					const TArray<TSharedPtr<FJsonValue>>* StatsArr = nullptr;
					if ((*ProfileObj)->TryGetArrayField(TEXT("Statistics"), StatsArr) && StatsArr)
					{
						for (const auto& StatVal : *StatsArr)
						{
							const TSharedPtr<FJsonObject>* StatObj = nullptr;
							if (StatVal->TryGetObject(StatObj) && StatObj)
							{
								const FString Name = GetOptionalStringField(*StatObj, TEXT("Name"));
								if (!Name.IsEmpty())
								{
									Profile.Statistics.Add(
										Name,
										static_cast<int32>((*StatObj)->GetNumberField(TEXT("Value")))
									);
								}
							}
						}
					}
				}
				CachedProfile = Profile;
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFPlayerProfile — Profile received for %s"), *Profile.PlayFabId);
			}
			OnProfileReceived.Broadcast(Result, Profile);
		})
	);
}


// ── Get Combined Info ────────────────────────────────────────────────────────

void UEPFPlayerProfileSubsystem::GetPlayerCombinedInfo(bool bGetStats, bool bGetPlayerData, bool bGetVirtualCurrency, bool bGetInventory)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	TSharedPtr<FJsonObject> InfoRequest = MakeShared<FJsonObject>();
	InfoRequest->SetBoolField(TEXT("GetPlayerStatistics"), bGetStats);
	InfoRequest->SetBoolField(TEXT("GetUserData"), bGetPlayerData);
	InfoRequest->SetBoolField(TEXT("GetUserVirtualCurrency"), bGetVirtualCurrency);
	InfoRequest->SetBoolField(TEXT("GetUserInventory"), bGetInventory);
	InfoRequest->SetBoolField(TEXT("GetPlayerProfile"), true);
	InfoRequest->SetBoolField(TEXT("GetUserAccountInfo"), true);

	// Profile constraints for combined info — only ShowDisplayName is safe by default.
	// The combined info call includes AccountInfo which already provides display name,
	// so the PlayerProfile section in the result may be empty when ShowDisplayName=false.
	TSharedPtr<FJsonObject> ProfileConstraints = MakeShared<FJsonObject>();
	ProfileConstraints->SetBoolField(TEXT("ShowDisplayName"), true);
	InfoRequest->SetObjectField(TEXT("ProfileConstraints"), ProfileConstraints);

	Body->SetObjectField(TEXT("InfoRequestParameters"), InfoRequest);

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetPlayerCombinedInfo"),
		Body,
		EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			FEPFPlayerProfile Profile;
			if (Result.bSuccess && Response.IsValid())
			{
				const TSharedPtr<FJsonObject>* InfoObj = nullptr;
				if (Response->TryGetObjectField(TEXT("InfoResultPayload"), InfoObj) && InfoObj)
				{
					// Account Info
					const TSharedPtr<FJsonObject>* AccInfo = nullptr;
					if ((*InfoObj)->TryGetObjectField(TEXT("AccountInfo"), AccInfo) && AccInfo)
					{
						Profile.PlayFabId = GetOptionalStringField(*AccInfo, TEXT("PlayFabId"));
						Profile.Created = GetOptionalStringField(*AccInfo, TEXT("Created"));

						const TSharedPtr<FJsonObject>* TitleInfo = nullptr;
						if ((*AccInfo)->TryGetObjectField(TEXT("TitleInfo"), TitleInfo) && TitleInfo)
						{
							Profile.DisplayName = GetOptionalStringField(*TitleInfo, TEXT("DisplayName"));
							Profile.LastLogin = GetOptionalStringField(*TitleInfo, TEXT("LastLogin"));
							Profile.AvatarUrl = GetOptionalStringField(*TitleInfo, TEXT("AvatarUrl"));
						}
					}

					// Player Profile
					const TSharedPtr<FJsonObject>* ProfileObj = nullptr;
					if ((*InfoObj)->TryGetObjectField(TEXT("PlayerProfile"), ProfileObj) && ProfileObj)
					{
						if (Profile.DisplayName.IsEmpty())
							Profile.DisplayName = GetOptionalStringField(*ProfileObj, TEXT("DisplayName"));
						if (Profile.AvatarUrl.IsEmpty())
							Profile.AvatarUrl = GetOptionalStringField(*ProfileObj, TEXT("AvatarUrl"));

						const TArray<TSharedPtr<FJsonValue>>* AccountsArr = nullptr;
						if ((*ProfileObj)->TryGetArrayField(TEXT("LinkedAccounts"), AccountsArr) && AccountsArr)
						{
							for (const auto& AccVal : *AccountsArr)
							{
								const TSharedPtr<FJsonObject>* AccObj = nullptr;
								if (AccVal->TryGetObject(AccObj) && AccObj)
								{
									const FString Platform = GetOptionalStringField(*AccObj, TEXT("Platform"));
									if (!Platform.IsEmpty())
									{
										Profile.LinkedAccounts.Add(Platform);
									}
								}
							}
						}
					}

					// Statistics
					const TArray<TSharedPtr<FJsonValue>>* StatsArr = nullptr;
					if ((*InfoObj)->TryGetArrayField(TEXT("PlayerStatistics"), StatsArr) && StatsArr)
					{
						for (const auto& StatVal : *StatsArr)
						{
							const TSharedPtr<FJsonObject>* StatObj = nullptr;
							if (StatVal->TryGetObject(StatObj) && StatObj)
							{
								const FString StatisticName = GetOptionalStringField(*StatObj, TEXT("StatisticName"));
								if (!StatisticName.IsEmpty())
								{
									Profile.Statistics.Add(
										StatisticName,
										static_cast<int32>((*StatObj)->GetNumberField(TEXT("Value")))
									);
								}
							}
						}
					}

					// Player Data
					const TSharedPtr<FJsonObject>* DataObj = nullptr;
					if ((*InfoObj)->TryGetObjectField(TEXT("UserData"), DataObj) && DataObj)
					{
						for (const auto& Pair : (*DataObj)->Values)
						{
							const TSharedPtr<FJsonObject>* ValueObj = nullptr;
							if (Pair.Value->TryGetObject(ValueObj) && ValueObj)
							{
								Profile.PlayerData.Add(Pair.Key, (*ValueObj)->GetStringField(TEXT("Value")));
							}
						}
					}
				}
				CachedProfile = Profile;
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFPlayerProfile — Combined info: %d stats, %d data keys"),
					Profile.Statistics.Num(), Profile.PlayerData.Num());
			}
			OnCombinedInfoReceived.Broadcast(Result, Profile);
		})
	);
}


// ── Update Avatar ────────────────────────────────────────────────────────────

void UEPFPlayerProfileSubsystem::UpdateAvatarUrl(const FString& AvatarUrl)
{
	if (AvatarUrl.IsEmpty()) { OnAvatarUpdated.Broadcast(FEPFResult::Failure(TEXT("AvatarUrl cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("ImageUrl"), AvatarUrl);

	SendPlayFabRequestDetailed(TEXT("/Client/UpdateAvatarUrl"), Body, EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this, AvatarUrl](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) CachedProfile.AvatarUrl = AvatarUrl;
			OnAvatarUpdated.Broadcast(Result);
		}));
}


// ── Queries ──────────────────────────────────────────────────────────────────

FEPFPlayerProfile UEPFPlayerProfileSubsystem::GetCachedProfile() const { return CachedProfile; }


// ── Get Account Info ─────────────────────────────────────────────────────────

void UEPFPlayerProfileSubsystem::GetAccountInfo(const FString& PlayFabId)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	if (!PlayFabId.IsEmpty())
	{
		Body->SetStringField(TEXT("PlayFabId"), PlayFabId);
	}

	SendPlayFabRequestDetailed(TEXT("/Client/GetAccountInfo"), Body, EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			FEPFAccountInfo Info;
			if (Result.bSuccess && Response.IsValid())
			{
				const TSharedPtr<FJsonObject>* AccInfo = nullptr;
				if (Response->TryGetObjectField(TEXT("AccountInfo"), AccInfo) && AccInfo)
				{
					Info.PlayFabId = GetOptionalStringField(*AccInfo, TEXT("PlayFabId"));
					Info.Created = GetOptionalStringField(*AccInfo, TEXT("Created"));

					const TSharedPtr<FJsonObject>* TitleInfo = nullptr;
					if ((*AccInfo)->TryGetObjectField(TEXT("TitleInfo"), TitleInfo) && TitleInfo)
					{
						Info.TitleDisplayName = GetOptionalStringField(*TitleInfo, TEXT("DisplayName"));
						Info.LastLogin = GetOptionalStringField(*TitleInfo, TEXT("LastLogin"));
						Info.Origination = GetOptionalStringField(*TitleInfo, TEXT("Origination"));
					}

					// Linked accounts
					const TSharedPtr<FJsonObject>* GoogleInfo = nullptr;
					if ((*AccInfo)->TryGetObjectField(TEXT("GoogleInfo"), GoogleInfo)) Info.LinkedAccounts.Add(TEXT("Google"));
					const TSharedPtr<FJsonObject>* SteamInfo = nullptr;
					if ((*AccInfo)->TryGetObjectField(TEXT("SteamInfo"), SteamInfo) && SteamInfo)
					{
						Info.LinkedAccounts.Add(TEXT("Steam"));
						// SteamPersonaName is the Steam username — the only name available
						// before the player sets a PlayFab title display name.
						(*SteamInfo)->TryGetStringField(TEXT("SteamPersonaName"), Info.SteamName);
					}
					const TSharedPtr<FJsonObject>* XboxInfo = nullptr;
					if ((*AccInfo)->TryGetObjectField(TEXT("XboxInfo"), XboxInfo)) Info.LinkedAccounts.Add(TEXT("Xbox"));
					const TSharedPtr<FJsonObject>* PSNInfo = nullptr;
					if ((*AccInfo)->TryGetObjectField(TEXT("PsnInfo"), PSNInfo)) Info.LinkedAccounts.Add(TEXT("PSN"));
					const TSharedPtr<FJsonObject>* FacebookInfo = nullptr;
					if ((*AccInfo)->TryGetObjectField(TEXT("FacebookInfo"), FacebookInfo)) Info.LinkedAccounts.Add(TEXT("Facebook"));
					const TSharedPtr<FJsonObject>* AppleInfo = nullptr;
					if ((*AccInfo)->TryGetObjectField(TEXT("AppleAccountInfo"), AppleInfo)) Info.LinkedAccounts.Add(TEXT("Apple"));

					UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFPlayerProfile — AccountInfo: %s (%d linked)"), *Info.PlayFabId, Info.LinkedAccounts.Num());
				}
			}
			OnAccountInfoReceived.Broadcast(Result, Info);
		}));
}
