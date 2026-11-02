// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedGameplay.h"

#include "EFFoundationVersion.h"
#include "Status/EFServiceStatus.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementEvaluator.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementSettings.h"

// Asset preloading (EFAssets) arrived in foundation 1.1.
EF_REQUIRE_FOUNDATION_VERSION(1, 1, "UnrealExtendedGameplay");

#define LOCTEXT_NAMESPACE "FUnrealExtendedGameplayModule"

void FUnrealExtendedGameplayModule::StartupModule()
{
	if (GetDefault<UEGPlacementSettings>()->bReportStatus)
	{
		PlacementStatusHandle = FEFServiceStatusRegistry::Register(TEXT("Placement"), []
		{
			FEFServiceStatus Status;
			Status.Service = TEXT("Placement");
			Status.State = EEFServiceState::Ready;
			const double AverageMs = FEGPlacementStats::Evaluations > 0 ? FEGPlacementStats::EvaluationSeconds * 1000.0 / double(FEGPlacementStats::Evaluations) : 0.0;
			const int64 Lookups = FEGPlacementStats::CacheHits + FEGPlacementStats::CacheMisses;
			Status.Summary = FString::Printf(TEXT("%d active sessions, %lld evaluations (%.3f ms average)"), FEGPlacementStats::ActiveSessions, FEGPlacementStats::Evaluations, AverageMs);
			Status.Details.Add(TEXT("Cache hit rate"), Lookups > 0 ? FString::Printf(TEXT("%.1f %%"), 100.0 * double(FEGPlacementStats::CacheHits) / double(Lookups)) : FString(TEXT("-")));
			Status.Details.Add(TEXT("Commits dispatched"), FString::Printf(TEXT("%lld"), FEGPlacementStats::Commits));
			return Status;
		});
	}
}

void FUnrealExtendedGameplayModule::ShutdownModule()
{
	if (PlacementStatusHandle != INDEX_NONE)
	{
		FEFServiceStatusRegistry::Unregister(PlacementStatusHandle);
		PlacementStatusHandle = INDEX_NONE;
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealExtendedGameplayModule, UnrealExtendedGameplay)
