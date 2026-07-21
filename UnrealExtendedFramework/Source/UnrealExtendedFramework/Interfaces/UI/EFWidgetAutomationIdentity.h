// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EFWidgetAutomationIdentity.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class UEFWidgetAutomationIdentity : public UInterface
{
	GENERATED_BODY()
};

/**
 * Stable automation identity for widgets.
 *
 * UI Lab interaction scripts, the focus visualizer, and automation assertions address widgets
 * through this identity instead of screen coordinates or widget-tree paths, so tests survive
 * layout and hierarchy changes. Runtime UI may also use it for analytics or accessibility ids.
 */
class UNREALEXTENDEDFRAMEWORK_API IEFWidgetAutomationIdentity
{
	GENERATED_BODY()

public:
	/** Returns the stable, unique-within-its-screen identity of this widget (e.g. "Tab_Audio"). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended UI|Automation")
	FName GetAutomationIdentity() const;
};
