// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSlowMotionNotify.h"

#include "Components/SkeletalMeshComponent.h"

UEGSlowMotionNotify::UEGSlowMotionNotify()
	: BeginSpeed(0.2f)
	, EndSpeed(1.0f)
{
}

void UEGSlowMotionNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	UEGTimeDilationSubsystem* const TimeDilation = MeshComp ? UEGTimeDilationSubsystem::Get(MeshComp) : nullptr;
	if (!TimeDilation)
	{
		return;
	}

	for (auto It = ActiveHandles.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}

	FEGTimeDilationParams Params = Blend;
	Params.Duration = 0.0f;

	// Refused (invalid handle) on clients.
	const FEGTimeDilationHandle Handle = TimeDilation->PushGlobalTimeDilation(BeginSpeed, Params);
	if (Handle.IsValid())
	{
		ActiveHandles.FindOrAdd(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp)).Add(Handle);
	}
}

void UEGSlowMotionNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	const TWeakObjectPtr<USkeletalMeshComponent> Key(MeshComp);
	TArray<FEGTimeDilationHandle>* Handles = ActiveHandles.Find(Key);
	if (!Handles || Handles->Num() == 0)
	{
		return;
	}

	FEGTimeDilationHandle Handle = (*Handles)[0];
	Handles->RemoveAt(0);
	if (Handles->Num() == 0)
	{
		ActiveHandles.Remove(Key);
	}

	if (UEGTimeDilationSubsystem* const TimeDilation = UEGTimeDilationSubsystem::Get(MeshComp))
	{
		if (!bRestorePreviousTimeDilation)
		{
			TimeDilation->SetGlobalRestoreValue(EndSpeed);
		}
		TimeDilation->RemoveTimeDilation(Handle);
	}
}
