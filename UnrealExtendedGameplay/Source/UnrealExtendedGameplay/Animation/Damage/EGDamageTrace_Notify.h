// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState_Trail.h"
#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "UnrealExtendedFramework/Data/EFEnums.h"
#include "UnrealExtendedFramework/Data/EFTraceData.h"

#include "EGDamageTrace_Notify.generated.h"

class UDamageType;
class UEGDamageTrace_DamageObject;

/** Settings for UEF_ApplyRadialDamage (AreaDamageRadius) and UEF_ApplyRadialDamageFalloff (the rest). */
USTRUCT(BlueprintType)
struct FEGNotifyRadialDamageStruct
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Trace Damage|Area Falloff")
	float AreaDamageMinimum = 5;

	UPROPERTY(EditAnywhere, Category = "Trace Damage|Area Falloff")
	float AreaDamageMaximum = 10;

	UPROPERTY(EditAnywhere, Category = "Trace Damage|Area")
	float AreaDamageRadius = 200;

	UPROPERTY(EditAnywhere, Category = "Trace Damage|Area Falloff")
	float AreaDamageInnerRadius = 100;

	UPROPERTY(EditAnywhere, Category = "Trace Damage|Area Falloff")
	float AreaDamageOuterRadius = 300;

	UPROPERTY(EditAnywhere, Category = "Trace Damage|Area Falloff")
	float AreaDamageFalloff = 1;
};

USTRUCT(BlueprintType)
struct FEGNotifyDamageStruct
{
	GENERATED_BODY()

public:
	/** Called for every hit actor, in addition to the built-in damage when that is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace Damage")
	TSubclassOf<UEGDamageTrace_DamageObject> CustomApplyDamageObject;

	/** Handed to the custom damage object as DamageTag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace Damage")
	FGameplayTag DamageCustomTag;

	/** Apply engine damage (ApplyDamageType below) to every hit actor. */
	UPROPERTY(EditAnywhere, Category = "Trace Damage")
	bool bApplyAnimNotifyDamage = true;

	UPROPERTY(EditAnywhere, Category = "Trace Damage|Area")
	TEnumAsByte<ECollisionChannel> AreaDamageBlockChannel = ECollisionChannel::ECC_WorldStatic;

	UPROPERTY(EditAnywhere, Category = "Trace Damage")
	TSubclassOf<UDamageType> DamageType;

	UPROPERTY(EditAnywhere, Category = "Trace Damage")
	TEnumAsByte<EFApplyDamageType> ApplyDamageType = UEF_ApplyDamage;

	UPROPERTY(EditAnywhere, Category = "Trace Damage", meta = (EditCondition = "ApplyDamageType != EFApplyDamageType::UEF_ApplyRadialDamageFalloff"))
	float AnimNotifyDamage = 10;

	UPROPERTY(EditAnywhere, Category = "Trace Damage|Area", meta = (EditCondition = "ApplyDamageType == EFApplyDamageType::UEF_ApplyRadialDamage || ApplyDamageType == EFApplyDamageType::UEF_ApplyRadialDamageFalloff"))
	FEGNotifyRadialDamageStruct RadialDamageStruct;
};

USTRUCT(BlueprintType)
struct FEGNotifyTraceStruct
{
	GENERATED_BODY()

public:
	/** Sphere, Box or Line. Start and End are filled from the trail's first and second sockets every trace. */
	UPROPERTY(EditAnywhere, Category = "Trace Settings")
	TEnumAsByte<ETraceShapes> TraceShape = ETraceShapes::Sphere;

	UPROPERTY(EditAnywhere, Category = "Trace Settings", meta = (EditCondition = "TraceShape==ETraceShapes::Sphere"))
	FSphereTraceStruct SphereTraceSettings;

	UPROPERTY(EditAnywhere, Category = "Trace Settings", meta = (EditCondition = "TraceShape==ETraceShapes::Box"))
	FBoxTraceStruct BoxTraceSettings;

	UPROPERTY(EditAnywhere, Category = "Trace Settings", meta = (EditCondition = "TraceShape==ETraceShapes::Line"))
	FLineTraceStruct LineTraceSettings;
};

/**
 * Traces between the trail's two sockets for the length of the window and damages each actor it
 * touches once per window. Inherits the trail notify, so a trail effect can be set as well.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGDamageTrace_Notify : public UAnimNotifyState_Trail
{
	GENERATED_BODY()

public:
	UEGDamageTrace_Notify();

	/**
	 * Trace and apply damage only where the owner has authority (server or standalone). Clients
	 * damaging their own copies of actors desynchronise health and fire death events locally; turn
	 * this off only for client-side or cosmetic-only setups.
	 */
	UPROPERTY(EditAnywhere, Category = "Trace Damage")
	bool bAuthorityOnly = true;

	/** Trace on every anim tick. Off traces on a timer at CollisionCalculationTickSpeed instead. */
	UPROPERTY(EditAnywhere, Category = "Trace Damage")
	bool UseEngineTickForCollisionCalculation = true;

	UPROPERTY(EditAnywhere, Category = "Trace Damage", meta = (EditCondition = "!UseEngineTickForCollisionCalculation", ClampMin = "0.001", Units = "s"))
	float CollisionCalculationTickSpeed = 0.01f;

	UPROPERTY(EditAnywhere, Category = "Trace Damage")
	FEGNotifyTraceStruct NotifyTraceStruct;

	UPROPERTY(EditAnywhere, Category = "Trace Damage")
	FEGNotifyDamageStruct NotifyDamageStruct;

	UPROPERTY(EditAnywhere, Category = "Trace Push")
	bool bShouldPush = false;

	/** Force added to a hit character's movement for one frame, away from the attacker. */
	UPROPERTY(EditAnywhere, Category = "Trace Push", meta = (EditCondition = "bShouldPush"))
	float PushStrength = 4000000.0f;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	void HandleDamage(USkeletalMeshComponent* MeshComp, const FHitResult& HitResult);

private:
	struct FActiveTrace
	{
		TArray<TWeakObjectPtr<AActor>> HitActors;
		FTimerHandle TimerHandle;
		/** Kept so the timer can still be cleared after the mesh is gone. */
		TWeakObjectPtr<UWorld> World;
	};

	/** One entry per mesh currently inside this window. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FActiveTrace> ActiveTraces;

	bool ShouldTrace(const USkeletalMeshComponent* MeshComp) const;
	bool RunTrace(USkeletalMeshComponent* MeshComp, TArray<FHitResult>& OutHits) const;
	void TraceAndApply(USkeletalMeshComponent* MeshComp);
	void HandleTraceTimer(TWeakObjectPtr<USkeletalMeshComponent> WeakMesh);
	void ApplyPush(USkeletalMeshComponent* MeshComp, AActor* HitActor) const;
	void StopTrace(const TWeakObjectPtr<USkeletalMeshComponent>& MeshKey);
	static void ClearTraceTimer(FActiveTrace& Trace);
};
