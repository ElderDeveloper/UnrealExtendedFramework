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
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Rotator + Rotator", CompactNodeTitle = "Rotator+"), Category="Math|Rotator" )
	static FRotator Add_RotatorRotator(const FRotator A , const FRotator B);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Rotator / float", CompactNodeTitle = "Rotator/"), Category="Math|Rotator" )
	static FRotator Divide_RotatorFloat(const FRotator A , const float B);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Rotator - Rotator", CompactNodeTitle = "Rotator-"), Category="Math|Rotator" )
	static FRotator Minus_RotatorRotator(const FRotator A , const FRotator B);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Rotator Yaw", CompactNodeTitle = "Yaw"), Category="Math|Rotator" )
	static float Rotator_GetYaw(const FRotator A) { return A.Yaw; }

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Rotator Pitch", CompactNodeTitle = "Pitch"), Category="Math|Rotator" )
	static float Rotator_GetPitch(const FRotator A) { return A.Pitch; }

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Rotator Roll", CompactNodeTitle = "Roll"), Category="Math|Rotator" )
	static float Rotator_GetRoll(const FRotator A) { return A.Roll; }



	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< VECTOR >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure , meta=(DisplayName = "Compare Vector Sizes", CompactNodeTitle = ">"),Category="Math|Vector")
	static bool CompareVectorSizes(const FVector IsBigger , const FVector IsLesser);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Vector X", CompactNodeTitle = "X"), Category="Math|Vector" )
	static float Vector_GetX(const FVector A) { return A.X; }

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Vector Y", CompactNodeTitle = "Y"), Category="Math|Vector" )
	static float Vector_GetY(const FVector A) { return A.Y; }

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Vector Z", CompactNodeTitle = "Z"), Category="Math|Vector" )
	static float Vector_GetZ(const FVector A) { return A.Z; }


	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Int To Vector", CompactNodeTitle = "*"), Category="Math|Vector" )
	static FVector IntToVector(const int32 Value) {	return FVector(Value,Value,Value);}

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Float To Vector", CompactNodeTitle = "*"), Category="Math|Vector" )
	static FVector FloatToVector(const float Value) {	return FVector(Value,Value,Value);}

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte To Vector", CompactNodeTitle = "*"), Category="Math|Vector" )
	static FVector ByteToVector(const uint8 Value) {	return FVector(Value,Value,Value);}
	
	

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< TRANSFORM >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Transform + Tranform", CompactNodeTitle = "Tranform+"), Category="Math|Tranform" )
	static  FTransform Add_TransformTransform(const FTransform A , const FTransform B);

	


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< FLOAT >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Increment Float", CompactNodeTitle = "Float +") ,Category="Math|Float")
	static float IncrementFloatBy(UPARAM(ref) float& Float , float Value = 1 );

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Add Floats", CompactNodeTitle = "FloatArray+"), Category="Math|Float")
	static float Add_Floats(const TArray<float> Array);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Average Floats", CompactNodeTitle = "FloatArrayAverage"), Category="Math|Float")
	static float Average_Floats(const TArray<float> Array);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Float / Int", CompactNodeTitle = "Float / Int"),  Category="Math|Float")
	static float Divide_FloatInt(float Float , int32 divider = 1 );

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Float / Byte", CompactNodeTitle = "Float / Byte"),  Category="Math|Float")
	static float Divide_FloatByte(float Float , uint8 divider = 1 );

	

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< INTEGER >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable,Category="Math|Integer")
	static int32 IncrementIntegerBy(UPARAM(ref) int32& Integer , int32 Value = 1 );
	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Add Integers", CompactNodeTitle = "IntegerArray+"), Category="Math|Integer")
	static int32 Add_Integers(const TArray<int32> Array);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Average Integers", CompactNodeTitle = "IntegerArrayAverage"), Category="Math|Integer")
	static int32 Average_Integers(const TArray<int32> Array);





	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< STRING >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>



	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Experiment >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	//Struct To Json String;




	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Access Every Property >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

};






template<typename TEnum> static FORCEINLINE FString GetEnumValueAsString(const FString& Name, TEnum Value)
	{

		const UEnum* enumPtr = FindObject<UEnum>(ANY_PACKAGE, *Name, true);
		if (!enumPtr)
		{
			return FString("Invalid");
		}

		return enumPtr->GetNameByValue((int64)Value).ToString();
	}