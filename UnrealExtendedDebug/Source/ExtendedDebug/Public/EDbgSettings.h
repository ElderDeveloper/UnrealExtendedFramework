// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Settings/EFDeveloperSettings.h"
#include "EDbgSettings.generated.h"

/**
 * Project Settings > Extended Framework > Extended Debug.
 *
 * Everything here applies to development builds only: Shipping builds compile the tools out.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Extended Debug"))
class EXTENDEDDEBUG_API UEDbgSettings : public UEFDeveloperSettings
{
	GENERATED_BODY()

public:
	UEDbgSettings();

	static const UEDbgSettings* Get() { return GetDefault<UEDbgSettings>(); }

	/** Off: no menu, no watches, no cheats, even in development builds (e.g. for a press build). */
	UPROPERTY(config, EditAnywhere, Category = "General")
	bool bEnabled = true;

	/** Opens and closes the debug menu. Extended.Debug.Menu does the same from the console. */
	UPROPERTY(config, EditAnywhere, Category = "Menu")
	FKey MenuKey;

	/** Clicking both thumbsticks together opens and closes the menu, for gamepad-only devices. */
	UPROPERTY(config, EditAnywhere, Category = "Menu")
	bool bGamepadChord = true;

	/**
	 * Lets clients run cheats marked Run On Server on a development server. Turn off for public
	 * playtests on development servers.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Cheats")
	bool bAllowRemoteCheats = true;

	/** Draws watches (Set Debug Watch) on screen. Extended.Debug.Watches toggles it at run time. */
	UPROPERTY(config, EditAnywhere, Category = "Watches")
	bool bShowWatches = true;

	/** A watch not updated for this long disappears; 0 keeps watches until cleared. */
	UPROPERTY(config, EditAnywhere, Category = "Watches", meta = (ClampMin = "0", Units = "Seconds"))
	float WatchTimeoutSeconds = 10.f;
};
