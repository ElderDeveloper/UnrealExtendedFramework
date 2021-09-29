// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UEExtendedVariableLibrary.generated.h"

UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedVariableLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	public:
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< ROTATOR >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Rotator + Rotator", CompactNodeTitle = "Rotator+", ScriptMethod = "Rotator+", ScriptOperator = "+"), Category="Math|Rotator" )
	static FRotator Add_RotatorRotator(const FRotator A , const FRotator B);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Rotator / float", CompactNodeTitle = "Rotator/", ScriptMethod = "Rotator/", ScriptOperator = "/"), Category="Math|Rotator" )
	static FRotator Divide_RotatorFloat(const FRotator A , const float B);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Rotator - Rotator", CompactNodeTitle = "Rotator-", ScriptMethod = "Rotator-", ScriptOperator = "-"), Category="Math|Rotator" )
	static FRotator Minus_RotatorRotator(const FRotator A , const FRotator B);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Rotator Yaw", CompactNodeTitle = "Yaw", ScriptMethod = "Yaw", ScriptOperator = "Yaw"), Category="Math|Rotator" )
	static float Rotator_GetYaw(const FRotator A) { return A.Yaw; }

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Rotator Pitch", CompactNodeTitle = "Pitch", ScriptMethod = "Pitch", ScriptOperator = "Pitch"), Category="Math|Rotator" )
	static float Rotator_GetPitch(const FRotator A) { return A.Pitch; }

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Rotator Roll", CompactNodeTitle = "Roll", ScriptMethod = "Roll", ScriptOperator = "Roll"), Category="Math|Rotator" )
	static float Rotator_GetRoll(const FRotator A) { return A.Roll; }



	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< VECTOR >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure , meta=(DisplayName = "Compare Vector Sizes", CompactNodeTitle = ">", ScriptMethod = ">", ScriptOperator = ">"),Category="Math|Vector")
	static bool CompareVectorSizes(const FVector IsBigger , const FVector IsLesser);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Vector X", CompactNodeTitle = "X", ScriptMethod = "X", ScriptOperator = "X"), Category="Math|Vector" )
	static float Vector_GetX(const FVector A) { return A.X; }

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Vector Y", CompactNodeTitle = "Y", ScriptMethod = "Y", ScriptOperator = "Y"), Category="Math|Vector" )
	static float Vector_GetY(const FVector A) { return A.Y; }

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Vector Z", CompactNodeTitle = "Z", ScriptMethod = "Z", ScriptOperator = "Z"), Category="Math|Vector" )
	static float Vector_GetZ(const FVector A) { return A.Z; }


	

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< TRANSFORM >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Transform + Tranform", CompactNodeTitle = "Tranform+", ScriptMethod = "Tranform+", ScriptOperator = "+"), Category="Math|Tranform" )
	static  FTransform Add_TransformTransform(const FTransform A , const FTransform B);

	


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< FLOAT >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable,Category="Math|Float")
	static int32 IncrementFloatBy(UPARAM(ref) float& Float , float Value = 1 );

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Add Floats", CompactNodeTitle = "FloatArray+", ScriptMethod = "FloatArray+", ScriptOperator = "+"), Category="Math|Float")
	static float Add_Floats(const TArray<float> Array);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Average Floats", CompactNodeTitle = "FloatArrayAverage", ScriptMethod = "FloatArrayAverage", ScriptOperator = "/"), Category="Math|Float")
	static float Average_Floats(const TArray<float> Array);


	

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< INTEGER >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable,Category="Math|Integer")
	static int32 IncrementIntegerBy(UPARAM(ref) int32& Integer , int32 Value = 1 );
	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Add Integers", CompactNodeTitle = "IntegerArray+", ScriptMethod = "IntegerArray+", ScriptOperator = "+"), Category="Math|Integer")
	static int32 Add_Integers(const TArray<int32> Array);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Average Integers", CompactNodeTitle = "IntegerArrayAverage", ScriptMethod = "IntegerArrayAverage", ScriptOperator = "/"), Category="Math|Integer")
	static int32 Average_Integers(const TArray<int32> Array);

	
	
};