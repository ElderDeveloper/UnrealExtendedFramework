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
	
};





UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedTraceData : public UObject
{
	GENERATED_BODY()
};
