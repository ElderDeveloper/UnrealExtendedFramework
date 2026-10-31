// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Shared/EEOSLog.h"
#include "EFLog.h"
#include "EOSShared.h"
#include "IEOSSDKManager.h"
#include "Misc/ConfigCacheIni.h"
#if WITH_EOS_SDK && !NO_LOGGING && EF_LOG_ENABLED
#include "Async/Async.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeRWLock.h"
#include "Templates/SharedPointer.h"
#include "eos_logging.h"
#endif

DEFINE_LOG_CATEGORY(LogExtendedEOS);

#if WITH_EOS_SDK && !NO_LOGGING && EF_LOG_ENABLED
namespace
{
	// EOSShared does not export its category object. Unreal synchronizes handles by name.
	FLogCategory<ELogVerbosity::Log, ELogVerbosity::All> SDKPassthrough(TEXT("LogEOSSDK"));

	// EOSShared reads its suppression rules from this section of the engine ini.
	const TCHAR* const SuppressionSection = TEXT("EOSSDK");

	// The SDK calls RouteSDKLog from its own threads as well (RTC audio capture), so the callback
	// must not read GConfig. The lists are read on the game thread into an immutable snapshot, and
	// a reload swaps the pointer; a reader keeps whichever snapshot it copied.
	struct FSuppressionLists
	{
		TArray<FString> Strings;
		TArray<FString> Categories;
		TArray<FString> Strings_VeryVerbose;
		TArray<FString> Categories_VeryVerbose;
	};
	FRWLock SuppressionLock;
	TSharedPtr<const FSuppressionLists, ESPMode::ThreadSafe> SuppressionLists;

	// Game thread only.
	void LoadSuppressionLists()
	{
		const TSharedRef<FSuppressionLists, ESPMode::ThreadSafe> Lists = MakeShared<FSuppressionLists, ESPMode::ThreadSafe>();
		if (GConfig)
		{
			GConfig->GetArray(SuppressionSection, TEXT("SuppressedLogStrings"), Lists->Strings, GEngineIni);
			GConfig->GetArray(SuppressionSection, TEXT("SuppressedLogCategories"), Lists->Categories, GEngineIni);
			GConfig->GetArray(SuppressionSection, TEXT("SuppressedLogStrings_VeryVerbose"), Lists->Strings_VeryVerbose, GEngineIni);
			GConfig->GetArray(SuppressionSection, TEXT("SuppressedLogCategories_VeryVerbose"), Lists->Categories_VeryVerbose, GEngineIni);
		}
		// Declared outside the lock so the old snapshot, if this was its last reference, is freed after it.
		TSharedPtr<const FSuppressionLists, ESPMode::ThreadSafe> Previous;
		{
			FWriteScopeLock Lock(SuppressionLock);
			Previous = SuppressionLists;
			SuppressionLists = Lists;
		}
	}

	TSharedPtr<const FSuppressionLists, ESPMode::ThreadSafe> GetSuppressionLists()
	{
		FReadScopeLock Lock(SuppressionLock);
		return SuppressionLists;
	}

	bool MatchesAny(const TArray<FString>& Patterns, const FString& Value)
	{
		return Patterns.ContainsByPredicate([&Value](const FString& Pattern) { return Value.Contains(Pattern); });
	}

	// Mirrors EOSShared's file-local ConvertLogLevel (EOSSDKManager.cpp).
	EOS_ELogLevel ToEOSLogLevel(ELogVerbosity::Type Verbosity)
	{
		switch (static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask))
		{
		case ELogVerbosity::NoLogging:		return EOS_ELogLevel::EOS_LOG_Off;
		case ELogVerbosity::Fatal:			return EOS_ELogLevel::EOS_LOG_Fatal;
		case ELogVerbosity::Error:			return EOS_ELogLevel::EOS_LOG_Error;
		case ELogVerbosity::Warning:		return EOS_ELogLevel::EOS_LOG_Warning;
		case ELogVerbosity::Verbose:		return EOS_ELogLevel::EOS_LOG_Verbose;
		case ELogVerbosity::VeryVerbose:	return EOS_ELogLevel::EOS_LOG_VeryVerbose;
		case ELogVerbosity::Display:
		case ELogVerbosity::Log:
		default:							return EOS_ELogLevel::EOS_LOG_Info;
		}
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
			const TSharedPtr<const FSuppressionLists, ESPMode::ThreadSafe> Lists = GetSuppressionLists();
			if (Lists)
			{
				if (Level < EOS_ELogLevel::EOS_LOG_VeryVerbose &&
					(MatchesAny(Lists->Strings_VeryVerbose, Text) ||
					 MatchesAny(Lists->Categories_VeryVerbose, Category)))
				{
					Level = EOS_ELogLevel::EOS_LOG_VeryVerbose;
				}
				if (Level < EOS_ELogLevel::EOS_LOG_Info &&
					(MatchesAny(Lists->Strings, Text) ||
					 MatchesAny(Lists->Categories, Category)))
				{
					Level = EOS_ELogLevel::EOS_LOG_Info;
				}
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
		// RouteSDKLog matches against this snapshot, so load it before the callback can be installed.
		LoadSuppressionLists();
		ConfigChangedHandle = FCoreDelegates::TSOnConfigSectionsChanged().AddRaw(this, &FExtendedEOSSharedModule::OnConfigSectionsChanged);

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
		FCoreDelegates::TSOnConfigSectionsChanged().Remove(ConfigChangedHandle);
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
		if (Result != EOS_EResult::EOS_Success)
		{
			return;
		}

		const EOS_EResult CallbackResult = EOS_Logging_SetCallback(&RouteSDKLog);
		bCallbackInstalled = CallbackResult == EOS_EResult::EOS_Success;
		if (!bCallbackInstalled)
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("Could not route EOS RTC logs: %s"), *LexToString(CallbackResult));
		}

		// Constructing SDKPassthrough during this DLL's static init moves it from zero-initialised
		// NoLogging to Log, and that broadcast makes EOSShared set the SDK to Info when the SDK is
		// already up. Linking the category to LogEOSSDK then copies the configured verbosity back
		// without a broadcast, so Unreal would keep e.g. Verbose while the SDK filters at Info.
		const ELogVerbosity::Type Verbosity = SDKPassthrough.GetVerbosity();
		const EOS_EResult LevelResult = EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_ALL_CATEGORIES, ToEOSLogLevel(Verbosity));
		if (LevelResult != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("Could not re-apply the EOS SDK log level %s: %s"), ToString(Verbosity), *LexToString(LevelResult));
		}
	}

	void OnConfigSectionsChanged(const FString& IniFilename, const TSet<FString>& SectionNames)
	{
		if (IniFilename != GEngineIni || !SectionNames.Contains(SuppressionSection))
		{
			return;
		}
		if (IsInGameThread())
		{
			LoadSuppressionLists();
		}
		else
		{
			// The lists are read from GConfig on the game thread only.
			AsyncTask(ENamedThreads::GameThread, [] { LoadSuppressionLists(); });
		}
	}

	FDelegateHandle PostInitializeHandle;
	FDelegateHandle ConfigChangedHandle;
	bool bCallbackInstalled = false;
#endif
};

IMPLEMENT_MODULE(FExtendedEOSSharedModule, ExtendedEOSShared);
