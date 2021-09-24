// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UEExtendedVariableLibrary.generated.h"

UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedVariableLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Rotator + Rotator", CompactNodeTitle = "Rotator+", ScriptMethod = "Rotator+", ScriptOperator = "+"), Category="Math|Rotator" )
	static FRotator Add_RotatorRotator(const FRotator A , const FRotator B);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Rotator / float", CompactNodeTitle = "Rotator/", ScriptMethod = "Rotator/", ScriptOperator = "/"), Category="Math|Rotator" )
	static FRotator Divide_RotatorFloat(const FRotator A , const float B);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Rotator - Rotator", CompactNodeTitle = "Rotator-", ScriptMethod = "Rotator-", ScriptOperator = "-"), Category="Math|Rotator" )
	static FRotator Minus_RotatorRotator(const FRotator A , const FRotator B);


	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Transform + Tranform", CompactNodeTitle = "Tranform+", ScriptMethod = "Tranform+", ScriptOperator = "+"), Category="Math|Tranform" )
	static  FTransform Add_TransformTransform(const FTransform A , const FTransform B);


	UFUNCTION(BlueprintCallable,Category="Math|Float")
	static int32 IncrementFloatBy(UPARAM(ref) float& Float , float Value = 1 );

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Add Floats", CompactNodeTitle = "FloatArray+", ScriptMethod = "FloatArray+", ScriptOperator = "+"), Category="Math|Float")
	static float Add_Floats(const TArray<float> Array);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Average Floats", CompactNodeTitle = "FloatArrayAverage", ScriptMethod = "FloatArrayAverage", ScriptOperator = "/"), Category="Math|Float")
	static float Average_Floats(const TArray<float> Array);



	UFUNCTION(BlueprintCallable,Category="Math|Integer")
	static int32 IncrementIntegerBy(UPARAM(ref) int32& Integer , int32 Value = 1 );
	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Add Integers", CompactNodeTitle = "IntegerArray+", ScriptMethod = "IntegerArray+", ScriptOperator = "+"), Category="Math|Integer")
	static int32 Add_Integers(const TArray<int32> Array);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Average Integers", CompactNodeTitle = "IntegerArrayAverage", ScriptMethod = "IntegerArrayAverage", ScriptOperator = "/"), Category="Math|Integer")
	static int32 Average_Integers(const TArray<int32> Array);
	
};