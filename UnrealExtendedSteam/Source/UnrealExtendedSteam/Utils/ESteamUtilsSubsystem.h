// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamUtilsSubsystem.generated.h"

/** Corner used for Steam overlay toast notifications. */
UENUM(BlueprintType)
enum class EESteamNotificationPosition : uint8
{
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight
};

/** Number of lines allowed for the Big Picture gamepad text entry (mirrors EGamepadTextInputLineMode). */
UENUM(BlueprintType)
enum class EESteamGamepadTextInputLineMode : uint8
{
	SingleLine,
	MultipleLines
};

/** Character handling for the Big Picture gamepad text entry (mirrors EGamepadTextInputMode). */
UENUM(BlueprintType)
enum class EESteamGamepadTextInputMode : uint8
{
	/** Normal, visible text. */
	Normal,
	/** Password entry, obscured text. */
	Password
};

/** Keyboard layout / dismissal behaviour for the floating gamepad text entry (mirrors EFloatingGamepadTextInputMode). */
UENUM(BlueprintType)
enum class EESteamFloatingGamepadTextInputMode : uint8
{
	/** Enter dismisses the keyboard. */
	SingleLine,
	/** User explicitly closes the keyboard. */
	MultipleLines,
	/** Email layout, enter dismisses. */
	Email,
	/** Numeric layout, enter dismisses. */
	Numeric
};

/** Context that text filtering is performed in (mirrors ETextFilteringContext). */
UENUM(BlueprintType)
enum class EESteamTextFilteringContext : uint8
{
	Unknown,
	/** Game content; only legally required filtering is applied. */
	GameContent,
	/** Chat from another player. */
	Chat,
	/** A character or item name. */
	Name
};

/** Protocol tested by GetIPv6ConnectivityState (mirrors ESteamIPv6ConnectivityProtocol). */
UENUM(BlueprintType)
enum class EESteamIPv6ConnectivityProtocol : uint8
{
	Invalid,
	HTTP,
	UDP
};

/** Result of GetIPv6ConnectivityState (mirrors ESteamIPv6ConnectivityState). */
UENUM(BlueprintType)
enum class EESteamIPv6ConnectivityState : uint8
{
	/** No test has been run yet, or Steam is unavailable. */
	Unknown,
	/** A request on IPv6 recently succeeded. */
	Good,
	/** A request on IPv6 failed (no address assigned or no upstream connectivity). */
	Bad
};

/** Fired when Steam requests the application to shut down (e.g. Steam client exiting). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamShutdownRequested);

/** Fired when the machine is running low on battery. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamLowBatteryPower, int32, MinutesRemaining);

/** Fired when the user's IP country changed (GetIPCountry has the new value). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamIPCountryChanged);

/** Fired when the full-screen gamepad text input closes (EnteredText is empty when not submitted). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamGamepadTextInputDismissed, bool, bSubmitted, const FString&, EnteredText);

/** Fired when the floating on-screen keyboard closes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamFloatingGamepadTextInputDismissed);

/** Fired when the app resumes from suspend (e.g. Steam Deck sleep). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamAppResumingFromSuspend);

/**
 * Wraps ISteamUtils: app id, overlay state, Steam Deck / Big Picture detection,
 * UI language, country, server clock, overlay notification placement, gamepad text
 * input, text filtering, image access and VR helpers.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamUtilsSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** The running app id (0 when Steam is unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|Utils")
	int32 GetAppId() const;

	/** Two-letter country code of the user's IP (empty when unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|Utils")
	FString GetIPCountry() const;

	/** Steam server wall clock, unix time (0 when unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|Utils")
	int64 GetServerRealTime() const;

	/** Seconds the app has been active. */
	UFUNCTION(BlueprintPure, Category = "Steam|Utils")
	int32 GetSecondsSinceAppActive() const;

	/** Seconds since the computer (not just the app) was last active. Wraps ISteamUtils::GetSecondsSinceComputerActive. */
	UFUNCTION(BlueprintPure, Category = "Steam|Utils")
	int32 GetSecondsSinceComputerActive() const;

	/** The universe this client is connected to (mirrors EUniverse; 0 = invalid/unavailable). Wraps ISteamUtils::GetConnectedUniverse. */
	UFUNCTION(BlueprintPure, Category = "Steam|Utils")
	int32 GetConnectedUniverse() const;

	/** True when the Steam overlay is enabled and injected. */
	UFUNCTION(BlueprintPure, Category = "Steam|Utils")
	bool IsOverlayEnabled() const;

	/** True when the overlay needs the game to Present a frame so it can render. Wraps ISteamUtils::BOverlayNeedsPresent. */
	UFUNCTION(BlueprintPure, Category = "Steam|Utils")
	bool OverlayNeedsPresent() const;

	UFUNCTION(BlueprintPure, Category = "Steam|Utils")
	bool IsSteamInBigPictureMode() const;

	UFUNCTION(BlueprintPure, Category = "Steam|Utils")
	bool IsSteamRunningOnSteamDeck() const;

	UFUNCTION(BlueprintPure, Category = "Steam|Utils")
	bool IsSteamChinaLauncher() const;

	/** True when Steam itself is running in VR mode. Wraps ISteamUtils::IsSteamRunningInVR. */
	UFUNCTION(BlueprintPure, Category = "Steam|Utils|VR")
	bool IsSteamRunningInVR() const;

	/** Asks SteamUI to create and render its OpenVR dashboard. Wraps ISteamUtils::StartVRDashboard. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utils|VR")
	void StartVRDashboard();

	/** True when HMD content is streamed via Steam Remote Play. Wraps ISteamUtils::IsVRHeadsetStreamingEnabled. */
	UFUNCTION(BlueprintPure, Category = "Steam|Utils|VR")
	bool IsVRHeadsetStreamingEnabled() const;

	/** Sets whether HMD content is streamed via Steam Remote Play. Wraps ISteamUtils::SetVRHeadsetStreamingEnabled. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utils|VR")
	void SetVRHeadsetStreamingEnabled(bool bEnabled);

	/** Steam client UI language (e.g. "english"). */
	UFUNCTION(BlueprintPure, Category = "Steam|Utils")
	FString GetSteamUILanguage() const;

	/** Battery power [0..100], 255 while on AC power, 0 when unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Utils")
	int32 GetCurrentBatteryPower() const;

	/** Places overlay toast notifications in the given corner. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utils")
	void SetOverlayNotificationPosition(EESteamNotificationPosition Position);

	/** Insets overlay toast notifications from the chosen corner, in pixels. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utils")
	void SetOverlayNotificationInset(int32 HorizontalInset, int32 VerticalInset);

	/** Has Steam Input translate controller input to mouse/keyboard for launcher-style UIs. Wraps ISteamUtils::SetGameLauncherMode. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utils")
	void SetGameLauncherMode(bool bLauncherMode);

	/** Believed IPv6 connectivity to the internet on the given protocol. Wraps ISteamUtils::GetIPv6ConnectivityState. */
	UFUNCTION(BlueprintPure, Category = "Steam|Utils")
	EESteamIPv6ConnectivityState GetIPv6ConnectivityState(EESteamIPv6ConnectivityProtocol Protocol) const;

	/**
	 * Shows the full-screen Big Picture gamepad text input. The entered text arrives on
	 * OnGamepadTextInputDismissed. Wraps ISteamUtils::ShowGamepadTextInput.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utils|GamepadText")
	bool ShowGamepadTextInput(EESteamGamepadTextInputLineMode LineMode, EESteamGamepadTextInputMode CharMode, const FString& Description, int32 MaxChars, const FString& ExistingText);

	/**
	 * Opens a floating keyboard over the game at the given text-field rectangle (pixels, relative to the
	 * game window). Dismissal arrives on OnFloatingGamepadTextInputDismissed. Wraps ISteamUtils::ShowFloatingGamepadTextInput.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utils|GamepadText")
	bool ShowFloatingGamepadTextInput(EESteamFloatingGamepadTextInputMode KeyboardMode, int32 X, int32 Y, int32 Width, int32 Height);

	/** Length in characters of the previously entered gamepad text. Wraps ISteamUtils::GetEnteredGamepadTextLength. */
	UFUNCTION(BlueprintPure, Category = "Steam|Utils|GamepadText")
	int32 GetEnteredGamepadTextLength() const;

	/** The previously entered gamepad text (empty when none). Wraps ISteamUtils::GetEnteredGamepadTextInput. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utils|GamepadText")
	FString GetEnteredGamepadTextInput() const;

	/** Dismisses the full-screen gamepad text input. Wraps ISteamUtils::DismissGamepadTextInput. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utils|GamepadText")
	bool DismissGamepadTextInput();

	/** Dismisses the floating gamepad keyboard. Wraps ISteamUtils::DismissFloatingGamepadTextInput. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utils|GamepadText")
	bool DismissFloatingGamepadTextInput();

	/**
	 * Initializes text filtering, loading dictionaries for the game's language.
	 * Returns false when filtering is unavailable (FilterText then acts as a passthrough).
	 * Wraps ISteamUtils::InitFilterText.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utils|Filtering")
	bool InitFilterText();

	/**
	 * Filters InputText for the given context/source, applying legally required and user-configured filtering.
	 * Returns the filtered string (passthrough when filtering is unavailable). Wraps ISteamUtils::FilterText.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utils|Filtering")
	FString FilterText(EESteamTextFilteringContext Context, FESteamId SourceSteamId, const FString& InputText) const;

	/** Width and height of a Steam image handle (avatars, etc.). Wraps ISteamUtils::GetImageSize. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utils|Image")
	bool GetImageSize(int32 ImageHandle, int32& OutWidth, int32& OutHeight) const;

	/** RGBA pixels of a Steam image handle. OutRGBA is 4*Width*Height bytes. Wraps ISteamUtils::GetImageRGBA. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utils|Image")
	bool GetImageRGBA(int32 ImageHandle, TArray<uint8>& OutRGBA, int32& OutWidth, int32& OutHeight) const;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Utils")
	FOnSteamShutdownRequested OnSteamShutdownRequested;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Utils")
	FOnSteamLowBatteryPower OnLowBatteryPower;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Utils")
	FOnSteamIPCountryChanged OnIPCountryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Utils|GamepadText")
	FOnSteamGamepadTextInputDismissed OnGamepadTextInputDismissed;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Utils|GamepadText")
	FOnSteamFloatingGamepadTextInputDismissed OnFloatingGamepadTextInputDismissed;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Utils")
	FOnSteamAppResumingFromSuspend OnAppResumingFromSuspend;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	friend class FESteamUtilsCallbacks;
	TSharedPtr<class FESteamUtilsCallbacks> Callbacks;
};
