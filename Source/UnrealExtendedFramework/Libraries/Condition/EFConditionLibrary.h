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

#pragma region Float
	UFUNCTION(BlueprintPure ,meta=(DisplayName = ">0", CompactNodeTitle = ">0") , Category="Condition|Float")
	static bool FloatIsBiggerThanZero(const float Value) { return Value > 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "<0", CompactNodeTitle = "<0") , Category="Condition|Float")
	static bool FloatIsLesserThanZero(const float Value) { return Value < 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "==0", CompactNodeTitle = "==0") , Category="Condition|Float")
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

#pragma endregion

	
#pragma region Int32
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = ">0", CompactNodeTitle = ">0") , Category="Condition|Int")
	static bool IntIsBiggerThanZero(const int32 Value) { return Value > 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "<0", CompactNodeTitle = "<0") , Category="Condition|Int")
	static bool IntIsLesserThanZero(const int32 Value) { return Value < 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "=0", CompactNodeTitle = "=0") , Category="Condition|Int")
	static bool IntIsEqualToZero(const int32 Value) { return Value == 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "!=0", CompactNodeTitle = "!=0") , Category="Condition|Int")
	static bool IntIsNotEqualToZero(const int32 Value) { return Value != 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = ">=0", CompactNodeTitle = ">=0") , Category="Condition|Float")
	static bool IntIsBiggerThanAndEqualZero(const int32 Value) { return Value >= 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "<=0", CompactNodeTitle = "<=0") , Category="Condition|Float")
	static bool IntIsLesserThanAndEqualZero(const int32 Value) { return Value <= 0; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "=-1", CompactNodeTitle = "=-1") , Category="Condition|Int")
	static bool IntIsEqualToMinusOne(const int32 Value) { return Value == -1; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "!=-1", CompactNodeTitle = "!=-1") , Category="Condition|Int")
	static bool IntIsNotEqualToMinusOne(const int32 Value) { return Value != -1; }

#pragma endregion

	
#pragma region Byte
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = ">0", CompactNodeTitle = ">0") , Category="Condition|Byte")
	static bool ByteIsBiggerThanZero(const uint8 Value) { return Value > 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "=0", CompactNodeTitle = "=0") , Category="Condition|Byte")
	static bool ByteIsEqualToZero(const uint8 Value) { return Value == 0; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "!=0", CompactNodeTitle = "!=0") , Category="Condition|Byte")
	static bool ByteIsNotEqualToZero(const uint8 Value) { return Value != 0; }
	
	UFUNCTION(BlueprintPure , Category="Condition|Int")
	static bool IsPlayingInEditor();

#pragma endregion

};
