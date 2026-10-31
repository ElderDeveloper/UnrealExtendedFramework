// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGTurnInPlaceTypes.h"
#include "UObject/Interface.h"

#include "EGTurnInPlaceAnimSource.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class UEGTurnInPlaceAnimSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * IEGTurnInPlaceAnimSource
 *
 * Implemented by the character's anim instance (UEGAnimInstance does it already). Returns the turn
 * curves cached on the worker thread by UEGTurnInPlaceAnimLibrary::ThreadSafeUpdateTurnInPlace,
 * at the same point the turn states were decided. Gameplay never reads the curves on its own
 * schedule, which would give it values from a different frame than the anim graph used.
 */
class UNREALEXTENDEDGAMEPLAY_API IEGTurnInPlaceAnimSource
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Turn In Place")
	FEGTurnInPlaceCurves GetTurnInPlaceCurves() const;

	virtual FEGTurnInPlaceCurves GetTurnInPlaceCurves_Implementation() const { return FEGTurnInPlaceCurves(); }
};
