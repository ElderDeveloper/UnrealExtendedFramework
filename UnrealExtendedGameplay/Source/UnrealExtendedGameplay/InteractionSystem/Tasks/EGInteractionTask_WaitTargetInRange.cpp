// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteractionTask_WaitTargetInRange.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UnrealExtendedGameplay/InteractionSystem/EGInteractionSystemComponent.h"

UEGInteractionTask_WaitTargetInRange::UEGInteractionTask_WaitTargetInRange(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

UEGInteractionTask_WaitTargetInRange* UEGInteractionTask_WaitTargetInRange::WaitTargetInRange(UEGInteraction* OwningInteraction, float MaxDistance, bool bRequireLineOfSight, float CheckInterval)
{
	UEGInteractionTask_WaitTargetInRange* Task = NewInteractionTask<UEGInteractionTask_WaitTargetInRange>(OwningInteraction);
	if (Task)
	{
		Task->MaxDistance = FMath::Max(MaxDistance, 0.0f);
		Task->bRequireLineOfSight = bRequireLineOfSight;
		Task->CheckInterval = FMath::Max(CheckInterval, 0.0f);
	}
	return Task;
}

void UEGInteractionTask_WaitTargetInRange::Activate()
{
	Super::Activate();

	if (!IsTargetInRange())
	{
		if (ShouldBroadcast())
		{
			OnOutOfRange.Broadcast();
		}
		EndTask();
	}
}

void UEGInteractionTask_WaitTargetInRange::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	Accumulator += DeltaTime;
	if (Accumulator < CheckInterval)
	{
		return;
	}
	Accumulator = 0.0f;

	if (IsTargetInRange())
	{
		return;
	}

	if (ShouldBroadcast())
	{
		OnOutOfRange.Broadcast();
	}
	EndTask();
}

bool UEGInteractionTask_WaitTargetInRange::IsTargetInRange() const
{
	const UEGInteraction* Owner = Interaction.Get();
	const AActor* Interactor = Owner ? Owner->GetInteractorActor() : nullptr;
	const AActor* Target = Owner ? Owner->GetTargetActor() : nullptr;
	if (!Interactor || !Target)
	{
		return false;
	}

	const UPrimitiveComponent* Part = Owner->GetHitComponent();
	const FVector TargetPoint = Part ? Part->GetComponentLocation() : Target->GetActorLocation();
	if (FVector::DistSquared(Interactor->GetActorLocation(), TargetPoint) > FMath::Square(MaxDistance))
	{
		return false;
	}

	if (!bRequireLineOfSight)
	{
		return true;
	}

	const UEGInteractionSystemComponent* System = Owner->GetInteractionSystem();
	if (!System)
	{
		return true;
	}

	// Same rule the focus trace and the server validator use, so the three never drift apart.
	FVector ViewLocation;
	FRotator ViewRotation;
	System->GetViewPoint(ViewLocation, ViewRotation);
	return System->HasLineOfSightToPoint(ViewLocation, Target, TargetPoint);
}
