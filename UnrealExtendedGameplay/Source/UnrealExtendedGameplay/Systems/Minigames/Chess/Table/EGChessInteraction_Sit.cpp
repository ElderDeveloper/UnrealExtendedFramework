// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessInteraction_Sit.h"

#include "EGChessTableActor.h"
#include "GameFramework/Pawn.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessUIConfig.h"

UEGChessInteraction_Sit::UEGChessInteraction_Sit()
{
	NetPolicy = EEGInteractionNetPolicy::ServerOnly;
	bShowPromptWhenUnavailable = false;
	bCancelOnFocusLost = false;
}

AEGChessTableActor* UEGChessInteraction_Sit::GetTable() const
{
	return Cast<AEGChessTableActor>(GetTargetActor());
}

bool UEGChessInteraction_Sit::CanActivate_Implementation() const
{
	const AEGChessTableActor* Table = GetTable();
	return Table && Super::CanActivate_Implementation() && Table->CanPawnSit(GetInteractorPawn());
}

FEGInteractionPrompt UEGChessInteraction_Sit::GetPresentation_Implementation() const
{
	FEGInteractionPrompt Prompt = Super::GetPresentation_Implementation();
	if (const AEGChessTableActor* Table = GetTable())
	{
		if (const UEGChessUIConfig* UI = Table->GetUIConfig())
		{
			if (!UI->SitPromptText.IsEmpty())
			{
				Prompt.Text = UI->SitPromptText;
			}
		}
	}
	return Prompt;
}

void UEGChessInteraction_Sit::ActivateInteraction_Implementation()
{
	if (HasAuthority())
	{
		if (AEGChessTableActor* Table = GetTable())
		{
			Table->RequestSeat(GetInteractorPawn(), GetHitComponent());
		}
	}
	EndInteraction(false);
}
