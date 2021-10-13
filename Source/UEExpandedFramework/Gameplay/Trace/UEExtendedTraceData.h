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
	TEnumAsByte<ETraceTypes> TraceType;
	
	UPROPERTY(BlueprintReadWrite)
	FHitResult HitResult;

	UPROPERTY(BlueprintReadWrite)
	TArray<FHitResult> HitResults;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector Start;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector End ;

	UPROPERTY(EditAnywhere,BlueprintReadWrite , meta = (EditCondition = "TraceType==ETraceTypes::TraceType"))
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditAnywhere,BlueprintReadWrite , meta = (EditCondition = "TraceType==ETraceTypes::ProfileType"))
	FName TraceProfileName;

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
		Start = FVector::ZeroVector;
		End = FVector::ZeroVector;
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
	TEnumAsByte<ETraceTypes> TraceType;
	
	UPROPERTY(BlueprintReadWrite)
	FHitResult HitResult;

	UPROPERTY(BlueprintReadWrite)
	TArray<FHitResult> HitResults;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector Start;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector End ;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float Radius;

	UPROPERTY(EditAnywhere,BlueprintReadWrite , meta = (EditCondition = "TraceType==ETraceTypes::TraceType"))
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditAnywhere,BlueprintReadWrite , meta = (EditCondition = "TraceType==ETraceTypes::ProfileType"))
	FName TraceProfileName;

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
		End = FVector::ZeroVector;
		Radius = 32.f;
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
	TEnumAsByte<ETraceTypes> TraceType;
	
	UPROPERTY(BlueprintReadWrite)
	FHitResult HitResult;

	UPROPERTY(BlueprintReadWrite)
	TArray<FHitResult> HitResults;

	UPROPERTY(BlueprintReadWrite)
	FVector Start ;

	UPROPERTY(BlueprintReadWrite)
	FVector End ;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float Radius;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float HalfHeight;
	
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TraceType==ETraceTypes::TraceType"))
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditDefaultsOnly , meta = (EditCondition = "TraceType==ETraceTypes::ProfileType"))
	FName TraceProfileName;

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
};





USTRUCT(BlueprintType)
struct FBoxTraceStruct
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<ETraceTypes> TraceType;
	
	UPROPERTY(BlueprintReadOnly)
	FHitResult HitResult;

	UPROPERTY(BlueprintReadOnly)
	TArray<FHitResult> HitResults;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector Start ;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector End ;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector HalfSize;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FRotator Orientation;
	
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TraceType==ETraceTypes::TraceType"))
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditDefaultsOnly , meta = (EditCondition = "TraceType==ETraceTypes::ProfileType"))
	FName TraceProfileName;

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
	
};





UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedTraceData : public UObject
{
	GENERATED_BODY()

};
