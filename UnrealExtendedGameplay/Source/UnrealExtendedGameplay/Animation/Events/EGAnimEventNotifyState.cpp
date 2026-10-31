// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGAnimEventNotifyState.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "EGAnimEventReceiver.h"

FString UEGAnimEventNotifyStateBase::GetNotifyName_Implementation() const
{
	const FGameplayTag Tag = GetEventTag();
	return Tag.IsValid() ? Tag.ToString() : Super::GetNotifyName_Implementation();
}

void UEGAnimEventNotifyStateBase::Send(EEGAnimEventPhase Phase, USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float Time, int32 WindowId) const
{
	FEGAnimEvent Event;
	Event.Tag = GetEventTag();
	Event.Phase = Phase;
	Event.Payload = GetEventPayload();
	Event.Mesh = MeshComp;
	Event.Animation = Animation;
	Event.Time = Time;
	Event.WindowId = WindowId;
	EGAnimEvents::Dispatch(Event);
}

void UEGAnimEventNotifyStateBase::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp)
	{
		return;
	}

	for (auto It = OpenWindows.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}

	const int32 WindowId = EGAnimEvents::NewWindowId();
	OpenWindows.FindOrAdd(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp)).Add(WindowId);
	Send(EEGAnimEventPhase::Begin, MeshComp, Animation, TotalDuration, WindowId);
}

void UEGAnimEventNotifyStateBase::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	if (!bSendTickEvents || !MeshComp)
	{
		return;
	}

	// With the window overlapping itself the tick cannot say which instance it is for; report the newest.
	const TArray<int32>* Windows = OpenWindows.Find(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp));
	if (Windows && Windows->Num() > 0)
	{
		Send(EEGAnimEventPhase::Tick, MeshComp, Animation, FrameDeltaTime, Windows->Last());
	}
}

void UEGAnimEventNotifyStateBase::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (!MeshComp)
	{
		return;
	}

	const TWeakObjectPtr<USkeletalMeshComponent> Key(MeshComp);
	TArray<int32>* Windows = OpenWindows.Find(Key);
	if (!Windows || Windows->Num() == 0)
	{
		return;
	}

	// Windows of one notify on one mesh close in the order they opened.
	const int32 WindowId = (*Windows)[0];
	Windows->RemoveAt(0);
	if (Windows->Num() == 0)
	{
		OpenWindows.Remove(Key);
	}
	Send(EEGAnimEventPhase::End, MeshComp, Animation, 0.0f, WindowId);
}
