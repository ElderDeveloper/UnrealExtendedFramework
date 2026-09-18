// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteractionProviderComponent.h"

#include "Components/PrimitiveComponent.h"
#include "EGInteraction.h"
#include "EGInteractionProviderInterface.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UEGInteractionProviderComponent::UEGInteractionProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Claims replicate and the event is a multicast, so the component has to be replicated.
	// The owner still decides whether it replicates at all.
	SetIsReplicatedByDefault(true);
}

UEGInteractionProviderComponent* UEGInteractionProviderComponent::FindProvider(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (const IEGInteractionProviderInterface* Interface = Cast<IEGInteractionProviderInterface>(Actor))
	{
		return Interface->GetInteractionProviderComponent();
	}

	return Actor->FindComponentByClass<UEGInteractionProviderComponent>();
}

void UEGInteractionProviderComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEGInteractionProviderComponent, Claims);
}

// =====================================================================
// Definitions
// =====================================================================

void UEGInteractionProviderComponent::GatherInteractions_Implementation(const FEGInteractionQuery& Query, TArray<FInstancedStruct>& OutDefinitions) const
{
	const FName HitComponentName = Query.HitComponent ? Query.HitComponent->GetFName() : NAME_None;

	for (const FInstancedStruct& Entry : Definitions)
	{
		const FEGInteractionDefinition* Definition = Entry.GetPtr<FEGInteractionDefinition>();
		if (!Definition || !Definition->bEnabled || !Definition->Id.IsValid() || !Definition->InteractionClass)
		{
			continue;
		}

		if (!Definition->PartComponent.IsNone() && Definition->PartComponent != HitComponentName)
		{
			continue;
		}

		OutDefinitions.Add(Entry);
	}
}

int32 UEGInteractionProviderComponent::FindDefinitionIndex(FGameplayTag Id) const
{
	return Definitions.IndexOfByPredicate([Id](const FInstancedStruct& Entry)
	{
		const FEGInteractionDefinition* Definition = Entry.GetPtr<FEGInteractionDefinition>();
		return Definition && Definition->Id == Id;
	});
}

bool UEGInteractionProviderComponent::FindDefinition(FGameplayTag Id, FInstancedStruct& OutDefinition) const
{
	const int32 Index = FindDefinitionIndex(Id);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	OutDefinition = Definitions[Index];
	return true;
}

void UEGInteractionProviderComponent::AddDefinition(const FInstancedStruct& Definition)
{
	const FEGInteractionDefinition* Base = Definition.GetPtr<FEGInteractionDefinition>();
	if (!Base || !Base->Id.IsValid())
	{
		return;
	}

	const int32 Existing = FindDefinitionIndex(Base->Id);
	if (Existing != INDEX_NONE)
	{
		Definitions[Existing] = Definition;
	}
	else
	{
		Definitions.Add(Definition);
	}
	NotifyDefinitionsChanged();
}

bool UEGInteractionProviderComponent::RemoveDefinition(FGameplayTag Id)
{
	const int32 Index = FindDefinitionIndex(Id);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	Definitions.RemoveAt(Index);
	NotifyDefinitionsChanged();
	return true;
}

bool UEGInteractionProviderComponent::SetDefinitionEnabled(FGameplayTag Id, bool bEnabled)
{
	const int32 Index = FindDefinitionIndex(Id);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	if (FEGInteractionDefinition* Definition = Definitions[Index].GetMutablePtr<FEGInteractionDefinition>())
	{
		if (Definition->bEnabled != bEnabled)
		{
			Definition->bEnabled = bEnabled;
			NotifyDefinitionsChanged();
		}
		return true;
	}
	return false;
}

// =====================================================================
// Claims
// =====================================================================

int32 UEGInteractionProviderComponent::FindClaimIndex(FGameplayTag Id) const
{
	return Claims.IndexOfByPredicate([Id](const FEGInteractionClaim& Claim) { return Claim.Id == Id; });
}

bool UEGInteractionProviderComponent::Claim(FGameplayTag Id, AActor* Holder)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Holder || !Id.IsValid())
	{
		return false;
	}

	const int32 Index = FindClaimIndex(Id);
	if (Index != INDEX_NONE)
	{
		FEGInteractionClaim& Existing = Claims[Index];
		// A stale holder (a destroyed pawn) never locks the target forever.
		if (Existing.Holder != Holder && IsValid(Existing.Holder))
		{
			return false;
		}

		Existing.Holder = Holder;
		OnClaimsChanged.Broadcast();
		return true;
	}

	FEGInteractionClaim& NewClaim = Claims.AddDefaulted_GetRef();
	NewClaim.Id = Id;
	NewClaim.Holder = Holder;
	OnClaimsChanged.Broadcast();
	return true;
}

void UEGInteractionProviderComponent::Release(FGameplayTag Id, AActor* Holder)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const int32 Index = FindClaimIndex(Id);
	if (Index == INDEX_NONE)
	{
		return;
	}

	if (Claims[Index].Holder != Holder && IsValid(Claims[Index].Holder))
	{
		return;
	}

	Claims.RemoveAt(Index);
	OnClaimsChanged.Broadcast();
}

bool UEGInteractionProviderComponent::IsClaimed(FGameplayTag Id) const
{
	const int32 Index = FindClaimIndex(Id);
	return Index != INDEX_NONE && IsValid(Claims[Index].Holder);
}

AActor* UEGInteractionProviderComponent::GetClaimHolder(FGameplayTag Id) const
{
	const int32 Index = FindClaimIndex(Id);
	return Index != INDEX_NONE && IsValid(Claims[Index].Holder) ? Claims[Index].Holder.Get() : nullptr;
}

bool UEGInteractionProviderComponent::IsClaimedByOther(FGameplayTag Id, const AActor* Interactor) const
{
	const AActor* Holder = GetClaimHolder(Id);
	return Holder != nullptr && Holder != Interactor;
}

void UEGInteractionProviderComponent::OnRep_Claims()
{
	OnClaimsChanged.Broadcast();
}

// =====================================================================
// Events
// =====================================================================

void UEGInteractionProviderComponent::BroadcastInteractionEvent(FGameplayTag Id, AActor* Interactor, EEGInteractionEventPhase Phase)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	MulticastInteractionEvent(Id, Interactor, Phase);
}

void UEGInteractionProviderComponent::MulticastInteractionEvent_Implementation(FGameplayTag Id, AActor* Interactor, EEGInteractionEventPhase Phase)
{
	OnInteractionEvent.Broadcast(Id, Interactor, Phase);
}

void UEGInteractionProviderComponent::NotifyFocus(bool bFocused, UPrimitiveComponent* HitComponent)
{
	// The same provider can be notified again when focus moves between its parts; the outline
	// is applied once and restored once, the event fires every time.
	ApplyHighlight(bFocused);
	OnFocusChanged.Broadcast(bFocused, HitComponent);
}

void UEGInteractionProviderComponent::ApplyHighlight(bool bEnabled)
{
	if (bHighlighted == bEnabled || (bEnabled && (!bHighlightOnFocus || !GetOwner())))
	{
		return;
	}

	bHighlighted = bEnabled;

	if (bEnabled)
	{
		HighlightCache.Reset();
		TInlineComponentArray<UPrimitiveComponent*> Primitives;
		GetOwner()->GetComponents(Primitives);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (!Primitive)
			{
				continue;
			}

			FCachedDepthState& Cached = HighlightCache.Add(Primitive);
			Cached.bRenderCustomDepth = Primitive->bRenderCustomDepth;
			Cached.StencilValue = Primitive->CustomDepthStencilValue;

			Primitive->SetCustomDepthStencilValue(HighlightStencilValue);
			Primitive->SetRenderCustomDepth(true);
		}
		return;
	}

	// Restore exactly what each primitive had, so an outline owned by another system survives.
	for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, FCachedDepthState>& Entry : HighlightCache)
	{
		if (UPrimitiveComponent* Primitive = Entry.Key.Get())
		{
			Primitive->SetCustomDepthStencilValue(Entry.Value.StencilValue);
			Primitive->SetRenderCustomDepth(Entry.Value.bRenderCustomDepth);
		}
	}
	HighlightCache.Reset();
}
