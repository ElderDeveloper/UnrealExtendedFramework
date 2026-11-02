// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/InteractionSystem/EGInteraction.h"
#include "UnrealExtendedGameplay/InteractionSystem/EGInteractionSystemTypes.h"
#include "UnrealExtendedGameplay/Systems/Placement/Session/EGPlacementSessionTypes.h"

#include "EGInteraction_PlacementMove.generated.h"

class UEGPlaceableDefinition;
class UEGPlacementSessionComponent;

/** Parameters for UEGInteraction_PlacementMove. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGInteractionDef_PlacementMove : public FEGInteractionDefinition
{
	GENERATED_BODY()

	/** Spec to use; empty uses Spec below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TSoftObjectPtr<UEGPlaceableDefinition> Placeable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FEGPlacementSpec Spec;

	/** Modes for this move; empty uses the session's default modes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TArray<FEGPlacementMode> Modes;
};

/**
 * Starts a move session on the interactor for the interacted actor and holds the provider's claim
 * on this interaction while the move is open, so nobody else moves it at the same time. The
 * preview is built from the actor's own meshes; the session's commit handler (UEGPlacementCommit_MoveActor
 * for the stock behaviour) receives the actor as the request's MovedActor.
 *
 * Projects subclass it to pick up instead of moving (override BuildSelection and the handler).
 */
UCLASS(Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API UEGInteraction_PlacementMove : public UEGInteraction
{
	GENERATED_BODY()

public:
	UEGInteraction_PlacementMove();

	virtual bool CanActivate_Implementation() const override;
	virtual void ActivateInteraction_Implementation() override;
	virtual void OnEndInteraction_Implementation(bool bCancelled) override;

	/** The selection to move the target with. Default: the definition's spec, the target's meshes, the target as MovedActor. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Placement")
	bool BuildSelection(AActor* Target, FEGPlacementSelection& OutSelection) const;
	virtual bool BuildSelection_Implementation(AActor* Target, FEGPlacementSelection& OutSelection) const;

	/** The interactor's session: on the pawn, else on its controller. */
	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	UEGPlacementSessionComponent* FindSession() const;

private:
	UFUNCTION()
	void HandleSessionEnded(bool bSessionCancelled);

	TWeakObjectPtr<UEGPlacementSessionComponent> Session;
	bool bClaimed = false;
};
