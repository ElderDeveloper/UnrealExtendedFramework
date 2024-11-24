// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimNotifies/AnimNotifyState_Trail.h"
#include "UnrealExtendedFramework/Data/EFEnums.h"
#include "UnrealExtendedFramework/Data/EFTraceData.h"
#include "UObject/Object.h"
#include "EGDamageTrace_Notify.generated.h"


class UEGDamageTrace_DamageObject;


USTRUCT(BlueprintType)
struct FEGNotifyRadialDamageStruct
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere)
	bool bEditEnabled = false;
	
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bEditEnabled"), Category="Trace Damage|Area Falloff")
	float AreaDamageMinimum = 5;
	
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bEditEnabled"), Category="Trace Damage|Area Falloff")
	float AreaDamageMaximum = 10;

	
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bEditEnabled"), Category="Trace Damage|Area")
	float AreaDamageRadius = 200;
	
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bEditEnabled"), Category="Trace Damage|Area Falloff")
	float AreaDamageInnerRadius = 100;
	
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bEditEnabled"), Category="Trace Damage|Area Falloff")
	float AreaDamageOuterRadius = 300;
	
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bEditEnabled"), Category="Trace Damage|Area Falloff")
	float AreaDamageFalloff = 1;
};


USTRUCT(BlueprintType)
struct FEGNotifyDamageStruct
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere ,BlueprintReadWrite)
	TSubclassOf<UEGDamageTrace_DamageObject> CustomApplyDamageObject;

	UPROPERTY(EditAnywhere ,BlueprintReadWrite)
	FGameplayTag DamageCustomTag;

	UPROPERTY(EditAnywhere)
	bool bApplyAnimNotifyDamage = true;

	UPROPERTY(EditAnywhere, Category="Trace Damage|Area")
	TEnumAsByte<ECollisionChannel> AreaDamageBlockChannel = ECollisionChannel::ECC_WorldStatic;

	UPROPERTY(EditAnywhere , Category="Trace Damage")
	TSubclassOf<UDamageType> DamageType;
	
	UPROPERTY(EditAnywhere , Category="Trace Damage")
	TEnumAsByte<EFApplyDamageType> ApplyDamageType = UEF_ApplyDamage;

	
	UPROPERTY(EditAnywhere, meta = (EditCondition = "!ApplyDamageType==EUEFApplyDamageType::UEF_ApplyRadialDamageFalloff") , Category="Trace Damage")
	float AnimNotifyDamage = 10;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "ApplyDamageType==EUEFApplyDamageType::UEF_ApplyRadialDamageFalloff"), Category="Trace Damage|Area Falloff")
	FEGNotifyRadialDamageStruct RadialDamageStruct;
	
	
	FEGNotifyDamageStruct()
	{}
	
};

USTRUCT(BlueprintType)
struct FEGNotifyTraceStruct
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere , Category="TraceSettings")
	TEnumAsByte<ETraceShapes>  TraceShape = ETraceShapes::Sphere;

	UPROPERTY(EditAnywhere  , meta = (EditCondition = "TraceShape==ETraceShapes::Sphere") , Category="Trace Settings")
	FSphereTraceStruct SphereTraceSettings;

	UPROPERTY(EditAnywhere  , meta = (EditCondition = "TraceShape==ETraceShapes::Box") , Category="Trace Settings")
	FBoxTraceStruct BoxTraceSettings;
	
	UPROPERTY(EditAnywhere  , meta = (EditCondition = "TraceShape==ETraceShapes::Line") , Category="Trace Settings")
	FLineTraceStruct LineTraceSettings;
	
};



UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGDamageTrace_Notify : public UAnimNotifyState_Trail
{
	GENERATED_BODY()

	UEGDamageTrace_Notify();
public:


	UPROPERTY(EditAnywhere , Category="Trace Damage")
	bool UseEngineTickForCollisionCalculation = true;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "!UseEngineTickForCollisionCalculation") , Category="Trace Damage")
	float CollisionCalculationTickSpeed = 0.01;
	


	UPROPERTY(EditAnywhere , Category="Trace Damage")
	FEGNotifyTraceStruct NotifyTraceStruct;
	

	UPROPERTY(EditAnywhere , Category="Trace Damage")
	FEGNotifyDamageStruct NotifyDamageStruct;

	
	UPROPERTY(EditAnywhere , Category="Trace Push")
	bool bShouldPush = false;

	UPROPERTY(EditAnywhere , Category="Trace Push")
	float PushStrength = 4000000.0;

private:
	
	UPROPERTY()
	TArray<AActor*> HitActorArray;

	UPROPERTY()
	USkeletalMeshComponent* OwnerMesh;

	FTimerHandle Handle;


	bool DrawSphereTrace(USkeletalMeshComponent* MeshComp , TArray<FHitResult>& HitResults);
	bool DrawBoxTrace(USkeletalMeshComponent* MeshComp, TArray<FHitResult>& HitResults);
	bool DrawLineTrace(USkeletalMeshComponent* MeshComp, TArray<FHitResult>& HitResults);
	void Tick(USkeletalMeshComponent* MeshComp,float FrameDeltaTime);
	
	void DamageTick();
	
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
public:
	
	void HandleDamage(USkeletalMeshComponent* MeshComp , FHitResult HitResult);
};


