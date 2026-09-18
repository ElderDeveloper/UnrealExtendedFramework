// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTask.h"
#include "UnrealExtendedGameplay/InteractionSystem/EGInteraction.h"

#include "EGInteractionTask.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEGInteractionTaskSimpleDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGInteractionTaskProgressDelegate, float, Progress);

/**
 * Base for latent work inside a UEGInteraction. Same shape as an ability task: a static
 * factory creates it, the interaction owns it, the system component ticks it, and it fires
 * dynamic delegates the interaction (or its Blueprint child) binds to.
 *
 * Delegates only fire while the interaction is still alive and has not torn the task down.
 * When the interaction ends first, tasks are destroyed silently.
 */
UCLASS(Abstract)
class UNREALEXTENDEDGAMEPLAY_API UEGInteractionTask : public UGameplayTask
{
	GENERATED_BODY()

public:
	template <class T>
	static T* NewInteractionTask(UEGInteraction* OwningInteraction, FName InstanceName = NAME_None)
	{
		if (!OwningInteraction)
		{
			return nullptr;
		}

		T* Task = NewObject<T>(OwningInteraction);
		Task->InstanceName = InstanceName;
		Task->Interaction = OwningInteraction;
		Task->InitTask(*OwningInteraction, OwningInteraction->GetGameplayTaskDefaultPriority());
		return Task;
	}

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System|Tasks")
	UEGInteraction* GetInteraction() const { return Interaction.Get(); }

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

	/** False once the owning interaction has ended or gone away. */
	bool ShouldBroadcast() const { return !bOwnerFinished && Interaction.IsValid(); }

	TWeakObjectPtr<UEGInteraction> Interaction;
	bool bOwnerFinished = false;
};
