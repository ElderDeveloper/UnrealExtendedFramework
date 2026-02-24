// EFSubtitleQueue.h — Subtitle scheduling & priority queue
#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Data/EFSubtitleData.h"
#include "EFSubtitleQueue.generated.h"

DECLARE_DELEGATE_OneParam(FOnActiveSubtitleChanged, const FEFActiveSubtitle& /*Active*/);
DECLARE_DELEGATE_OneParam(FOnSubtitleExpired, int32 /*RequestId*/);

/**
 * Manages the ordering, interruption, and expiration of subtitles per local player.
 * Supports Replace, Queue, PriorityQueue, and Stack modes.
 */
USTRUCT()
struct FEFSubtitleQueue
{
	GENERATED_BODY()

public:

	// Set the queue mode
	void SetQueueMode(EEFSubtitleQueueMode InMode) { QueueMode = InMode; }
	EEFSubtitleQueueMode GetQueueMode() const { return QueueMode; }

	// Set max stacked subtitles (only relevant in Stack mode)
	void SetMaxStacked(int32 InMax) { MaxStacked = FMath::Max(1, InMax); }

	// Enqueue a new subtitle. Returns the RequestId.
	int32 Enqueue(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request);

	// Remove a subtitle by request ID (from active or pending)
	void Cancel(int32 RequestId);

	// Clear everything
	void ClearAll();

	// Tick the queue — handles expiration and advancement. Call each frame.
	void Tick(float DeltaTime);

	// Get the currently active subtitle (may be invalid if empty)
	const FEFActiveSubtitle& GetActive() const;

	// Get all active subtitles (for Stack mode)
	const TArray<FEFActiveSubtitle>& GetAllActive() const { return ActiveSubtitles; }

	// Is the queue empty?
	bool IsEmpty() const { return ActiveSubtitles.Num() == 0 && PendingSubtitles.Num() == 0; }

	// Delegates
	FOnActiveSubtitleChanged OnActiveChanged;
	FOnSubtitleExpired OnExpired;

private:
	EEFSubtitleQueueMode QueueMode = EEFSubtitleQueueMode::Replace;
	int32 MaxStacked = 3;
	int32 NextRequestId = 1;

	TArray<FEFActiveSubtitle> ActiveSubtitles;
	TArray<FEFActiveSubtitle> PendingSubtitles;

	// Calculate the effective duration for a subtitle
	float CalculateDuration(const FEFSubtitleEntry& Entry) const;

	// Try to advance: move from pending to active
	void TryAdvance();

	// Static invalid subtitle for returning references
	static FEFActiveSubtitle InvalidSubtitle;
};
