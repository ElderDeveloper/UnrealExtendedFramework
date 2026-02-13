
// Fill out your copyright notice in the Description page of Project Settings.

#include "EFAsyncCharacterMoveTo.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "AIResources.h"
#include "GameplayTasksComponent.h"

UEFAsyncCharacterMoveTo::UEFAsyncCharacterMoveTo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsPausable = true;
	bTickingTask = true;

	AcceptanceRadius = 50.f;
	PathPointThreshold = 50.f;
	CurrentPathIndex = 0;
	MoveResult = EPathFollowingResult::Invalid;
	bIsMoving = false;
	bNeedsRepath = false;
	bIsRecovering = false;
	PathFindingRequestID = 0;
	NavMeshRecoveryRadius = 1000.f;
	RecoveryTargetPoint = FVector::ZeroVector;

	AddRequiredResource(UAIResource_Movement::StaticClass());
	AddClaimedResource(UAIResource_Movement::StaticClass());
}


UEFAsyncCharacterMoveTo* UEFAsyncCharacterMoveTo::ForceMoveToLocationOrActor(
	AAIController* Controller,
	FVector InGoalLocation,
	AActor* InGoalActor,

	float InAcceptanceRadius,
	float InPathPointThreshold,
	bool bLockAILogic)
{
	if (!Controller)
	{
		return nullptr;
	}

	// Check if there's already an active movement task for this controller
	if (UEFAsyncCharacterMoveTo* ExistingTask = FindExistingMoveTask(Controller))
	{
		// Update existing task instead of creating new one
		if (InGoalActor)
		{
			ExistingTask->UpdateGoalActor(InGoalActor);
		}
		else
		{
			ExistingTask->UpdateGoalLocation(InGoalLocation);
		}
        
		ExistingTask->AcceptanceRadius = InAcceptanceRadius;
		ExistingTask->PathPointThreshold = InPathPointThreshold;
		return ExistingTask;
	}

	// Create new task only if none exists
	UEFAsyncCharacterMoveTo* MyTask = UAITask::NewAITask<UEFAsyncCharacterMoveTo>(*Controller, EAITaskPriority::High);
	if (MyTask)
	{
		MyTask->OwnerController = Controller;
		MyTask->GoalActor = InGoalActor;
		MyTask->GoalLocation = InGoalLocation;

		MyTask->AcceptanceRadius = InAcceptanceRadius;
		MyTask->PathPointThreshold = InPathPointThreshold;

		APawn* Pawn = Controller->GetPawn();
		MyTask->CachedPawn = Pawn;
        
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			MyTask->CachedMovementComponent = Character->GetCharacterMovement();
		}

		if (bLockAILogic)
		{
			MyTask->RequestAILogicLocking();
		}
	}

	return MyTask;
}

void UEFAsyncCharacterMoveTo::UpdateGoalLocation(FVector NewGoalLocation)
{
	GoalLocation = NewGoalLocation;
	GoalActor.Reset();
	bNeedsRepath = true;
}

void UEFAsyncCharacterMoveTo::UpdateGoalActor(AActor* NewGoalActor)
{
	GoalActor = NewGoalActor;
	bNeedsRepath = true;
}

void UEFAsyncCharacterMoveTo::Activate()
{
	Super::Activate();

	if (!CachedPawn.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UEFAsyncCharacterMoveTo Activate: Invalid pawn!"));
		FinishMoveTask(EPathFollowingResult::Invalid);
		return;
	}

	RequestPath();
}


void UEFAsyncCharacterMoveTo::OnDestroy(bool TaskOwnerFinished)
{
	// Clear velocity when task ends
	if (CachedMovementComponent.IsValid())
	{
		CachedMovementComponent->Velocity = FVector::ZeroVector;
	}

	// Clear timer
	if (OwnerController)
	{
		OwnerController->GetWorldTimerManager().ClearTimer(RepathTimerHandle);
	}

	CurrentPath = nullptr;
	bIsMoving = false;

	Super::OnDestroy(TaskOwnerFinished);
}

void UEFAsyncCharacterMoveTo::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!CachedPawn.IsValid() || !OwnerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("UEFAsyncCharacterMoveTo TickTask: Invalid pawn or controller!"));
		FinishMoveTask(EPathFollowingResult::Invalid);
		return;
	}

	if (bIsRecovering)
	{
		MoveTowardsRecoveryPoint(DeltaTime);
		return;
	}

	if (bNeedsRepath)
	{
		bNeedsRepath = false;
		RequestPath();
		return;
	}

	if (bIsMoving && CurrentPath.IsValid() && CurrentPath->GetPathPoints().Num() > 0)
	{
		MoveAlongPath(DeltaTime);
	}
}


void UEFAsyncCharacterMoveTo::RequestPath()
{
    if (!CachedPawn.IsValid() || !OwnerController)
    {
        UE_LOG(LogTemp, Warning, TEXT("UEFAsyncCharacterMoveTo RequestPath: Invalid pawn or controller!"));
        FinishMoveTask(EPathFollowingResult::Invalid);
        return;
    }

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!NavSys)
    {
        UE_LOG(LogTemp, Warning, TEXT("UEFAsyncCharacterMoveTo RequestPath: No navigation system found!"));
        FinishMoveTask(EPathFollowingResult::Invalid);
        return;
    }

    const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
    if (!NavData)
    {
        UE_LOG(LogTemp, Warning, TEXT("UEFAsyncCharacterMoveTo RequestPath: No nav data found!"));
        FinishMoveTask(EPathFollowingResult::Invalid);
        return;
    }

    const FVector StartLocation = CachedPawn->GetActorLocation();
    const FVector TargetLocation = GetCurrentGoalLocation();

    if (HasReachedGoal(StartLocation))
    {
        FinishMoveTask(EPathFollowingResult::Success);
        return;
    }

    FPathFindingQuery Query;
    Query.StartLocation = StartLocation;
    Query.EndLocation = TargetLocation;
    Query.NavData = NavData;
    Query.Owner = OwnerController;
    
    // Get query filter from the navigation data's default filter
    Query.QueryFilter = NavData->GetDefaultQueryFilter();

    if (const INavAgentInterface* NavAgent = Cast<INavAgentInterface>(CachedPawn.Get()))
    {
        Query.NavAgentProperties = NavAgent->GetNavAgentPropertiesRef();
    }

    FPathFindingResult Result = NavSys->FindPathSync(Query, EPathFindingMode::Regular);

    UE_LOG(LogTemp, Log, TEXT("UEFAsyncCharacterMoveTo: FindPathSync Result=%d, PathValid=%d, PathPoints=%d, IsReady=%d"),
        (int32)Result.Result,
        Result.Path.IsValid() ? 1 : 0,
        Result.Path.IsValid() ? Result.Path->GetPathPoints().Num() : 0,
        Result.Path.IsValid() ? Result.Path->IsReady() : 0);

    if (Result.IsSuccessful() && Result.Path.IsValid())
    {
        const TArray<FNavPathPoint>& PathPoints = Result.Path->GetPathPoints();
        if (PathPoints.Num() > 0)
        {
            CurrentPath = Result.Path;
            CurrentPathIndex = 0;
            bIsMoving = true;
            UE_LOG(LogTemp, Log, TEXT("UEFAsyncCharacterMoveTo: Path found with %d points"), PathPoints.Num());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("UEFAsyncCharacterMoveTo: Path valid but empty, creating direct path"));

            FNavPathSharedPtr DirectPath = MakeShareable(new FNavigationPath());
            DirectPath->GetPathPoints().Add(FNavPathPoint(StartLocation));
            DirectPath->GetPathPoints().Add(FNavPathPoint(TargetLocation));
            DirectPath->MarkReady();

            CurrentPath = DirectPath;
            CurrentPathIndex = 0;
            bIsMoving = true;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("UEFAsyncCharacterMoveTo RequestPath: Path finding failed! Result: %d"),
            (int32)Result.Result);
        FinishMoveTask(EPathFollowingResult::Invalid);
    }
}


void UEFAsyncCharacterMoveTo::OnPathFindingComplete(uint32 RequestID, ENavigationQueryResult::Type Result, FNavPathSharedPtr ResultPath)
{
	if (RequestID != PathFindingRequestID)
	{
		return;
	}

	if (Result != ENavigationQueryResult::Success || !ResultPath.IsValid() || ResultPath->GetPathPoints().Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UEFAsyncCharacterMoveTo OnPathFindingComplete: Invalid path!"));
		FinishMoveTask(EPathFollowingResult::Invalid);
		return;
	}

	CurrentPath = ResultPath;
	CurrentPathIndex = 0;
	bIsMoving = true;
}

void UEFAsyncCharacterMoveTo::MoveAlongPath(float DeltaTime)
{
    if (!CachedPawn.IsValid() || !CurrentPath.IsValid() || !CachedMovementComponent.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("UEFAsyncCharacterMoveTo MoveAlongPath: Invalid path, pawn, or movement component!"));
        FinishMoveTask(EPathFollowingResult::Invalid);
        return;
    }

    const TArray<FNavPathPoint>& PathPoints = CurrentPath->GetPathPoints();

    if (CurrentPathIndex >= PathPoints.Num())
    {
        FinishMoveTask(EPathFollowingResult::Success);
        return;
    }

    const FVector CurrentLocation = CachedPawn->GetActorLocation();

    // Check if we've reached the final goal
    if (HasReachedGoal(CurrentLocation))
    {
        // Clear input instead of directly setting velocity
        CachedMovementComponent->ConsumeInputVector();
        FinishMoveTask(EPathFollowingResult::Success);
        return;
    }

    // Get target path point
    FVector TargetPoint = PathPoints[CurrentPathIndex].Location;
    TargetPoint.Z = CurrentLocation.Z;

    // Check if we've reached current path point
    if (HasReachedPathPoint(CurrentLocation, TargetPoint, PathPointThreshold))
    {
        CurrentPathIndex++;

        if (CurrentPathIndex >= PathPoints.Num())
        {
            CachedMovementComponent->ConsumeInputVector();
            FinishMoveTask(EPathFollowingResult::Success);
            return;
        }

        TargetPoint = PathPoints[CurrentPathIndex].Location;
        TargetPoint.Z = CurrentLocation.Z;
    }

    // Calculate direction and use AddInputVector instead of setting velocity directly
    const FVector Direction = (TargetPoint - CurrentLocation).GetSafeNormal();
    
    if (!Direction.IsNearlyZero())
    {
        // Use AddInputVector to work with character movement component's rotation settings
        CachedMovementComponent->AddInputVector(Direction);
        
        // If using controller desired rotation, update the controller's control rotation
        if (CachedMovementComponent->bUseControllerDesiredRotation && OwnerController)
        {
            const FRotator DesiredRotation = Direction.Rotation();
            OwnerController->SetControlRotation(FRotator(0.f, DesiredRotation.Yaw, 0.f));
        }
        // If using orient rotation to movement, the character movement component will handle it automatically
    }
}


void UEFAsyncCharacterMoveTo::FinishMoveTask(EPathFollowingResult::Type InResult)
{
	bIsMoving = false;
	MoveResult = InResult;

	// Clear input instead of directly setting velocity
	if (CachedMovementComponent.IsValid())
	{
		CachedMovementComponent->ConsumeInputVector();
	}

	EndTask();

	if (InResult == EPathFollowingResult::Invalid)
	{
		OnRequestFailed.Broadcast();
	}
	else
	{
		OnMoveFinished.Broadcast(InResult, OwnerController);
	}
}


bool UEFAsyncCharacterMoveTo::HasReachedPathPoint(const FVector& CurrentLocation, const FVector& TargetPoint, float Threshold) const
{
	return FVector::Dist2D(CurrentLocation, TargetPoint) <= Threshold;
}

bool UEFAsyncCharacterMoveTo::HasReachedGoal(const FVector& CurrentLocation) const
{
	const FVector TargetLocation = GetCurrentGoalLocation();
	return FVector::Dist2D(CurrentLocation, TargetLocation) <= AcceptanceRadius;
}

FVector UEFAsyncCharacterMoveTo::GetCurrentGoalLocation() const
{
	if (GoalActor.IsValid())
	{
		return GoalActor->GetActorLocation();
	}
	return GoalLocation;
}


bool UEFAsyncCharacterMoveTo::IsOnNavMesh() const
{
	if (!CachedPawn.IsValid())
	{
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		return false;
	}

	FNavLocation NavLocation;
	const FVector CurrentLocation = CachedPawn->GetActorLocation();

	return NavSys->ProjectPointToNavigation(CurrentLocation, NavLocation, FVector(50.f, 50.f, 100.f));
}

bool UEFAsyncCharacterMoveTo::FindNearestNavMeshPoint(FVector& OutNavMeshPoint) const
{
	if (!CachedPawn.IsValid())
	{
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		return false;
	}

	const FVector CurrentLocation = CachedPawn->GetActorLocation();
	FNavLocation NavLocation;

	if (NavSys->ProjectPointToNavigation(CurrentLocation, NavLocation, FVector(NavMeshRecoveryRadius, NavMeshRecoveryRadius, NavMeshRecoveryRadius)))
	{
		OutNavMeshPoint = NavLocation.Location;
		return true;
	}

	return false;
}

bool UEFAsyncCharacterMoveTo::TryRecoverToNavMesh()
{
	return FindNearestNavMeshPoint(RecoveryTargetPoint);
}

void UEFAsyncCharacterMoveTo::MoveTowardsRecoveryPoint(float DeltaTime)
{
	if (!CachedPawn.IsValid() || !CachedMovementComponent.IsValid())
	{
		FinishMoveTask(EPathFollowingResult::Invalid);
		return;
	}

	const FVector CurrentLocation = CachedPawn->GetActorLocation();
	const float DistanceToRecovery = FVector::Dist2D(CurrentLocation, RecoveryTargetPoint);

	if (DistanceToRecovery <= 50.f)
	{
		UE_LOG(LogTemp, Log, TEXT("UEFAsyncCharacterMoveTo: Recovery complete, resuming normal pathfinding"));
		bIsRecovering = false;
		RequestPath();
		return;
	}

	FVector Direction = (RecoveryTargetPoint - CurrentLocation).GetSafeNormal();

	if (!Direction.IsNearlyZero())
	{
		CachedMovementComponent->AddInputVector(Direction);

		if (CachedMovementComponent->bUseControllerDesiredRotation && OwnerController)
		{
			const FRotator DesiredRotation = Direction.Rotation();
			OwnerController->SetControlRotation(FRotator(0.f, DesiredRotation.Yaw, 0.f));
		}
	}
}


UEFAsyncCharacterMoveTo* UEFAsyncCharacterMoveTo::FindExistingMoveTask(AAIController* Controller)
{
	if (!Controller)
	{
		return nullptr;
	}

	UGameplayTasksComponent* TasksComponent = Controller->GetGameplayTasksComponent();
	if (!TasksComponent)
	{
		return nullptr;
	}

	// Use FindResourceConsumingTaskByName with the class name
	UGameplayTask* ExistingTask = TasksComponent->FindResourceConsumingTaskByName(UEFAsyncCharacterMoveTo::StaticClass()->GetFName());
    
	if (ExistingTask)
	{
		UE_LOG(LogTemp, Log, TEXT("UEFAsyncCharacterMoveTo: Found existing move task for controller with name"));
		return Cast<UEFAsyncCharacterMoveTo>(ExistingTask);
	}

	// Pass the controller itself as IGameplayTaskOwnerInterface
	TArray<UGameplayTask*> Tasks;
	TasksComponent->FindAllResourceConsumingTasksOwnedBy(*Controller, Tasks);

	for (UGameplayTask* Task : Tasks)
	{
		if (UEFAsyncCharacterMoveTo* MoveTask = Cast<UEFAsyncCharacterMoveTo>(Task))
		{
			return MoveTask;
		}
	}

	return nullptr;
}