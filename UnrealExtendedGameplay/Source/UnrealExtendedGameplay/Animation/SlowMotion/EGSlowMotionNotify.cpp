// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSlowMotionNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

TMap<TWeakObjectPtr<UWorld>, UEGSlowMotionNotify::FWorldSlowMotion> UEGSlowMotionNotify::ActiveWorlds;

UEGSlowMotionNotify::UEGSlowMotionNotify()
	: BeginSpeed(0.2f)
	, EndSpeed(1.0f)
{
}

UWorld* UEGSlowMotionNotify::GetAuthorityWorld(const USkeletalMeshComponent* MeshComp)
{
	UWorld* const World = MeshComp ? MeshComp->GetWorld() : nullptr;
	return (World && World->GetNetMode() != NM_Client) ? World : nullptr;
}

void UEGSlowMotionNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	UWorld* const World = GetAuthorityWorld(MeshComp);
	if (!World)
	{
		return;
	}

	// Drop entries for worlds that were torn down mid-window (end of PIE, level travel).
	for (auto It = ActiveWorlds.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}

	FWorldSlowMotion& State = ActiveWorlds.FindOrAdd(TWeakObjectPtr<UWorld>(World));
	if (State.Count++ == 0)
	{
		State.PreviousTimeDilation = UGameplayStatics::GetGlobalTimeDilation(World);
	}
	UGameplayStatics::SetGlobalTimeDilation(World, BeginSpeed);
}

void UEGSlowMotionNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	UWorld* const World = GetAuthorityWorld(MeshComp);
	if (!World)
	{
		return;
	}

	const TWeakObjectPtr<UWorld> Key(World);
	FWorldSlowMotion* State = ActiveWorlds.Find(Key);
	if (!State || --State->Count > 0)
	{
		// Another window is still running; it owns the time dilation now.
		return;
	}

	const float TargetDilation = bRestorePreviousTimeDilation ? State->PreviousTimeDilation : EndSpeed;
	ActiveWorlds.Remove(Key);
	UGameplayStatics::SetGlobalTimeDilation(World, TargetDilation);
}
