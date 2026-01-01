// Fill out your copyright notice in the Description page of Project Settings.


#include "EGLineOfSightComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UEGLineOfSightComponent::UEGLineOfSightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	EyeComponent = nullptr;
}


void UEGLineOfSightComponent::BeginPlay()
{
	Super::BeginPlay();
	TraceDelegate.BindUObject(this, &UEGLineOfSightComponent::OnTraceCompleted);
}


void UEGLineOfSightComponent::TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	PerformAsyncTraces();
}


void UEGLineOfSightComponent::SetEyeComponent(USceneComponent* InEye)
{
	EyeComponent = InEye;
}


void UEGLineOfSightComponent::RegisterLineOfSightActor(AActor* TargetActor, USkeletalMeshComponent* SkeletalMesh, const TArray<FName>& BoneNames)
{
	if (!TargetActor)
		return;

	FEGLOSTargetInfo NewTarget;
	NewTarget.TargetActor = TargetActor;
	NewTarget.SkeletalMesh = SkeletalMesh;
	NewTarget.BoneNames = BoneNames;
	
	// If no bones are specified but we have a generic actor, we might want to trace to the actor location.
	// We will assume that if BoneNames is empty, we just trace to ActorLocation.
	// But clearer is to just add NAME_None or handle empty implicitly as Root.

	RegisteredTargets.Add(NewTarget);
}


bool UEGLineOfSightComponent::IsActorVisible(AActor* TargetActor) const
{
	for (const FEGLOSTargetInfo& Info : RegisteredTargets)
	{
		if (Info.TargetActor.Get() == TargetActor)
		{
			return Info.bIsVisible;
		}
	}
	return false;
}


void UEGLineOfSightComponent::PerformAsyncTraces()
{
	if (!bLineOfSightEnabled)
	{
		return;
	}
	
	if (!EyeComponent)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector StartLocation = EyeComponent->GetComponentLocation();
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	for (int32 i = 0; i < RegisteredTargets.Num(); ++i)
	{
		FEGLOSTargetInfo& Info = RegisteredTargets[i];
		AActor* TargetActor = Info.TargetActor.Get();

		// Cleanup invalid targets
		if (!TargetActor)
		{
			continue;
		}

		// Throttle: Only send if previous batch completed
		if (Info.PendingTraces > 0)
		{
			continue;
		}

		// Distance check
		float DistSq = FVector::DistSquared(StartLocation, TargetActor->GetActorLocation());
		if (DistSq > FMath::Square(MaxSightDistance))
		{
			// Too far
			Info.VisibleCount = 0;
			Info.bIsVisible = false;
			
			// Check if visibility changed to trigger delegate
			if (Info.bIsVisible != Info.bWasVisible)
			{
				Info.bWasVisible = Info.bIsVisible;
				OnLineOfSightChanged.Broadcast(TargetActor, Info.bIsVisible);
			}
			continue;
		}

		// Reset counters for new batch
		Info.VisibleCount = 0;

		// After distance check, before gathering bone locations
		FVector ToTarget = TargetActor->GetActorLocation() - StartLocation;
		ToTarget.Normalize();

		FVector ForwardDir = EyeComponent->GetForwardVector();
		float DotProduct = FVector::DotProduct(ForwardDir, ToTarget);
		float AngleThreshold = FMath::Cos(FMath::DegreesToRadians(FieldOfViewDegrees * 0.5f));

		if (DotProduct < AngleThreshold)
		{
			// Target is outside FOV - mark not visible, skip traces
			Info.bIsVisible = false;
			if (Info.bIsVisible != Info.bWasVisible)
			{
				Info.bWasVisible = Info.bIsVisible;
				OnLineOfSightChanged.Broadcast(TargetActor, Info.bIsVisible);
			}
			continue;
		}

		// Determine trace points
		TArray<FVector> TargetLocations;

		if (Info.SkeletalMesh.IsValid() && Info.BoneNames.Num() > 0)
		{
			for (const FName& BoneName : Info.BoneNames)
			{
				TargetLocations.Add(Info.SkeletalMesh->GetSocketLocation(BoneName));
			}
		}
		else
		{
			// Fallback to Actor Location
			TargetLocations.Add(TargetActor->GetActorLocation());
		}

		Info.PendingTraces = TargetLocations.Num();
		Info.TotalScheduledTraces = TargetLocations.Num();

		for (const FVector& EndLoc : TargetLocations)
		{
			FTraceHandle Handle = World->AsyncLineTraceByChannel(EAsyncTraceType::Single, StartLocation, EndLoc, LineOfSightChannel, QueryParams, FCollisionResponseParams::DefaultResponseParam, &TraceDelegate);
			TraceHandleToTargetIndex.Add(Handle, i);
			if (bDebugLineOfSight)
			{
				DrawDebugLine(World, StartLocation, EndLoc, FColor::Cyan, false, 0.f,0,0.2);
			}
		}
	}
}


void UEGLineOfSightComponent::OnTraceCompleted(const FTraceHandle& Handle, FTraceDatum& Datum)
{
	// Find which target this belongs to
	int32* TargetIndexPtr = TraceHandleToTargetIndex.Find(Handle);
	if (!TargetIndexPtr)
	{
		return;
	}
	
	int32 Index = *TargetIndexPtr;
	TraceHandleToTargetIndex.Remove(Handle);

	if (RegisteredTargets.IsValidIndex(Index))
	{
		FEGLOSTargetInfo& Info = RegisteredTargets[Index];
		
		Info.PendingTraces--; // Decrement pending count

		// Check Visibility
		bool bHitTarget = false;
		if (Datum.OutHits.Num() > 0)
		{
			const FHitResult& Hit = Datum.OutHits[0];
			AActor* HitActor = Hit.GetActor();
			AActor* TargetActor = Info.TargetActor.Get();

			// If bBlockingHit is true:
			//    Visible if HitActor == Target.
			//    Not Visible if HitActor != Target.
			// If bBlockingHit is false:
			//    Visible (reached end point uninterrupted).
			
			if (Hit.bBlockingHit)
			{
				if (HitActor && (HitActor == TargetActor || HitActor->IsAttachedTo(TargetActor)))
				{
					bHitTarget = true;
					if (bDebugLineOfSight)
					{
						DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 10.0f, FColor::Green, false, 0.5f);
					}
				}
				else
				{
					// Blocked by something else
					if (bDebugLineOfSight)
					{
						DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 10.0f, FColor::Red, false, 0.5f);
					}
				}
			}
			else
			{
				// No blocking hit -> Trace continued to end -> Visible
				bHitTarget = true;
			}
		}
		// If sweep doesn't hit anything, it might just mean it didn't block. 

		if (bHitTarget)
		{
			Info.VisibleCount++;
		}
		
		// If all traces for this batch returned
		if (Info.PendingTraces <= 0)
		{
			Info.PendingTraces = 0;

			// Calculate Final Visibility for this frame
			float Ratio = 0.0f;
			if (Info.TotalScheduledTraces > 0)
			{
				Ratio = (float)Info.VisibleCount / (float)Info.TotalScheduledTraces;
			}

			Info.bIsVisible = (Ratio >= MinVisibilityRatio);

			// Check for State Change
			if (Info.bIsVisible != Info.bWasVisible)
			{
				Info.bWasVisible = Info.bIsVisible;
				// Broadcast change
				if (AActor* Target = Info.TargetActor.Get())
				{
					OnLineOfSightChanged.Broadcast(Target, Info.bIsVisible);
				}
			}
		}
	}
}

