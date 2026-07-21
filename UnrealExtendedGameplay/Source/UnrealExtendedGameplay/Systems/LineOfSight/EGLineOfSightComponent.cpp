// Fill out your copyright notice in the Description page of Project Settings.


#include "EGLineOfSightComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "NavigationPath.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "AI/Navigation/NavigationTypes.h"

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
	ProcessPendingBroadcasts(); // Process all pending broadcasts once at end of tick
}

void UEGLineOfSightComponent::UpdateLineOfSightConfig(const float NewFieldOfViewDegrees, const float NewMaxSightDistance)
{
	FieldOfViewDegrees = NewFieldOfViewDegrees;
	MaxSightDistance = NewMaxSightDistance;
}


void UEGLineOfSightComponent::SetEyeComponent(USceneComponent* InEye)
{
	EyeComponent = InEye;
}


void UEGLineOfSightComponent::RegisterLineOfSightActor(AActor* TargetActor, USkeletalMeshComponent* SkeletalMesh, const TArray<FName>& BoneNames)
{
	if (!TargetActor)
		return;
	
	// Clean up stale entries and check for duplicates simultaneously
	for (int32 i = RegisteredTargets.Num() - 1; i >= 0; --i)
	{
		AActor* ExistingTarget = RegisteredTargets[i].TargetActor.Get();
		if (!ExistingTarget)
		{
			RegisteredTargets.RemoveAt(i); // Use RemoveAt instead of RemoveAtSwap to preserve indices
			continue;
		}

		if (ExistingTarget == TargetActor)
		{
			// Update existing entry instead of returning
			RegisteredTargets[i].SkeletalMesh = SkeletalMesh;
			RegisteredTargets[i].BoneNames = BoneNames;
			return;
		}
	}

	FEGLOSTargetInfo NewTarget;
	NewTarget.TargetActor = TargetActor;
	NewTarget.SkeletalMesh = SkeletalMesh;
	NewTarget.BoneNames = BoneNames;
	
	RegisteredTargets.Add(NewTarget);
}

void UEGLineOfSightComponent::RegisterAllPlayerActors(const TArray<FName>& BoneNames)
{
	if (const auto GameState = UGameplayStatics::GetGameState(this))
	{
		for (const auto PlayerStates : GameState->PlayerArray)
		{
			if (PlayerStates && PlayerStates->GetPawn()) // Added null check for PlayerStates
			{
				if (const auto Character = Cast<ACharacter>(PlayerStates->GetPawn()))
				{
					RegisterLineOfSightActor(Character, Character->GetMesh(), BoneNames);
				}
			}
		}
	}
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

	// Clean up invalid targets first (iterate backwards to safely remove)
	for (int32 i = RegisteredTargets.Num() - 1; i >= 0; --i)
	{
		if (!RegisteredTargets[i].TargetActor.IsValid())
		{
			RegisteredTargets.RemoveAt(i);
		}
	}

	for (int32 i = 0; i < RegisteredTargets.Num(); ++i)
	{
		FEGLOSTargetInfo& Info = RegisteredTargets[i];
		AActor* TargetActor = Info.TargetActor.Get();

		// Already cleaned up above, but double-check
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
			UpdateTargetVisibility(Info, false);
			if (bDebugLineOfSight)
			{
				DrawDebugPoint(World, TargetActor->GetActorLocation(), 15.0f, FColor::Orange, false, 0.1f);
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
			UpdateTargetVisibility(Info, false);
			// Target is outside FOV - mark not visible, skip traces
			if (bDebugLineOfSight)
			{
				DrawDebugPoint(World, TargetActor->GetActorLocation(), 15.0f, FColor::Yellow, false, 0.1f);
			}
			continue;
		}

		// NavMesh reachability check - if enabled, treat unreachable targets as not visible
		if (bRequireNavMeshReachability)
		{
			if (!CheckNavMeshReachability(Info))
			{
				UpdateTargetVisibility(Info, false);
				if (bDebugLineOfSight)
				{
					DrawDebugPoint(World, TargetActor->GetActorLocation(), 15.0f, FColor::Purple, false, 0.1f);
				}
				continue;
			}
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
				DrawDebugLine(World, StartLocation, EndLoc, FColor::Cyan, false, 0.f, 0, 0.2f);
			}
		}
	}
	// Removed ProcessPendingBroadcasts() call here - now called once in TickComponent
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

	if (!RegisteredTargets.IsValidIndex(Index))
	{
		return;
	}

	FEGLOSTargetInfo& Info = RegisteredTargets[Index];
	
	// Check if target is still valid
	AActor* TargetActor = Info.TargetActor.Get();
	if (!TargetActor)
	{
		Info.PendingTraces = 0;
		return;
	}
	
	Info.PendingTraces--; // Decrement pending count

	// Check Visibility
	bool bHitTarget = false;
	if (Datum.OutHits.Num() > 0)
	{
		const FHitResult& Hit = Datum.OutHits[0];
		AActor* HitActor = Hit.GetActor();
		
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
			// No blocking hit means nothing blocked the trace - target is visible
			bHitTarget = true;
			if (bDebugLineOfSight)
			{
				DrawDebugPoint(GetWorld(), Datum.End, 10.0f, FColor::Blue, false, 0.5f);
			}
		}
	}
	else
	{
		// No hits at all - trace reached end unobstructed
		bHitTarget = true;
		if (bDebugLineOfSight)
		{
			DrawDebugPoint(GetWorld(), Datum.End, 10.0f, FColor::Blue, false, 0.5f);
		}
	}

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
		
		UpdateTargetVisibility(Info, (Ratio >= MinVisibilityRatio));
	}

	// Removed ProcessPendingBroadcasts() call here - now called once in TickComponent
}

void UEGLineOfSightComponent::UpdateTargetVisibility(FEGLOSTargetInfo& Info, bool bNewVisibility)
{
	Info.bIsVisible = bNewVisibility;

	if (Info.bIsVisible != Info.bWasVisible)
	{
		Info.bWasVisible = Info.bIsVisible;
        
		// Don't broadcast yet! Just store the data.
		if (AActor* Target = Info.TargetActor.Get())
		{
			FPendingLOSChange Change;
			Change.Target = Target;
			Change.bIsVisible = Info.bIsVisible;
			PendingBroadcasts.Add(Change);
		}
	}
}

void UEGLineOfSightComponent::ProcessPendingBroadcasts()
{
	for (const FPendingLOSChange& Change : PendingBroadcasts)
	{
		if (bDebugLineOfSight)
			UE_LOG(LogTemp, Log, TEXT("Target '%s' visibility: %d"), *GetNameSafe(Change.Target), Change.bIsVisible);
		OnLineOfSightChanged.Broadcast(Change.Target, Change.bIsVisible);
	}
	PendingBroadcasts.Reset();
}

bool UEGLineOfSightComponent::CheckNavMeshReachability(FEGLOSTargetInfo& Info)
{
	AActor* TargetActor = Info.TargetActor.Get();
	if (!TargetActor)
	{
		return false;
	}

	// Throttle navmesh checks based on interval
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - Info.LastNavMeshCheckTime < NavMeshReachabilityCheckInterval)
	{
		// Return cached result
		return Info.bIsNavMeshReachable;
	}
	Info.LastNavMeshCheckTime = CurrentTime;

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return true; // Fail open if no owner
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		// No navigation system - fail open (assume reachable)
		Info.bIsNavMeshReachable = true;
		return true;
	}

	const FVector StartLocation = Owner->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();

	// Use synchronous path finding to check reachability
	UNavigationPath* NavPath = NavSys->FindPathToLocationSynchronously(GetWorld(), StartLocation, TargetLocation, Owner);

	if (NavPath && NavPath->IsValid() && !NavPath->IsPartial())
	{
		// Full path exists - target is reachable
		Info.bIsNavMeshReachable = true;
		
		if (bDebugLineOfSight)
		{
			// Draw path in green
			for (int32 j = 0; j < NavPath->PathPoints.Num() - 1; ++j)
			{
				DrawDebugLine(GetWorld(), NavPath->PathPoints[j], NavPath->PathPoints[j + 1], FColor::Green, false, NavMeshReachabilityCheckInterval, 0, 2.0f);
			}
		}
	}
	else
	{
		// No path or partial path - target is unreachable
		Info.bIsNavMeshReachable = false;
		
		if (bDebugLineOfSight)
		{
			// Draw a line to target in purple to indicate unreachable
			DrawDebugLine(GetWorld(), StartLocation, TargetLocation, FColor::Purple, false, NavMeshReachabilityCheckInterval, 0, 2.0f);
		}
	}

	return Info.bIsNavMeshReachable;
}
