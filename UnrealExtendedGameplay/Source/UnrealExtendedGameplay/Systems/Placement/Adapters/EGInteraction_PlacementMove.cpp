// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteraction_PlacementMove.h"

#include "Assets/EFAssetLoading.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "UnrealExtendedGameplay/InteractionSystem/EGInteractionProviderComponent.h"
#include "UnrealExtendedGameplay/Systems/Placement/Preview/EGPlacementVisuals.h"
#include "UnrealExtendedGameplay/Systems/Placement/Profile/EGPlaceableDefinition.h"
#include "UnrealExtendedGameplay/Systems/Placement/Session/EGPlacementSessionComponent.h"

UEGInteraction_PlacementMove::UEGInteraction_PlacementMove()
{
	NetPolicy = EEGInteractionNetPolicy::LocalPredicted;
	// Moving an object drags the aim off it; the move stays open until the session ends.
	bCancelOnFocusLost = false;
}

UEGPlacementSessionComponent* UEGInteraction_PlacementMove::FindSession() const
{
	const AActor* Interactor = GetInteractorActor();
	if (!Interactor)
	{
		return nullptr;
	}
	if (UEGPlacementSessionComponent* Found = Interactor->FindComponentByClass<UEGPlacementSessionComponent>())
	{
		return Found;
	}
	const APawn* Pawn = Cast<APawn>(Interactor);
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	return Controller ? Controller->FindComponentByClass<UEGPlacementSessionComponent>() : nullptr;
}

bool UEGInteraction_PlacementMove::CanActivate_Implementation() const
{
	if (!Super::CanActivate_Implementation() || !GetTargetActor())
	{
		return false;
	}
	const UEGInteractionProviderComponent* Provider = GetProvider();
	if (Provider && Provider->IsClaimedByOther(GetInteractionId(), GetInteractorActor()))
	{
		return false;
	}
	return !IsLocallyControlled() || FindSession() != nullptr;
}

bool UEGInteraction_PlacementMove::BuildSelection_Implementation(AActor* Target, FEGPlacementSelection& OutSelection) const
{
	if (!Target)
	{
		return false;
	}
	const FEGInteractionDef_PlacementMove* Definition = GetDefinition<FEGInteractionDef_PlacementMove>();
	const UEGPlaceableDefinition* Placeable = Definition ? EFAssets::Resolve(Definition->Placeable, TEXT("UEGInteraction_PlacementMove")) : nullptr;
	if (Placeable)
	{
		OutSelection.Spec = Placeable->Spec;
		OutSelection.Source = Placeable->GetSourceHandle();
	}
	else if (Definition)
	{
		OutSelection.Spec = Definition->Spec;
	}
	if (OutSelection.Spec.Shape.IsEmpty())
	{
		return false;
	}
	if (Definition)
	{
		OutSelection.Modes = Definition->Modes;
	}
	OutSelection.Visual = EGPlacementVisuals::BuildFromActor(Target);
	OutSelection.Scale = static_cast<float>(Target->GetActorScale3D().X);
	OutSelection.MovedActor = Target;
	return true;
}

void UEGInteraction_PlacementMove::ActivateInteraction_Implementation()
{
	AActor* Target = GetTargetActor();
	if (HasAuthority())
	{
		UEGInteractionProviderComponent* Provider = GetProvider();
		bClaimed = Provider && Provider->Claim(GetInteractionId(), GetInteractorActor());
		if (Provider && !bClaimed)
		{
			EndInteraction(true);
			return;
		}
	}
	if (!IsLocallyControlled())
	{
		// The server instance only holds the claim; the interactor's client runs the session.
		return;
	}
	UEGPlacementSessionComponent* Found = FindSession();
	FEGPlacementSelection Selection;
	if (!Found || !BuildSelection(Target, Selection) || !Found->BeginSession(Selection))
	{
		EndInteraction(true);
		return;
	}
	Session = Found;
	Found->OnSessionEnded.AddUniqueDynamic(this, &UEGInteraction_PlacementMove::HandleSessionEnded);
}

void UEGInteraction_PlacementMove::HandleSessionEnded(bool bSessionCancelled)
{
	if (UEGPlacementSessionComponent* Active = Session.Get())
	{
		Active->OnSessionEnded.RemoveDynamic(this, &UEGInteraction_PlacementMove::HandleSessionEnded);
	}
	Session.Reset();
	if (IsActive())
	{
		EndInteraction(bSessionCancelled);
	}
}

void UEGInteraction_PlacementMove::OnEndInteraction_Implementation(bool bCancelled)
{
	if (UEGPlacementSessionComponent* Active = Session.Get())
	{
		Session.Reset();
		Active->OnSessionEnded.RemoveDynamic(this, &UEGInteraction_PlacementMove::HandleSessionEnded);
		if (Active->IsPlacing() && Active->GetSelectionRef().MovedActor == GetTargetActor())
		{
			Active->Cancel();
		}
	}
	if (bClaimed && HasAuthority())
	{
		if (UEGInteractionProviderComponent* Provider = GetProvider())
		{
			Provider->Release(GetInteractionId(), GetInteractorActor());
		}
	}
	bClaimed = false;
	Super::OnEndInteraction_Implementation(bCancelled);
}
