// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"

#include "EGChessAIProfile.generated.h"

class UEGChessAI;

/** Weights of the rule-based AI's scoring rules. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGChessAIWeights
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	float Material = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	float HangingPenalty = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	float Rescue = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	float Promotion = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	float Check = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	float Castling = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	float Development = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	float Centre = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	float KingSafety = 2.0f;
};

/**
 * How an AI plays and feels. The AI class is chosen here, so a stronger engine replaces the
 * rule-based one without touching anything else.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGChessAIProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UEGChessAIProfile();

	/** Shown in the HUD in place of a player name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	FText DisplayName;

	/** Empty uses UEGChessAI_RuleBased. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TSubclassOf<UEGChessAI> AIClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	FEGChessAIWeights Weights;

	/** Random jitter added to every move's score, in score units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0"))
	float Randomness = 2.0f;

	/** Chance (0..1) that the hanging-piece checks are skipped for a move choice. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0", ClampMax = "1"))
	float SkipHangingCheckChance = 0.0f;

	/** Chance (0..1) that an available mate in one is overlooked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0", ClampMax = "1"))
	float MissMateChance = 0.0f;

	/** Reject moves that let the opponent win material or mate on the reply. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	bool bOneMoveLookahead = false;

	/** Pause before answering, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	FVector2D ThinkDelayRange = FVector2D(0.8, 2.5);

	/** Extra think time per legal move available, so complex positions take longer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0"))
	float ThinkDelayPerLegalMove = 0.02f;

	/** Accept a draw when behind by more than this much material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0"))
	float AcceptDrawWhenBehindBy = 2.5f;

	/** With equal material, accept a draw from this full-move number... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0"))
	int32 AcceptEqualDrawAfterMove = 40;

	/** ...when no more than this many pieces are left on the board. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "2"))
	int32 AcceptEqualDrawMaxPieces = 12;

	/** Takebacks allowed per game against this AI. Negative is unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	int32 MaxTakebacks = -1;
};
