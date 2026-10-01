// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "EGChessTimeControl.generated.h"

/** Base time plus increment. A table with no time control plays untimed. */
UCLASS(BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGChessTimeControl : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Shown in the HUD, e.g. "Blitz 3+2". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "1", Units = "s"))
	float BaseSeconds = 300.0f;

	/** Added to the mover's clock after every move. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0", Units = "s"))
	float IncrementSeconds = 0.0f;

	/** The low-time warning starts at this many seconds left. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0", Units = "s"))
	float LowTimeThresholdSeconds = 20.0f;
};
