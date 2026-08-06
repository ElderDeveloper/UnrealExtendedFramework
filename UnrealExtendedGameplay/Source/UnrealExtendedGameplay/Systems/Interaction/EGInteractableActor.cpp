// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteractableActor.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"

AEGInteractableActor::AEGInteractableActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	// The success and hold notifications are multicasts, so the actor has to replicate.
	bReplicates = true;
}

bool AEGInteractableActor::UsesHoldActivation() const
{
	return ActivationMode == EEGInteractionActivation::Hold && HoldDuration > 0.0f;
}

bool AEGInteractableActor::IsHeldByOtherInteractor(const AActor* Interactor) const
{
	// A stale holder (disconnected, destroyed pawn) must never lock the actor forever.
	const AActor* Holder = ActiveInteractor.Get();
	return bHoldActive && Holder != nullptr && Holder != Interactor;
}

float AEGInteractableActor::GetHoldProgress() const
{
	if (!bHoldActive || HoldDuration <= 0.0f)
	{
		return 0.0f;
	}

	const UWorld* World = GetWorld();
	const float Elapsed = (HasAuthority() || !World) ? HoldElapsed : (World->GetTimeSeconds() - HoldStartTime);
	return FMath::Clamp(Elapsed / HoldDuration, 0.0f, 1.0f);
}

bool AEGInteractableActor::CanInteract_Implementation(AActor* Interactor) const
{
	// A running hold belongs to whoever started it; everyone else is locked out until it ends.
	return Interactor != nullptr && bIsInteractable && !IsHeldByOtherInteractor(Interactor);
}

bool AEGInteractableActor::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority())
	{
		return false;
	}

	MulticastInteracted(Interactor);
	return true;
}

void AEGInteractableActor::InteractionStart_Implementation(AActor* Interactor)
{
	// Execute_*, never the bare name: the interface's own CanInteract/Interact are assert-only
	// stubs, and only the Execute_ form dispatches to a Blueprint override.
	if (!HasAuthority() || !Execute_CanInteract(this, Interactor))
	{
		return;
	}

	if (!UsesHoldActivation())
	{
		Execute_Interact(this, Interactor);
		return;
	}

	HoldElapsed = 0.0f;
	SetHoldActive(true, Interactor);
	MulticastHoldStateChanged(true, Interactor);
}

void AEGInteractableActor::InteractionTick_Implementation(AActor* Interactor, float DeltaTime)
{
	if (!HasAuthority() || !bHoldActive || ActiveInteractor.Get() != Interactor)
	{
		return;
	}

	HoldElapsed += DeltaTime;
	if (HoldElapsed < HoldDuration)
	{
		return;
	}

	// Clear the hold first so a still-pressed input cannot re-trigger on the next tick.
	SetHoldActive(false, Interactor);
	MulticastHoldStateChanged(false, Interactor);
	Execute_Interact(this, Interactor);
}

void AEGInteractableActor::InteractionEnd_Implementation(AActor* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bHoldActive)
	{
		SetHoldActive(false, Interactor);
		MulticastHoldStateChanged(false, Interactor);
	}

	HoldElapsed = 0.0f;
	ActiveInteractor.Reset();
}

FEGInteractionPresentation AEGInteractableActor::GetInteractionPresentation_Implementation(AActor* Interactor, const FHitResult& HitResult) const
{
	(void)Interactor;
	(void)HitResult;

	FEGInteractionPresentation Presentation;
	Presentation.Text = PromptText;
	Presentation.Icon = PromptIcon;
	return Presentation;
}

void AEGInteractableActor::FocusStateChanged_Implementation(bool bIsFocused, const FHitResult& HitResult)
{
	ApplyFocusHighlight(bIsFocused);
	BP_OnFocusChanged(bIsFocused, HitResult);
}

void AEGInteractableActor::ApplyFocusHighlight(bool bEnabled) const
{
	if (!bHighlightOnFocus)
	{
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*> Primitives;
	GetComponents(Primitives);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (!Primitive)
		{
			continue;
		}

		if (bEnabled)
		{
			Primitive->SetCustomDepthStencilValue(HighlightStencilValue);
		}
		Primitive->SetRenderCustomDepth(bEnabled);
	}
}

void AEGInteractableActor::SetHoldActive(bool bNewHoldActive, AActor* Interactor)
{
	bHoldActive = bNewHoldActive;
	ActiveInteractor = bNewHoldActive ? Interactor : nullptr;
	if (bNewHoldActive)
	{
		HoldStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	}
}

void AEGInteractableActor::MulticastInteracted_Implementation(AActor* Interactor)
{
	OnInteracted.Broadcast(Interactor);
	BP_OnInteracted(Interactor);
}

void AEGInteractableActor::MulticastHoldStateChanged_Implementation(bool bStarted, AActor* Interactor)
{
	if (!HasAuthority())
	{
		// Clients track the hold locally so GetHoldProgress can drive UI, and so their own
		// CanInteract check hides the prompt while somebody else is holding.
		SetHoldActive(bStarted, Interactor);
	}

	OnHoldStateChanged.Broadcast(bStarted, Interactor);
	BP_OnHoldStateChanged(bStarted, Interactor);
}
