// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "EGQuestManager.generated.h"

class UEGQuestGraph;

/** Asset discovery and editor helper functions. Runtime execution belongs exclusively to UEGQuestComponent. */
UCLASS()
class UNREALEXTENDEDQUEST_API UEGQuestManager : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static int32 LoadAllQuestsIntoMemory(bool bAsync = false);
	static TArray<UEGQuestGraph*> GetAllQuestsFromMemory();
	static TArray<UEGQuestGraph*> GetQuestsWithDuplicateGUIDs();
	static UWorld* GetQuestWorld();

	UFUNCTION(BlueprintPure, Category = "Quest|Helper", DisplayName = "Is Object A Custom Event")
	static bool IsObjectACustomEvent(const UObject* Object);
	UFUNCTION(BlueprintPure, Category = "Quest|Helper", DisplayName = "Is Object A Custom Text Argument")
	static bool IsObjectACustomTextArgument(const UObject* Object);

private:
	static bool bCalledLoadAllQuestsIntoMemory;
};
