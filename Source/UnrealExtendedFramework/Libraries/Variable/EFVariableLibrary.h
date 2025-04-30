﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EFVariableLibrary.generated.h"


UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFVariableLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()


	public:
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< ROTATOR >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Rotator+Rotator", CompactNodeTitle = "Rotator+"), Category="Math|Rotator" )
	static FRotator Add_RotatorRotator(const FRotator A , const FRotator B);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Rotator/float", CompactNodeTitle = "Rotator/"), Category="Math|Rotator" )
	static FRotator Divide_RotatorFloat(const FRotator A , const float B);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Rotator-Rotator", CompactNodeTitle = "Rotator-"), Category="Math|Rotator" )
	static FRotator Minus_RotatorRotator(const FRotator A , const FRotator B);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Rotator Yaw", CompactNodeTitle = "Yaw"), Category="Math|Rotator" )
	static float Rotator_GetYaw(const FRotator A) { return A.Yaw; }

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Rotator Pitch", CompactNodeTitle = "Pitch"), Category="Math|Rotator" )
	static float Rotator_GetPitch(const FRotator A) { return A.Pitch; }

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Rotator Roll", CompactNodeTitle = "Roll"), Category="Math|Rotator" )
	static float Rotator_GetRoll(const FRotator A) { return A.Roll; }



	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< VECTOR >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure , meta=(DisplayName = "Compare Vector Sizes"),Category="Math|Vector")
	static bool CompareVectorSizes(const FVector IsBigger , const FVector IsLesser);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Vector X", CompactNodeTitle = "X"), Category="Math|Vector" )
	static float Vector_GetX(const FVector A) { return A.X; }

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Vector Y", CompactNodeTitle = "Y"), Category="Math|Vector" )
	static float Vector_GetY(const FVector A) { return A.Y; }

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Vector Z", CompactNodeTitle = "Z"), Category="Math|Vector" )
	static float Vector_GetZ(const FVector A) { return A.Z; }

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Normalize Scaled"), Category="Math|Vector" )
	static FVector Vector_NormalizeScaled(const FVector Vector , float Tolerance = 0.0001 , float Scale = 1);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Vector Up Down"), Category="Math|Vector" )
	static void VectorGetUpDownValues(const FVector& VectorRef , float UpValue , float DownValue , FVector& UpVector , FVector& DownVector);


	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Int To Vector"), Category="Math|Vector" )
	static FVector IntToVector(const int32 Value) {	return FVector(Value,Value,Value);}

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Float To Vector"), Category="Math|Vector" )
	static FVector FloatToVector(const float Value) {	return FVector(Value,Value,Value);}

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte To Vector"), Category="Math|Vector" )
	static FVector ByteToVector(const uint8 Value) {	return FVector(Value,Value,Value);}
	
	

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< TRANSFORM >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Transform + Tranform", CompactNodeTitle = "Tranform+"), Category="Math|Tranform" )
	static  FTransform Add_TransformTransform(const FTransform A , const FTransform B);

	


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< FLOAT >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Increment Float", CompactNodeTitle = "Float+") ,Category="Math|Float")
	static float IncrementFloatBy(UPARAM(ref) float& Float , float Value = 1 );

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Add Floats", CompactNodeTitle = "FloatArray+"), Category="Math|Float")
	static float Add_Floats(const TArray<float> Array);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Average Floats", CompactNodeTitle = "FloatArrayAverage"), Category="Math|Float")
	static float Average_Floats(const TArray<float> Array);

	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< INTEGER >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable,Category="Math|Integer")
	static int32 IncrementIntegerBy(UPARAM(ref) int32& Integer , int32 Value = 1 );
	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Add Integers", CompactNodeTitle = "IntegerArray+"), Category="Math|Integer")
	static int32 Add_Integers(const TArray<int32> Array);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Average Integers", CompactNodeTitle = "IntegerArrayAverage"), Category="Math|Integer")
	static int32 Average_Integers(const TArray<int32> Array);
	

	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< STRING >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	UFUNCTION(BlueprintPure ,Category = "GameModeSettings")
	static FString GetFIntPointAsString(const FIntPoint& Point , const FString Separator = "x");

	UFUNCTION(BlueprintPure ,Category = "GameModeSettings")
	static FString GetScreenResolutionAsString(UGameUserSettings*& UserSettings , const FString Separator = "x");
	

};

