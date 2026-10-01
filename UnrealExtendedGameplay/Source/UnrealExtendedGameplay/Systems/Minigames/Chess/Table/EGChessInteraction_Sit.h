// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/InteractionSystem/EGInteraction.h"

#include "EGChessInteraction_Sit.generated.h"

class AEGChessTableActor;

/**
 * Sit down at a chess table. Server only: the table picks the seat (the chair looked at, else the
 * nearest free one), grants it and starts the seat sequence. The prompt disappears while no seat
 * is free and while the interactor already sits at this table.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGChessInteraction_Sit : public UEGInteraction
{
	GENERATED_BODY()

public:
	UEGChessInteraction_Sit();

	virtual bool CanActivate_Implementation() const override;
	virtual FEGInteractionPrompt GetPresentation_Implementation() const override;
	virtual void ActivateInteraction_Implementation() override;

private:
	AEGChessTableActor* GetTable() const;
};
