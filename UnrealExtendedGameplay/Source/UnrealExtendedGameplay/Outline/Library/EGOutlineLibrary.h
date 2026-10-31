// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EGOutlineLibrary.generated.h"


class UEGOutlinerAbstract;

UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGOutlineLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure , Category = "Outliner")
	static UEGOutlinerAbstract* GetOutlinerManagerFromActor(const AActor* Actor);

	UFUNCTION(BlueprintPure , Category = "Outliner")
	static UEGOutlinerAbstract* GetOutlinerManagerFromController(const AController* Controller);

	UFUNCTION(BlueprintPure , Category = "Outliner")
	static UEGOutlinerAbstract* GetOutlinerManagerFromComponent(const UActorComponent* Component);
};
