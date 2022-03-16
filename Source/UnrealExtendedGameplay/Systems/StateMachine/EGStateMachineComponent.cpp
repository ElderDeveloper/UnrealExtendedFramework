// Fill out your copyright notice in the Description page of Project Settings.


#include "EGStateMachineComponent.h"


#define BROADCAST(Delegate , Tag) if (Delegate.IsBound()) Delegate.Broadcast(Tag)
 

UEGStateMachineComponent::UEGStateMachineComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}




bool UEGStateMachineComponent::SwitchState(FGameplayTag stateTag)
{
	if (CanAcceptState(stateTag))
	{
		bCanTickState = false;
		EndState();
		StateTag = stateTag;
		InitState();
		bCanTickState = true;
		
		BROADCAST(OnStateChangedDelegate,StateTag);
		
		return true;
	}
	
	return false;
}




void UEGStateMachineComponent::InitState()
{
	BROADCAST(OnInitStateDelegate , StateTag);
}




void UEGStateMachineComponent::TickState(float DeltaTime)
{
	if (OnTickStateDelegate.IsBound()) OnTickStateDelegate.Broadcast(DeltaTime,StateTag);
}




void UEGStateMachineComponent::EndState()
{
	if (StateHistory.Num()>= StateHistorySize) StateHistory.RemoveAt(0);
	StateHistory.Push(StateTag);
	BROADCAST(OnEndStateDelegate , StateTag);
}




bool UEGStateMachineComponent::CanAcceptState(FGameplayTag stateTag) const
{
	if (PossibleStateTags.Num() == 0) return true;

	if (PossibleStateTags.HasTagExact(stateTag))
	{
		if ( ! stateTag.MatchesTagExact(StateTag))
			return true;
		
		if (bDebug) UE_LOG(LogTemp , Warning , TEXT("EGStateMachine: SwitchState From %s Requested The Same State Witch Is %s") , *GetOwner()->GetName(), *stateTag.ToString()); 
		return false;
	}

	if (bDebug) UE_LOG(LogTemp , Warning , TEXT("EGStateMachine: SwitchState From %s Requested A Tag That is Not a Part Of PossibleStateTags. Tag Is %s") , *GetOwner()->GetName(), *stateTag.ToString()); 
	return false;
}




void UEGStateMachineComponent::BeginPlay()
{
	Super::BeginPlay();
	SwitchState(InitialStateTag);
}




void UEGStateMachineComponent::TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bCanTickState) TickState(DeltaTime);
}

