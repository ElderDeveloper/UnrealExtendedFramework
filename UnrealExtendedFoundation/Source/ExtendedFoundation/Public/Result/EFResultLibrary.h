// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Result/EFResult.h"
#include "EFResultLibrary.generated.h"

/** Blueprint access to FEFResult. Break the struct for the individual fields. */
UCLASS(meta = (ScriptName = "EFResultLibrary"))
class EXTENDEDFOUNDATION_API UEFResultLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Result", meta = (DisplayName = "Is Success (Result)"))
	static bool IsSuccess(const FEFResult& Result) { return Result.IsSuccess(); }

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Result")
	static bool IsCancelled(const FEFResult& Result) { return Result.IsCancelled(); }

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Result")
	static bool IsTimedOut(const FEFResult& Result) { return Result.IsTimedOut(); }

	/** Compares the code without regard to case. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Result")
	static bool HasResultCode(const FEFResult& Result, const FString& Code) { return Result.HasCode(*Code); }

	/** One line for logs and developer UI. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Result", meta = (DisplayName = "To String (Result)", CompactNodeTitle = "->", BlueprintAutocast))
	static FString DescribeResult(const FEFResult& Result) { return Result.ToString(); }

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Result")
	static FEFResult MakeSuccessResult(FName Source) { return FEFResult::Success(Source); }

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Result")
	static FEFResult MakeFailureResult(FName Source, const FString& Code, const FString& Message, bool bRetryable = false)
	{
		return FEFResult::Failure(Source, Code, Message, bRetryable);
	}
};
