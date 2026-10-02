// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Shared/EEOSLog.h"
#include "EFLog.h"
#include "EOSShared.h"
#include "IEOSSDKManager.h"
#include "Misc/ConfigCacheIni.h"
#if WITH_EOS_SDK && !NO_LOGGING && EF_LOG_ENABLED
#include "eos_logging.h"
#endif

DEFINE_LOG_CATEGORY(LogExtendedEOS);

#if WITH_EOS_SDK && !NO_LOGGING && EF_LOG_ENABLED
namespace
{
	// EOSShared does not export its category object. Unreal synchronizes handles by name.
	FLogCategory<ELogVerbosity::Log, ELogVerbosity::All> SDKPassthrough(TEXT("LogEOSSDK"));
	bool MatchesSuppression(const TCHAR* Key, const FString& Text)
	{
		TArray<FString> Patterns;
		GConfig->GetArray(TEXT("EOSSDK"), Key, Patterns, GEngineIni);
		return Patterns.ContainsByPredicate([&Text](const FString& Pattern) { return Text.Contains(Pattern); });
	}

	void EOS_CALL RouteSDKLog(const EOS_LogMessage* Message)
	{
		if (!Message || !Message->Category || !Message->Message)
		{
			return;
		}
		const FString Category = FString(UTF8_TO_TCHAR(Message->Category)).TrimStartAndEnd();
		const FString Text = FString(UTF8_TO_TCHAR(Message->Message)).TrimStartAndEnd();
		const bool bRTC = Category.StartsWith(TEXT("LogEOSRTC"), ESearchCase::CaseSensitive);
		EOS_ELogLevel Level = Message->Level;
		// Preserve EOSShared's runtime suppression rules for SDK messages left in Unreal's log.
		if (!bRTC)
		{
			if (Level < EOS_ELogLevel::EOS_LOG_VeryVerbose &&
				(MatchesSuppression(TEXT("SuppressedLogStrings_VeryVerbose"), Text) ||
				 MatchesSuppression(TEXT("SuppressedLogCategories_VeryVerbose"), Category)))
			{
				Level = EOS_ELogLevel::EOS_LOG_VeryVerbose;
			}
			if (Level < EOS_ELogLevel::EOS_LOG_Info &&
				(MatchesSuppression(TEXT("SuppressedLogStrings"), Text) ||
				 MatchesSuppression(TEXT("SuppressedLogCategories"), Category)))
			{
				Level = EOS_ELogLevel::EOS_LOG_Info;
			}
		}
		// EF_LOG has no Fatal level. Keep the engine's fatal/crash behavior even for RTC.
		if (Level == EOS_ELogLevel::EOS_LOG_Fatal)
		{
			UE_LOG(SDKPassthrough, Fatal, TEXT("%s: %s"), *Category, *Text);
			return;
		}
#define EEOS_ROUTE_LEVEL(SDKLevel, UELevel) \
		case EOS_ELogLevel::SDKLevel: \
			if (bRTC) { EF_LOG(EOSRTC, UELevel, TEXT("%s: %s"), *Category, *Text); } \
			else { UE_LOG(SDKPassthrough, UELevel, TEXT("%s: %s"), *Category, *Text); } \
			break
		switch (Level)
		{
			EEOS_ROUTE_LEVEL(EOS_LOG_Error, Error);
			EEOS_ROUTE_LEVEL(EOS_LOG_Warning, Warning);
			EEOS_ROUTE_LEVEL(EOS_LOG_Info, Log);
			EEOS_ROUTE_LEVEL(EOS_LOG_Verbose, Verbose);
			EEOS_ROUTE_LEVEL(EOS_LOG_VeryVerbose, VeryVerbose);
			default: break;
		}
#undef EEOS_ROUTE_LEVEL
	}
}
#endif

class FExtendedEOSSharedModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if WITH_EOS_SDK && !NO_LOGGING && EF_LOG_ENABLED
		// A link dependency does not start EOSShared; the manager must be registered first.
		FModuleManager::Get().LoadModuleChecked<IModuleInterface>(TEXT("EOSShared"));
		if (IEOSSDKManager* Manager = IEOSSDKManager::Get())
		{
			PostInitializeHandle = Manager->OnPostInitializeSDK.AddRaw(this, &FExtendedEOSSharedModule::OnSDKInitialized);
			if (Manager->IsInitialized())
			{
				OnSDKInitialized(EOS_EResult::EOS_Success);
			}
		}
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_EOS_SDK && !NO_LOGGING && EF_LOG_ENABLED
		if (IEOSSDKManager* Manager = IEOSSDKManager::Get())
		{
			Manager->OnPostInitializeSDK.Remove(PostInitializeHandle);
			if (bCallbackInstalled && Manager->IsInitialized())
			{
				// The SDK has no callback getter. Clear our pointer before the module can unload.
				EOS_Logging_SetCallback(nullptr);
			}
		}
		EFLog::Flush();
#endif
	}

	// EOS owns a process-wide callback; this module must remain loaded for the SDK lifetime.
	virtual bool SupportsDynamicReloading() override { return false; }

private:
#if WITH_EOS_SDK && !NO_LOGGING && EF_LOG_ENABLED
	void OnSDKInitialized(EOS_EResult Result)
	{
		if (Result == EOS_EResult::EOS_Success)
		{
			const EOS_EResult CallbackResult = EOS_Logging_SetCallback(&RouteSDKLog);
			bCallbackInstalled = CallbackResult == EOS_EResult::EOS_Success;
			if (!bCallbackInstalled)
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("Could not route EOS RTC logs: %s"), *LexToString(CallbackResult));
			}
		}
	}
	FDelegateHandle PostInitializeHandle;
	bool bCallbackInstalled = false;
#endif
};

IMPLEMENT_MODULE(FExtendedEOSSharedModule, ExtendedEOSShared);
