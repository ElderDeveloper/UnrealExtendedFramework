// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UEExtendedConditionLibrary.generated.h"

/**
 * 
 */

UENUM()
enum EConditionOutput
{
	OutTrue,
	OutIsFalse
};

UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedConditionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable,meta = (CompactNodeTitle = ">0",CustomStructureParam = "Value" , ExpandEnumAsExecs = "OutPins"))
	static void IsBiggerThanZero(const float& Value , TEnumAsByte<EConditionOutput>& OutPins);

	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Float >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure ,meta=(DisplayName = ">0", CompactNodeTitle = ">0", ScriptMethod = ">", ScriptOperator = ">") , Category="Condition|Float")
	static bool FloatIsBiggerThanZero(const float Value) { return Value > 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "<0", CompactNodeTitle = "<0", ScriptMethod = "<", ScriptOperator = "<") , Category="Condition|Float")
	static bool FloatIsLesserThanZero(const float Value) { return Value < 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "=0", CompactNodeTitle = "=0", ScriptMethod = "=", ScriptOperator = "=") , Category="Condition|Float")
	static bool FloatIsEqualToZero(const float Value) { return Value == 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "!=0", CompactNodeTitle = "!=0", ScriptMethod = "!=", ScriptOperator = "!=") , Category="Condition|Float")
	static bool FloatIsNotEqualToZero(const float Value) { return Value > 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = ">=0", CompactNodeTitle = ">=0", ScriptMethod = ">", ScriptOperator = ">") , Category="Condition|Float")
	static bool FloatIsBiggerThanAndEqualZero(const float Value) { return Value >= 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "<=0", CompactNodeTitle = "<=0", ScriptMethod = "<", ScriptOperator = "<") , Category="Condition|Float")
	static bool FloatIsLesserThanAndEqualZero(const float Value) { return Value <= 0; }




	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< INT32 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure ,meta=(DisplayName = ">0", CompactNodeTitle = ">0", ScriptMethod = ">", ScriptOperator = ">") , Category="Condition|Int")
	static bool IntIsBiggerThanZero(const int32 Value) { return Value > 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "<0", CompactNodeTitle = "<0", ScriptMethod = "<", ScriptOperator = "<") , Category="Condition|Int")
	static bool IntIsLesserThanZero(const int32 Value) { return Value < 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "=0", CompactNodeTitle = "=0", ScriptMethod = "=", ScriptOperator = "=") , Category="Condition|Int")
	static bool IntIsEqualToZero(const int32 Value) { return Value == 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "!=0", CompactNodeTitle = "!=0", ScriptMethod = "!=", ScriptOperator = "!=") , Category="Condition|Int")
	static bool IntIsNotEqualToZero(const int32 Value) { return Value > 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = ">=0", CompactNodeTitle = ">=0", ScriptMethod = ">", ScriptOperator = ">") , Category="Condition|Float")
	static bool IntIsBiggerThanAndEqualZero(const int32 Value) { return Value >= 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "<=0", CompactNodeTitle = "<=0", ScriptMethod = "<", ScriptOperator = "<") , Category="Condition|Float")
	static bool IntIsLesserThanAndEqualZero(const int32 Value) { return Value <= 0; }





	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< BYTE >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure ,meta=(DisplayName = ">0", CompactNodeTitle = ">0", ScriptMethod = ">", ScriptOperator = ">") , Category="Condition|Byte")
	static bool ByteIsBiggerThanZero(const uint8 Value) { return Value > 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "<0", CompactNodeTitle = "<0", ScriptMethod = "<", ScriptOperator = "<") , Category="Condition|Byte")
	static bool ByteIsLesserThanZero(const uint8 Value) { return Value < 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "=0", CompactNodeTitle = "=0", ScriptMethod = "=", ScriptOperator = "=") , Category="Condition|Byte")
	static bool ByteIsEqualToZero(const uint8 Value) { return Value == 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "!=0", CompactNodeTitle = "!=0", ScriptMethod = "!=", ScriptOperator = "!=") , Category="Condition|Byte")
	static bool ByteIsNotEqualToZero(const uint8 Value) { return Value > 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = ">=0", CompactNodeTitle = ">=0", ScriptMethod = ">", ScriptOperator = ">") , Category="Condition|Byte")
	static bool ByteIsBiggerThanAndEqualZero(const uint8 Value) { return Value >= 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "<=0", CompactNodeTitle = "<=0", ScriptMethod = "<", ScriptOperator = "<") , Category="Condition|Byte")
	static bool ByteIsLesserThanAndEqualZero(const uint8 Value) { return Value <= 0; } 
};
