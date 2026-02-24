// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/Object.h"
#include "EFTraceData.generated.h"


/** Specifies whether the trace uses a collision channel, profile name, or object type filter. */
UENUM(BlueprintType, Blueprintable)
enum ETraceTypes
{
	TraceType		UMETA(DisplayName = "Channel"),
	ProfileType		UMETA(DisplayName = "Profile"),
	ObjectsType		UMETA(DisplayName = "Object"),
};

/** Specifies the geometric shape used for the trace sweep. */
UENUM(BlueprintType, Blueprintable)
enum ETraceShapes
{
	Line	UMETA(DisplayName = "Line Trace"),
	Sphere  UMETA(DisplayName = "Sphere Trace"),
	Capsule UMETA(DisplayName = "Capsule Trace"),
	Box		UMETA(DisplayName = "Box Trace")
};


/**
 * Shared helper functions for accessing FHitResult data from trace structs.
 * All trace structs inherit from this to avoid duplicating ~12 accessor functions.
 */
USTRUCT(BlueprintType)
struct FTraceHitHelpers
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	FHitResult HitResult;

	UPROPERTY(BlueprintReadWrite)
	TArray<FHitResult> HitResults;

	/** Location in world space of the actual contact point with the impacted object. */
	FVector GetHitImpactPoint() const { return HitResult.ImpactPoint; }

	/** Normal of the hit in world space, for the object that was hit. */
	FVector GetHitImpactNormal() const { return HitResult.ImpactNormal; }

	/** The location where the moving shape would end up against the impacted object. */
	FVector GetHitLocation() const { return HitResult.Location; }

	/** Normal of the hit for the swept object. Equal to ImpactNormal for line tests. */
	FVector GetHitNormal() const { return HitResult.Normal; }

	/** Start location of the trace. */
	FVector GetHitTraceStart() const { return HitResult.TraceStart; }

	/** End location of the trace (furthest point in the attempted sweep). */
	FVector GetHitTraceEnd() const { return HitResult.TraceEnd; }

	/** Actor hit by the trace. */
	AActor* GetHitActor() const { return HitResult.GetActor(); }

	/** PrimitiveComponent hit by the trace. */
	UPrimitiveComponent* GetHitComponent() const { return HitResult.Component.Get(); }

	/** Distance from TraceStart to the hit Location. 0 if initial overlap. */
	float GetHitDistance() const { return HitResult.Distance; }

	/** True if this was a blocking hit. */
	bool GetHitBlockingHit() const { return HitResult.bBlockingHit; }

	/** Name of the bone hit (for skeletal meshes). */
	FName GetHitBoneName() const { return HitResult.BoneName; }

	/** Physical material that was hit. Requires bReturnPhysicalMaterial. */
	UPhysicalMaterial* GetHitPhysMaterial() const { return HitResult.PhysMaterial.Get(); }

	/** Populates Array with all actors from the multi-hit results. */
	void GetAllActorsFromHitArray(TArray<AActor*>& Array) const
	{
		Array.Reserve(Array.Num() + HitResults.Num());
		for (const auto& Hit : HitResults)
		{
			Array.Add(Hit.GetActor());
		}
	}
};


USTRUCT(BlueprintType)
struct FLineTraceStruct : public FTraceHitHelpers
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<ETraceTypes> TraceType = ETraceTypes::TraceType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Start = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector End = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "TraceType==ETraceTypes::TraceType"))
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "TraceType==ETraceTypes::ProfileType"))
	FName TraceProfileName = FName();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "TraceType==ETraceTypes::ObjectsType"))
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bTraceComplex = false;

	UPROPERTY(BlueprintReadWrite)
	TArray<AActor*> ActorsToIgnore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType = EDrawDebugTrace::Type::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIgnoreSelf = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor TraceColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor TraceHitColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DrawTime = 5.f;

	FLineTraceStruct() {}

	FLineTraceStruct(FVector start, FVector end)
		: Start(start), End(end)
	{}
};




USTRUCT(BlueprintType)
struct FSphereTraceStruct : public FTraceHitHelpers
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<ETraceTypes> TraceType = ETraceTypes::TraceType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Start = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector End = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Radius = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "TraceType==ETraceTypes::TraceType"))
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "TraceType==ETraceTypes::ProfileType"))
	FName TraceProfileName = FName();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "TraceType==ETraceTypes::ObjectsType"))
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bTraceComplex = false;

	UPROPERTY(BlueprintReadWrite)
	TArray<AActor*> ActorsToIgnore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType = EDrawDebugTrace::Type::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIgnoreSelf = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor TraceColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor TraceHitColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DrawTime = 5.f;
	
	FSphereTraceStruct() {}

	FSphereTraceStruct(float radius)
		: Radius(radius)
	{}
	
	FSphereTraceStruct(FVector start, FVector end, float radius)
		: Start(start), End(end), Radius(radius)
	{}
};




USTRUCT(BlueprintType)
struct FCapsuleTraceStruct : public FTraceHitHelpers
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<ETraceTypes> TraceType = ETraceTypes::TraceType;

	UPROPERTY(BlueprintReadWrite)
	FVector Start = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite)
	FVector End = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Radius = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HalfHeight = 0.f;
	
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TraceType==ETraceTypes::TraceType"))
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TraceType==ETraceTypes::ProfileType"))
	FName TraceProfileName = FName();

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TraceType==ETraceTypes::ObjectsType"))
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bTraceComplex = false;

	UPROPERTY(BlueprintReadWrite)
	TArray<AActor*> ActorsToIgnore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType = EDrawDebugTrace::Type::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIgnoreSelf = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor TraceColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor TraceHitColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DrawTime = 5.f;

	FCapsuleTraceStruct() {}
	
	FCapsuleTraceStruct(float radius, float halfHeight)
		: Radius(radius), HalfHeight(halfHeight)
	{}
	
	FCapsuleTraceStruct(FVector start, FVector end, float radius, float halfHeight)
		: Start(start), End(end), Radius(radius), HalfHeight(halfHeight)
	{}
};




USTRUCT(BlueprintType)
struct FBoxTraceStruct : public FTraceHitHelpers
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<ETraceTypes> TraceType = ETraceTypes::TraceType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Start = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector End = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector HalfSize = FVector(32, 32, 32);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator Orientation = FRotator::ZeroRotator;
	
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TraceType==ETraceTypes::TraceType"))
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TraceType==ETraceTypes::ProfileType"))
	FName TraceProfileName = FName();

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TraceType==ETraceTypes::ObjectsType"))
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bTraceComplex = false;

	UPROPERTY(BlueprintReadWrite)
	TArray<AActor*> ActorsToIgnore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType = EDrawDebugTrace::Type::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIgnoreSelf = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor TraceColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor TraceHitColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DrawTime = 5.f;

	FBoxTraceStruct() {}

	FBoxTraceStruct(FVector halfSize, FRotator orientation)
		: HalfSize(halfSize), Orientation(orientation)
	{}
	
	FBoxTraceStruct(FVector start, FVector end, FVector halfSize, FRotator orientation)
		: Start(start), End(end), HalfSize(halfSize), Orientation(orientation)
	{}
};



UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFTraceData : public UObject
{
	GENERATED_BODY()
};
