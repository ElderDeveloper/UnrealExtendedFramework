
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/AITask.h"
#include "Navigation/PathFollowingComponent.h"
#include "EFAsyncCharacterMoveTo.generated.h"

class AAIController;
class UCharacterMovementComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEFMoveTaskCompletedSignature, TEnumAsByte<EPathFollowingResult::Type>, Result, AAIController*, AIController);

/**
 * Custom AI movement task that moves characters along nav mesh path using SetActorLocation.
 * Ignores collisions and forces movement along the generated path.
 * Sets velocity for animation support.
 */
UCLASS(BlueprintType, Blueprintable)
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncCharacterMoveTo : public UAITask
{
	GENERATED_BODY()

public:
	UEFAsyncCharacterMoveTo(const FObjectInitializer& ObjectInitializer);

	/**
	 * Move to a location or actor using SetActorLocation (ignores collisions)
	 * @param Controller The AI Controller
	 * @param GoalLocation Target location (used if GoalActor is null)
	 * @param GoalActor Target actor to move towards
	 * @param AcceptanceRadius How close to the goal is acceptable
	 * @param bLockAILogic Whether to lock AI logic during movement
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (DefaultToSelf = "Controller", DisplayName = "Force Move To Location or Actor"))
	static UEFAsyncCharacterMoveTo* ForceMoveToLocationOrActor(
		AAIController* Controller,
		FVector GoalLocation,
		AActor* GoalActor = nullptr,
		float AcceptanceRadius = 50.f,
		float PathPointThreshold = 50.f,
		bool bLockAILogic = true);

	/** Update the goal location while task is running (for external tick updates) */
	UFUNCTION(BlueprintCallable, Category = "AI|Tasks")
	void UpdateGoalLocation(FVector NewGoalLocation);

	/** Update the goal actor while task is running */
	UFUNCTION(BlueprintCallable, Category = "AI|Tasks")
	void UpdateGoalActor(AActor* NewGoalActor);

	/** Check if the task is currently moving */
	UFUNCTION(BlueprintCallable, Category = "AI|Tasks")
	bool IsCurrentlyMoving() const { return bIsMoving; }

	/** Threshold for reaching intermediate path points */
	UPROPERTY(BlueprintReadWrite, Category = "AI|Tasks")
	float PathPointThreshold;

	/** Get the current move result */
	EPathFollowingResult::Type GetMoveResult() const { return MoveResult; }

	/** Called when movement finishes */
	UPROPERTY(BlueprintAssignable)
	FEFMoveTaskCompletedSignature OnMoveFinished;

	/** Called when path request fails */
	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnRequestFailed;

protected:
	virtual void Activate() override;
	virtual void OnDestroy(bool TaskOwnerFinished) override;
	virtual void TickTask(float DeltaTime) override;

	/** Request a new path to the current goal */
	void RequestPath();

	/** Called when path finding completes */
	void OnPathFindingComplete(uint32 RequestID, ENavigationQueryResult::Type Result, FNavPathSharedPtr ResultPath);

	/** Move along the current path */
	void MoveAlongPath(float DeltaTime);

	/** Finish the move task with result */
	void FinishMoveTask(EPathFollowingResult::Type InResult);

	/** Check if we've reached the current path point */
	bool HasReachedPathPoint(const FVector& CurrentLocation, const FVector& TargetPoint, float Threshold) const;

	/** Check if we've reached the final goal */
	bool HasReachedGoal(const FVector& CurrentLocation) const;

	/** Get the current goal location (from actor or direct location) */
	FVector GetCurrentGoalLocation() const;
	

	/** Try to find path back to navmesh when character is off-mesh */
	bool TryRecoverToNavMesh();
    
	/** Check if character is currently on navmesh */
	bool IsOnNavMesh() const;
    
	/** Find nearest point on navmesh from current location */
	bool FindNearestNavMeshPoint(FVector& OutNavMeshPoint) const;
    
	/** Move towards recovery point */
	void MoveTowardsRecoveryPoint(float DeltaTime);
	
	/** Find existing move task for controller */
	static UEFAsyncCharacterMoveTo* FindExistingMoveTask(AAIController* Controller);
	

private:
	/** Target location for movement */
	UPROPERTY()
	FVector GoalLocation;

	/** Target actor for movement */
	UPROPERTY()
	TWeakObjectPtr<AActor> GoalActor;

	/** Acceptance radius for goal */
	UPROPERTY()
	float AcceptanceRadius;

	/** Current path being followed */
	FNavPathSharedPtr CurrentPath;

	/** Current index in the path points */
	int32 CurrentPathIndex;

	/** Movement result */
	TEnumAsByte<EPathFollowingResult::Type> MoveResult;

	/** Is task currently performing movement */
	uint8 bIsMoving : 1;

	/** Has the goal been updated and needs repath */
	uint8 bNeedsRepath : 1;

	/** Cached character movement component */
	UPROPERTY()
	TWeakObjectPtr<UCharacterMovementComponent> CachedMovementComponent;

	/** Cached pawn */
	UPROPERTY()
	TWeakObjectPtr<APawn> CachedPawn;

	/** Path finding request ID */
	uint32 PathFindingRequestID;

	/** Timer handle for repath delay */
	FTimerHandle RepathTimerHandle;
	

	/** Search radius for finding navmesh when off-mesh */
	UPROPERTY()
	float NavMeshRecoveryRadius;
    
	/** Is currently in recovery mode (moving back to navmesh) */
	uint8 bIsRecovering : 1;
    
	/** Recovery target point on navmesh */
	UPROPERTY()
	FVector RecoveryTargetPoint;
	
};
