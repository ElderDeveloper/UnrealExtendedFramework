// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSSanctionsSubsystem.h"
#include "UnrealExtendedEOS.h"
#include "OnlineSubsystemUtils.h"

#include "eos_sanctions.h"
#include "eos_sanctions_types.h"
#include "eos_reports.h"
#include "eos_reports_types.h"
#include "eos_sdk.h"

void UEEOSSanctionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSSanctionsSubsystem::Deinitialize()
{
	CachedSanctions.Empty();
	Super::Deinitialize();
}

void UEEOSSanctionsSubsystem::QueryActiveSanctions(const FString& TargetUserId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryActiveSanctions"));
		OnSanctionsQueried.Broadcast(false, TArray<FEEOSSanction>());
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::QueryActiveSanctions — Platform handle not available"));
		OnSanctionsQueried.Broadcast(false, TArray<FEEOSSanction>());
		return;
	}

	EOS_HSanctions SanctionsHandle = EOS_Platform_GetSanctionsInterface(PlatformHandle);
	if (!SanctionsHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::QueryActiveSanctions — Sanctions interface not available"));
		OnSanctionsQueried.Broadcast(false, TArray<FEEOSSanction>());
		return;
	}

	// Get local user ID
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	FUniqueNetIdPtr LocalUserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	EOS_ProductUserId LocalPUID = nullptr;
	if (LocalUserId.IsValid())
	{
		LocalPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*LocalUserId->ToString()));
	}

	// Target user
	EOS_ProductUserId TargetPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*TargetUserId));

	EOS_Sanctions_QueryActivePlayerSanctionsOptions Options = {};
	Options.ApiVersion = EOS_SANCTIONS_QUERYACTIVEPLAYERSANCTIONS_API_LATEST;
	Options.TargetUserId = TargetPUID;
	Options.LocalUserId = LocalPUID;

	struct FQueryContext
	{
		UEEOSSanctionsSubsystem* Self;
		FString TargetId;
	};

	FQueryContext* Context = new FQueryContext();
	Context->Self = this;
	Context->TargetId = TargetUserId;

	EOS_Sanctions_QueryActivePlayerSanctions(SanctionsHandle, &Options, Context,
		[](const EOS_Sanctions_QueryActivePlayerSanctionsCallbackInfo* Data)
		{
			FQueryContext* Ctx = static_cast<FQueryContext*>(Data->ClientData);
			if (!Ctx) return;

			UEEOSSanctionsSubsystem* Self = Ctx->Self;
			FString TargetId = Ctx->TargetId;
			delete Ctx;

			if (!Self) return;

			Self->CachedSanctions.Empty();

			if (Data->ResultCode == EOS_EResult::EOS_Success)
			{
				EOS_HPlatform PlatformHandle = Self->GetPlatformHandle();
				EOS_HSanctions SanctionsHandle = EOS_Platform_GetSanctionsInterface(PlatformHandle);

				// Get the count of sanctions
				EOS_Sanctions_GetPlayerSanctionCountOptions CountOptions = {};
				CountOptions.ApiVersion = EOS_SANCTIONS_GETPLAYERSANCTIONCOUNT_API_LATEST;
				CountOptions.TargetUserId = Data->TargetUserId;

				uint32_t SanctionCount = EOS_Sanctions_GetPlayerSanctionCount(SanctionsHandle, &CountOptions);

				for (uint32_t i = 0; i < SanctionCount; ++i)
				{
					EOS_Sanctions_CopyPlayerSanctionByIndexOptions CopyOptions = {};
					CopyOptions.ApiVersion = EOS_SANCTIONS_COPYPLAYERSANCTIONBYINDEX_API_LATEST;
					CopyOptions.TargetUserId = Data->TargetUserId;
					CopyOptions.SanctionIndex = i;

					EOS_Sanctions_PlayerSanction* OutSanction = nullptr;
					EOS_EResult CopyResult = EOS_Sanctions_CopyPlayerSanctionByIndex(SanctionsHandle, &CopyOptions, &OutSanction);

					if (CopyResult == EOS_EResult::EOS_Success && OutSanction)
					{
						FEEOSSanction Sanction;
						Sanction.SanctionId = ANSI_TO_TCHAR(OutSanction->ReferenceId);
						Sanction.Action = ANSI_TO_TCHAR(OutSanction->Action);
						Sanction.TimePlaced = FDateTime::FromUnixTimestamp(OutSanction->TimePlaced);
						Sanction.TimeExpires = OutSanction->TimeExpires > 0
							? FDateTime::FromUnixTimestamp(OutSanction->TimeExpires)
							: FDateTime(0); // Permanent sanction
						Sanction.bIsPermanent = (OutSanction->TimeExpires == 0);

						Self->CachedSanctions.Add(Sanction);

						EOS_Sanctions_PlayerSanction_Release(OutSanction);
					}
				}

				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSanctionsSubsystem: Queried %d active sanctions for '%s'"),
					Self->CachedSanctions.Num(), *TargetId);
				Self->OnSanctionsQueried.Broadcast(true, Self->CachedSanctions);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSanctionsSubsystem::QueryActiveSanctions — Failed: %s"),
					ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
				Self->OnSanctionsQueried.Broadcast(false, Self->CachedSanctions);
			}
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSanctionsSubsystem::QueryActiveSanctions — Querying sanctions for '%s'..."), *TargetUserId);
}

void UEEOSSanctionsSubsystem::SendPlayerReport(const FString& TargetUserId, const FString& Reason, const FString& Message)
{
	SendPlayerReportWithCategory(TargetUserId, TEXT("Other"), Reason, Message);
}

void UEEOSSanctionsSubsystem::SendPlayerReportWithCategory(const FString& TargetUserId, const FString& Category, const FString& Reason, const FString& Message)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("SendPlayerReport"));
		OnPlayerReportSent.Broadcast(false);
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::SendPlayerReport — Platform handle not available"));
		OnPlayerReportSent.Broadcast(false);
		return;
	}

	EOS_HReports ReportsHandle = EOS_Platform_GetReportsInterface(PlatformHandle);
	if (!ReportsHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::SendPlayerReport — Reports interface not available"));
		OnPlayerReportSent.Broadcast(false);
		return;
	}

	// Get local reporter user ID
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	FUniqueNetIdPtr LocalUserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LocalUserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::SendPlayerReport — No logged-in user"));
		OnPlayerReportSent.Broadcast(false);
		return;
	}

	EOS_ProductUserId ReporterPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*LocalUserId->ToString()));
	EOS_ProductUserId ReportedPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*TargetUserId));

	// Map category string to EOS report category
	EOS_EPlayerReportsCategory EosCategory = EOS_EPlayerReportsCategory::EOS_PRC_Other;
	if (Category.Equals(TEXT("Cheating"), ESearchCase::IgnoreCase))
	{
		EosCategory = EOS_EPlayerReportsCategory::EOS_PRC_Cheating;
	}
	else if (Category.Equals(TEXT("Exploiting"), ESearchCase::IgnoreCase))
	{
		EosCategory = EOS_EPlayerReportsCategory::EOS_PRC_Exploiting;
	}
	else if (Category.Equals(TEXT("OffensiveProfile"), ESearchCase::IgnoreCase))
	{
		EosCategory = EOS_EPlayerReportsCategory::EOS_PRC_OffensiveProfile;
	}
	else if (Category.Equals(TEXT("VerbalAbuse"), ESearchCase::IgnoreCase))
	{
		EosCategory = EOS_EPlayerReportsCategory::EOS_PRC_VerbalAbuse;
	}
	else if (Category.Equals(TEXT("Scamming"), ESearchCase::IgnoreCase))
	{
		EosCategory = EOS_EPlayerReportsCategory::EOS_PRC_Scamming;
	}
	else if (Category.Equals(TEXT("Spamming"), ESearchCase::IgnoreCase))
	{
		EosCategory = EOS_EPlayerReportsCategory::EOS_PRC_Spamming;
	}

	// Build message string — combine Reason and Message
	FString CombinedMessage = FString::Printf(TEXT("[%s] %s"), *Reason, *Message);
	FTCHARToUTF8 MessageAnsi(*CombinedMessage);

	EOS_Reports_SendPlayerBehaviorReportOptions Options = {};
	Options.ApiVersion = EOS_REPORTS_SENDPLAYERBEHAVIORREPORT_API_LATEST;
	Options.ReporterUserId = ReporterPUID;
	Options.ReportedUserId = ReportedPUID;
	Options.Category = EosCategory;
	Options.Message = MessageAnsi.Get();
	Options.Context = nullptr; // Optional JSON context

	EOS_Reports_SendPlayerBehaviorReport(ReportsHandle, &Options, this,
		[](const EOS_Reports_SendPlayerBehaviorReportCompleteCallbackInfo* Data)
		{
			UEEOSSanctionsSubsystem* Self = static_cast<UEEOSSanctionsSubsystem*>(Data->ClientData);
			if (!Self) return;

			bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
			if (bSuccess)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSanctionsSubsystem: Player report sent successfully"));
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSanctionsSubsystem::SendPlayerReport — Failed: %s"),
					ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}
			Self->OnPlayerReportSent.Broadcast(bSuccess);
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSanctionsSubsystem::SendPlayerReport — Reporting '%s' [%s]: %s"), *TargetUserId, *Category, *Reason);
}

void UEEOSSanctionsSubsystem::AppealSanction(const FString& SanctionId, EEOSSanctionAppealType AppealType, const FString& AppealMessage)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("AppealSanction"));
		OnSanctionAppealSent.Broadcast(false);
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::AppealSanction — Platform handle not available"));
		OnSanctionAppealSent.Broadcast(false);
		return;
	}

	EOS_HSanctions SanctionsHandle = EOS_Platform_GetSanctionsInterface(PlatformHandle);
	if (!SanctionsHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::AppealSanction — Sanctions interface not available"));
		OnSanctionAppealSent.Broadcast(false);
		return;
	}

	// Get local user ID
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	FUniqueNetIdPtr LocalUserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LocalUserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::AppealSanction — No logged-in user"));
		OnSanctionAppealSent.Broadcast(false);
		return;
	}

	EOS_ProductUserId LocalPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*LocalUserId->ToString()));

	// Map our appeal type to the EOS SDK reason code
	EOS_ESanctionAppealReason EosReason = EOS_ESanctionAppealReason::EOS_SAR_Invalid;
	switch (AppealType)
	{
	case EEOSSanctionAppealType::IncorrectSanction:   EosReason = EOS_ESanctionAppealReason::EOS_SAR_IncorrectSanction; break;
	case EEOSSanctionAppealType::CompromisedAccount:  EosReason = EOS_ESanctionAppealReason::EOS_SAR_CompromisedAccount; break;
	case EEOSSanctionAppealType::UnfairPunishment:    EosReason = EOS_ESanctionAppealReason::EOS_SAR_UnfairPunishment; break;
	case EEOSSanctionAppealType::AppealForForgiveness: EosReason = EOS_ESanctionAppealReason::EOS_SAR_AppealForForgiveness; break;
	}

	FTCHARToUTF8 ReferenceIdAnsi(*SanctionId);

	EOS_Sanctions_CreatePlayerSanctionAppealOptions Options = {};
	Options.ApiVersion = EOS_SANCTIONS_CREATEPLAYERSANCTIONAPPEAL_API_LATEST;
	Options.LocalUserId = LocalPUID;
	Options.Reason = EosReason;
	Options.ReferenceId = ReferenceIdAnsi.Get();

	EOS_Sanctions_CreatePlayerSanctionAppeal(SanctionsHandle, &Options, this,
		[](const EOS_Sanctions_CreatePlayerSanctionAppealCallbackInfo* Data)
		{
			UEEOSSanctionsSubsystem* Self = static_cast<UEEOSSanctionsSubsystem*>(Data->ClientData);
			if (!Self) return;

			bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
			if (bSuccess)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSanctionsSubsystem: Sanction appeal submitted for '%s'"),
					ANSI_TO_TCHAR(Data->ReferenceId));
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSanctionsSubsystem::AppealSanction — Failed: %s"),
					ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}
			Self->OnSanctionAppealSent.Broadcast(bSuccess);
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSanctionsSubsystem::AppealSanction — Appealing sanction '%s' (type=%d)..."),
		*SanctionId, static_cast<int32>(AppealType));
}

TArray<FEEOSSanction> UEEOSSanctionsSubsystem::GetCachedSanctions() const
{
	return CachedSanctions;
}

bool UEEOSSanctionsSubsystem::HasActiveSanctions() const
{
	return CachedSanctions.Num() > 0;
}

bool UEEOSSanctionsSubsystem::GetSanctionById(const FString& SanctionId, FEEOSSanction& OutSanction) const
{
	for (const auto& Sanction : CachedSanctions)
	{
		if (Sanction.SanctionId == SanctionId)
		{
			OutSanction = Sanction;
			return true;
		}
	}
	return false;
}

int32 UEEOSSanctionsSubsystem::GetSanctionCount() const
{
	return CachedSanctions.Num();
}
