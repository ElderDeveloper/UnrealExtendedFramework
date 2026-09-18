// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "EGInteractionSystemTypes.h"

#include "EGInteractionProviderComponent.generated.h"

/**
 * UEGInteractionProviderComponent
 *
 * The target side of the interaction system. Holds the authored definitions, answers gather
 * queries, and keeps the two pieces of shared state a target needs: replicated claims for
 * exclusivity and a cosmetic multicast for remote feedback.
 *
 * Definitions are not replicated. Placed and spawned actors carry the same authored list on
 * every machine. A provider whose list depends on runtime state should override
 * GatherInteractions and derive the list from replicated properties instead of mutating it.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGInteractionProviderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEGInteractionProviderComponent();

	/** Resolve the provider for an actor: interface first, component search second. */
	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	static UEGInteractionProviderComponent* FindProvider(const AActor* Actor);

	// -----------------------------------------------------------------
	// Definitions
	// -----------------------------------------------------------------

	/** Authored interactions. Use AddDefinition/RemoveDefinition/SetDefinitionEnabled for runtime edits. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extended|Interaction System",
		meta = (BaseStruct = "/Script/UnrealExtendedGameplay.EGInteractionDefinition"))
	TArray<FInstancedStruct> Definitions;

	/**
	 * Answer a query with the definitions available for it. The default returns the authored
	 * list filtered by bEnabled and PartComponent. Runs on the client for prompts and on the
	 * server for validation, so every term must be state both sides agree on.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Interaction System")
	void GatherInteractions(const FEGInteractionQuery& Query, TArray<FInstancedStruct>& OutDefinitions) const;
	virtual void GatherInteractions_Implementation(const FEGInteractionQuery& Query, TArray<FInstancedStruct>& OutDefinitions) const;

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System")
	bool FindDefinition(FGameplayTag Id, FInstancedStruct& OutDefinition) const;

	/** Local mutation. Call it on every machine, or derive the list in GatherInteractions instead. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System")
	void AddDefinition(const FInstancedStruct& Definition);

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System")
	bool RemoveDefinition(FGameplayTag Id);

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System")
	bool SetDefinitionEnabled(FGameplayTag Id, bool bEnabled);

	/**
	 * Bumped on every definition change. A system component focused on this provider re-gathers
	 * when it sees a new revision. Providers that derive GatherInteractions from replicated
	 * state call NotifyDefinitionsChanged from their OnRep.
	 */
	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	int32 GetDefinitionsRevision() const { return DefinitionsRevision; }

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System")
	void NotifyDefinitionsChanged() { ++DefinitionsRevision; }

	// -----------------------------------------------------------------
	// Claims
	// -----------------------------------------------------------------

	/** Mark Id as in use by Holder. Authority only. Fails when somebody else holds it. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Extended|Interaction System")
	bool Claim(FGameplayTag Id, AActor* Holder);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Extended|Interaction System")
	void Release(FGameplayTag Id, AActor* Holder);

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	bool IsClaimed(FGameplayTag Id) const;

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	AActor* GetClaimHolder(FGameplayTag Id) const;

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	bool IsClaimedByOther(FGameplayTag Id, const AActor* Interactor) const;

	UPROPERTY(BlueprintAssignable, Category = "Extended|Interaction System")
	FEGInteractionClaimsChanged OnClaimsChanged;

	// -----------------------------------------------------------------
	// Events
	// -----------------------------------------------------------------

	/** Fires on every machine the owner is relevant to. Driven by BroadcastInteractionEvent. */
	UPROPERTY(BlueprintAssignable, Category = "Extended|Interaction System")
	FEGInteractionProviderEvent OnInteractionEvent;

	/** Local focus notification from the interactor that is looking at this provider. */
	UPROPERTY(BlueprintAssignable, Category = "Extended|Interaction System")
	FEGInteractionProviderFocus OnFocusChanged;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Extended|Interaction System")
	void BroadcastInteractionEvent(FGameplayTag Id, AActor* Interactor, EEGInteractionEventPhase Phase);

	/** Called by the interactor's system component. Local only. */
	void NotifyFocus(bool bFocused, UPrimitiveComponent* HitComponent);

	// -----------------------------------------------------------------
	// Focus highlight
	// -----------------------------------------------------------------

	/** Custom-depth outline on every primitive while focused. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Focus")
	bool bHighlightOnFocus = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Focus", meta = (ClampMin = "0", ClampMax = "255", EditCondition = "bHighlightOnFocus"))
	int32 HighlightStencilValue = 2;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastInteractionEvent(FGameplayTag Id, AActor* Interactor, EEGInteractionEventPhase Phase);

	UPROPERTY(ReplicatedUsing = OnRep_Claims)
	TArray<FEGInteractionClaim> Claims;

	UFUNCTION()
	void OnRep_Claims();

	/** Applies or restores the custom-depth outline. Restores whatever each primitive had before. */
	void ApplyHighlight(bool bEnabled);

private:
	struct FCachedDepthState
	{
		bool bRenderCustomDepth = false;
		int32 StencilValue = 0;
	};

	int32 FindDefinitionIndex(FGameplayTag Id) const;
	int32 FindClaimIndex(FGameplayTag Id) const;

	TMap<TWeakObjectPtr<UPrimitiveComponent>, FCachedDepthState> HighlightCache;
	int32 DefinitionsRevision = 0;
	bool bHighlighted = false;
};
