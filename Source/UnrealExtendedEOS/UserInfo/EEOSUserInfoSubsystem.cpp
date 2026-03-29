// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSUserInfoSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineUserInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "UnrealExtendedEOS.h"

#include "eos_sdk.h"
#include "eos_connect.h"

void UEEOSUserInfoSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSUserInfoSubsystem::Deinitialize()
{
	CachedSearchResults.Empty();
	CachedMappings.Empty();
	Super::Deinitialize();
}

// ── User Lookup ──────────────────────────────────────────────────────────────

void UEEOSUserInfoSubsystem::QueryUserInfo(const FString& EpicAccountId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryUserInfo"));
		OnUserInfoQueried.Broadcast(false, FEEOSUserInfo());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineUserPtr UserInterface = EOSSub->GetUserInterface();
	if (!UserInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSUserInfoSubsystem::QueryUserInfo — User interface not available"));
		OnUserInfoQueried.Broadcast(false, FEEOSUserInfo());
		return;
	}

	FUniqueNetIdPtr LocalUserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LocalUserId.IsValid())
	{
		OnUserInfoQueried.Broadcast(false, FEEOSUserInfo());
		return;
	}

	TArray<FUniqueNetIdRef> UserIds;
	{
		FUniqueNetIdPtr ParsedId = EOSSub->GetIdentityInterface()->CreateUniquePlayerId(EpicAccountId);
		if (!ParsedId.IsValid())
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem::QueryUserInfo — Invalid EpicAccountId '%s'"), *EpicAccountId);
			OnUserInfoQueried.Broadcast(false, FEEOSUserInfo());
			return;
		}
		UserIds.Add(ParsedId->AsShared());
	}

	// Remove any previous delegate to prevent accumulation
	if (QueryUserInfoDelegateHandle.IsValid())
	{
		UserInterface->OnQueryUserInfoCompleteDelegates[0].Remove(QueryUserInfoDelegateHandle);
	}

	QueryUserInfoDelegateHandle = UserInterface->OnQueryUserInfoCompleteDelegates[0].AddLambda(
		[this, EOSSub, EpicAccountId, UserInterface](int32 LocalUserNum, bool bWasSuccessful, const TArray<FUniqueNetIdRef>& QueriedIds, const FString& ErrorStr)
		{
			// Remove ourselves to prevent accumulation on next call
			UserInterface->OnQueryUserInfoCompleteDelegates[0].Remove(QueryUserInfoDelegateHandle);
			QueryUserInfoDelegateHandle.Reset();

			if (bWasSuccessful && QueriedIds.Num() > 0)
			{
				IOnlineUserPtr UserIf = EOSSub->GetUserInterface();
				if (UserIf.IsValid())
				{
					TSharedPtr<FOnlineUser> UserData = UserIf->GetUserInfo(LocalUserNum, *QueriedIds[0]);
					if (UserData.IsValid())
					{
						CachedUserInfo.EpicAccountId = UserData->GetUserId()->ToString();
						CachedUserInfo.DisplayName = UserData->GetDisplayName();
						UserData->GetUserAttribute(TEXT("country"), CachedUserInfo.Country);
						UserData->GetUserAttribute(TEXT("preferredLanguage"), CachedUserInfo.PreferredLanguage);

						UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUserInfoSubsystem: Queried user '%s' — %s"),
							*CachedUserInfo.EpicAccountId, *CachedUserInfo.DisplayName);
						OnUserInfoQueried.Broadcast(true, CachedUserInfo);
						return;
					}
				}
			}

			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem: QueryUserInfo failed for '%s'"), *EpicAccountId);
			OnUserInfoQueried.Broadcast(false, FEEOSUserInfo());
		});

	UserInterface->QueryUserInfo(0, UserIds);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUserInfoSubsystem::QueryUserInfo — Querying '%s'..."), *EpicAccountId);
}

void UEEOSUserInfoSubsystem::FindUserByDisplayName(const FString& DisplayName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("FindUserByDisplayName"));
		OnUserSearchComplete.Broadcast(false, TArray<FEEOSUserInfo>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineUserPtr UserInterface = EOSSub->GetUserInterface();
	if (!UserInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSUserInfoSubsystem::FindUserByDisplayName — User interface not available"));
		OnUserSearchComplete.Broadcast(false, TArray<FEEOSUserInfo>());
		return;
	}

	// Use QueryUserIdMapping to find a user by display name
	FUniqueNetIdPtr LocalUserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LocalUserId.IsValid())
	{
		OnUserSearchComplete.Broadcast(false, TArray<FEEOSUserInfo>());
		return;
	}

	// FOnQueryUserMappingComplete is a regular delegate — CreateLambda works
	UserInterface->QueryUserIdMapping(*LocalUserId, DisplayName,
		IOnlineUser::FOnQueryUserMappingComplete::CreateLambda(
			[this, DisplayName](bool bWasSuccessful, const FUniqueNetId& RequestingUserId, const FString& InDisplayName, const FUniqueNetId& FoundUserId, const FString& Error)
			{
				CachedSearchResults.Empty();

				if (bWasSuccessful)
				{
					FEEOSUserInfo Info;
					Info.EpicAccountId = FoundUserId.ToString();
					Info.DisplayName = InDisplayName;
					CachedSearchResults.Add(Info);

					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUserInfoSubsystem: Found user '%s' — ID=%s"),
						*InDisplayName, *Info.EpicAccountId);
				}
				else
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem: FindUserByDisplayName failed for '%s' — %s"),
						*DisplayName, *Error);
				}

				OnUserSearchComplete.Broadcast(bWasSuccessful, CachedSearchResults);
			}));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUserInfoSubsystem::FindUserByDisplayName — Searching '%s'..."), *DisplayName);
}

void UEEOSUserInfoSubsystem::QueryUserInfoBatch(const TArray<FString>& EpicAccountIds)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryUserInfoBatch"));
		OnUserSearchComplete.Broadcast(false, TArray<FEEOSUserInfo>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineUserPtr UserInterface = EOSSub->GetUserInterface();
	if (!UserInterface.IsValid())
	{
		OnUserSearchComplete.Broadcast(false, TArray<FEEOSUserInfo>());
		return;
	}

	TArray<FUniqueNetIdRef> UserIds;
	for (const FString& Id : EpicAccountIds)
	{
		FUniqueNetIdPtr ParsedId = EOSSub->GetIdentityInterface()->CreateUniquePlayerId(Id);
		if (ParsedId.IsValid())
		{
			UserIds.Add(ParsedId->AsShared());
		}
		else
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem::QueryUserInfoBatch — Skipping invalid ID '%s'"), *Id);
		}
	}

	if (UserIds.Num() == 0)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem::QueryUserInfoBatch — No valid IDs to query"));
		OnUserSearchComplete.Broadcast(false, TArray<FEEOSUserInfo>());
		return;
	}

	// Remove any previous delegate to prevent accumulation
	if (QueryBatchDelegateHandle.IsValid())
	{
		UserInterface->OnQueryUserInfoCompleteDelegates[0].Remove(QueryBatchDelegateHandle);
	}

	QueryBatchDelegateHandle = UserInterface->OnQueryUserInfoCompleteDelegates[0].AddLambda(
		[this, EOSSub, EpicAccountIds, UserInterface](int32 LocalUserNum, bool bWasSuccessful, const TArray<FUniqueNetIdRef>& QueriedIds, const FString& ErrorStr)
		{
			// Remove ourselves to prevent accumulation on next call
			UserInterface->OnQueryUserInfoCompleteDelegates[0].Remove(QueryBatchDelegateHandle);
			QueryBatchDelegateHandle.Reset();

			CachedSearchResults.Empty();

			if (bWasSuccessful)
			{
				IOnlineUserPtr UserIf = EOSSub->GetUserInterface();
				if (UserIf.IsValid())
				{
					for (const FUniqueNetIdRef& UserId : QueriedIds)
					{
						TSharedPtr<FOnlineUser> UserData = UserIf->GetUserInfo(LocalUserNum, *UserId);
						if (UserData.IsValid())
						{
							FEEOSUserInfo Info;
							Info.EpicAccountId = UserData->GetUserId()->ToString();
							Info.DisplayName = UserData->GetDisplayName();
							UserData->GetUserAttribute(TEXT("country"), Info.Country);
							UserData->GetUserAttribute(TEXT("preferredLanguage"), Info.PreferredLanguage);
							CachedSearchResults.Add(Info);
						}
					}
				}
			}

			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUserInfoSubsystem: Batch query %s — %d results"),
				bWasSuccessful ? TEXT("succeeded") : TEXT("failed"), CachedSearchResults.Num());
			OnUserSearchComplete.Broadcast(bWasSuccessful, CachedSearchResults);
		});

	UserInterface->QueryUserInfo(0, UserIds);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUserInfoSubsystem::QueryUserInfoBatch — Querying %d users..."), EpicAccountIds.Num());
}

// ── External Account Mapping ─────────────────────────────────────────────────

void UEEOSUserInfoSubsystem::QueryExternalAccountMappings(const TArray<FString>& ExternalAccountIds, EEOSExternalCredentialType AccountType)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryExternalAccountMappings"));
		OnExternalMappingsQueried.Broadcast(false, TArray<FEEOSExternalAccountMapping>());
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSUserInfoSubsystem::QueryExternalAccountMappings — Platform handle not available"));
		OnExternalMappingsQueried.Broadcast(false, TArray<FEEOSExternalAccountMapping>());
		return;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSUserInfoSubsystem::QueryExternalAccountMappings — Connect interface not available"));
		OnExternalMappingsQueried.Broadcast(false, TArray<FEEOSExternalAccountMapping>());
		return;
	}

	// Get the logged-in Product User ID
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	FUniqueNetIdPtr LocalUserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LocalUserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSUserInfoSubsystem::QueryExternalAccountMappings — No logged-in user"));
		OnExternalMappingsQueried.Broadcast(false, TArray<FEEOSExternalAccountMapping>());
		return;
	}

	FString UserIdStr = LocalUserId->ToString();
	EOS_ProductUserId LocalPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*UserIdStr));

	// Map our credential type to EOS external account type
	EOS_EExternalAccountType EosAccountType = EOS_EExternalAccountType::EOS_EAT_EPIC;
	switch (AccountType)
	{
	case EEOSExternalCredentialType::Steam:     EosAccountType = EOS_EExternalAccountType::EOS_EAT_STEAM; break;
	case EEOSExternalCredentialType::PSN:        EosAccountType = EOS_EExternalAccountType::EOS_EAT_PSN; break;
	case EEOSExternalCredentialType::XboxLive:   EosAccountType = EOS_EExternalAccountType::EOS_EAT_XBL; break;
	case EEOSExternalCredentialType::Nintendo:   EosAccountType = EOS_EExternalAccountType::EOS_EAT_NINTENDO; break;
	case EEOSExternalCredentialType::Discord:    EosAccountType = EOS_EExternalAccountType::EOS_EAT_DISCORD; break;
	case EEOSExternalCredentialType::Apple:      EosAccountType = EOS_EExternalAccountType::EOS_EAT_APPLE; break;
	default:                                    EosAccountType = EOS_EExternalAccountType::EOS_EAT_EPIC; break;
	}

	// Build the array of external account ID strings for the EOS SDK (const char**)
	TArray<FTCHARToUTF8> ConvertedIds;
	TArray<const char*> RawIdPtrs;
	for (const FString& ExtId : ExternalAccountIds)
	{
		ConvertedIds.Emplace(*ExtId);
		RawIdPtrs.Add(ConvertedIds.Last().Get());
	}

	EOS_Connect_QueryExternalAccountMappingsOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_QUERYEXTERNALACCOUNTMAPPINGS_API_LATEST;
	Options.LocalUserId = LocalPUID;
	Options.AccountIdType = EosAccountType;
	Options.ExternalAccountIds = RawIdPtrs.GetData();
	Options.ExternalAccountIdCount = static_cast<uint32_t>(RawIdPtrs.Num());

	// We need to keep the input IDs and account type for the callback's GetExternalAccountMapping calls
	struct FQueryMappingsContext
	{
		UEEOSUserInfoSubsystem* Self;
		TArray<FString> QueriedIds;
		EOS_EExternalAccountType AccountType;
	};

	FQueryMappingsContext* Ctx = new FQueryMappingsContext();
	Ctx->Self = this;
	Ctx->QueriedIds = ExternalAccountIds;
	Ctx->AccountType = EosAccountType;

	EOS_Connect_QueryExternalAccountMappings(ConnectHandle, &Options, Ctx,
		[](const EOS_Connect_QueryExternalAccountMappingsCallbackInfo* Data)
		{
			FQueryMappingsContext* Ctx = static_cast<FQueryMappingsContext*>(Data->ClientData);
			if (!Ctx) return;

			UEEOSUserInfoSubsystem* Self = Ctx->Self;
			TArray<FString> QueriedIds = MoveTemp(Ctx->QueriedIds);
			EOS_EExternalAccountType AccountType = Ctx->AccountType;
			delete Ctx;

			if (!Self) return;

			Self->CachedMappings.Empty();

			if (Data->ResultCode == EOS_EResult::EOS_Success)
			{
				EOS_HPlatform PlatformHandle = Self->GetPlatformHandle();
				EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);

				// Now retrieve mappings for each queried external ID
				for (const FString& ExtId : QueriedIds)
				{
					FTCHARToUTF8 ExtIdAnsi(*ExtId);

					EOS_Connect_GetExternalAccountMappingsOptions GetOptions = {};
					GetOptions.ApiVersion = EOS_CONNECT_GETEXTERNALACCOUNTMAPPING_API_LATEST;
					GetOptions.LocalUserId = Data->LocalUserId;
					GetOptions.AccountIdType = AccountType;
					GetOptions.TargetExternalUserId = ExtIdAnsi.Get();

					EOS_ProductUserId MappedPUID = EOS_Connect_GetExternalAccountMapping(ConnectHandle, &GetOptions);
					if (EOS_ProductUserId_IsValid(MappedPUID))
					{
						char PUIDBuffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
						int32_t BufferSize = sizeof(PUIDBuffer);
						FString PUIDStr;
						if (EOS_ProductUserId_ToString(MappedPUID, PUIDBuffer, &BufferSize) == EOS_EResult::EOS_Success)
						{
							PUIDStr = ANSI_TO_TCHAR(PUIDBuffer);
						}

						FEEOSExternalAccountMapping Mapping;
						Mapping.ExternalAccountId = ExtId;
						Mapping.ProductUserId = PUIDStr;
						// ExternalAccountType and DisplayName would come from a separate query
						Self->CachedMappings.Add(Mapping);
					}
				}

				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUserInfoSubsystem: External account mapping query succeeded — %d mappings found"),
					Self->CachedMappings.Num());
				Self->OnExternalMappingsQueried.Broadcast(true, Self->CachedMappings);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSUserInfoSubsystem::QueryExternalAccountMappings — Failed: %s"),
					ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
				Self->OnExternalMappingsQueried.Broadcast(false, Self->CachedMappings);
			}
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUserInfoSubsystem::QueryExternalAccountMappings — Querying %d external IDs via EOS Connect..."), ExternalAccountIds.Num());
}

// ── Queries ──────────────────────────────────────────────────────────────────

FEEOSUserInfo UEEOSUserInfoSubsystem::GetCachedUserInfo() const
{
	return CachedUserInfo;
}

TArray<FEEOSUserInfo> UEEOSUserInfoSubsystem::GetCachedSearchResults() const
{
	return CachedSearchResults;
}

TArray<FEEOSExternalAccountMapping> UEEOSUserInfoSubsystem::GetCachedMappings() const
{
	return CachedMappings;
}
