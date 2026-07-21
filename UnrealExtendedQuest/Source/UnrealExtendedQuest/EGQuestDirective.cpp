// Copyright Devil of the Plague. All Rights Reserved.
#include "EGQuestDirective.h"

FDelegateHandle UEGQuestDirectiveSubsystem::Subscribe(const FGameplayTagQuery& Query,
	FEGQuestDirectiveNative::FDelegate&& Callback)
{
	FSubscription& Subscription = Subscriptions.AddDefaulted_GetRef();
	Subscription.Handle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
	Subscription.Query = Query;
	Subscription.Callback = MoveTemp(Callback);
	return Subscription.Handle;
}

void UEGQuestDirectiveSubsystem::Unsubscribe(FDelegateHandle Handle)
{
	Subscriptions.RemoveAll([Handle](const FSubscription& Subscription){ return Subscription.Handle == Handle; });
}

void UEGQuestDirectiveSubsystem::Dispatch(FGuid QuestInstanceGuid, const FEGQuestDirective& Directive,
	EEGQuestDirectivePhase Phase)
{
	if (!Directive.DirectiveTag.IsValid()) return;
	OnDirective.Broadcast(QuestInstanceGuid, Directive, Phase);
	FGameplayTagContainer Tags(Directive.DirectiveTag);
	const TArray<FSubscription> Copy = Subscriptions;
	for (const FSubscription& Subscription : Copy)
		if (Subscription.Query.IsEmpty() || Subscription.Query.Matches(Tags))
			Subscription.Callback.ExecuteIfBound(QuestInstanceGuid, Directive, Phase);
}
