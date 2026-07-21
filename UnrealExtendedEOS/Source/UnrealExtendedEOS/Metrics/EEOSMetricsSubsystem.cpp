// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSMetricsSubsystem.h"
#include "Shared/EEOSSettings.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "UnrealExtendedEOS.h"

#if WITH_EOS_SDK
#include "IEOSSDKManager.h"
#include "eos_metrics.h"
#include "eos_metrics_types.h"
#include "eos_auth.h"
#include "eos_auth_types.h"
#include "eos_sdk.h"
#endif

void UEEOSMetricsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSMetricsSubsystem::Deinitialize()
{
	if (bSessionActive)
	{
		EndPlayerSession();
	}
	Super::Deinitialize();
}

#if WITH_EOS_SDK
static EOS_HMetrics GetMetricsHandle()
{
	IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
	if (!SDKManager) return nullptr;

	TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
	if (Platforms.Num() == 0) return nullptr;

	EOS_HPlatform PlatformHandle = *Platforms[0];
	return EOS_Platform_GetMetricsInterface(PlatformHandle);
}
#endif

void UEEOSMetricsSubsystem::BeginPlayerSession(const FString& GameSessionId, const FString& ServerIp, const FString& GameMode)
{
	if (bSessionActive)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMetricsSubsystem::BeginPlayerSession — Session already active (ID=%s)"), *ActiveSessionId);
		return;
	}

#if WITH_EOS_SDK
	EOS_HMetrics MetricsHandle = GetMetricsHandle();
	if (MetricsHandle)
	{
		// Get Epic Account ID
		EOS_EpicAccountId EpicAccountId = nullptr;
		IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
		if (SDKManager)
		{
			TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
			if (Platforms.Num() > 0)
			{
				EOS_HPlatform PlatformHandle = *Platforms[0];
				EOS_HAuth AuthHandle = EOS_Platform_GetAuthInterface(PlatformHandle);
				if (AuthHandle)
				{
					int32_t NumAccounts = EOS_Auth_GetLoggedInAccountsCount(AuthHandle);
					if (NumAccounts > 0)
					{
						EpicAccountId = EOS_Auth_GetLoggedInAccountByIndex(AuthHandle, 0);
					}
				}
			}
		}

		// Resolve the identity to open the session with. Epic-account logins use the EAS id;
		// everyone else uses a STABLE per-user external id — the local PUID from the identity
		// net id (its ToString() is the composite "<EpicAccountId>|<ProductUserId>"). "local"
		// is only a last resort: a constant collapses every non-Epic player into one identity.
		bool bUseEpicAccount = false;
		FString AccountIdStr;
		if (EpicAccountId)
		{
			char EasBuffer[EOS_EPICACCOUNTID_MAX_LENGTH + 1];
			int32_t EasBufferSize = sizeof(EasBuffer);
			if (EOS_EpicAccountId_ToString(EpicAccountId, EasBuffer, &EasBufferSize) == EOS_EResult::EOS_Success)
			{
				bUseEpicAccount = true;
				AccountIdStr = ANSI_TO_TCHAR(EasBuffer);
			}
		}
		if (!bUseEpicAccount)
		{
			if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
			{
				IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
				if (Identity.IsValid())
				{
					FUniqueNetIdPtr LocalUserId = Identity->GetUniquePlayerId(0);
					if (LocalUserId.IsValid())
					{
						AccountIdStr = UEEOSBlueprintLibrary::ExtractProductUserId(LocalUserId->ToString());
					}
				}
			}
			if (AccountIdStr.IsEmpty())
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMetricsSubsystem::BeginPlayerSession — No Epic account and no Product User ID available; falling back to the constant external id \"local\" (all such players share one metrics identity)"));
				AccountIdStr = TEXT("local");
			}
		}

		EOS_Metrics_BeginPlayerSessionOptions Options = {};
		Options.ApiVersion = EOS_METRICS_BEGINPLAYERSESSION_API_LATEST;

		// DisplayName — use GameMode or a default
		FString DisplayName = GameMode.IsEmpty() ? TEXT("Player") : GameMode;
		FTCHARToUTF8 DisplayNameUtf8(*DisplayName);
		FTCHARToUTF8 ServerIpUtf8(*ServerIp);
		FTCHARToUTF8 SessionIdUtf8(*GameSessionId);
		FTCHARToUTF8 ExternalIdUtf8(*AccountIdStr);

		Options.DisplayName = DisplayNameUtf8.Get();
		Options.ControllerType = EOS_EUserControllerType::EOS_UCT_MouseKeyboard;
		Options.ServerIp = ServerIp.IsEmpty() ? nullptr : ServerIpUtf8.Get();
		Options.GameSessionId = GameSessionId.IsEmpty() ? nullptr : SessionIdUtf8.Get();

		if (bUseEpicAccount)
		{
			Options.AccountIdType = EOS_EMetricsAccountIdType::EOS_MAIT_Epic;
			Options.AccountId.Epic = EpicAccountId;
		}
		else
		{
			Options.AccountIdType = EOS_EMetricsAccountIdType::EOS_MAIT_External;
			Options.AccountId.External = ExternalIdUtf8.Get();
		}

		EOS_EResult Result = EOS_Metrics_BeginPlayerSession(MetricsHandle, &Options);
		if (Result == EOS_EResult::EOS_Success)
		{
			// Cache the exact identity used so EndPlayerSession closes THIS backend session
			// even if the login state changes mid-session
			bSessionBeganViaSDK = true;
			bSessionUsedEpicAccount = bUseEpicAccount;
			SessionAccountId = AccountIdStr;

			bSessionActive = true;
			SessionStartTime = FDateTime::UtcNow();
			ActiveSessionId = GameSessionId.IsEmpty() ? FGuid::NewGuid().ToString() : GameSessionId;
			ActiveGameMode = GameMode;
			ActiveServerIp = ServerIp;
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMetricsSubsystem: Player session started (SDK) — ID=%s"), *ActiveSessionId);
			OnPlayerSessionStarted.Broadcast(ActiveSessionId);
			return;
		}
		else
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMetricsSubsystem: EOS_Metrics_BeginPlayerSession failed — %s"),
				ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		}
	}
#endif

	// Fallback — local tracking
	bSessionActive = true;
	SessionStartTime = FDateTime::UtcNow();
	ActiveSessionId = GameSessionId.IsEmpty() ? FGuid::NewGuid().ToString() : GameSessionId;
	ActiveGameMode = GameMode;
	ActiveServerIp = ServerIp;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMetricsSubsystem: Player session started (local) — ID=%s"), *ActiveSessionId);
	OnPlayerSessionStarted.Broadcast(ActiveSessionId);
}

void UEEOSMetricsSubsystem::EndPlayerSession()
{
	if (!bSessionActive)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMetricsSubsystem::EndPlayerSession — No active session"));
		return;
	}

#if WITH_EOS_SDK
	// End with the EXACT identity cached at BeginPlayerSession — re-deriving the current
	// account here would orphan the backend session if the login state changed mid-session.
	// If the SDK Begin never succeeded there is no backend session to close.
	if (bSessionBeganViaSDK)
	{
		EOS_HMetrics MetricsHandle = GetMetricsHandle();
		if (MetricsHandle && !SessionAccountId.IsEmpty())
		{
			EOS_Metrics_EndPlayerSessionOptions Options = {};
			Options.ApiVersion = EOS_METRICS_ENDPLAYERSESSION_API_LATEST;

			// Must outlive the SDK call for the external branch
			FTCHARToUTF8 ExternalIdUtf8(*SessionAccountId);

			if (bSessionUsedEpicAccount)
			{
				Options.AccountIdType = EOS_EMetricsAccountIdType::EOS_MAIT_Epic;
				Options.AccountId.Epic = EOS_EpicAccountId_FromString(TCHAR_TO_ANSI(*SessionAccountId));
			}
			else
			{
				Options.AccountIdType = EOS_EMetricsAccountIdType::EOS_MAIT_External;
				Options.AccountId.External = ExternalIdUtf8.Get();
			}

			EOS_EResult Result = EOS_Metrics_EndPlayerSession(MetricsHandle, &Options);
			if (Result == EOS_EResult::EOS_Success)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMetricsSubsystem: Player session ended (SDK) — ID=%s"), *ActiveSessionId);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMetricsSubsystem: EOS_Metrics_EndPlayerSession failed — %s"),
					ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
			}
		}
		else
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMetricsSubsystem::EndPlayerSession — Metrics interface no longer available; backend session ID=%s could not be closed"), *ActiveSessionId);
		}
	}
#endif

	// Clear the cached Begin identity along with the session
	bSessionBeganViaSDK = false;
	bSessionUsedEpicAccount = false;
	SessionAccountId.Empty();

	FString EndedSessionId = ActiveSessionId;
	bSessionActive = false;
	ActiveSessionId.Empty();
	ActiveGameMode.Empty();
	ActiveServerIp.Empty();
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMetricsSubsystem: Player session ended — ID=%s"), *EndedSessionId);
	OnPlayerSessionEnded.Broadcast(EndedSessionId);
}

bool UEEOSMetricsSubsystem::IsSessionActive() const
{
	return bSessionActive;
}

FString UEEOSMetricsSubsystem::GetCurrentSessionId() const
{
	return ActiveSessionId;
}

float UEEOSMetricsSubsystem::GetSessionDuration() const
{
	if (!bSessionActive) return 0.0f;
	return (FDateTime::UtcNow() - SessionStartTime).GetTotalSeconds();
}
