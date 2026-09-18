// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteraction.h"

#include "Components/SkeletalMeshComponent.h"
#include "EGInteractionProviderComponent.h"
#include "EGInteractionSystemComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTask.h"

// =====================================================================
// FEGInteractionActorInfo
// =====================================================================

void FEGInteractionActorInfo::Init(UEGInteractionSystemComponent* InSystem)
{
	System = InSystem;
	OwnerActor = InSystem ? InSystem->GetOwner() : nullptr;
	Pawn = Cast<APawn>(OwnerActor);
	SkeletalMesh = OwnerActor ? OwnerActor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

AController* FEGInteractionActorInfo::GetController() const
{
	return Pawn ? Pawn->GetController() : nullptr;
}

APlayerController* FEGInteractionActorInfo::GetPlayerController() const
{
	return Cast<APlayerController>(GetController());
}

bool FEGInteractionActorInfo::IsLocallyControlled() const
{
	if (Pawn)
	{
		return Pawn->IsLocallyControlled();
	}
	return OwnerActor ? OwnerActor->HasLocalNetOwner() : false;
}

bool FEGInteractionActorInfo::HasAuthority() const
{
	return OwnerActor ? OwnerActor->HasAuthority() : false;
}

// =====================================================================
// UEGInteraction
// =====================================================================

UEGInteraction::UEGInteraction()
{
}

UWorld* UEGInteraction::GetWorld() const
{
	// The CDO answers null without touching the outer chain, so the Blueprint editor sees
	// GetWorld as implemented and exposes world-context nodes to subclasses.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	if (ActorInfo.OwnerActor)
	{
		return ActorInfo.OwnerActor->GetWorld();
	}

	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

// ---------------------------------------------------------------------
// Driven by the system component
// ---------------------------------------------------------------------

void UEGInteraction::InitInteraction(UEGInteractionSystemComponent* InSystem, const FEGInteractionSpec& InSpec)
{
	ActorInfo.Init(InSystem);
	Spec = InSpec;
}

void UEGInteraction::UpdateSpecHit(const FHitResult& InHit, UPrimitiveComponent* InHitComponent)
{
	Spec.Hit = InHit;
	Spec.HitComponent = InHitComponent;
}

void UEGInteraction::UpdateSpecDefinition(const FInstancedStruct& InDefinition)
{
	Spec.Definition = InDefinition;
}

bool UEGInteraction::HasActiveTasksOtherThan(const UGameplayTask* Task) const
{
	for (const UGameplayTask* Active : ActiveTasks)
	{
		if (Active && Active != Task && !Active->IsFinished())
		{
			return true;
		}
	}
	return false;
}

void UEGInteraction::BeginActivation(int32 InActivationId, bool bInPredicting)
{
	if (bActive)
	{
		return;
	}

	ActivationId = InActivationId;
	ActivationInputTag = GetInputTag();
	bActive = true;
	bPredicting = bInPredicting;
	bServerPending = false;

	if (ActorInfo.System)
	{
		ActorInfo.System->NotifyInteractionStarted(this);
	}

	// May end synchronously: an instant interaction does its work and calls EndInteraction here.
	ActivateInteraction();
}

void UEGInteraction::BeginServerPending(int32 InActivationId)
{
	ActivationId = InActivationId;
	ActivationInputTag = GetInputTag();
	bServerPending = true;
}

void UEGInteraction::HandleInputReleased()
{
	if (!bActive)
	{
		return;
	}

	OnInputReleasedNative.Broadcast();
	OnInputReleased();
}

void UEGInteraction::HandleServerResult(bool bAccepted)
{
	bPredicting = false;
	OnServerResult(bAccepted);

	if (bAccepted)
	{
		return;
	}

	if (bServerPending)
	{
		bServerPending = false;
		if (ActorInfo.System)
		{
			ActorInfo.System->NotifyActivationRejected(this);
		}
		return;
	}

	if (bActive)
	{
		// The server never ran this, so there is nothing to tell it.
		EndInteractionInternal(true, false);
		if (ActorInfo.System)
		{
			ActorInfo.System->NotifyActivationRejected(this);
		}
	}
}

void UEGInteraction::HandleServerEnded(bool bCancelled)
{
	if (bActive)
	{
		EndInteractionInternal(bCancelled, false);
		return;
	}

	if (bServerPending)
	{
		bServerPending = false;
		OnEndedNative.Broadcast(bCancelled);
		if (ActorInfo.System)
		{
			ActorInfo.System->NotifyInteractionEnded(this, bCancelled, false);
		}
	}
}

void UEGInteraction::HandleRevoked()
{
	if (bActive)
	{
		EndInteractionInternal(true, true);
	}
	bServerPending = false;
	EndAllTasks();
}

// ---------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------

bool UEGInteraction::CanActivate_Implementation() const
{
	const FEGInteractionDefinition* Definition = Spec.GetBase();
	return Definition && Definition->bEnabled && IsValid(Spec.Provider) && IsValid(Spec.Provider->GetOwner());
}

FEGInteractionPrompt UEGInteraction::GetPresentation_Implementation() const
{
	FEGInteractionPrompt Prompt;
	if (const FEGInteractionDefinition* Definition = Spec.GetBase())
	{
		Prompt.Text = Definition->Text;
		Prompt.SubText = Definition->SubText;
		Prompt.Icon = Definition->Icon;
	}
	Prompt.bEnabled = CanActivate();
	Prompt.bVisible = Prompt.bEnabled || bShowPromptWhenUnavailable;
	return Prompt;
}

void UEGInteraction::EndInteraction(bool bCancelled)
{
	EndInteractionInternal(bCancelled, true);
}

void UEGInteraction::EndInteractionInternal(bool bCancelled, bool bNotifyServer)
{
	if (!bActive || bEnding)
	{
		return;
	}

	bEnding = true;
	EndAllTasks();
	OnEndInteraction(bCancelled);

	bActive = false;
	bPredicting = false;
	bEnding = false;

	OnEndedNative.Broadcast(bCancelled);
	if (ActorInfo.System)
	{
		ActorInfo.System->NotifyInteractionEnded(this, bCancelled, bNotifyServer);
	}
}

void UEGInteraction::SetHoldProgress(float Progress)
{
	if (ActorInfo.System)
	{
		ActorInfo.System->SetPromptProgress(Spec.Handle, FMath::Clamp(Progress, 0.0f, 1.0f));
	}
}

void UEGInteraction::SendProviderEvent(EEGInteractionEventPhase Phase)
{
	if (IsValid(Spec.Provider) && HasAuthority())
	{
		Spec.Provider->BroadcastInteractionEvent(Spec.Id, ActorInfo.OwnerActor, Phase);
	}
}

// ---------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------

AActor* UEGInteraction::GetTargetActor() const
{
	return IsValid(Spec.Provider) ? Spec.Provider->GetOwner() : nullptr;
}

FGameplayTag UEGInteraction::GetInputTag() const
{
	const FEGInteractionDefinition* Definition = Spec.GetBase();
	return Definition ? Definition->InputTag : FGameplayTag();
}

FEGInteractionDefinition UEGInteraction::GetDefinitionBase() const
{
	const FEGInteractionDefinition* Definition = Spec.GetBase();
	return Definition ? *Definition : FEGInteractionDefinition();
}

// ---------------------------------------------------------------------
// Tasks
// ---------------------------------------------------------------------

UGameplayTasksComponent* UEGInteraction::GetGameplayTasksComponent(const UGameplayTask& Task) const
{
	return ActorInfo.System;
}

AActor* UEGInteraction::GetGameplayTaskOwner(const UGameplayTask* Task) const
{
	return ActorInfo.OwnerActor;
}

AActor* UEGInteraction::GetGameplayTaskAvatar(const UGameplayTask* Task) const
{
	return ActorInfo.Pawn ? static_cast<AActor*>(ActorInfo.Pawn) : ActorInfo.OwnerActor.Get();
}

void UEGInteraction::OnGameplayTaskInitialized(UGameplayTask& Task)
{
	ActiveTasks.AddUnique(&Task);
}

void UEGInteraction::OnGameplayTaskActivated(UGameplayTask& Task)
{
	ActiveTasks.AddUnique(&Task);
}

void UEGInteraction::OnGameplayTaskDeactivated(UGameplayTask& Task)
{
	ActiveTasks.Remove(&Task);
}

void UEGInteraction::EndAllTasks()
{
	// Tasks remove themselves from ActiveTasks as they end, so walk a copy.
	TArray<TObjectPtr<UGameplayTask>> Tasks = ActiveTasks;
	for (UGameplayTask* Task : Tasks)
	{
		if (Task && !Task->IsFinished())
		{
			Task->TaskOwnerEnded();
		}
	}
	ActiveTasks.Reset();
}
