// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamScreenshotsSubsystem.generated.h"

/** VR screenshot layout for AddVRScreenshotToLibrary (mirrors EVRScreenshotType). */
UENUM(BlueprintType)
enum class EESteamVRScreenshotType : uint8
{
	None,
	Mono,
	Stereo,
	MonoCubemap,
	MonoPanorama,
	StereoPanorama
};

/** Fired when the user pressed the screenshot hotkey while screenshots are hooked by the game. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamScreenshotRequested);

/**
 * Fired when a screenshot has been added to the user's library (or failed to). ScreenshotHandle
 * is the handle of the newly written screenshot (0 on failure), usable with the tagging APIs —
 * this lets overlay/hotkey captures be tagged after they are written by Steam.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamScreenshotReady, bool, bSuccess, int32, ScreenshotHandle);

/**
 * Wraps ISteamScreenshots: triggering overlay screenshots, hooking the screenshot
 * hotkey and adding image files to the user's screenshot library.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamScreenshotsSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/**
	 * Asks the Steam overlay to take a screenshot. When screenshots are hooked
	 * (HookScreenshots(true)), OnScreenshotRequested fires instead and the game
	 * is expected to call AddScreenshotToLibrary itself.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Screenshots")
	void TriggerScreenshot();

	/**
	 * Toggles whether the game handles the screenshot hotkey instead of the overlay.
	 * While hooked, hotkey presses arrive on OnScreenshotRequested.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Screenshots")
	void HookScreenshots(bool bHook);

	/** True when the game has hooked screenshot handling. */
	UFUNCTION(BlueprintPure, Category = "Steam|Screenshots")
	bool IsScreenshotsHooked() const;

	/**
	 * Adds an image file (JPEG, TGA or PNG) to the user's screenshot library.
	 * ThumbnailPath may be empty (Steam generates a thumbnail); when provided it must be
	 * 200 pixels wide with the same aspect ratio as the screenshot.
	 * Returns a screenshot handle usable with SetLocation (0 on failure); completion
	 * is reported on OnScreenshotReady.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Screenshots")
	int32 AddScreenshotToLibrary(const FString& PngOrJpgFilePath, const FString& ThumbnailPath, int32 Width, int32 Height);

	/**
	 * Writes a screenshot to the user's library directly from raw pixel data.
	 * RGB must be tightly packed 24-bit RGB and exactly Width * Height * 3 bytes long.
	 * Returns a screenshot handle usable with the tagging APIs (0 on failure).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Screenshots")
	int32 WriteScreenshot(const TArray<uint8>& RGB, int32 Width, int32 Height);

	/**
	 * Adds a VR screenshot to the user's library from disk. Filename is the standard 2D
	 * library image; VRFilename is the image matching the given VR type. Both must be
	 * JPEG, TGA or PNG. Returns a screenshot handle usable with the tagging APIs (0 on failure).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Screenshots")
	int32 AddVRScreenshotToLibrary(EESteamVRScreenshotType Type, const FString& Filename, const FString& VRFilename);

	/** Sets location metadata (e.g. the map name) on a screenshot handle returned by AddScreenshotToLibrary. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Screenshots")
	bool SetLocation(int32 ScreenshotHandle, const FString& Location);

	/** Tags a user as visible in a screenshot (up to 32 per screenshot). Returns false on failure. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Screenshots")
	bool TagUser(int32 ScreenshotHandle, FESteamId UserId);

	/** Tags a published Workshop file as visible in a screenshot (up to 32 per screenshot). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Screenshots")
	bool TagPublishedFile(int32 ScreenshotHandle, int64 PublishedFileId);

	UPROPERTY(BlueprintAssignable, Category = "Steam|Screenshots")
	FOnSteamScreenshotRequested OnScreenshotRequested;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Screenshots")
	FOnSteamScreenshotReady OnScreenshotReady;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	friend class FESteamScreenshotsCallbacks;
	TSharedPtr<class FESteamScreenshotsCallbacks> Callbacks;
};
