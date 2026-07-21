// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSSanctionsSubsystem.h"
#include "Shared/EEOSBlueprintLibrary.h"
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
	// Normalize the target up front — accept a bare PUID or a composite
	// "<EpicAccountId>|<ProductUserId>" net-id string. Every broadcast (including
	// failures) carries this bare PUID for correlation; empty when unparseable.
	const FString TargetPUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(TargetUserId);

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryActiveSanctions"));
		OnSanctionsQueried.Broadcast(false, TargetPUIDStr, TArray<FEEOSSanction>());
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::QueryActiveSanctions — Platform handle not available"));
		OnSanctionsQueried.Broadcast(false, TargetPUIDStr, TArray<FEEOSSanction>());
		return;
	}

	EOS_HSanctions SanctionsHandle = EOS_Platform_GetSanctionsInterface(PlatformHandle);
	if (!SanctionsHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::QueryActiveSanctions — Sanctions interface not available"));
		OnSanctionsQueried.Broadcast(false, TargetPUIDStr, TArray<FEEOSSanction>());
		return;
	}

	// The target is required, and EOS_ProductUserId_FromString performs NO
	// validation, so guard on the extracted string.
	if (TargetPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::QueryActiveSanctions — Target '%s' has no Product User ID"), *TargetUserId);
		OnSanctionsQueried.Broadcast(false, FString(), TArray<FEEOSSanction>());
		return;
	}
	EOS_ProductUserId TargetPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*TargetPUIDStr));

	// Get local user ID — LocalUserId is optional for this query, so an absent
	// identity interface or PUID half just leaves it null (logged for visibility).
	// ToString() is the composite "<EpicAccountId>|<ProductUserId>".
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr Identity = EOSSub ? EOSSub->GetIdentityInterface() : nullptr;
	if (!Identity.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSanctionsSubsystem::QueryActiveSanctions — Identity interface not available; querying without a local user"));
	}
	FUniqueNetIdPtr LocalUserId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
	EOS_ProductUserId LocalPUID = nullptr;
	if (LocalUserId.IsValid())
	{
		const FString LocalPUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(LocalUserId->ToString());
		if (!LocalPUIDStr.IsEmpty())
		{
			LocalPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*LocalPUIDStr));
		}
	}

	EOS_Sanctions_QueryActivePlayerSanctionsOptions Options = {};
	Options.ApiVersion = EOS_SANCTIONS_QUERYACTIVEPLAYERSANCTIONS_API_LATEST;
	Options.TargetUserId = TargetPUID;
	Options.LocalUserId = LocalPUID;

	struct FQueryContext
	{
		TWeakObjectPtr<UEEOSSanctionsSubsystem> Self;
		FString TargetPuid; // bare PUID — broadcast for correlation
	};

	FQueryContext* Context = new FQueryContext();
	Context->Self = this;
	Context->TargetPuid = TargetPUIDStr;

	EOS_Sanctions_QueryActivePlayerSanctions(SanctionsHandle, &Options, Context,
		[](const EOS_Sanctions_QueryActivePlayerSanctionsCallbackInfo* Data)
		{
			TUniquePtr<FQueryContext> Ctx(static_cast<FQueryContext*>(Data->ClientData));
			if (!Ctx) return;

			UEEOSSanctionsSubsystem* Self = Ctx->Self.Get();
			FString TargetId = Ctx->TargetPuid;

			if (!Self) return;

			if (Data->ResultCode == EOS_EResult::EOS_Success)
			{
				EOS_HPlatform PlatformHandle = Self->GetPlatformHandle();
				EOS_HSanctions SanctionsHandle = PlatformHandle ? EOS_Platform_GetSanctionsInterface(PlatformHandle) : nullptr;
				if (!SanctionsHandle)
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSanctionsSubsystem::QueryActiveSanctions — Sanctions interface no longer available for '%s'"), *TargetId);
					Self->OnSanctionsQueried.Broadcast(false, TargetId, Self->CachedSanctions);
					return;
				}

				// FAIL-CLOSED: rebuild the cache ONLY once the query succeeded and
				// the results are readable — every failure path above/below keeps
				// the last-known-good list so a banned player never reads as clean.
				Self->CachedSanctions.Empty();

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
				Self->OnSanctionsQueried.Broadcast(true, TargetId, Self->CachedSanctions);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSanctionsSubsystem::QueryActiveSanctions — Failed for '%s': %s (cached sanctions retained — fail closed)"),
					*TargetId, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
				// The RETAINED last-known-good list rides along with bSuccess=false.
				Self->OnSanctionsQueried.Broadcast(false, TargetId, Self->CachedSanctions);
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
	// Normalize the reported player up front — every broadcast (including
	// failures) carries the bare PUID for correlation; empty when unparseable.
	const FString ReportedPUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(TargetUserId);

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("SendPlayerReport"));
		OnPlayerReportSent.Broadcast(false, ReportedPUIDStr);
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::SendPlayerReport — Platform handle not available"));
		OnPlayerReportSent.Broadcast(false, ReportedPUIDStr);
		return;
	}

	EOS_HReports ReportsHandle = EOS_Platform_GetReportsInterface(PlatformHandle);
	if (!ReportsHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::SendPlayerReport — Reports interface not available"));
		OnPlayerReportSent.Broadcast(false, ReportedPUIDStr);
		return;
	}

	// Get local reporter user ID (identity interface may be absent — fail observably)
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr Identity = EOSSub ? EOSSub->GetIdentityInterface() : nullptr;
	FUniqueNetIdPtr LocalUserId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
	if (!LocalUserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::SendPlayerReport — No logged-in user (or Identity interface unavailable)"));
		OnPlayerReportSent.Broadcast(false, ReportedPUIDStr);
		return;
	}

	// ToString() is the composite "<EpicAccountId>|<ProductUserId>" — extract the PUID halves.
	// EOS_ProductUserId_FromString performs NO validation, so guard on the strings instead.
	const FString ReporterPUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(LocalUserId->ToString());
	if (ReporterPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::SendPlayerReport — Logged-in user has no Product User ID (no Connect session)"));
		OnPlayerReportSent.Broadcast(false, ReportedPUIDStr);
		return;
	}

	if (ReportedPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::SendPlayerReport — Target '%s' has no Product User ID"), *TargetUserId);
		OnPlayerReportSent.Broadcast(false, FString());
		return;
	}

	EOS_ProductUserId ReporterPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*ReporterPUIDStr));
	EOS_ProductUserId ReportedPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*ReportedPUIDStr));

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

	struct FReportContext
	{
		TWeakObjectPtr<UEEOSSanctionsSubsystem> Self;
		FString ReportedPuid; // bare PUID — broadcast for correlation
	};

	FReportContext* Context = new FReportContext();
	Context->Self = this;
	Context->ReportedPuid = ReportedPUIDStr;

	EOS_Reports_SendPlayerBehaviorReport(ReportsHandle, &Options, Context,
		[](const EOS_Reports_SendPlayerBehaviorReportCompleteCallbackInfo* Data)
		{
			TUniquePtr<FReportContext> Ctx(static_cast<FReportContext*>(Data->ClientData));
			if (!Ctx) return;

			UEEOSSanctionsSubsystem* Self = Ctx->Self.Get();
			if (!Self) return;

			bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
			if (bSuccess)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSanctionsSubsystem: Player report sent successfully for '%s'"), *Ctx->ReportedPuid);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSanctionsSubsystem::SendPlayerReport — Failed for '%s': %s"),
					*Ctx->ReportedPuid, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}
			Self->OnPlayerReportSent.Broadcast(bSuccess, Ctx->ReportedPuid);
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSanctionsSubsystem::SendPlayerReport — Reporting '%s' [%s]: %s"), *TargetUserId, *Category, *Reason);
}

void UEEOSSanctionsSubsystem::AppealSanction(const FString& SanctionId, EEOSSanctionAppealType AppealType, const FString& AppealMessage)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("AppealSanction"));
		OnSanctionAppealSent.Broadcast(false, SanctionId);
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::AppealSanction — Platform handle not available"));
		OnSanctionAppealSent.Broadcast(false, SanctionId);
		return;
	}

	EOS_HSanctions SanctionsHandle = EOS_Platform_GetSanctionsInterface(PlatformHandle);
	if (!SanctionsHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::AppealSanction — Sanctions interface not available"));
		OnSanctionAppealSent.Broadcast(false, SanctionId);
		return;
	}

	// Get local user ID (identity interface may be absent — fail observably)
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr Identity = EOSSub ? EOSSub->GetIdentityInterface() : nullptr;
	FUniqueNetIdPtr LocalUserId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
	if (!LocalUserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::AppealSanction — No logged-in user (or Identity interface unavailable)"));
		OnSanctionAppealSent.Broadcast(false, SanctionId);
		return;
	}

	// ToString() is the composite "<EpicAccountId>|<ProductUserId>" — extract the PUID half.
	// EOS_ProductUserId_FromString performs NO validation, so guard on the string instead.
	const FString LocalPUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(LocalUserId->ToString());
	if (LocalPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSanctionsSubsystem::AppealSanction — Logged-in user has no Product User ID (no Connect session)"));
		OnSanctionAppealSent.Broadcast(false, SanctionId);
		return;
	}
	EOS_ProductUserId LocalPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*LocalPUIDStr));

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

	struct FAppealContext
	{
		TWeakObjectPtr<UEEOSSanctionsSubsystem> Self;
		FString SanctionId; // appealed sanction ReferenceId — broadcast for correlation
	};

	FAppealContext* Context = new FAppealContext();
	Context->Self = this;
	Context->SanctionId = SanctionId;

	EOS_Sanctions_CreatePlayerSanctionAppeal(SanctionsHandle, &Options, Context,
		[](const EOS_Sanctions_CreatePlayerSanctionAppealCallbackInfo* Data)
		{
			TUniquePtr<FAppealContext> Ctx(static_cast<FAppealContext*>(Data->ClientData));
			if (!Ctx) return;

			UEEOSSanctionsSubsystem* Self = Ctx->Self.Get();
			if (!Self) return;

			bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
			if (bSuccess)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSanctionsSubsystem: Sanction appeal submitted for '%s'"),
					ANSI_TO_TCHAR(Data->ReferenceId));
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSanctionsSubsystem::AppealSanction — Failed for '%s': %s"),
					*Ctx->SanctionId, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}
			Self->OnSanctionAppealSent.Broadcast(bSuccess, Ctx->SanctionId);
		});

	// NOTE: AppealMessage is NOT transmitted — the EOS SDK appeal API has no
	// message field (EOS_Sanctions_CreatePlayerSanctionAppealOptions carries only
	// LocalUserId, Reason and ReferenceId). Logged locally for audit purposes.
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSanctionsSubsystem::AppealSanction — Appealing sanction '%s' (type=%d). AppealMessage (local log only, not sent): %s"),
		*SanctionId, static_cast<int32>(AppealType), *AppealMessage);
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
