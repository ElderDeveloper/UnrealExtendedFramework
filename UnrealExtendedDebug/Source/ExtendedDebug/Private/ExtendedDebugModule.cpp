// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedDebugModule.h"

#include "Analytics/EFAnalyticsSubsystem.h"
#include "EDbgSubsystem.h"
#include "EFFoundationVersion.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "Platform/EFPlatformSubsystem.h"
#include "Status/EFServiceStatus.h"

// Fails the build with a readable message if this copy is paired with an older foundation.
EF_REQUIRE_FOUNDATION_VERSION(1, 2, "UnrealExtendedDebug");

DEFINE_LOG_CATEGORY(LogExtendedDebug);

namespace EDbgBuiltInCheats
{
	const FName TimeDilation(TEXT("TimeDilation"));
	const FName ShowStatus(TEXT("ShowStatus"));
	const FName UnlockAchievement(TEXT("UnlockAchievement"));
	const FName AnalyticsConsent(TEXT("AnalyticsConsent"));
	const FName ToggleWatches(TEXT("ToggleWatches"));

	FEDbgCheatInfo Make(FName Name, const TCHAR* Help, const TCHAR* ArgumentHint, bool bRunOnServer)
	{
		FEDbgCheatInfo Info;
		Info.Name = Name;
		Info.Help = Help;
		Info.Category = TEXT("Extended");
		Info.ArgumentHint = ArgumentHint;
		Info.bRunOnServer = bRunOnServer;
		return Info;
	}
}

void FExtendedDebugModule::StartupModule()
{
#if EXTENDED_DEBUG_ENABLED
	using namespace EDbgBuiltInCheats;

	UEDbgSubsystem::RegisterGlobalCheat(Make(TimeDilation, TEXT("Slows down or speeds up the game (1 is normal)."), TEXT("<scale>"), true),
		[](const FEDbgCheatContext& Context)
		{
			if (Context.World)
			{
				UGameplayStatics::SetGlobalTimeDilation(Context.World, FMath::Clamp(Context.GetFloat(0, 1.f), 0.0001f, 20.f));
			}
		});

	UEDbgSubsystem::RegisterGlobalCheat(Make(ShowStatus, TEXT("Writes Extended.Status to the log."), TEXT(""), false),
		[](const FEDbgCheatContext&)
		{
			TArray<FString> Lines;
			FEFServiceStatusRegistry::FormatReport(true).ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);
			for (const FString& Line : Lines)
			{
				UE_LOG(LogExtendedDebug, Display, TEXT("%s"), *Line);
			}
		});

	UEDbgSubsystem::RegisterGlobalCheat(Make(UnlockAchievement, TEXT("Unlocks an achievement through the platform layer (game id)."), TEXT("<achievement id>"), false),
		[](const FEDbgCheatContext& Context)
		{
			UEFPlatformSubsystem* Platform = UEFPlatformSubsystem::Get(Context.World);
			const FString Id = Context.GetString(0);
			if (Platform && !Id.IsEmpty())
			{
				Platform->UnlockAchievement(FName(*Id), [Id](const FEFResult& Result)
				{
					UE_LOG(LogExtendedDebug, Display, TEXT("UnlockAchievement %s: %s"), *Id, *Result.ToString());
				});
			}
		});

	UEDbgSubsystem::RegisterGlobalCheat(Make(AnalyticsConsent, TEXT("Sets the analytics consent: Granted, Denied or Unknown."), TEXT("<Granted|Denied|Unknown>"), false),
		[](const FEDbgCheatContext& Context)
		{
			if (UEFAnalyticsSubsystem* Analytics = UEFAnalyticsSubsystem::Get(Context.World))
			{
				const FString Value = Context.GetString(0);
				Analytics->SetConsent(Value.Equals(TEXT("Granted"), ESearchCase::IgnoreCase) ? EEFAnalyticsConsent::Granted
					: Value.Equals(TEXT("Denied"), ESearchCase::IgnoreCase) ? EEFAnalyticsConsent::Denied : EEFAnalyticsConsent::Unknown);
			}
		});

	UEDbgSubsystem::RegisterGlobalCheat(Make(ToggleWatches, TEXT("Shows or hides the watch overlay."), TEXT(""), false),
		[](const FEDbgCheatContext& Context)
		{
			if (UEDbgSubsystem* Debug = UEDbgSubsystem::Get(Context.World))
			{
				Debug->SetWatchesVisible(!Debug->AreWatchesVisible());
			}
		});
#endif
}

void FExtendedDebugModule::ShutdownModule()
{
#if EXTENDED_DEBUG_ENABLED
	using namespace EDbgBuiltInCheats;
	for (const FName& Name : { TimeDilation, ShowStatus, UnlockAchievement, AnalyticsConsent, ToggleWatches })
	{
		UEDbgSubsystem::UnregisterGlobalCheat(Name);
	}
#endif
}

IMPLEMENT_MODULE(FExtendedDebugModule, ExtendedDebug)
