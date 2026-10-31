// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EFTestMessageTypes.generated.h"

/** Payloads for the foundation's message bus and analytics tests. Not for use in games. */
USTRUCT()
struct FEFTestMessage
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Value = 0;

	UPROPERTY()
	FString Label;
};

USTRUCT()
struct FEFTestChildMessage : public FEFTestMessage
{
	GENERATED_BODY()

	UPROPERTY()
	bool bFlag = false;
};

USTRUCT()
struct FEFTestOtherMessage
{
	GENERATED_BODY()

	UPROPERTY()
	float Amount = 0.f;
};
