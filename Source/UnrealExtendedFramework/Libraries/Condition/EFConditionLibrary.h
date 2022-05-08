// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/KismetMathLibrary.h"
#include "UnrealExtendedFramework/Data/EFEnums.h"
#include "EFConditionLibrary.generated.h"


UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFConditionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable,meta = (DisplayName = "Exec >0",CompactNodeTitle = ">0",CustomStructureParam = "Value" , ExpandEnumAsExecs = "OutPins") , Category="ConditionLibrary")
	static void IsBiggerThanZero(const float& Value , TEnumAsByte<EFConditionOutput>& OutPins)
	{
		Value > 0 ? UEF_True :  OutPins = UEF_False;
	}

	
	UFUNCTION(BlueprintCallable,meta = (DisplayName = "Exec >=0",CompactNodeTitle = "=>0",CustomStructureParam = "Value" , ExpandEnumAsExecs = "OutPins"), Category="ConditionLibrary")
	static void IsBiggerThanOrEqualZero(const float& Value , TEnumAsByte<EFConditionOutput>& OutPins)
	{
		Value >= 0 ? UEF_True :  OutPins = UEF_False;
	}

	
	UFUNCTION(BlueprintCallable,meta = (DisplayName = "Exec ==0",CompactNodeTitle = "==0",CustomStructureParam = "Value" , ExpandEnumAsExecs = "OutPins"), Category="ConditionLibrary")
	static void IsEqualZero(const float& Value , TEnumAsByte<EFConditionOutput>& OutPins)
	{
		Value == 0 ? UEF_True :  OutPins = UEF_False;
	}

	
	UFUNCTION(BlueprintCallable,meta = (DisplayName = "Exec <0",CompactNodeTitle = "<0",CustomStructureParam = "Value" , ExpandEnumAsExecs = "OutPins"), Category="ConditionLibrary")
	static void IsLessThanZero(const float& Value , TEnumAsByte<EFConditionOutput>& OutPins)
	{
		Value < 0 ? UEF_True :  OutPins = UEF_False;
	}

	
	UFUNCTION(BlueprintCallable,meta = (DisplayName = "Exec <=0",CompactNodeTitle = "<=0",CustomStructureParam = "Value" , ExpandEnumAsExecs = "OutPins"), Category="ConditionLibrary")
	static void IsLessThanOrEqualZero(const float& Value , TEnumAsByte<EFConditionOutput>& OutPins)
	{
		Value <= 0 ? UEF_True :  OutPins = UEF_False;
	}

	
	UFUNCTION(BlueprintCallable,meta = (DisplayName = "Exec ==-1",CompactNodeTitle = "==-1",CustomStructureParam = "Value" , ExpandEnumAsExecs = "OutPins"), Category="ConditionLibrary")
	static void IsEqualToMinusOne(const float& Value , TEnumAsByte<EFConditionOutput>& OutPins)
	{
		Value == -1 ? UEF_True :  OutPins = UEF_False;
	}

	
	UFUNCTION(BlueprintCallable,meta = (DisplayName = "Exec ==-1",CompactNodeTitle = "==-1",CustomStructureParam = "Value" , ExpandEnumAsExecs = "OutPins"), Category="ConditionLibrary")
	static void IsNotEqualToMinusOne(const float& Value , TEnumAsByte<EFConditionOutput>& OutPins)
	{
		Value != -1 ? UEF_True :  OutPins = UEF_False;
	}

	
	UFUNCTION(BlueprintCallable,meta = (DisplayName = "Exec >input",CompactNodeTitle = ">input",CustomStructureParam = "Value" , ExpandEnumAsExecs = "OutPins") , Category="ConditionLibrary")
	static void IsBiggerThan(const float& Value , TEnumAsByte<EFConditionOutput>& OutPins , float input = 0)
	{
		Value > input ? UEF_True :  OutPins = UEF_False;
	}

	
	UFUNCTION(BlueprintCallable,meta = (DisplayName = "Exec >= input",CompactNodeTitle = ">=input",CustomStructureParam = "Value" , ExpandEnumAsExecs = "OutPins") , Category="ConditionLibrary")
	static void IsBiggerThanOrEqual(const float& Value , TEnumAsByte<EFConditionOutput>& OutPins , float input = 0)
	{
		Value >= input ? UEF_True :  OutPins = UEF_False;
	}

	
	UFUNCTION(BlueprintCallable,meta = (DisplayName = "Exec <input",CompactNodeTitle = "<input",CustomStructureParam = "Value" , ExpandEnumAsExecs = "OutPins") , Category="ConditionLibrary")
	static void IsLessThan(const float& Value , TEnumAsByte<EFConditionOutput>& OutPins , float input = 0)
	{
		Value < input ? UEF_True :  OutPins = UEF_False;
	}

	
	UFUNCTION(BlueprintCallable,meta = (DisplayName = "Exec <=input",CompactNodeTitle = "<=input",CustomStructureParam = "Value" , ExpandEnumAsExecs = "OutPins") , Category="ConditionLibrary")
	static void IsLessThanOrEqual(const float& Value , TEnumAsByte<EFConditionOutput>& OutPins , float input = 0)
	{
		Value <= input ? UEF_True :  OutPins = UEF_False;
	}

	
	UFUNCTION(BlueprintCallable,meta = (DisplayName = "Exec ==input",CompactNodeTitle = "==input",CustomStructureParam = "Value" , ExpandEnumAsExecs = "OutPins") , Category="ConditionLibrary")
	static void IsEqualTo(const double& Value , TEnumAsByte<EFConditionOutput>& OutPins ,const double Input = 0 ,const double Tolerance = 0.02)
	{
		FMath::Abs<double>(Value - Input) <= Tolerance ? OutPins = UEF_True : OutPins = UEF_False;
	}


	
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Float >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure ,meta=(DisplayName = ">0", CompactNodeTitle = ">0") , Category="Condition|Float")
	static bool FloatIsBiggerThanZero(const float Value) { return Value > 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "<0", CompactNodeTitle = "<0") , Category="Condition|Float")
	static bool FloatIsLesserThanZero(const float Value) { return Value < 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "=0", CompactNodeTitle = "=0") , Category="Condition|Float")
	static bool FloatIsEqualToZero(const float Value) { return Value == 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "!=0", CompactNodeTitle = "!=0") , Category="Condition|Float")
	static bool FloatIsNotEqualToZero(const float Value) { return Value != 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = ">=0", CompactNodeTitle = ">=0") , Category="Condition|Float")
	static bool FloatIsBiggerThanAndEqualZero(const float Value) { return Value >= 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "<=0", CompactNodeTitle = "<=0") , Category="Condition|Float")
	static bool FloatIsLesserThanAndEqualZero(const float Value) { return Value <= 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "=-1", CompactNodeTitle = "=-1") , Category="Condition|Int")
	static bool FloatIsEqualToMinusOne(const float Value) { return Value == -1; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "!=-1", CompactNodeTitle = "!=-1") , Category="Condition|Int")
	static bool FloatIsNotEqualToMinusOne(const float Value) { return Value != -1; }



	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< INT32 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure ,meta=(DisplayName = ">0", CompactNodeTitle = ">0") , Category="Condition|Int")
	static bool IntIsBiggerThanZero(const int32 Value) { return Value > 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "<0", CompactNodeTitle = "<0") , Category="Condition|Int")
	static bool IntIsLesserThanZero(const int32 Value) { return Value < 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "=0", CompactNodeTitle = "=0") , Category="Condition|Int")
	static bool IntIsEqualToZero(const int32 Value) { return Value == 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "!=0", CompactNodeTitle = "!=0") , Category="Condition|Int")
	static bool IntIsNotEqualToZero(const int32 Value) { return Value > 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = ">=0", CompactNodeTitle = ">=0") , Category="Condition|Float")
	static bool IntIsBiggerThanAndEqualZero(const int32 Value) { return Value >= 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "<=0", CompactNodeTitle = "<=0") , Category="Condition|Float")
	static bool IntIsLesserThanAndEqualZero(const int32 Value) { return Value <= 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "=-1", CompactNodeTitle = "=-1") , Category="Condition|Int")
	static bool IntIsEqualToMinusOne(const int32 Value) { return Value == -1; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "!=-1", CompactNodeTitle = "!=-1") , Category="Condition|Int")
	static bool IntIsNotEqualToMinusOne(const int32 Value) { return Value != -1; }




	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< BYTE >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure ,meta=(DisplayName = ">0", CompactNodeTitle = ">0") , Category="Condition|Byte")
	static bool ByteIsBiggerThanZero(const uint8 Value) { return Value > 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "<0", CompactNodeTitle = "<0") , Category="Condition|Byte")
	static bool ByteIsLesserThanZero(const uint8 Value) { return Value < 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "=0", CompactNodeTitle = "=0") , Category="Condition|Byte")
	static bool ByteIsEqualToZero(const uint8 Value) { return Value == 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "!=0", CompactNodeTitle = "!=0") , Category="Condition|Byte")
	static bool ByteIsNotEqualToZero(const uint8 Value) { return Value > 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = ">=0", CompactNodeTitle = ">=0") , Category="Condition|Byte")
	static bool ByteIsBiggerThanAndEqualZero(const uint8 Value) { return Value >= 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "<=0", CompactNodeTitle = "<=0") , Category="Condition|Byte")
	static bool ByteIsLesserThanAndEqualZero(const uint8 Value) { return Value <= 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "=-1", CompactNodeTitle = "=-1") , Category="Condition|Int")
	static bool ByteIsEqualToMinusOne(const uint8 Value) { return Value == -1; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "!=-1", CompactNodeTitle = "!=-1") , Category="Condition|Int")
	static bool ByteIsNotEqualToMinusOne(const uint8 Value) { return Value != -1; }

};
