#include "EGQuestSearchUtilities.h"

#include "UnrealExtendedQuest/EGQuestHelper.h"
#include "UnrealExtendedQuest/Events/EGQuestGraphEvents.h"
#include "UnrealExtendedQuestEditor/Editor/Graph/EGQuestEdGraph.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode.h"

bool FEGQuestSearchUtilities::IsNamedEventInArray(FName Name, const TArray<TObjectPtr<UEGQuestEventCustom>>& Events)
{
	for (const TObjectPtr<UEGQuestEventCustom>& Event : Events)
	{
		const UEGQuestEvent_NamedEvent* Named = Cast<UEGQuestEvent_NamedEvent>(Event);
		if (Named != nullptr && Named->GetEventName() == Name) return true;
	}
	return false;
}

bool FEGQuestSearchUtilities::IsEventOfClassInArray(const UClass* EventClass, const TArray<TObjectPtr<UEGQuestEventCustom>>& Events)
{
	for (const TObjectPtr<UEGQuestEventCustom>& Event : Events)
	{
		if (Event != nullptr && Event->GetClass() == EventClass) return true;
	}
	return false;
}

TSharedPtr<FEGQuestSearchFoundResult> FEGQuestSearchUtilities::GetGraphNodesForEventEventName(FName Name, const UEGQuestGraph* Quest)
{
	TSharedPtr<FEGQuestSearchFoundResult> Result = FEGQuestSearchFoundResult::Make();
	if (!Quest || !Quest->GetGraph()) return Result;
	const UEGQuestEdGraph* Graph = CastChecked<UEGQuestEdGraph>(Quest->GetGraph());
	for (const UEGQuestGraphNode* Node : Graph->GetAllQuestGraphNodes())
	{
		if (IsNamedEventInArray(Name, Node->GetQuestNode().GetNodeEnterEvents()))
		{
			Result->GraphNodes.AddUnique(Node);
		}
	}
	return Result;
}

TSharedPtr<FEGQuestSearchFoundResult> FEGQuestSearchUtilities::GetGraphNodesForCustomEvent(const UClass* EventClass, const UEGQuestGraph* Quest)
{
	TSharedPtr<FEGQuestSearchFoundResult> Result = FEGQuestSearchFoundResult::Make();
	if (!Quest || !Quest->GetGraph()) return Result;
	const UEGQuestEdGraph* Graph = CastChecked<UEGQuestEdGraph>(Quest->GetGraph());
	for (const UEGQuestGraphNode* Node : Graph->GetAllQuestGraphNodes())
	{
		if (IsEventOfClassInArray(EventClass, Node->GetQuestNode().GetNodeEnterEvents())) Result->GraphNodes.Add(Node);
	}
	return Result;
}

bool FEGQuestSearchUtilities::DoesGUIDContainString(const FGuid& GUID, const FString& SearchString, FString& OutGUIDString)
{
	const FString Search = SearchString.TrimStartAndEnd();
	for (const EGuidFormats Format : { EGuidFormats::Digits, EGuidFormats::DigitsWithHyphens,
		EGuidFormats::DigitsWithHyphensInBraces, EGuidFormats::DigitsWithHyphensInParentheses,
		EGuidFormats::HexValuesInBraces, EGuidFormats::UniqueObjectGuid })
	{
		const FString Value = GUID.ToString(Format);
		if (Value.Contains(Search)) { OutGUIDString = Value; return true; }
	}
	return false;
}

bool FEGQuestSearchUtilities::DoesObjectClassNameContainString(const UObject* Object, const FString& SearchString, FString& OutNameString)
{
	if (!Object) return false;
	const FString Name = FEGQuestHelper::CleanObjectName(Object->GetClass()->GetName());
	if (Name.Contains(SearchString)) { OutNameString = Name; return true; }
	return false;
}
