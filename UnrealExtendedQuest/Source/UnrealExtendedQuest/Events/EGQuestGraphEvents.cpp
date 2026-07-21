// Copyright Devil of the Plague. All Rights Reserved.
#include "EGQuestGraphEvents.h"

#include "UnrealExtendedQuest/EGQuestContext.h"

void UEGQuestEvent_NamedEvent::EnterEvent_Implementation(UEGQuestContext* Context)
{
	if (Context != nullptr)
	{
		Context->OnNamedQuestEvent.Broadcast(*Context, EventName);
	}
}

FString UEGQuestEvent_NamedEvent::GetEditorDisplayString_Implementation(UEGQuestGraph* OwnerQuest)
{
	return FString::Printf(TEXT("Broadcast %s"), *EventName.ToString());
}

bool UEGQuestEvent_NamedEvent::ValidateForCompile(FString& OutError) const
{
	if (EventName.IsNone())
	{
		OutError = TEXT("is missing its event name");
		return false;
	}
	return true;
}

void UEGQuestEvent_NamedEvent::GetSearchTerms(TArray<FEGQuestSearchTerm>& OutTerms) const
{
	OutTerms.Emplace(TEXT("EventName"), EventName.ToString());
}

void UEGQuestEvent_GameplayNotify::EnterEvent_Implementation(UEGQuestContext* Context)
{
	if (Context != nullptr && NotifyTag.IsValid())
	{
		Context->OnGameplayNotify.Broadcast(*Context, NotifyTag, NotifyMagnitude);
	}
}

FString UEGQuestEvent_GameplayNotify::GetEditorDisplayString_Implementation(UEGQuestGraph* OwnerQuest)
{
	return FString::Printf(TEXT("Notify %s (%s)"), *NotifyTag.ToString(), *FString::SanitizeFloat(NotifyMagnitude));
}

bool UEGQuestEvent_GameplayNotify::ValidateForCompile(FString& OutError) const
{
	if (!NotifyTag.IsValid())
	{
		OutError = TEXT("is missing its notify tag");
		return false;
	}
	return true;
}

void UEGQuestEvent_GameplayNotify::GetSearchTerms(TArray<FEGQuestSearchTerm>& OutTerms) const
{
	OutTerms.Emplace(TEXT("NotifyTag"), NotifyTag.ToString());
	OutTerms.Emplace(TEXT("NotifyMagnitude"), FString::SanitizeFloat(NotifyMagnitude), EEGQuestSearchTermKind::Number);
}
