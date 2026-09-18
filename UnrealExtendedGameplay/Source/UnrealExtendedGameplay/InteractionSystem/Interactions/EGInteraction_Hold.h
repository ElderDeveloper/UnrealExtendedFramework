// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/InteractionSystem/EGInteraction.h"

#include "EGInteraction_Hold.generated.h"

class UEGInteractionTask_WaitHold;

/**
 * Hold-to-complete. Reads its duration from an FEGInteractionDef_Hold definition, claims the
 * definition on the provider while holding so nobody else can start it, publishes progress,
 * and calls OnHoldCompleted when the hold lands.
 *
 * Subclasses do the work in OnHoldCompleted. If nothing latent is started there, the
 * interaction ends on its own right after; otherwise it ends when the last task does, or when
 * the subclass calls EndInteraction.
 */
UCLASS(Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API UEGInteraction_Hold : public UEGInteraction
{
	GENERATED_BODY()

public:
	/** Claim the definition on the provider for the duration of the hold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extended|Interaction System")
	bool bClaimWhileHolding = true;

	/** Fire the provider's cosmetic event on start, completion and cancel. Authority only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extended|Interaction System")
	bool bSendProviderEvents = true;

	virtual bool CanActivate_Implementation() const override;
	virtual void ActivateInteraction_Implementation() override;
	virtual void OnEndInteraction_Implementation(bool bCancelled) override;

	/** The hold reached its duration. Do the work here. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Interaction System")
	void OnHoldCompleted();
	virtual void OnHoldCompleted_Implementation() {}

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	float GetHoldTime() const;

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	float GetHoldProgress() const;

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	bool HasHoldCompleted() const { return bHoldCompleted; }

protected:
	UFUNCTION()
	void HandleHoldProgress(float Progress);

	UFUNCTION()
	void HandleHoldCompleted();

	UFUNCTION()
	void HandleHoldCancelled();

	UPROPERTY(Transient)
	TObjectPtr<UEGInteractionTask_WaitHold> HoldTask;

	bool bHoldCompleted = false;
};
