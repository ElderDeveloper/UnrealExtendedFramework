// EFSubtitleQueuePolicy.cpp
#include "EFSubtitleQueuePolicy.h"

FEFActiveSubtitle UEFSubtitleQueuePolicy::InvalidSubtitle = FEFActiveSubtitle();
TArray<FEFActiveSubtitle> UEFSubtitleQueuePolicy::EmptyActive;

void UEFSubtitleQueuePolicy_Default::BindQueueDelegates()
{
	if (bDelegatesBound)
	{
		return;
	}

	Queue.OnActiveChanged.BindLambda([this](const FEFActiveSubtitle& Active)
	{
		OnActiveChanged.ExecuteIfBound(Active);
	});
	Queue.OnExpired.BindLambda([this](int32 RequestId)
	{
		OnExpired.ExecuteIfBound(RequestId);
	});
	bDelegatesBound = true;
}

void UEFSubtitleQueuePolicy_Default::Configure(EEFSubtitleQueueMode Mode, int32 MaxStacked)
{
	BindQueueDelegates();
	Queue.SetQueueMode(Mode);
	Queue.SetMaxStacked(MaxStacked);
}

int32 UEFSubtitleQueuePolicy_Default::Enqueue(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request)
{
	BindQueueDelegates();
	return Queue.Enqueue(Entry, Request);
}

void UEFSubtitleQueuePolicy_Default::Cancel(int32 RequestId)
{
	BindQueueDelegates();
	Queue.Cancel(RequestId);
}

void UEFSubtitleQueuePolicy_Default::ClearAll()
{
	BindQueueDelegates();
	Queue.ClearAll();
}

void UEFSubtitleQueuePolicy_Default::Tick(float DeltaTime)
{
	BindQueueDelegates();
	Queue.Tick(DeltaTime);
}

const FEFActiveSubtitle& UEFSubtitleQueuePolicy_Default::GetActive() const
{
	return Queue.GetActive();
}

const TArray<FEFActiveSubtitle>& UEFSubtitleQueuePolicy_Default::GetAllActive() const
{
	return Queue.GetAllActive();
}

bool UEFSubtitleQueuePolicy_Default::IsEmpty() const
{
	return Queue.IsEmpty();
}

EEFSubtitleQueueMode UEFSubtitleQueuePolicy_Default::GetQueueMode() const
{
	return Queue.GetQueueMode();
}
