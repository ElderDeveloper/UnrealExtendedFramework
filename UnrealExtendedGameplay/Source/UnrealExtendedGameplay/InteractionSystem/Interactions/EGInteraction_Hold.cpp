// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteraction_Hold.h"

#include "UnrealExtendedGameplay/InteractionSystem/EGInteractionProviderComponent.h"
#include "UnrealExtendedGameplay/InteractionSystem/Tasks/EGInteractionTask_WaitHold.h"

DEFINE_LOG_CATEGORY_STATIC(LogEGInteractionHold, Log, All);

bool UEGInteraction_Hold::CanActivate_Implementation() const
{
	if (!Super::CanActivate_Implementation())
	{
		return false;
	}

	if (bClaimWhileHolding && GetProvider() && GetProvider()->IsClaimedByOther(GetInteractionId(), GetInteractorActor()))
	{
		return false;
	}

	return true;
}

float UEGInteraction_Hold::GetHoldTime() const
{
	if (const FEGInteractionDef_Hold* Definition = GetDefinition<FEGInteractionDef_Hold>())
	{
		return Definition->HoldTime;
	}

	UE_LOG(LogEGInteractionHold, Warning, TEXT("%s on %s was authored with a plain definition; hold interactions need FEGInteractionDef_Hold. Using 1s."),
		*GetClass()->GetName(), *GetNameSafe(GetTargetActor()));
	return 1.0f;
}

float UEGInteraction_Hold::GetHoldProgress() const
{
	return HoldTask && !HoldTask->IsFinished() ? HoldTask->GetProgress() : (bHoldCompleted ? 1.0f : 0.0f);
}

void UEGInteraction_Hold::ActivateInteraction_Implementation()
{
	bHoldCompleted = false;

	if (HasAuthority())
	{
		if (bClaimWhileHolding && GetProvider() && !GetProvider()->Claim(GetInteractionId(), GetInteractorActor()))
		{
			EndInteraction(true);
			return;
		}
		if (bSendProviderEvents)
		{
			SendProviderEvent(EEGInteractionEventPhase::Started);
		}
	}

	HoldTask = UEGInteractionTask_WaitHold::WaitHold(this, GetHoldTime(), true);
	if (!HoldTask)
	{
		EndInteraction(true);
		return;
	}

	HoldTask->OnProgress.AddDynamic(this, &UEGInteraction_Hold::HandleHoldProgress);
	HoldTask->OnCompleted.AddDynamic(this, &UEGInteraction_Hold::HandleHoldCompleted);
	HoldTask->OnCancelled.AddDynamic(this, &UEGInteraction_Hold::HandleHoldCancelled);
	HoldTask->ReadyForActivation();
}

void UEGInteraction_Hold::HandleHoldProgress(float Progress)
{
	SetHoldProgress(Progress);
}

void UEGInteraction_Hold::HandleHoldCompleted()
{
	bHoldCompleted = true;
	SetHoldProgress(1.0f);

	// The hold task is still mid-completion while this runs (it ends itself right after the
	// broadcast), so it must not count as "still running" when deciding whether to end.
	const UGameplayTask* CompletingTask = HoldTask;
	HoldTask = nullptr;

	OnHoldCompleted();

	// Nothing latent was started by the subclass, so the interaction is done.
	if (IsActive() && !HasActiveTasksOtherThan(CompletingTask))
	{
		EndInteraction(false);
	}
}

void UEGInteraction_Hold::HandleHoldCancelled()
{
	HoldTask = nullptr;
	EndInteraction(true);
}

void UEGInteraction_Hold::OnEndInteraction_Implementation(bool bCancelled)
{
	HoldTask = nullptr;
	SetHoldProgress(0.0f);

	if (HasAuthority())
	{
		if (bClaimWhileHolding && GetProvider())
		{
			GetProvider()->Release(GetInteractionId(), GetInteractorActor());
		}
		if (bSendProviderEvents)
		{
			SendProviderEvent(bCancelled ? EEGInteractionEventPhase::Cancelled : EEGInteractionEventPhase::Completed);
		}
	}
}
