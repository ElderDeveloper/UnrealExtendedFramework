// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EEOSLobbyLifecycleTestObserver.generated.h"

UCLASS()
class UEEOSLobbyLifecycleTestObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 DestroyCount = 0;
	int32 PromotionCount = 0;
	int32 CreateCount = 0;
	int32 JoinCount = 0;
	bool bLastSuccess = false;
	FString LastId;
	UFUNCTION() void Destroyed(bool bSuccess, const FString& Id) { ++DestroyCount; bLastSuccess = bSuccess; LastId = Id; }
	UFUNCTION() void Promoted(bool bSuccess, const FString& Id) { ++PromotionCount; bLastSuccess = bSuccess; LastId = Id; }
	UFUNCTION() void Created(bool bSuccess, const FString& Id) { ++CreateCount; }
	UFUNCTION() void Joined(bool bSuccess, const FString& Id) { ++JoinCount; }
};
