// EFLoadingScreenSubsystem.cpp
#include "EFLoadingScreenSubsystem.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "MoviePlayer.h"
#include "UObject/UObjectGlobals.h"
#include "UnrealExtendedFramework/Systems/Loading/Data/EFLoadingScreenSettings.h"
#include "UnrealExtendedFramework/Systems/Loading/Widget/SEFLoadingScreenWidget.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

DEFINE_LOG_CATEGORY(LogEFLoadingScreen);

void UEFLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// The context variant carries the owning game instance, which is what lets several PIE
	// instances in one process each answer only their own map loads.
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(
		this,
		&UEFLoadingScreenSubsystem::HandlePreLoadMap);

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this,
		&UEFLoadingScreenSubsystem::HandlePostLoadMap);
}

void UEFLoadingScreenSubsystem::Deinitialize()
{
	if (PreLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
		PreLoadMapHandle.Reset();
	}

	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	ActiveHolds.Reset();
	ReadinessQueries.Reset();
	bAwaitingPostLoadMap = false;
	HideLoadingScreen();
	Super::Deinitialize();
}

bool UEFLoadingScreenSubsystem::ShowLoadingScreen()
{
	// Whatever wait was running belongs to a world that is about to go away (or to a hold that
	// is now superseded by a real travel); the next PostLoadMap starts a fresh one.
	ClearWorldReadinessPolling();
	PrepareLoadingSession();

	const bool bShowingViewportLoadingScreen = ShowViewportLoadingScreen();
	const bool bMoviePlayerPrepared = SetupMoviePlayerLoadingScreen();
	return bShowingViewportLoadingScreen || bMoviePlayerPrepared;
}

void UEFLoadingScreenSubsystem::HideLoadingScreen()
{
	UE_LOG(LogEFLoadingScreen, Verbose, TEXT("HideLoadingScreen: stopping loading screen."));
	ClearWorldReadinessPolling();
	StopMoviePlayerLoadingScreen();
	HideViewportLoadingScreen();

	ActiveBackgroundTexture.Reset();
	ActiveLoadingTip = FText::GetEmpty();
	bLoadingTipResolved = false;
	bLoadingScreenRequested = false;
}

void UEFLoadingScreenSubsystem::HideAllLoadingScreens()
{
	HideLoadingScreen();
}

bool UEFLoadingScreenSubsystem::IsLoadingScreenVisible() const
{
	if (!bLoadingScreenRequested)
	{
		return false;
	}

	if (ViewportLoadingScreenWidget.IsValid())
	{
		return true;
	}

	return bUsingMoviePlayerLoadingScreen && IsMoviePlayerEnabled() && GetMoviePlayer()->IsMovieCurrentlyPlaying();
}

float UEFLoadingScreenSubsystem::GetMapTravelDelay() const
{
	return GetDefault<UEFLoadingScreenSettings>()->MapTravelDelay;
}

int32 UEFLoadingScreenSubsystem::RequestLoadingScreenHold(FName Reason)
{
	const int32 HoldId = NextHoldId++;
	ActiveHolds.Add(HoldId, Reason);
	UE_LOG(LogEFLoadingScreen, Log, TEXT("Loading screen: hold %d requested by '%s' (%d active)."),
		HoldId, *Reason.ToString(), ActiveHolds.Num());

	// A hold happens while the world is ticking, so only the viewport overlay applies; MoviePlayer
	// is for blocking loads and a travel that follows prepares it through ShowLoadingScreen.
	if (!IsLoadingScreenVisible())
	{
		PrepareLoadingSession();
		if (!ShowViewportLoadingScreen())
		{
			// No viewport (dedicated server, or too early in boot): the hold is still tracked so
			// a readiness test elsewhere respects it, there is just nothing to draw.
			UE_LOG(LogEFLoadingScreen, Verbose, TEXT("Loading screen: hold %d has no viewport to draw on."), HoldId);
		}
	}

	return HoldId;
}

void UEFLoadingScreenSubsystem::ReleaseLoadingScreenHold(int32 HoldId)
{
	FName Reason;
	if (!ActiveHolds.RemoveAndCopyValue(HoldId, Reason))
	{
		UE_LOG(LogEFLoadingScreen, Verbose, TEXT("Loading screen: release of unknown hold %d ignored."), HoldId);
		return;
	}

	UE_LOG(LogEFLoadingScreen, Log, TEXT("Loading screen: hold %d released by '%s' (%d remaining)."),
		HoldId, *Reason.ToString(), ActiveHolds.Num());

	if (ActiveHolds.Num() > 0 || !bLoadingScreenRequested)
	{
		return;
	}

	// A running wait re-asks IsWorldReadyToHideLoadingScreen every poll, and a travel in flight
	// starts its own wait from PostLoadMap — only a hold-only session needs one started here.
	if (bAwaitingPostLoadMap || IsWorldReadinessWaitActive())
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	BeginWorldReadinessWait(GameInstance ? GameInstance->GetWorld() : nullptr);
}

FDelegateHandle UEFLoadingScreenSubsystem::RegisterReadinessQuery(FEFLoadingScreenReadinessQuery Query)
{
	if (!Query.IsBound())
	{
		return FDelegateHandle();
	}

	const FDelegateHandle Handle = Query.GetHandle();
	ReadinessQueries.Add(MoveTemp(Query));
	return Handle;
}

void UEFLoadingScreenSubsystem::UnregisterReadinessQuery(FDelegateHandle Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}

	ReadinessQueries.RemoveAll([&Handle](const FEFLoadingScreenReadinessQuery& Query)
	{
		return Query.GetHandle() == Handle;
	});
}

void UEFLoadingScreenSubsystem::PrepareLoadingSession()
{
	// One texture per loading session so every widget created during this load shows the same image.
	if (!ActiveBackgroundTexture.IsValid())
	{
		ActiveBackgroundTexture.Reset(ResolveLoadingScreenTexture());
	}

	// Same for the tip, so the MoviePlayer and viewport widgets never show two different lines.
	if (!bLoadingTipResolved)
	{
		ActiveLoadingTip = ResolveLoadingTip();
		bLoadingTipResolved = true;
	}
}

TSharedRef<SWidget> UEFLoadingScreenSubsystem::CreateLoadingScreenWidget() const
{
	return SNew(SEFLoadingScreenWidget)
		.BackgroundTexture(ActiveBackgroundTexture.Get())
		.LoadingTip(ActiveLoadingTip);
}

UTexture2D* UEFLoadingScreenSubsystem::ResolveLoadingScreenTexture() const
{
	const UEFLoadingScreenSettings* Settings = GetDefault<UEFLoadingScreenSettings>();
	if (!Settings)
	{
		return nullptr;
	}

	// Try candidates in random order so a broken entry falls back to the other images instead of always BG fallback.
	TArray<int32> RemainingIndices;
	RemainingIndices.Reserve(Settings->BackgroundImages.Num());
	for (int32 Index = 0; Index < Settings->BackgroundImages.Num(); ++Index)
	{
		RemainingIndices.Add(Index);
	}

	while (RemainingIndices.Num() > 0)
	{
		const int32 Pick = FMath::RandRange(0, RemainingIndices.Num() - 1);
		const int32 ImageIndex = RemainingIndices[Pick];
		RemainingIndices.RemoveAtSwap(Pick);

		if (UTexture2D* Texture = Settings->BackgroundImages[ImageIndex].LoadSynchronous())
		{
			return Texture;
		}

		UE_LOG(LogEFLoadingScreen, Warning, TEXT("Loading screen: BackgroundImages[%d] ('%s') failed to load; trying another image."),
			ImageIndex,
			*Settings->BackgroundImages[ImageIndex].ToString());
	}

	return Settings->BackgroundImage.LoadSynchronous();
}

FText UEFLoadingScreenSubsystem::ResolveLoadingTip() const
{
	const UEFLoadingScreenSettings* Settings = GetDefault<UEFLoadingScreenSettings>();
	if (!Settings || !Settings->bShowLoadingTip)
	{
		return FText::GetEmpty();
	}

	// Collect non-blank entries first so a half-filled row in the settings array never shows as an empty line.
	TArray<int32> CandidateIndices;
	CandidateIndices.Reserve(Settings->LoadingTips.Num());
	for (int32 Index = 0; Index < Settings->LoadingTips.Num(); ++Index)
	{
		if (!Settings->LoadingTips[Index].IsEmptyOrWhitespace())
		{
			CandidateIndices.Add(Index);
		}
	}

	if (CandidateIndices.Num() == 0)
	{
		UE_LOG(LogEFLoadingScreen, Verbose, TEXT("Loading screen: no usable entries in LoadingTips; the tip line is hidden."));
		return FText::GetEmpty();
	}

	const int32 TipIndex = CandidateIndices[FMath::RandRange(0, CandidateIndices.Num() - 1)];
	return Settings->LoadingTips[TipIndex];
}

bool UEFLoadingScreenSubsystem::ShowViewportLoadingScreen()
{
	if (ViewportLoadingScreenWidget.IsValid())
	{
		bLoadingScreenRequested = true;
		return true;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UGameViewportClient* GameViewportClient = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
	if (!GameViewportClient)
	{
		return false;
	}

	ViewportLoadingScreenWidget = CreateLoadingScreenWidget();
	GameViewportClient->AddViewportWidgetContent(ViewportLoadingScreenWidget.ToSharedRef(), 10000);
	bLoadingScreenRequested = true;
	return true;
}

bool UEFLoadingScreenSubsystem::SetupMoviePlayerLoadingScreen()
{
	if (!IsMoviePlayerEnabled())
	{
		UE_LOG(LogEFLoadingScreen, Warning, TEXT("ShowLoadingScreen: MoviePlayer is not enabled."));
		return false;
	}

	IGameMoviePlayer* MoviePlayer = GetMoviePlayer();

	// Already handed over for this session and MoviePlayer still holds it: nothing to redo. The
	// second half of the test matters — MoviePlayer drops its attributes when a movie finishes,
	// so a flag left over from the last travel must not stop the next one being prepared.
	if (bUsingMoviePlayerLoadingScreen && MoviePlayer->LoadingScreenIsPrepared())
	{
		return true;
	}

	if (MoviePlayer->IsMovieCurrentlyPlaying())
	{
		if (bUsingMoviePlayerLoadingScreen)
		{
			return true;
		}

		MoviePlayer->StopMovie();
	}

	const UEFLoadingScreenSettings* Settings = GetDefault<UEFLoadingScreenSettings>();
	const bool bAllowEngineTick = Settings && Settings->bAllowEngineTickDuringMoviePlayerLoadingScreen;
	const bool bWaitForWorldReadiness = Settings && Settings->bWaitForWorldPartitionBeforeHiding;
	const bool bManualMoviePlayerStop = (Settings && Settings->bWaitForManualStop)
		|| (bWaitForWorldReadiness && bAllowEngineTick);

	if (bWaitForWorldReadiness && !bAllowEngineTick)
	{
		UE_LOG(LogEFLoadingScreen, Log, TEXT("Loading screen: MoviePlayer engine tick is disabled; MoviePlayer will auto-complete and the viewport overlay will handle post-load world-readiness wait."));
	}

	FLoadingScreenAttributes LoadingScreenAttributes;
	LoadingScreenAttributes.WidgetLoadingScreen = CreateLoadingScreenWidget();
	LoadingScreenAttributes.MinimumLoadingScreenDisplayTime = Settings ? Settings->MinimumLoadingScreenDisplayTime : -1.0f;
	LoadingScreenAttributes.bAutoCompleteWhenLoadingCompletes = !bManualMoviePlayerStop && (!Settings || Settings->bAutoCompleteWhenLoadingCompletes);
	LoadingScreenAttributes.bWaitForManualStop = bManualMoviePlayerStop;
	LoadingScreenAttributes.bMoviesAreSkippable = false;
	LoadingScreenAttributes.bAllowEngineTick = bAllowEngineTick;

	MoviePlayer->SetupLoadingScreen(LoadingScreenAttributes);
	bLoadingScreenRequested = true;
	bUsingMoviePlayerLoadingScreen = true;
	return true;
}

void UEFLoadingScreenSubsystem::StopMoviePlayerLoadingScreen()
{
	if (IsMoviePlayerEnabled() && bUsingMoviePlayerLoadingScreen && GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		GetMoviePlayer()->StopMovie();
	}

	bUsingMoviePlayerLoadingScreen = false;
}

void UEFLoadingScreenSubsystem::HideViewportLoadingScreen()
{
	if (!ViewportLoadingScreenWidget.IsValid())
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UGameViewportClient* GameViewportClient = GameInstance->GetGameViewportClient())
		{
			GameViewportClient->RemoveViewportWidgetContent(ViewportLoadingScreenWidget.ToSharedRef());
		}
	}

	ViewportLoadingScreenWidget.Reset();
}

void UEFLoadingScreenSubsystem::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	// The delegate is global; in PIE several game instances hear every load. Only ours matters.
	if (WorldContext.OwningGameInstance != GetGameInstance())
	{
		return;
	}

	// Our world is about to be torn down, so any readiness wait on it is moot — whether or not
	// the screen is raised automatically, the matching PostLoadMap is ours to handle.
	ClearWorldReadinessPolling();
	bAwaitingPostLoadMap = true;

	// A dedicated server has no viewport and no MoviePlayer; raising the screen there is pure waste.
	if (IsRunningDedicatedServer())
	{
		return;
	}

	const UEFLoadingScreenSettings* Settings = GetDefault<UEFLoadingScreenSettings>();
	if (!Settings || !Settings->bShowAutomaticallyOnMapTravel)
	{
		return;
	}

	UE_LOG(LogEFLoadingScreen, Log, TEXT("Loading screen: raising automatically for map travel to '%s'."), *MapName);
	ShowLoadingScreen();
}

void UEFLoadingScreenSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// A world that belongs to another game instance is another instance's load, even if we are
	// waiting on our own; a null world (failed load) is only ours if our PreLoadMap set the flag.
	if (LoadedWorld && LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	if (!bAwaitingPostLoadMap)
	{
		return;
	}
	bAwaitingPostLoadMap = false;

	if (!bLoadingScreenRequested)
	{
		return;
	}

	if (!LoadedWorld)
	{
		UE_LOG(LogEFLoadingScreen, Warning, TEXT("Loading screen: map load produced no world; hiding."));
		HideLoadingScreen();
		return;
	}

	const UEFLoadingScreenSettings* Settings = GetDefault<UEFLoadingScreenSettings>();
	if (bUsingMoviePlayerLoadingScreen && !(Settings && Settings->bAllowEngineTickDuringMoviePlayerLoadingScreen))
	{
		// MoviePlayer has done its part (or is about to auto-complete); the overlay carries the
		// rest. Re-created rather than reused so it is a fresh, fully constructed widget on top.
		StopMoviePlayerLoadingScreen();
		HideViewportLoadingScreen();
		ShowViewportLoadingScreen();
		bLoadingScreenRequested = ViewportLoadingScreenWidget.IsValid();
		if (!bLoadingScreenRequested)
		{
			return;
		}
	}

	BeginWorldReadinessWait(LoadedWorld);
}

void UEFLoadingScreenSubsystem::BeginWorldReadinessWait(UWorld* World)
{
	ClearWorldReadinessPolling();

	if (!World)
	{
		UE_LOG(LogEFLoadingScreen, Warning, TEXT("Loading screen: no world to wait on; hiding."));
		HideLoadingScreen();
		return;
	}

	const UEFLoadingScreenSettings* Settings = GetDefault<UEFLoadingScreenSettings>();
	const float HideDelay = Settings ? Settings->HideDelayAfterWorldReady : 0.0f;
	const float PollInterval = Settings ? Settings->WorldReadinessPollInterval : 0.1f;

	if (HideDelay <= 0.0f && IsWorldReadyToHideLoadingScreen(World))
	{
		UE_LOG(LogEFLoadingScreen, Log, TEXT("Loading screen: world '%s' is already ready; hiding."), *World->GetName());
		HideLoadingScreen();
		return;
	}

	PendingLoadedWorld = World;
	WorldReadinessStartedAtSeconds = FPlatformTime::Seconds();
	WorldReadySinceSeconds = -1.0;

	UE_LOG(LogEFLoadingScreen, Log, TEXT("Loading screen: waiting for world '%s' readiness (%s)."),
		*World->GetName(), *DescribeReadinessBlockers(World));

	// The core ticker rather than the world's timer manager: it runs on real time, so a paused
	// world can neither freeze the poll nor keep the timeout from ever firing.
	WorldReadinessTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UEFLoadingScreenSubsystem::TickWorldReadiness),
		PollInterval);
}

bool UEFLoadingScreenSubsystem::TickWorldReadiness(float /*DeltaTime*/)
{
	UWorld* LoadedWorld = PendingLoadedWorld.Get();
	if (!LoadedWorld)
	{
		UE_LOG(LogEFLoadingScreen, Warning, TEXT("Loading screen: the world being waited on is gone; hiding."));
		HideLoadingScreen();
		return false;
	}

	const UEFLoadingScreenSettings* Settings = GetDefault<UEFLoadingScreenSettings>();
	const double MaxWaitTime = Settings ? Settings->WorldReadinessMaxWaitTime : 60.0;
	const double HideDelay = Settings ? Settings->HideDelayAfterWorldReady : 0.0;
	const double Now = FPlatformTime::Seconds();
	const double ElapsedSeconds = Now - WorldReadinessStartedAtSeconds;
	const bool bTimedOut = MaxWaitTime > 0.0 && ElapsedSeconds >= MaxWaitTime;
	const bool bWorldReady = IsWorldReadyToHideLoadingScreen(LoadedWorld);

	if (bWorldReady)
	{
		if (WorldReadySinceSeconds < 0.0)
		{
			WorldReadySinceSeconds = Now;
			if (HideDelay > 0.0)
			{
				UE_LOG(LogEFLoadingScreen, Log, TEXT("Loading screen: world ready after %.2fs; holding for %.2fs hide delay."), ElapsedSeconds, HideDelay);
			}
		}
	}
	else
	{
		WorldReadySinceSeconds = -1.0;
	}

	const bool bHideDelayElapsed = WorldReadySinceSeconds >= 0.0 && Now - WorldReadySinceSeconds >= HideDelay;
	if (!(bWorldReady && bHideDelayElapsed) && !bTimedOut)
	{
		return true;
	}

	if (bTimedOut && !bWorldReady)
	{
		UE_LOG(LogEFLoadingScreen, Warning, TEXT("Loading screen: readiness wait timed out after %.1fs — hiding anyway. Still blocking: %s."),
			ElapsedSeconds, *DescribeReadinessBlockers(LoadedWorld));

		// Holds are discarded on purpose: one forgotten release must not keep the screen up
		// forever, or block the next travel's wait after this one gave up.
		ActiveHolds.Reset();
	}
	else
	{
		UE_LOG(LogEFLoadingScreen, Log, TEXT("Loading screen: world ready after %.2fs; hiding."), ElapsedSeconds);
	}

	HideLoadingScreen();
	return false;
}

bool UEFLoadingScreenSubsystem::IsWorldReadyToHideLoadingScreen(UWorld* World) const
{
	if (!World || !World->AreActorsInitialized())
	{
		return false;
	}

	if (HasLoadingScreenHolds())
	{
		return false;
	}

	const UEFLoadingScreenSettings* Settings = GetDefault<UEFLoadingScreenSettings>();
	if ((!Settings || Settings->bWaitForWorldPartitionBeforeHiding) && !IsWorldStreamingSettled(World))
	{
		return false;
	}

	for (const FEFLoadingScreenReadinessQuery& Query : ReadinessQueries)
	{
		if (Query.IsBound() && !Query.Execute(World))
		{
			return false;
		}
	}

	return true;
}

bool UEFLoadingScreenSubsystem::IsWorldStreamingSettled(const UWorld* World) const
{
	for (const ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (!StreamingLevel)
		{
			continue;
		}

		if (StreamingLevel->ShouldBeLoaded() && !StreamingLevel->IsLevelLoaded())
		{
			return false;
		}

		if (StreamingLevel->ShouldBeVisible() && !StreamingLevel->IsLevelVisible())
		{
			return false;
		}
	}

	if (UWorldPartitionSubsystem* WorldPartitionSubsystem = World->GetSubsystem<UWorldPartitionSubsystem>())
	{
		if (!WorldPartitionSubsystem->IsAllStreamingCompleted())
		{
			return false;
		}
	}

	return true;
}

FString UEFLoadingScreenSubsystem::DescribeReadinessBlockers(const UWorld* World) const
{
	TArray<FString> Blockers;

	if (!World->AreActorsInitialized())
	{
		Blockers.Add(TEXT("actors not initialized"));
	}

	const UEFLoadingScreenSettings* Settings = GetDefault<UEFLoadingScreenSettings>();
	if ((!Settings || Settings->bWaitForWorldPartitionBeforeHiding) && !IsWorldStreamingSettled(World))
	{
		Blockers.Add(TEXT("level streaming"));
	}

	for (const TPair<int32, FName>& Hold : ActiveHolds)
	{
		Blockers.Add(FString::Printf(TEXT("hold %d '%s'"), Hold.Key, *Hold.Value.ToString()));
	}

	int32 VetoingQueries = 0;
	for (const FEFLoadingScreenReadinessQuery& Query : ReadinessQueries)
	{
		if (Query.IsBound() && !Query.Execute(World))
		{
			++VetoingQueries;
		}
	}
	if (VetoingQueries > 0)
	{
		Blockers.Add(FString::Printf(TEXT("%d readiness quer%s"), VetoingQueries, VetoingQueries == 1 ? TEXT("y") : TEXT("ies")));
	}

	return Blockers.Num() > 0 ? FString::Join(Blockers, TEXT(", ")) : FString(TEXT("nothing"));
}

void UEFLoadingScreenSubsystem::ClearWorldReadinessPolling()
{
	if (WorldReadinessTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(WorldReadinessTickerHandle);
		WorldReadinessTickerHandle.Reset();
	}

	PendingLoadedWorld.Reset();
	WorldReadinessStartedAtSeconds = 0.0;
	WorldReadySinceSeconds = -1.0;
}
