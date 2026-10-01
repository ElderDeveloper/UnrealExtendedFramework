// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessMatchTypes.h"

#include "EGChessSoundSet.generated.h"

class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGChessSoundEntry
{
	GENERATED_BODY()

	/** Wave, cue or MetaSound. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TSoftObjectPtr<USoundBase> Sound;

	/** Optional per-piece sounds, so a king lands heavier than a pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TMap<EEGChessPieceType, TSoftObjectPtr<USoundBase>> PieceOverrides;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0"))
	float Volume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0.1"))
	float PitchMin = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0.1"))
	float PitchMax = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TSoftObjectPtr<USoundAttenuation> Attenuation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TSoftObjectPtr<USoundConcurrency> Concurrency;
};

/** Board and game sounds. Widget sounds (button clicks) belong to the game's widgets. */
UCLASS(BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGChessSoundSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TMap<EEGChessEvent, FEGChessSoundEntry> Sounds;

	void GatherSoftPaths(TArray<FSoftObjectPath>& OutPaths) const
	{
		for (const TPair<EEGChessEvent, FEGChessSoundEntry>& Pair : Sounds)
		{
			auto Add = [&OutPaths](const FSoftObjectPath& Path) { if (Path.IsValid()) { OutPaths.AddUnique(Path); } };
			Add(Pair.Value.Sound.ToSoftObjectPath());
			Add(Pair.Value.Attenuation.ToSoftObjectPath());
			Add(Pair.Value.Concurrency.ToSoftObjectPath());
			for (const TPair<EEGChessPieceType, TSoftObjectPtr<USoundBase>>& Override : Pair.Value.PieceOverrides)
			{
				Add(Override.Value.ToSoftObjectPath());
			}
		}
	}
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGChessEffectEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TSoftObjectPtr<UNiagaraSystem> System;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0.01"))
	float Scale = 1.0f;
};

/** Optional Niagara per event, through the same routing as sounds. */
UCLASS(BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGChessEffectsSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TMap<EEGChessEvent, FEGChessEffectEntry> Effects;

	void GatherSoftPaths(TArray<FSoftObjectPath>& OutPaths) const
	{
		for (const TPair<EEGChessEvent, FEGChessEffectEntry>& Pair : Effects)
		{
			if (Pair.Value.System.ToSoftObjectPath().IsValid())
			{
				OutPaths.AddUnique(Pair.Value.System.ToSoftObjectPath());
			}
		}
	}
};
