// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGAnimEventTypes.h"
#include "UObject/Interface.h"

#include "EGAnimEventReceiver.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class UEGAnimEventReceiver : public UInterface
{
	GENERATED_BODY()
};

/**
 * IEGAnimEventReceiver
 *
 * Implemented by anything that reacts to animation events: the owning actor, any of its
 * components, or the mesh's anim instance. Anim event notifies carry only a tag and a payload;
 * the behaviour lives in the receivers, so animation assets never depend on gameplay classes.
 */
class UNREALEXTENDEDGAMEPLAY_API IEGAnimEventReceiver
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Anim Events")
	void ReceiveAnimEvent(const FEGAnimEvent& Event);
};

namespace EGAnimEvents
{
	/** A process-unique id for a new notify window. */
	UNREALEXTENDEDGAMEPLAY_API int32 NewWindowId();

	/**
	 * Delivers the event to every receiver on the mesh's owner: the actor, then its components,
	 * then the mesh's anim instance.
	 */
	UNREALEXTENDEDGAMEPLAY_API void Dispatch(const FEGAnimEvent& Event);
}
