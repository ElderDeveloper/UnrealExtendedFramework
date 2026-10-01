// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessMatchTypes.h"

#include "EGChessTableConfig.generated.h"

class UEGChessAnimationSet;
class UEGChessEffectsSet;
class UEGChessInputConfig;
class UEGChessPieceSet;
class UEGChessSoundSet;
class UEGChessTableStyle;
class UEGChessUIConfig;

/**
 * The one thing a table must be given. Several tables can share one; a placed table can still
 * override the match rules on its own instance.
 *
 * The sets referenced here are small. The heavy content inside them (meshes, montages, sounds,
 * widget classes) is soft-referenced and loaded asynchronously by the table.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGChessTableConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UEGChessTableStyle> TableStyle = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UEGChessPieceSet> PieceSet = nullptr;

	/** For pawns that do not bring their own through IEGChessCharacterInterface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UEGChessAnimationSet> AnimationSet = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UEGChessSoundSet> SoundSet = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UEGChessEffectsSet> EffectsSet = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UEGChessUIConfig> UI = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UEGChessInputConfig> Input = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	FEGChessMatchRules MatchRules;
};
