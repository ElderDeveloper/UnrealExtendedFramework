// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFTypes.h"
#include "UObject/Object.h"
#include "EPFTestAsyncListener.generated.h"

/**
 * Stands in for a Blueprint graph in the plugin's automation tests: bound to a PlayFab async node's
 * output pins, it records which one fired. Not for use in games.
 */
UCLASS(Transient, NotBlueprintable, HideDropdown)
class UEPFTestAsyncListener : public UObject
{
	GENERATED_BODY()

public:
	/** For pins with no parameters, e.g. Get Shared Group Data's OnSuccess. */
	UFUNCTION()
	void HandleSuccess()
	{
		++SuccessCount;
	}

	/** For the OnFailure pin every PlayFab async node has. */
	UFUNCTION()
	void HandleFailure(const FEPFError& Error, const FString& ErrorMessage)
	{
		++FailureCount;
		LastError = Error;
		LastErrorMessage = ErrorMessage;
	}

	bool HasFinished() const
	{
		return SuccessCount + FailureCount > 0;
	}

	int32 SuccessCount = 0;
	int32 FailureCount = 0;
	FEPFError LastError;
	FString LastErrorMessage;
};
