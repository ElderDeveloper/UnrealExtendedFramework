// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/Object.h"
#include "UEExtendedTraceData.generated.h"


UENUM(BlueprintType,Blueprintable)
enum ETraceTypes
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
	FHitResult HitResult;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TArray<FHitResult> HitResults;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector Start;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector End ;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<ETraceTypeQuery> TraceChannel;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bTraceComplex;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TArray<AActor*> ActorsToIgnore;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bIgnoreSelf;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FLinearColor TraceColor;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FLinearColor TraceHitColor;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float DrawTime;
	

	FLineTraceStruct()
	{
		Start = FVector::ZeroVector;
		End = FVector::ZeroVector;
		bTraceComplex = false;
		bIgnoreSelf = true;
		TraceChannel = ETraceTypeQuery::TraceTypeQuery1;
		DrawDebugType = EDrawDebugTrace::Type::None;
		TraceColor = FLinearColor::Red;
		TraceHitColor = FLinearColor::Green;
		DrawTime = 5.f;
	}

	FLineTraceStruct(FVector start , FVector end )
	{
		Start =start;
		End = end;

		bTraceComplex = false;
		bIgnoreSelf = true;
		TraceChannel = ETraceTypeQuery::TraceTypeQuery1;
		DrawDebugType = EDrawDebugTrace::Type::None;
		TraceColor = FLinearColor::Red;
		TraceHitColor = FLinearColor::Green;
		DrawTime = 5.f;
	}
	
};





USTRUCT(BlueprintType)
struct FSphereTraceStruct
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FHitResult HitResult;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TArray<FHitResult> HitResults;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector Start;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector End ;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float Radius;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<ETraceTypeQuery> TraceChannel;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bTraceComplex;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TArray<AActor*> ActorsToIgnore;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bIgnoreSelf;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FLinearColor TraceColor;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FLinearColor TraceHitColor;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float DrawTime;
	
	FSphereTraceStruct()
	{
		Start = FVector::ZeroVector;
		End = FVector::ZeroVector;
		Radius = 32.f;
		bTraceComplex = false;
		bIgnoreSelf = true;
		TraceChannel = ETraceTypeQuery::TraceTypeQuery1;
		DrawDebugType = EDrawDebugTrace::Type::None;
		TraceColor = FLinearColor::Red;
		TraceHitColor = FLinearColor::Green;
		DrawTime = 5.f;
	}

	FSphereTraceStruct(FVector start , FVector end , float radius )
	{
		Start =start;
		End = end;
		Radius = radius;

		bTraceComplex = false;
		bIgnoreSelf = true;
		TraceChannel = ETraceTypeQuery::TraceTypeQuery1;
		DrawDebugType = EDrawDebugTrace::Type::None;
		TraceColor = FLinearColor::Red;
		TraceHitColor = FLinearColor::Green;
		DrawTime = 5.f;
	}
};





USTRUCT(BlueprintType)
struct FCapsuleTrace
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FHitResult HitResult;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TArray<FHitResult> HitResults;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector Start ;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector End ;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float Radius;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float HalfHeight;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<ETraceTypeQuery> TraceChannel;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bTraceComplex;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TArray<AActor*> ActorsToIgnore;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bIgnoreSelf;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FLinearColor TraceColor;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FLinearColor TraceHitColor;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float DrawTime;
	
	FCapsuleTrace()
	{
		Start = FVector::ZeroVector;
		End = FVector::ZeroVector;
		Radius = 32.f;
		HalfHeight = 32.f;
		bTraceComplex = false;
		bIgnoreSelf = true;
		TraceChannel = ETraceTypeQuery::TraceTypeQuery1;
		DrawDebugType = EDrawDebugTrace::Type::None;
		TraceColor = FLinearColor::Red;
		TraceHitColor = FLinearColor::Green;
		DrawTime = 5.f;
	}

	FCapsuleTrace(FVector start , FVector end , float radius , float half )
	{
		Start =start;
		End = end;
		Radius = radius;
		HalfHeight = half;

		bTraceComplex = false;
		bIgnoreSelf = true;
		TraceChannel = ETraceTypeQuery::TraceTypeQuery1;
		DrawDebugType = EDrawDebugTrace::Type::None;
		TraceColor = FLinearColor::Red;
		TraceHitColor = FLinearColor::Green;
		DrawTime = 5.f;
	}
};





USTRUCT(BlueprintType)
struct FBoxTrace
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FHitResult HitResult;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TArray<FHitResult> HitResults;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector Start ;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector End ;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FVector HalfSize;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<ETraceTypeQuery> TraceChannel;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bTraceComplex;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TArray<AActor*> ActorsToIgnore;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bIgnoreSelf;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FLinearColor TraceColor;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FLinearColor TraceHitColor;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float DrawTime;
	
	FBoxTrace()
	{
		Start = FVector::ZeroVector;
		End = FVector::ZeroVector;
		HalfSize = FVector(32,32,32);
		bTraceComplex = false;
		bIgnoreSelf = true;
		TraceChannel = ETraceTypeQuery::TraceTypeQuery1;
		DrawDebugType = EDrawDebugTrace::Type::None;
		TraceColor = FLinearColor::Red;
		TraceHitColor = FLinearColor::Green;
		DrawTime = 5.f;
	}

	FBoxTrace(FVector start , FVector end , FVector half )
	{
		Start =start;
		End = end;
		HalfSize = half;

		bTraceComplex = false;
		bIgnoreSelf = true;
		TraceChannel = ETraceTypeQuery::TraceTypeQuery1;
		DrawDebugType = EDrawDebugTrace::Type::None;
		TraceColor = FLinearColor::Red;
		TraceHitColor = FLinearColor::Green;
		DrawTime = 5.f;
	}
};





UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedTraceData : public UObject
{
	GENERATED_BODY()

};
