// Fill out your copyright notice in the Description page of Project Settings.

#include "EGState.h"

#include "EGStateMachineComponent.h"
#include "AIController.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle Defaults
// ─────────────────────────────────────────────────────────────────────────────

void UEGState::OnEnter_Implementation() {}
void UEGState::OnTick_Implementation(float DeltaTime) {}
void UEGState::OnExit_Implementation() {}
void UEGState::OnPause_Implementation() {}
void UEGState::OnResume_Implementation() {}
bool UEGState::CanEnterState_Implementation() const { return true; }

// ─────────────────────────────────────────────────────────────────────────────
// State Machine Control
// ─────────────────────────────────────────────────────────────────────────────

void UEGState::RequestStateChangeByClass(TSubclassOf<UEGState> StateClass)
{
	if (OwningComponent)
	{
		OwningComponent->SwitchStateByClass(StateClass);
	}
}

void UEGState::RequestPushStateByClass(TSubclassOf<UEGState> StateClass)
{
	if (OwningComponent)
	{
		OwningComponent->PushStateByClass(StateClass);
	}
}

void UEGState::RequestPopState()
{
	if (OwningComponent)
	{
		OwningComponent->PopState();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Accessors
// ─────────────────────────────────────────────────────────────────────────────

AActor* UEGState::GetOwnerActor() const
{
	return OwningComponent ? OwningComponent->GetOwner() : nullptr;
}

AController* UEGState::GetController() const
{
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwnerActor()))
	{
		return OwnerPawn->GetController();
	}
	return nullptr;
}

AAIController* UEGState::GetAIController() const
{
	return Cast<AAIController>(GetController());
}

AActor* UEGState::GetStateContextActor() const
{
	return OwningComponent ? OwningComponent->GetStateContextActor() : nullptr;
}

void UEGState::ClearStateContextActor()
{
	if (OwningComponent)
	{
		OwningComponent->ClearStateContextActor();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Debug
// ─────────────────────────────────────────────────────────────────────────────

void UEGState::DebugLog(const FString& Message) const
{
	if (OwningComponent)
	{
		OwningComponent->DebugLog(FString::Printf(TEXT("[%s] %s"), *GetClass()->GetName(), *Message));
	}
}

UWorld* UEGState::GetWorld() const
{
	if (OwningComponent)
	{
		return OwningComponent->GetWorld();
	}
	return nullptr;
}
