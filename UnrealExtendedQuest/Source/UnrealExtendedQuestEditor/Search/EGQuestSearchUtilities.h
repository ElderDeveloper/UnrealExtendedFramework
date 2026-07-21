#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedQuest/EGQuestEventCustom.h"
#include "UnrealExtendedQuest/EGQuestTextArgument.h"

class UEGQuestGraph;
class UEGQuestGraphNode;

struct UNREALEXTENDEDQUESTEDITOR_API FEGQuestSearchFoundResult
{
	static TSharedPtr<FEGQuestSearchFoundResult> Make() { return MakeShared<FEGQuestSearchFoundResult>(); }
	TArray<TWeakObjectPtr<const UEGQuestGraphNode>> GraphNodes;
};

class UNREALEXTENDEDQUESTEDITOR_API FEGQuestSearchUtilities
{
public:
	static TSharedPtr<FEGQuestSearchFoundResult> GetGraphNodesForEventEventName(FName Name, const UEGQuestGraph* Quest);
	static TSharedPtr<FEGQuestSearchFoundResult> GetGraphNodesForCustomEvent(const UClass* EventClass, const UEGQuestGraph* Quest);

	static bool IsNamedEventInArray(FName Name, const TArray<TObjectPtr<UEGQuestEventCustom>>& Events);
	static bool IsEventOfClassInArray(const UClass* EventClass, const TArray<TObjectPtr<UEGQuestEventCustom>>& Events);
	static bool DoesGUIDContainString(const FGuid& GUID, const FString& SearchString, FString& OutGUIDString);
	static bool DoesObjectClassNameContainString(const UObject* Object, const FString& SearchString, FString& OutNameString);
};
