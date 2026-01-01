// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EGLineOfSightComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLineOfSightChanged, AActor*, Target, bool, bIsVisible);

struct FEGLOSTargetInfo
{
	TWeakObjectPtr<AActor> TargetActor;
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMesh;
	TArray<FName> BoneNames;
	
	bool bIsVisible;
	bool bWasVisible; // For detecting changes

	// State tracking for async traces
	int32 PendingTraces;
	int32 VisibleCount;
	int32 TotalScheduledTraces; // Need to know total for ratio

	FEGLOSTargetInfo()
		: bIsVisible(false)
		, bWasVisible(false)
		, PendingTraces(0)
		, VisibleCount(0)
		, TotalScheduledTraces(0)
	{}
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGLineOfSightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEGLineOfSightComponent();
	
	UFUNCTION(BlueprintCallable, Category = "Line Of Sight")
	void SetEyeComponent(USceneComponent* InEye);

	/** Registers an actor to be tracked for line of sight
	 *  @param TargetActor - The actor to look for
	 *  @param SkeletalMesh - Optional: If provided, specific bones will be traced
	 *  @param BoneNames - List of bones to trace to. If empty and SkeletalMesh is valid, might fallback to root or ignore.
	 */
	UFUNCTION(BlueprintCallable, Category = "Line Of Sight")
	void RegisterLineOfSightActor(AActor* TargetActor, USkeletalMeshComponent* SkeletalMesh, const TArray<FName>& BoneNames);
	
	UFUNCTION(BlueprintCallable, Category = "Line Of Sight")
	bool IsActorVisible(AActor* TargetActor) const;

	UPROPERTY(BlueprintAssignable, Category = "Line Of Sight")
	FOnLineOfSightChanged OnLineOfSightChanged;

protected:
	void PerformAsyncTraces();
	void OnTraceCompleted(const FTraceHandle& Handle, FTraceDatum& Datum);

	UPROPERTY(Transient)
	USceneComponent* EyeComponent;
	TArray<FEGLOSTargetInfo> RegisteredTargets;

	// Map TraceHandle to a specific Target Index and what we are tracing for (if complex)
	// For simplicity, we might just store the Index and completion status
	TMap<FTraceHandle, int32> TraceHandleToTargetIndex;

	FTraceDelegate TraceDelegate;
	
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;


public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line Of Sight|Settings")
	bool bLineOfSightEnabled = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line Of Sight|Trace Settings")
	TEnumAsByte<ECollisionChannel> LineOfSightChannel = ECC_Visibility;
	
	// Maximum distance to see a target.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line Of Sight|Settings")
	float MaxSightDistance = 5000.0f;

	// Ratio (0.0 - 1.0) of successful traces required to consider target visible.
	// E.g. If 0.5, and we trace Head and Chest, seeing just one is enough (1/2 = 0.5).
	// If 1.0, must see ALL bones.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line Of Sight|Settings", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinVisibilityRatio = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line Of Sight|Settings")
	float FieldOfViewDegrees = 120.0f; // Half-angle, so 120° = 240° total cone

	// Debug drawing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line Of Sight|Debug")
	bool bDebugLineOfSight = false;
};
