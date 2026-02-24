// EFSubtitleQueue.cpp

#include "EFSubtitleQueue.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Data/EFSubtitleProjectSettings.h"

FEFActiveSubtitle FEFSubtitleQueue::InvalidSubtitle = FEFActiveSubtitle();


float FEFSubtitleQueue::CalculateDuration(const FEFSubtitleEntry& Entry) const
{
	const auto* Settings = GetDefault<UEFSubtitleProjectSettings>();
	const FEFSubtitleDurationSettings& DurSettings = Settings ? Settings->DurationSettings : FEFSubtitleDurationSettings();

	// If the entry has a duration set and we're forced to use it, return it
	if (DurSettings.bForceSubtitleDuration && Entry.Duration > 0.0f)
	{
		return Entry.Duration;
	}

	// If the entry has a non-zero duration, use it
	if (Entry.Duration > 0.0f)
	{
		return Entry.Duration;
	}

	// Auto-calculate from text length
	const int32 TextLength = Entry.Text.ToString().Len();
	const float AutoDuration = (TextLength * DurSettings.TimePerLetter) + DurSettings.TimeAfterComplete;
	return FMath::Max(AutoDuration, 1.0f); // Minimum 1 second
}


int32 FEFSubtitleQueue::Enqueue(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request)
{
	FEFActiveSubtitle NewSub;
	NewSub.Entry = Entry;
	NewSub.Request = Request;
	NewSub.RequestId = NextRequestId++;
	NewSub.ElapsedTime = 0.0f;
	NewSub.RemainingTime = CalculateDuration(Entry);

	// Apply delay
	if (Entry.Delay > 0.0f)
	{
		NewSub.RemainingTime += Entry.Delay;
	}

	switch (QueueMode)
	{
	case EEFSubtitleQueueMode::Replace:
	{
		// Clear everything and set this as active
		ActiveSubtitles.Empty();
		PendingSubtitles.Empty();
		ActiveSubtitles.Add(NewSub);
		OnActiveChanged.ExecuteIfBound(NewSub);
		break;
	}

	case EEFSubtitleQueueMode::Queue:
	{
		if (ActiveSubtitles.Num() == 0)
		{
			ActiveSubtitles.Add(NewSub);
			OnActiveChanged.ExecuteIfBound(NewSub);
		}
		else
		{
			PendingSubtitles.Add(NewSub);
		}
		break;
	}

	case EEFSubtitleQueueMode::PriorityQueue:
	{
		if (ActiveSubtitles.Num() == 0)
		{
			ActiveSubtitles.Add(NewSub);
			OnActiveChanged.ExecuteIfBound(NewSub);
		}
		else
		{
			const FEFActiveSubtitle& Current = ActiveSubtitles[0];
			// Higher priority interrupts, unless current is uninterruptible
			if (Entry.Priority > Current.Entry.Priority && !Current.Entry.bUninterruptible)
			{
				// Move current to pending front, replace with new
				PendingSubtitles.Insert(ActiveSubtitles[0], 0);
				ActiveSubtitles[0] = NewSub;
				OnActiveChanged.ExecuteIfBound(NewSub);
			}
			else
			{
				// Insert into pending sorted by priority (high first)
				int32 InsertIdx = 0;
				for (; InsertIdx < PendingSubtitles.Num(); ++InsertIdx)
				{
					if (Entry.Priority > PendingSubtitles[InsertIdx].Entry.Priority)
					{
						break;
					}
				}
				PendingSubtitles.Insert(NewSub, InsertIdx);
			}
		}
		break;
	}

	case EEFSubtitleQueueMode::Stack:
	{
		if (ActiveSubtitles.Num() < MaxStacked)
		{
			ActiveSubtitles.Add(NewSub);
			OnActiveChanged.ExecuteIfBound(NewSub);
		}
		else
		{
			// Remove the oldest, add the new one
			const int32 OldId = ActiveSubtitles[0].RequestId;
			ActiveSubtitles.RemoveAt(0);
			OnExpired.ExecuteIfBound(OldId);

			ActiveSubtitles.Add(NewSub);
			OnActiveChanged.ExecuteIfBound(NewSub);
		}
		break;
	}
	}

	return NewSub.RequestId;
}


void FEFSubtitleQueue::Cancel(int32 RequestId)
{
	// Check active
	for (int32 i = ActiveSubtitles.Num() - 1; i >= 0; --i)
	{
		if (ActiveSubtitles[i].RequestId == RequestId)
		{
			ActiveSubtitles.RemoveAt(i);
			OnExpired.ExecuteIfBound(RequestId);
			TryAdvance();
			return;
		}
	}

	// Check pending
	for (int32 i = PendingSubtitles.Num() - 1; i >= 0; --i)
	{
		if (PendingSubtitles[i].RequestId == RequestId)
		{
			PendingSubtitles.RemoveAt(i);
			return;
		}
	}
}


void FEFSubtitleQueue::ClearAll()
{
	for (const auto& Sub : ActiveSubtitles)
	{
		OnExpired.ExecuteIfBound(Sub.RequestId);
	}
	ActiveSubtitles.Empty();
	PendingSubtitles.Empty();
}


void FEFSubtitleQueue::Tick(float DeltaTime)
{
	// Tick active subtitles
	for (int32 i = ActiveSubtitles.Num() - 1; i >= 0; --i)
	{
		ActiveSubtitles[i].ElapsedTime += DeltaTime;
		ActiveSubtitles[i].RemainingTime -= DeltaTime;

		if (ActiveSubtitles[i].RemainingTime <= 0.0f)
		{
			const int32 ExpiredId = ActiveSubtitles[i].RequestId;
			ActiveSubtitles.RemoveAt(i);
			OnExpired.ExecuteIfBound(ExpiredId);
		}
	}

	// Try to advance pending to active
	TryAdvance();
}


const FEFActiveSubtitle& FEFSubtitleQueue::GetActive() const
{
	if (ActiveSubtitles.Num() > 0)
	{
		return ActiveSubtitles.Last();
	}
	return InvalidSubtitle;
}


void FEFSubtitleQueue::TryAdvance()
{
	if (QueueMode == EEFSubtitleQueueMode::Stack)
	{
		// Fill up to max stacked from pending
		while (ActiveSubtitles.Num() < MaxStacked && PendingSubtitles.Num() > 0)
		{
			FEFActiveSubtitle Next = PendingSubtitles[0];
			PendingSubtitles.RemoveAt(0);
			ActiveSubtitles.Add(Next);
			OnActiveChanged.ExecuteIfBound(Next);
		}
	}
	else
	{
		// For Replace/Queue/PriorityQueue: advance if no active
		if (ActiveSubtitles.Num() == 0 && PendingSubtitles.Num() > 0)
		{
			FEFActiveSubtitle Next = PendingSubtitles[0];
			PendingSubtitles.RemoveAt(0);
			ActiveSubtitles.Add(Next);
			OnActiveChanged.ExecuteIfBound(Next);
		}
	}
}
