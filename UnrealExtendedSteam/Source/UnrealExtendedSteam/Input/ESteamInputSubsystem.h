// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Shared/ESteamSubsystem.h"
#include "ESteamInputSubsystem.generated.h"

/**
 * Type of a Steam Input device (mirrors Steamworks ESteamInputType).
 * Device types the wrapper does not expose (Apple MFi, Android, single JoyCons, PS3...)
 * are reported as Unknown.
 */
UENUM(BlueprintType)
enum class EESteamInputType : uint8
{
	Unknown,
	SteamController,
	XBox360Controller,
	XBoxOneController,
	/** DirectInput controllers. */
	GenericGamepad,
	PS4Controller,
	PS5Controller,
	SwitchProController,
	/** Steam Link app on-screen virtual controller. */
	MobileTouch,
	SteamDeckController
};

/**
 * Source input mode driving an analog action (mirrors EInputSourceMode). Describes how the
 * bound physical input is interpreted (joystick, trigger, mouse region...).
 */
UENUM(BlueprintType)
enum class EESteamInputSourceMode : uint8
{
	None,
	Dpad,
	Buttons,
	FourButtons,
	AbsoluteMouse,
	RelativeMouse,
	JoystickMove,
	JoystickMouse,
	JoystickCamera,
	ScrollWheel,
	Trigger,
	TouchMenu,
	MouseJoystick,
	MouseRegion,
	RadialMenu,
	SingleButton,
	Switches
};

/** Requested glyph resolution for GetGlyphPNGForActionOrigin (mirrors ESteamInputGlyphSize). */
UENUM(BlueprintType)
enum class EESteamInputGlyphSize : uint8
{
	/** 32x32 pixels. */
	Small,
	/** 128x128 pixels. */
	Medium,
	/** 256x256 pixels. */
	Large
};

/** Haptic target for TriggerSimpleHapticEvent (mirrors EControllerHapticLocation). */
UENUM(BlueprintType)
enum class EESteamControllerHapticLocation : uint8
{
	Left,
	Right,
	Both
};

/** Trackpad selector for the legacy haptic pulse APIs (mirrors ESteamControllerPad). */
UENUM(BlueprintType)
enum class EESteamControllerPad : uint8
{
	Left,
	Right
};

/**
 * Motion sensor snapshot for a controller (mirrors InputMotionData_t). Populated only for
 * controllers with an IMU; calling GetMotionData wakes the IMU until the app closes.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSTEAM_API FESteamInputMotionData
{
	GENERATED_BODY()

	/** Absolute rotation of the controller since wakeup (identity while the IMU warms up). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Input")
	FQuat RotQuat = FQuat::Identity;

	/** Positional acceleration, raw hardware units mapping to roughly -2G..+2G per axis. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Input")
	FVector Accel = FVector::ZeroVector;

	/** Angular velocity, raw hardware units mapping to roughly -2000..+2000 deg/s per axis (X pitch, Y roll, Z yaw). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Input")
	FVector RotVel = FVector::ZeroVector;
};

/** State of a digital (button-like) action (mirrors InputDigitalActionData_t). */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSTEAM_API FESteamDigitalActionData
{
	GENERATED_BODY()

	/** True while the action is pressed. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Input")
	bool bState = false;

	/** True when the action is available to be bound in the active action set. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Input")
	bool bActive = false;
};

/** State of an analog (stick/trigger/mouse-like) action (mirrors InputAnalogActionData_t). */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSTEAM_API FESteamAnalogActionData
{
	GENERATED_BODY()

	/** How the bound physical input is interpreted (matches the action set configuration). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Input")
	EESteamInputSourceMode Mode = EESteamInputSourceMode::None;

	/** Horizontal state in [-1, 1]; delta updates for mouse actions. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Input")
	float X = 0.f;

	/** Vertical state in [-1, 1]; delta updates for mouse actions. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Input")
	float Y = 0.f;

	/** True when the action is available to be bound in the active action set. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Input")
	bool bActive = false;
};

/**
 * Fired when a controller connected. Also fires once per already-connected controller
 * right after InitSteamInput succeeds.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamInputDeviceConnected, int64, ControllerHandle);

/** Fired when a controller disconnected. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamInputDeviceDisconnected, int64, ControllerHandle);

/**
 * Wraps ISteamInput: controller enumeration, action sets, digital/analog actions,
 * vibration and the binding panel.
 *
 * Steam Input needs an explicit opt-in: call InitSteamInput before anything else. While
 * initialized, the subsystem pumps ISteamInput::RunFrame every frame through a core ticker.
 *
 * Controller and action handles are Steamworks uint64 values passed through Blueprint as
 * int64 — treat them as opaque and only feed back values received from this subsystem.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamInputSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// ---- Lifecycle ----

	/**
	 * Initializes Steam Input (idempotent) and enables device connect/disconnect callbacks;
	 * each already-connected controller immediately fires OnDeviceConnected.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input")
	bool InitSteamInput();

	/** Shuts Steam Input down (safe to call when not initialized). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input")
	void ShutdownSteamInput();

	/** True while Steam Input is initialized through InitSteamInput. */
	UFUNCTION(BlueprintPure, Category = "Steam|Input")
	bool IsSteamInputInitialized() const { return bSteamInputInitialized; }

	// ---- Controllers ----

	/**
	 * Lists the currently connected Steam Input enabled controllers.
	 * Returns the number of controllers written to OutHandles (0 when not initialized).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input")
	int32 GetConnectedControllers(TArray<int64>& OutHandles) const;

	/** The device type behind a controller handle (Unknown when unavailable or unmapped). */
	UFUNCTION(BlueprintPure, Category = "Steam|Input")
	EESteamInputType GetInputTypeForHandle(int64 ControllerHandle) const;

	// ---- Action sets ----

	/**
	 * Looks up the handle of an action set (e.g. "Menu", "Walk"). Query once on startup
	 * and store the handle. Returns 0 when unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input|Actions")
	int64 GetActionSetHandle(const FString& ActionSetName) const;

	/**
	 * Switches a controller to the given action set. Cheap — safe to call every frame
	 * from state loops instead of on state transitions.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input|Actions")
	void ActivateActionSet(int64 ControllerHandle, int64 ActionSetHandle);

	/** The action set currently active on a controller (0 when unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|Input|Actions")
	int64 GetCurrentActionSet(int64 ControllerHandle) const;

	// ---- Action set layers ----

	/**
	 * Enables an action set layer on top of the active action set. Layers stack and let you
	 * override a subset of bindings (e.g. a "Zoomed" layer over "Walk") without switching sets.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input|Actions")
	void ActivateActionSetLayer(int64 ControllerHandle, int64 ActionSetLayerHandle);

	/** Disables a single previously activated action set layer on a controller. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input|Actions")
	void DeactivateActionSetLayer(int64 ControllerHandle, int64 ActionSetLayerHandle);

	/** Disables every active action set layer on a controller. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input|Actions")
	void DeactivateAllActionSetLayers(int64 ControllerHandle);

	/**
	 * Lists the action set layers currently active on a controller (top of the stack last).
	 * Returns the number written to OutLayerHandles (0 when not initialized).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input|Actions")
	int32 GetActiveActionSetLayers(int64 ControllerHandle, TArray<int64>& OutLayerHandles) const;

	// ---- Digital actions ----

	/** Looks up the handle of a digital action. Query once on startup and store the handle. Returns 0 when unavailable. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input|Actions")
	int64 GetDigitalActionHandle(const FString& ActionName) const;

	/** Current state of a digital action on a controller (inactive default when unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|Input|Actions")
	FESteamDigitalActionData GetDigitalActionData(int64 ControllerHandle, int64 ActionHandle) const;

	/**
	 * The input origins (EInputActionOrigin values, passed through as int32) a digital action is
	 * bound to for the given action set on a controller — use them to draw the right glyphs.
	 * Returns the number written to OutOrigins (0 when unavailable; up to 8).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input|Actions")
	int32 GetDigitalActionOrigins(int64 ControllerHandle, int64 ActionSetHandle, int64 ActionHandle, TArray<int32>& OutOrigins) const;

	/** Localized human-readable name of a digital action (empty when unavailable). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input|Actions")
	FString GetStringForDigitalActionName(int64 ActionHandle) const;

	// ---- Analog actions ----

	/** Looks up the handle of an analog action. Query once on startup and store the handle. Returns 0 when unavailable. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input|Actions")
	int64 GetAnalogActionHandle(const FString& ActionName) const;

	/** Current state of an analog action on a controller (inactive default when unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|Input|Actions")
	FESteamAnalogActionData GetAnalogActionData(int64 ControllerHandle, int64 ActionHandle) const;

	/**
	 * The input origins (EInputActionOrigin values, passed through as int32) an analog action is
	 * bound to for the given action set on a controller. Returns the number written to OutOrigins.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input|Actions")
	int32 GetAnalogActionOrigins(int64 ControllerHandle, int64 ActionSetHandle, int64 ActionHandle, TArray<int32>& OutOrigins) const;

	/** Localized human-readable name of an analog action (empty when unavailable). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input|Actions")
	FString GetStringForAnalogActionName(int64 ActionHandle) const;

	/** Stops the residual momentum of an analog action (e.g. a flick-scroll) on a controller. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input|Actions")
	void StopAnalogActionMomentum(int64 ControllerHandle, int64 ActionHandle);

	// ---- Origins & glyphs ----

	/**
	 * Local filesystem path to a PNG glyph for an input origin (EInputActionOrigin as int32).
	 * @param Flags ESteamInputGlyphStyle bitmask (0 = default knockout style). Empty on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input|Glyphs")
	FString GetGlyphPNGForActionOrigin(int32 Origin, EESteamInputGlyphSize Size, int32 Flags) const;

	/**
	 * Local filesystem path to an SVG glyph for an input origin (EInputActionOrigin as int32).
	 * @param Flags ESteamInputGlyphStyle bitmask (0 = default knockout style). Empty on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input|Glyphs")
	FString GetGlyphSVGForActionOrigin(int32 Origin, int32 Flags) const;

	/** Localized human-readable name of an input origin (EInputActionOrigin as int32); empty when unavailable. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input|Glyphs")
	FString GetStringForActionOrigin(int32 Origin) const;

	// ---- Gamepad index mapping ----

	/** The Steam "gamepad index" (XInput-style slot) for a controller handle (-1 when unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|Input")
	int32 GetGamepadIndexForController(int64 ControllerHandle) const;

	/** The controller handle backing a Steam "gamepad index" slot (0 when unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|Input")
	int64 GetControllerForGamepadIndex(int32 Index) const;

	/**
	 * Reads the motion sensor (IMU) snapshot for a controller. The first call per controller
	 * wakes the IMU, which then stays active until the app closes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input")
	FESteamInputMotionData GetMotionData(int64 ControllerHandle) const;

	/**
	 * The controller's binding revision (major.minor). Returns false when no revision is
	 * available (e.g. a bindings file without a version).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input")
	bool GetDeviceBindingRevision(int64 ControllerHandle, int32& OutMajor, int32& OutMinor) const;

	/** The Remote Play session id driving a controller, or 0 when it is a local device. */
	UFUNCTION(BlueprintPure, Category = "Steam|Input")
	int32 GetRemotePlaySessionID(int64 ControllerHandle) const;

	// ---- Output ----

	/**
	 * Triggers a rumble event on a controller (translated to haptic pulses on Steam
	 * Controllers). Speeds are motor speeds clamped to [0, 65535].
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input")
	void TriggerVibration(int64 ControllerHandle, int32 LeftSpeed, int32 RightSpeed);

	/**
	 * Extended rumble that also drives the two trigger motors on supported controllers
	 * (DualSense/DualShock). All speeds are motor speeds clamped to [0, 65535].
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input")
	void TriggerVibrationExtended(int64 ControllerHandle, int32 LeftSpeed, int32 RightSpeed, int32 LeftTriggerSpeed, int32 RightTriggerSpeed);

	/**
	 * Fires a single pre-authored haptic event on controllers with haptic actuators.
	 * @param Intensity      Primary actuator intensity [0, 255].
	 * @param GainDB         Primary gain in decibels [-127, 127] (attenuation/boost).
	 * @param OtherIntensity Secondary actuator intensity [0, 255].
	 * @param OtherGainDB    Secondary gain in decibels [-127, 127].
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input")
	void TriggerSimpleHapticEvent(int64 ControllerHandle, EESteamControllerHapticLocation Location, int32 Intensity, int32 GainDB, int32 OtherIntensity, int32 OtherGainDB);

	/**
	 * Sets the LED color on controllers with a user-visible LED (e.g. DualSense light bar).
	 * @param Flags ESteamInputLEDFlag: 0 sets the color, 1 restores the user's default.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input")
	void SetLEDColor(int64 ControllerHandle, uint8 R, uint8 G, uint8 B, int32 Flags);

	/**
	 * Legacy Steam Controller haptic pulse on a single trackpad. Prefer TriggerVibration /
	 * TriggerSimpleHapticEvent on modern controllers; this only affects the original Steam Controller.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input")
	void Legacy_TriggerHapticPulse(int64 ControllerHandle, EESteamControllerPad Pad, int32 DurationMicroSec);

	/**
	 * Opens the Steam Input binding screen for a controller (overlay in Big Picture Mode,
	 * a separate window otherwise).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Input")
	bool ShowBindingPanel(int64 ControllerHandle);

	// ---- Events ----

	UPROPERTY(BlueprintAssignable, Category = "Steam|Input")
	FOnSteamInputDeviceConnected OnDeviceConnected;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Input")
	FOnSteamInputDeviceDisconnected OnDeviceDisconnected;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	/** Core ticker callback pumping ISteamInput::RunFrame while initialized. */
	bool TickRunFrame(float DeltaTime);

	void RemoveRunFrameTicker();

	friend class FESteamInputCallbacks;
	TSharedPtr<class FESteamInputCallbacks> Callbacks;

	FTSTicker::FDelegateHandle RunFrameTickerHandle;
	bool bSteamInputInitialized = false;
};
