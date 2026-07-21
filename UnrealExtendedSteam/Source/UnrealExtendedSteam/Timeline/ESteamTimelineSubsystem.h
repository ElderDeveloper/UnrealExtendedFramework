// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "ESteamTimelineSubsystem.generated.h"

/** Color/state of the timeline bar (maps ETimelineGameMode). */
UENUM(BlueprintType)
enum class EESteamTimelineGameMode : uint8
{
	Invalid,
	Playing,
	Staging,
	Menus,
	LoadingScreen
};

/** How strongly Steam should offer a timeline event as a clip (maps ETimelineEventClipPriority). */
UENUM(BlueprintType)
enum class EESteamTimelineClipPriority : uint8
{
	Invalid,
	None,
	Standard,
	Featured
};

/** Fired when a DoesEventRecordingExist query completes for a timeline event. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamTimelineEventRecordingExists, int64, EventHandle, bool, bRecordingExists);

/**
 * Wraps ISteamTimeline (Game Recording timeline markers).
 *
 * The interface requires Steamworks SDK 1.62+ (this wrapper targets the 1.64 "V004" API:
 * SetTimelineTooltip/ClearTimelineTooltip — the older SetTimelineStateDescription names are
 * not supported) and a recent Steam client; on older SDKs or clients every call degrades to
 * a no-op with a verbose log.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamTimelineSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Sets the tooltip describing the current game state (replaces the previous one). TimeDelta offsets the change in seconds (negative = in the past). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	void SetTimelineTooltip(const FString& Description, float TimeDelta);

	/** Clears the timeline tooltip. TimeDelta offsets the change in seconds (negative = in the past). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	void ClearTimelineTooltip(float TimeDelta);

	/** Changes the color of the timeline bar to reflect the current game mode. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	void SetTimelineGameMode(EESteamTimelineGameMode Mode);

	/**
	 * Adds an instantaneous event marker to the timeline.
	 * Icon is a name uploaded via the Steamworks partner site or a built-in "steam_" icon.
	 * Priority orders markers in the UI (0..1000). StartOffsetSeconds is relative to now (negative = past).
	 * Returns the timeline event handle (0 when unavailable); handles are only valid in this game process.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	int64 AddInstantaneousTimelineEvent(const FString& Title, const FString& Description, const FString& Icon,
		int32 Priority, float StartOffsetSeconds, EESteamTimelineClipPriority ClipPriority = EESteamTimelineClipPriority::None);

	/**
	 * Adds a time-range event to the timeline (see AddInstantaneousTimelineEvent for the shared parameters).
	 * Duration is the range length in seconds (max 600).
	 * Returns the timeline event handle (0 when unavailable).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	int64 AddRangeTimelineEvent(const FString& Title, const FString& Description, const FString& Icon,
		int32 Priority, float StartOffsetSeconds, float Duration, EESteamTimelineClipPriority ClipPriority = EESteamTimelineClipPriority::None);

	// ---- Open-ended range events ----

	/**
	 * Begins an open-ended range event whose duration is not yet known (e.g. a fight that is still
	 * happening). Finish it with EndRangeTimelineEvent using the returned handle. StartOffsetSeconds
	 * is relative to now (negative = past). Returns the timeline event handle (0 when unavailable).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	int64 StartRangeTimelineEvent(const FString& Title, const FString& Description, const FString& Icon,
		int32 Priority, float StartOffsetSeconds, EESteamTimelineClipPriority ClipPriority = EESteamTimelineClipPriority::None);

	/** Updates the title/description/icon/priority of an in-progress range event (from StartRangeTimelineEvent). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	void UpdateRangeTimelineEvent(int64 EventHandle, const FString& Title, const FString& Description, const FString& Icon,
		int32 Priority, EESteamTimelineClipPriority ClipPriority = EESteamTimelineClipPriority::None);

	/** Ends an in-progress range event. EndOffsetSeconds is relative to now (negative = the range ended in the past). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	void EndRangeTimelineEvent(int64 EventHandle, float EndOffsetSeconds);

	/** Removes a timeline event (instantaneous or range) added in this game process. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	void RemoveTimelineEvent(int64 EventHandle);

	/**
	 * Asynchronously checks whether a background recording exists for a timeline event. The
	 * result arrives on OnEventRecordingExists.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	void DoesEventRecordingExist(int64 EventHandle);

	// ---- Game phases ----

	/** Starts a new game phase (match, chapter, run...). Any running phase is ended implicitly. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	void StartGamePhase();

	/** Ends the running game phase. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	void EndGamePhase();

	/** Sets a persistent id for the running game phase (max 64 chars), e.g. a match id. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	void SetGamePhaseId(const FString& PhaseId);

	/**
	 * Adds a tag (well-defined option with an optional icon) to the running game phase.
	 * Multiple tags may share a group. Priority orders tags in the UI (0..1000).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	void AddGamePhaseTag(const FString& TagName, const FString& TagIcon, const FString& TagGroup, int32 Priority);

	/**
	 * Sets a free-text attribute on the running game phase (only the last value per group is shown).
	 * Priority orders attributes in the UI (0..1000).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	void SetGamePhaseAttribute(const FString& AttributeGroup, const FString& AttributeValue, int32 Priority);

	// ---- Overlay deep links ----

	/** Opens the Steam overlay to a game phase previously identified with SetGamePhaseId. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	void OpenOverlayToGamePhase(const FString& PhaseId);

	/** Opens the Steam overlay to a timeline event handle from this game process. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Timeline")
	void OpenOverlayToTimelineEvent(int64 EventHandle);

	UPROPERTY(BlueprintAssignable, Category = "Steam|Timeline")
	FOnSteamTimelineEventRecordingExists OnEventRecordingExists;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	/** Logs the standard "timeline unavailable" verbose message for the given call site. */
	void LogTimelineUnavailable(const TCHAR* Context) const;

	friend class FESteamTimelineCallbacks;
	TSharedPtr<class FESteamTimelineCallbacks> Callbacks;
};
