// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/Object.h"
#include "UEExtendedTraceData.generated.h"


UENUM(BlueprintType,Blueprintable)
enum ETraceTypes
{
	TraceType		UMETA(DisplayName = "Channel"),
	ProfileType		UMETA(DisplayName = "Profile"),
	ObjectsType		UMETA(DisplayName = "Object"),
};

UENUM(BlueprintType,Blueprintable)
enum ETraceShapes
{
	Line	UMETA(DisplayName = "Line Trace"),
	Sphere  UMETA(DisplayName = "Sphere Trace"),
	Capsule UMETA(DisplayName = "Capsule Trace"),
	Box		UMETA(DisplayName = "Box Trace")
};





USTRUCT(BlueprintType)
struct FLineTraceStruct
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<ETraceTypes> TraceType = ETraceTypes::TraceType;
	
	UPROPERTY(BlueprintReadWrite)
	FHitResult HitResult;

	UPROPERTY(BlueprintReadWrite)
	TArray<FHitResult> HitResults;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector Start = FVector::ZeroVector;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector End = FVector::ZeroVector ;

	UPROPERTY(EditAnywhere,BlueprintReadWrite , meta = (EditCondition = "TraceType==ETraceTypes::TraceType"))
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditAnywhere,BlueprintReadWrite , meta = (EditCondition = "TraceType==ETraceTypes::ProfileType"))
	FName TraceProfileName = FName();

	UPROPERTY(EditAnywhere,BlueprintReadWrite, meta = (EditCondition = "TraceType==ETraceTypes::ObjectsType"))
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bTraceComplex = false;

	UPROPERTY(BlueprintReadWrite)
	TArray<AActor*> ActorsToIgnore;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType = EDrawDebugTrace::Type::None;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bIgnoreSelf = true;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FLinearColor TraceColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FLinearColor TraceHitColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float DrawTime = 5.f;

	FLineTraceStruct()
	{
		
	}

	FLineTraceStruct(FVector start , FVector end )
	{
		Start =start;
		End = end;
	}


	/**
	 * Location in world space of the actual contact of the trace shape (box, sphere, ray, etc) with the impacted object.
	 * Example: for a sphere trace test, this is the point where the surface of the sphere touches the other object.
	 * @note: In the case of initial overlap (bStartPenetrating=true), ImpactPoint will be the same as Location because there is no meaningful single impact point to report.
	 */
	FVector GetHitImpactPoint() const { return HitResult.ImpactPoint; }

	/**
	 * Normal of the hit in world space, for the object that was hit by the sweep, if any.
	 * For example if a sphere hits a flat plane, this is a normalized vector pointing out from the plane.
	 * In the case of impact with a corner or edge of a surface, usually the "most opposing" normal (opposed to the query direction) is chosen.
	 */
	FVector GetHitImpactNormal() const { return HitResult.ImpactNormal; }

	/**
	 * The location in world space where the moving shape would end up against the impacted object, if there is a hit. Equal to the point of impact for line tests.
	 * Example: for a sphere trace test, this is the point where the center of the sphere would be located when it touched the other object.
	 * For swept movement (but not queries) this may not equal the final location of the shape since hits are pulled back slightly to prevent precision issues from overlapping another surface.
	 */
	FVector GetHitLocation() const { return HitResult.Location; }

	/**
	 * Normal of the hit in world space, for the object that was swept. Equal to ImpactNormal for line tests.
	 * This is computed for capsules and spheres, otherwise it will be the same as ImpactNormal.
	 * Example: for a sphere trace test, this is a normalized vector pointing in towards the center of the sphere at the point of impact.
	 */
	FVector GetHitNormal() const { return HitResult.Normal; }

	/**
	 * Start location of the trace.
	 * For example if a sphere is swept against the world, this is the starting location of the center of the sphere.
	 */
	FVector GetHitTraceStart() const { return HitResult.TraceStart; }

	/**
	 * End location of the trace; this is NOT where the impact occurred (if any), but the furthest point in the attempted sweep.
	 * For example if a sphere is swept against the world, this would be the center of the sphere if there was no blocking hit.
	 */
	FVector GetHitTraceEnd() const { return HitResult.TraceEnd; }

	/** Actor hit by the trace. */
	AActor* GetHitActor() const { return HitResult.GetActor(); }

	/** PrimitiveComponent hit by the trace. */
	UPrimitiveComponent* GetHitComponent() const { return HitResult.Component.Get(); }

	/** The distance from the TraceStart to the Location in world space. This value is 0 if there was an initial overlap (trace started inside another colliding object). */
	float GetHitDistance() const { return HitResult.Distance; }

	/** Indicates if this hit was a result of blocking collision. If false, there was no hit or it was an overlap/touch instead. */
	bool GetHitBlockingHit() const { return HitResult.bBlockingHit; }

	/** Name of bone we hit (for skeletal meshes). */
	FName GetHitBoneName() const { return HitResult.BoneName; }

	/**
	 * Physical material that was hit.
	 * @note Must set bReturnPhysicalMaterial on the swept PrimitiveComponent or in the query params for this to be returned.
	 */
	UPhysicalMaterial* GetHitPhysMaterial() const { return HitResult.PhysMaterial.Get();}
	
};





USTRUCT(BlueprintType)
struct FSphereTraceStruct
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<ETraceTypes> TraceType = ETraceTypes::TraceType;
	
	UPROPERTY(BlueprintReadWrite)
	FHitResult HitResult;

	UPROPERTY(BlueprintReadWrite)
	TArray<FHitResult> HitResults;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector Start = FVector::ZeroVector;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector End = FVector::ZeroVector ;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float Radius = 0.f;

	UPROPERTY(EditAnywhere,BlueprintReadWrite , meta = (EditCondition = "TraceType==ETraceTypes::TraceType"))
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditAnywhere,BlueprintReadWrite , meta = (EditCondition = "TraceType==ETraceTypes::ProfileType"))
	FName TraceProfileName = FName();

	UPROPERTY(EditAnywhere,BlueprintReadWrite, meta = (EditCondition = "TraceType==ETraceTypes::ObjectsType"))
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bTraceComplex = false;

	UPROPERTY(BlueprintReadWrite)
	TArray<AActor*> ActorsToIgnore;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType = EDrawDebugTrace::Type::None;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bIgnoreSelf = true;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FLinearColor TraceColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FLinearColor TraceHitColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float DrawTime = 5.f;
	
	FSphereTraceStruct()
	{
		Start = FVector::ZeroVector;
		End  = FVector::ZeroVector;
	}
	FSphereTraceStruct(FVector start , FVector end , float radius )
	{
		Start =start;
		End = end;
		Radius = radius;
	}


	/**
	 * Location in world space of the actual contact of the trace shape (box, sphere, ray, etc) with the impacted object.
	 * Example: for a sphere trace test, this is the point where the surface of the sphere touches the other object.
	 * @note: In the case of initial overlap (bStartPenetrating=true), ImpactPoint will be the same as Location because there is no meaningful single impact point to report.
	 */
	FVector GetHitImpactPoint() const { return HitResult.ImpactPoint; }

	/**
	 * Normal of the hit in world space, for the object that was hit by the sweep, if any.
	 * For example if a sphere hits a flat plane, this is a normalized vector pointing out from the plane.
	 * In the case of impact with a corner or edge of a surface, usually the "most opposing" normal (opposed to the query direction) is chosen.
	 */
	FVector GetHitImpactNormal() const { return HitResult.ImpactNormal; }

	/**
	 * The location in world space where the moving shape would end up against the impacted object, if there is a hit. Equal to the point of impact for line tests.
	 * Example: for a sphere trace test, this is the point where the center of the sphere would be located when it touched the other object.
	 * For swept movement (but not queries) this may not equal the final location of the shape since hits are pulled back slightly to prevent precision issues from overlapping another surface.
	 */
	FVector GetHitLocation() const { return HitResult.Location; }

	/**
	 * Normal of the hit in world space, for the object that was swept. Equal to ImpactNormal for line tests.
	 * This is computed for capsules and spheres, otherwise it will be the same as ImpactNormal.
	 * Example: for a sphere trace test, this is a normalized vector pointing in towards the center of the sphere at the point of impact.
	 */
	FVector GetHitNormal() const { return HitResult.Normal; }

	/**
	 * Start location of the trace.
	 * For example if a sphere is swept against the world, this is the starting location of the center of the sphere.
	 */
	FVector GetHitTraceStart() const { return HitResult.TraceStart; }

	/**
	 * End location of the trace; this is NOT where the impact occurred (if any), but the furthest point in the attempted sweep.
	 * For example if a sphere is swept against the world, this would be the center of the sphere if there was no blocking hit.
	 */
	FVector GetHitTraceEnd() const { return HitResult.TraceEnd; }

	/** Actor hit by the trace. */
	AActor* GetHitActor() const { return HitResult.GetActor(); }

	/** PrimitiveComponent hit by the trace. */
	UPrimitiveComponent* GetHitComponent() const { return HitResult.Component.Get(); }

	/** The distance from the TraceStart to the Location in world space. This value is 0 if there was an initial overlap (trace started inside another colliding object). */
	float GetHitDistance() const { return HitResult.Distance; }

	/** Indicates if this hit was a result of blocking collision. If false, there was no hit or it was an overlap/touch instead. */
	bool GetHitBlockingHit() const { return HitResult.bBlockingHit; }

	/** Name of bone we hit (for skeletal meshes). */
	FName GetHitBoneName() const { return HitResult.BoneName; }

	/**
	 * Physical material that was hit.
	 * @note Must set bReturnPhysicalMaterial on the swept PrimitiveComponent or in the query params for this to be returned.
	 */
	UPhysicalMaterial* GetHitPhysMaterial() const { return HitResult.PhysMaterial.Get();}
};





USTRUCT(BlueprintType)
struct FCapsuleTraceStruct
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<ETraceTypes> TraceType = ETraceTypes::TraceType;
	
	UPROPERTY(BlueprintReadWrite)
	FHitResult HitResult;

	UPROPERTY(BlueprintReadWrite)
	TArray<FHitResult> HitResults;

	UPROPERTY(BlueprintReadWrite)
	FVector Start = FVector::ZeroVector ;

	UPROPERTY(BlueprintReadWrite)
	FVector End  = FVector::ZeroVector;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float Radius = 0.f;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float HalfHeight = 0.f;
	
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TraceType==ETraceTypes::TraceType"))
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditDefaultsOnly , meta = (EditCondition = "TraceType==ETraceTypes::ProfileType"))
	FName TraceProfileName = FName();

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TraceType==ETraceTypes::ObjectsType"))
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bTraceComplex = false;

	UPROPERTY(BlueprintReadWrite)
	TArray<AActor*> ActorsToIgnore;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType = EDrawDebugTrace::Type::None;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bIgnoreSelf = true;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FLinearColor TraceColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FLinearColor TraceHitColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float DrawTime = 5.f;

	FCapsuleTraceStruct()
	{
	}
	FCapsuleTraceStruct(FVector start , FVector end , float radius , float halfHeight )
	{
		Start =start;
		End = end;
		Radius = radius;
		HalfHeight = halfHeight;
	}


	/**
	 * Location in world space of the actual contact of the trace shape (box, sphere, ray, etc) with the impacted object.
	 * Example: for a sphere trace test, this is the point where the surface of the sphere touches the other object.
	 * @note: In the case of initial overlap (bStartPenetrating=true), ImpactPoint will be the same as Location because there is no meaningful single impact point to report.
	 */
	FVector GetHitImpactPoint() const { return HitResult.ImpactPoint; }

	/**
	 * Normal of the hit in world space, for the object that was hit by the sweep, if any.
	 * For example if a sphere hits a flat plane, this is a normalized vector pointing out from the plane.
	 * In the case of impact with a corner or edge of a surface, usually the "most opposing" normal (opposed to the query direction) is chosen.
	 */
	FVector GetHitImpactNormal() const { return HitResult.ImpactNormal; }

	/**
	 * The location in world space where the moving shape would end up against the impacted object, if there is a hit. Equal to the point of impact for line tests.
	 * Example: for a sphere trace test, this is the point where the center of the sphere would be located when it touched the other object.
	 * For swept movement (but not queries) this may not equal the final location of the shape since hits are pulled back slightly to prevent precision issues from overlapping another surface.
	 */
	FVector GetHitLocation() const { return HitResult.Location; }

	/**
	 * Normal of the hit in world space, for the object that was swept. Equal to ImpactNormal for line tests.
	 * This is computed for capsules and spheres, otherwise it will be the same as ImpactNormal.
	 * Example: for a sphere trace test, this is a normalized vector pointing in towards the center of the sphere at the point of impact.
	 */
	FVector GetHitNormal() const { return HitResult.Normal; }

	/**
	 * Start location of the trace.
	 * For example if a sphere is swept against the world, this is the starting location of the center of the sphere.
	 */
	FVector GetHitTraceStart() const { return HitResult.TraceStart; }

	/**
	 * End location of the trace; this is NOT where the impact occurred (if any), but the furthest point in the attempted sweep.
	 * For example if a sphere is swept against the world, this would be the center of the sphere if there was no blocking hit.
	 */
	FVector GetHitTraceEnd() const { return HitResult.TraceEnd; }

	/** Actor hit by the trace. */
	AActor* GetHitActor() const { return HitResult.GetActor(); }

	/** PrimitiveComponent hit by the trace. */
	UPrimitiveComponent* GetHitComponent() const { return HitResult.Component.Get(); }

	/** The distance from the TraceStart to the Location in world space. This value is 0 if there was an initial overlap (trace started inside another colliding object). */
	float GetHitDistance() const { return HitResult.Distance; }

	/** Indicates if this hit was a result of blocking collision. If false, there was no hit or it was an overlap/touch instead. */
	bool GetHitBlockingHit() const { return HitResult.bBlockingHit; }

	/** Name of bone we hit (for skeletal meshes). */
	FName GetHitBoneName() const { return HitResult.BoneName; }

	/**
	 * Physical material that was hit.
	 * @note Must set bReturnPhysicalMaterial on the swept PrimitiveComponent or in the query params for this to be returned.
	 */
	UPhysicalMaterial* GetHitPhysMaterial() const { return HitResult.PhysMaterial.Get();}
};





USTRUCT(BlueprintType)
struct FBoxTraceStruct
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<ETraceTypes> TraceType = ETraceTypes::TraceType;
	
	UPROPERTY(BlueprintReadOnly)
	FHitResult HitResult;

	UPROPERTY(BlueprintReadOnly)
	TArray<FHitResult> HitResults;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector Start = FVector::ZeroVector ;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector End = FVector::ZeroVector ;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector HalfSize = FVector::ZeroVector;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FRotator Orientation = FRotator::ZeroRotator;
	
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TraceType==ETraceTypes::TraceType"))
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditDefaultsOnly , meta = (EditCondition = "TraceType==ETraceTypes::ProfileType"))
	FName TraceProfileName = FName();

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TraceType==ETraceTypes::ObjectsType"))
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bTraceComplex = false;

	UPROPERTY(BlueprintReadWrite)
	TArray<AActor*> ActorsToIgnore;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType = EDrawDebugTrace::Type::None;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bIgnoreSelf = true;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FLinearColor TraceColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FLinearColor TraceHitColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float DrawTime = 5.f;

	FBoxTraceStruct()
	{
		
	}
	FBoxTraceStruct(FVector start , FVector end , FVector halfSize , FRotator orientation )
	{
		Start =start;
		End = end;
		HalfSize = halfSize;
		Orientation = orientation;
	}


	/**
	 * Location in world space of the actual contact of the trace shape (box, sphere, ray, etc) with the impacted object.
	 * Example: for a sphere trace test, this is the point where the surface of the sphere touches the other object.
	 * @note: In the case of initial overlap (bStartPenetrating=true), ImpactPoint will be the same as Location because there is no meaningful single impact point to report.
	 */
	FVector GetHitImpactPoint() const { return HitResult.ImpactPoint; }

	/**
	 * Normal of the hit in world space, for the object that was hit by the sweep, if any.
	 * For example if a sphere hits a flat plane, this is a normalized vector pointing out from the plane.
	 * In the case of impact with a corner or edge of a surface, usually the "most opposing" normal (opposed to the query direction) is chosen.
	 */
	FVector GetHitImpactNormal() const { return HitResult.ImpactNormal; }

	/**
	 * The location in world space where the moving shape would end up against the impacted object, if there is a hit. Equal to the point of impact for line tests.
	 * Example: for a sphere trace test, this is the point where the center of the sphere would be located when it touched the other object.
	 * For swept movement (but not queries) this may not equal the final location of the shape since hits are pulled back slightly to prevent precision issues from overlapping another surface.
	 */
	FVector GetHitLocation() const { return HitResult.Location; }

	/**
	 * Normal of the hit in world space, for the object that was swept. Equal to ImpactNormal for line tests.
	 * This is computed for capsules and spheres, otherwise it will be the same as ImpactNormal.
	 * Example: for a sphere trace test, this is a normalized vector pointing in towards the center of the sphere at the point of impact.
	 */
	FVector GetHitNormal() const { return HitResult.Normal; }

	/**
	 * Start location of the trace.
	 * For example if a sphere is swept against the world, this is the starting location of the center of the sphere.
	 */
	FVector GetHitTraceStart() const { return HitResult.TraceStart; }

	/**
	 * End location of the trace; this is NOT where the impact occurred (if any), but the furthest point in the attempted sweep.
	 * For example if a sphere is swept against the world, this would be the center of the sphere if there was no blocking hit.
	 */
	FVector GetHitTraceEnd() const { return HitResult.TraceEnd; }

	/** Actor hit by the trace. */
	AActor* GetHitActor() const { return HitResult.GetActor(); }

	/** PrimitiveComponent hit by the trace. */
	UPrimitiveComponent* GetHitComponent() const { return HitResult.Component.Get(); }

	/** The distance from the TraceStart to the Location in world space. This value is 0 if there was an initial overlap (trace started inside another colliding object). */
	float GetHitDistance() const { return HitResult.Distance; }

	/** Indicates if this hit was a result of blocking collision. If false, there was no hit or it was an overlap/touch instead. */
	bool GetHitBlockingHit() const { return HitResult.bBlockingHit; }

	/** Name of bone we hit (for skeletal meshes). */
	FName GetHitBoneName() const { return HitResult.BoneName; }

	/**
	 * Physical material that was hit.
	 * @note Must set bReturnPhysicalMaterial on the swept PrimitiveComponent or in the query params for this to be returned.
	 */
	UPhysicalMaterial* GetHitPhysMaterial() const { return HitResult.PhysMaterial.Get();}
	
};





UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedTraceData : public UObject
{
	GENERATED_BODY()
};
