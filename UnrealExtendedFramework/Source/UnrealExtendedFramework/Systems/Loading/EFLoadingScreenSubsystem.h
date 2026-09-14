// EFLoadingScreenSubsystem.h — Async loading screen owner (GameInstanceSubsystem)
#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "EFLoadingScreenSubsystem.generated.h"

class SWidget;
class UTexture2D;
class UWorld;
struct FWorldContext;

DECLARE_LOG_CATEGORY_EXTERN(LogEFLoadingScreen, Log, All);

/**
 * Answers "may the loading screen come down yet?" for one game system. Return false to keep it up.
 * Asked on every readiness poll, so keep it cheap and side-effect free.
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FEFLoadingScreenReadinessQuery, const UWorld* /*World*/);

/**
 * Shared owner for the Extended Framework async loading screen.
 *
 * ShowLoadingScreen() puts the same Slate surface up twice: through MoviePlayer, which covers the
 * blocking map load, and as a game-viewport overlay, which covers everything after it. That split
 * exists because MoviePlayer stops at map load while the world is usually still streaming in.
 *
 * Hiding is driven by world readiness rather than by the map finishing: after PostLoadMap a poll
 * waits until the world is READY — actors initialized, every streaming level that should be loaded
 * and visible is so, World Partition streaming complete, no holds outstanding and every registered
 * readiness query answering true — then an optional settle delay. The poll runs on the core
 * ticker with real time, so a paused world can neither stall it nor stop its timeout.
 *
 * Map travel is automatic: the subsystem binds PreLoadMap / PostLoadMap for ITS OWN game instance
 * (other PIE instances are ignored) and raises the screen on every travel unless
 * bShowAutomaticallyOnMapTravel is off. A manual flow can still do:
 *
 *     Subsystem->ShowLoadingScreen();                          // before travelling
 *     // ... wait GetMapTravelDelay() so the screen is actually up ...
 *     World->ServerTravel(...) / UGameplayStatics::OpenLevel(...);
 *
 * Two ways a game extends the readiness gate:
 *
 *  - HOLDS cover work that has no map load of its own, e.g. streaming a sub-level set inside one
 *    persistent world. RequestLoadingScreenHold() raises the viewport overlay if nothing is up and
 *    keeps it up; releasing the last hold starts the same readiness wait a map load uses, so the
 *    hide delay and streaming checks apply either way. WorldReadinessMaxWaitTime still bounds the
 *    wait: on timeout every hold is discarded (and named in the log) so one forgotten release
 *    cannot hang the screen forever, or the next travel's wait after it.
 *
 *  - QUERIES let a system veto the hide without owning a hold: RegisterReadinessQuery() adds a
 *    delegate that is asked on every poll while the screen is up. Useful for "my replicated state
 *    has not arrived yet" style conditions where the system does not know when it started waiting.
 *
 * HideLoadingScreen() is only needed for a boot/main-menu flow that never travels, or to abort.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFLoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Raises the loading screen. Safe to call while it is already up: a screen that came up as a
	 * viewport overlay (a hold, or the previous travel's readiness wait still running) gets its
	 * MoviePlayer half prepared here, so a back-to-back travel is still covered during the block.
	 * Returns false only when neither MoviePlayer nor a viewport could take it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Loading")
	bool ShowLoadingScreen();

	/** Takes the screen down at once. Does not touch holds — an explicit hide is an abort. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Loading")
	void HideLoadingScreen();

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Loading")
	void HideAllLoadingScreens();

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Loading")
	bool IsLoadingScreenVisible() const;

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Loading|Travel")
	float GetMapTravelDelay() const;

	/**
	 * Keeps the loading screen up until the matching ReleaseLoadingScreenHold(), raising the
	 * viewport overlay first if nothing is showing. Reason is only for the log. Returns the hold id.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Loading|Holds")
	int32 RequestLoadingScreenHold(FName Reason);

	/**
	 * Releases one hold. When it was the last one and no map load is in flight, the readiness
	 * wait starts on the current world and the screen hides once that passes. Unknown ids are
	 * ignored, so releasing after a timeout already discarded the hold is harmless.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Loading|Holds")
	void ReleaseLoadingScreenHold(int32 HoldId);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Loading|Holds")
	bool HasLoadingScreenHolds() const { return ActiveHolds.Num() > 0; }

	/** Adds a veto asked on every readiness poll. Unregister with the returned handle before the owner dies. */
	FDelegateHandle RegisterReadinessQuery(FEFLoadingScreenReadinessQuery Query);
	void UnregisterReadinessQuery(FDelegateHandle Handle);

	/**
	 * True when the given world has finished initializing actors and streaming (level streaming and
	 * World Partition), no hold is outstanding and every readiness query agrees. Exposed so a boot
	 * flow that never travels can run the same readiness test.
	 */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Loading")
	bool IsWorldReadyToHideLoadingScreen(UWorld* World) const;

private:
	/** Picks the texture and tip once per loading session, so every widget in the session matches. */
	void PrepareLoadingSession();
	TSharedRef<SWidget> CreateLoadingScreenWidget() const;
	UTexture2D* ResolveLoadingScreenTexture() const;
	FText ResolveLoadingTip() const;
	bool ShowViewportLoadingScreen();
	/** Hands MoviePlayer the widget for the next blocking load. False when MoviePlayer is unavailable. */
	bool SetupMoviePlayerLoadingScreen();
	void StopMoviePlayerLoadingScreen();
	void HideViewportLoadingScreen();
	void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	/** Starts (or restarts) the readiness poll for World; hides at once if it is already ready. */
	void BeginWorldReadinessWait(UWorld* World);
	bool TickWorldReadiness(float DeltaTime);
	void ClearWorldReadinessPolling();
	bool IsWorldReadinessWaitActive() const { return WorldReadinessTickerHandle.IsValid(); }
	/** Level streaming + World Partition part of the readiness test, without holds or queries. */
	bool IsWorldStreamingSettled(const UWorld* World) const;
	/** Names whatever is still blocking the hide, for the timeout log. */
	FString DescribeReadinessBlockers(const UWorld* World) const;

	TSharedPtr<SWidget> ViewportLoadingScreenWidget;
	TStrongObjectPtr<UTexture2D> ActiveBackgroundTexture;
	FText ActiveLoadingTip;
	bool bLoadingTipResolved = false;

	TWeakObjectPtr<UWorld> PendingLoadedWorld;
	FTSTicker::FDelegateHandle WorldReadinessTickerHandle;
	double WorldReadinessStartedAtSeconds = 0.0;
	double WorldReadySinceSeconds = -1.0;

	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;
	bool bLoadingScreenRequested = false;
	bool bUsingMoviePlayerLoadingScreen = false;
	/** Set by this instance's own PreLoadMap; the next PostLoadMap is only handled while it is set. */
	bool bAwaitingPostLoadMap = false;

	TMap<int32, FName> ActiveHolds;
	int32 NextHoldId = 1;
	TArray<FEFLoadingScreenReadinessQuery> ReadinessQueries;
};
