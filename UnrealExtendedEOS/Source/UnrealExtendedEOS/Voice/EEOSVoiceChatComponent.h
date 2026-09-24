// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Sound/SoundAttenuation.h"
#include "EEOSVoiceChatComponent.generated.h"

class AController;
class APawn;
class APlayerState;
class UAudioComponent;
class UEEOSVoiceSubsystem;
class UInputAction;
class UInputMappingContext;
class APlayerController;
struct FEEOSVoiceCaptureState;
class UAudioCapture;

UENUM(BlueprintType)
enum class EEOSVoiceInputMode : uint8
{
	/** Transmit while the push-to-talk action is held. */
	PushToTalk,
	/** Transmit while the microphone level stays above the detection threshold. */
	VoiceActivation,
	/** One press of the push-to-talk action opens the microphone, the next press closes it. */
	PushToToggle
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEEOSComponentRoomEvent, const FString&, RoomName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEEOSComponentRoomFailure, const FString&, RoomName, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEEOSMicrophoneMuteChanged, bool, bMuted);

/**
	* The role of a voice chat component in the world.
	*/
UENUM(BlueprintType)
enum class EEOSVoiceRole : uint8
{
	/** Sends AND receives voice. Standard player voice chat. */
	Transceiver		UMETA(DisplayName = "Transceiver (Send + Receive)"),

	/** Only sends voice to the room. E.g., an in-world microphone. */
	Source			UMETA(DisplayName = "Source (Send Only)"),

	/** Only receives voice from the room. E.g., a speaker, radio, TV. */
	Listener		UMETA(DisplayName = "Listener (Receive Only)"),
};

/**
	* Where a voice chat component gets its room name.
	*/
UENUM(BlueprintType)
enum class EEOSVoiceRoomSource : uint8
{
	/** The current lobby's voice room. EOS generates the name; the component follows it when the lobby changes. */
	Lobby			UMETA(DisplayName = "Current Lobby"),

	/** RoomName as written. It must be a voice channel this client is already in. */
	Named			UMETA(DisplayName = "Room Name"),
};

/**
	* A voice point with component-owned microphone input and multiple room bindings.
	* Lobby membership is automatic. Custom rooms use trusted-server join credentials.
	* Joining a room and binding a spatial point to it are separate operations.
	* Receive volume remains global per player in IVoiceChatUser; contributions use their maximum.
	*/
UCLASS(ClassGroup = (EOS), meta = (BlueprintSpawnableComponent, DisplayName = "EOS Voice Chat"))
class UNREALEXTENDEDEOS_API UEEOSVoiceChatComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UEEOSVoiceChatComponent();

	// ── Configuration ────────────────────────────────────────────────────────

	/** The role of this voice point. Determines send/receive behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
	EEOSVoiceRole Role = EEOSVoiceRole::Transceiver;

	/** Where the room name comes from. Current Lobby follows the lobby's voice room on its own. */
	UPROPERTY(BlueprintReadOnly, Category = "Voice|Deprecated", meta = (DeprecatedProperty, DeprecationMessage = "Use Follow Lobby Room and Room Bindings."))
	EEOSVoiceRoomSource RoomSource = EEOSVoiceRoomSource::Lobby;

	/**
	 * The voice room this component uses. With Room Source = Room Name this is the room to join and
	 * must be a voice channel this client is in. With Current Lobby it holds the followed lobby room
	 * and is replaced when the lobby changes.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Voice|Deprecated", meta = (DeprecatedProperty, DeprecationMessage = "Use Room Bindings."))
	FString RoomName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Rooms")
	bool bFollowLobbyRoom = true;
	/** Additional spatial bindings. Binding does not itself join EOS. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Rooms")
	TSet<FString> RoomBindings;
	/** The authored mode. A game's settings layer may replace it at runtime through SetInputMode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Microphone")
	EEOSVoiceInputMode InputMode = EEOSVoiceInputMode::PushToTalk;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Microphone", meta = (ClampMin = "0.001", ClampMax = "0.5"))
	float DetectionThreshold = 0.02f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Microphone", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float DetectionReleaseSeconds = 0.28f;
	/**
	 * Microphone volume sent to the room: 0 is silent, 1 leaves the microphone unchanged and 2 is the
	 * most EOS will boost it. Applied to the local voice user once this component is the live local
	 * source; the voice user has one microphone volume, so the last local component to apply wins.
	 * A game's settings layer may replace it at runtime through SetMicrophoneVolume.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Microphone", meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0"))
	float MicrophoneVolume = 1.0f;
	/**
	 * Testing aid for the key modes: record what the key captured and play it back on this
	 * machine once the key is released (push-to-toggle: once it is toggled off). Nothing extra
	 * reaches the room. With the microphone test
	 * on, the take is made without sending anything at all. Needs a joined room, because EOS only
	 * delivers captured audio for rooms it is sending to.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Microphone")
	bool bReplayOnRelease = false;
	/** Longest take kept for a replay. Audio past it is dropped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Microphone", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float ReplayMaxSeconds = 15.0f;
	/**
	 * Testing aid: capture the default microphone with Unreal's own audio capture instead of EOS, so
	 * the level meter, IsTalking and the replay work alone, without a lobby, another player or transmit
	 * permission. Nothing from this stream is ever sent; transmission still needs permission as usual.
	 * Pair it with bReplayOnRelease to hear a take.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Microphone")
	bool bLocalCaptureTest = false;
	/** Register the authored context for the local player and read its action values directly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Input", meta = (DisplayName = "Use Enhanced Input"))
	bool bAutoBindInput = true;
	/**
	 * The microphone key. Push-to-talk transmits while it is held; push-to-toggle opens the
	 * microphone on one press and closes it on the next. Voice activation ignores it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Input")
	TObjectPtr<UInputAction> PushToTalkAction;
	/** Authored context containing PushToTalkAction and its player-mappable keys. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Input")
	TObjectPtr<UInputMappingContext> PushToTalkContext;
	/** Optional action from the owning game's existing mapping context. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice|Input")
	TObjectPtr<UInputAction> ToggleMuteAction;

	UPROPERTY(BlueprintAssignable, Category = "Voice|Microphone")
	FEEOSMicrophoneMuteChanged OnMicrophoneMuteChanged;
	UPROPERTY(BlueprintAssignable, Category = "Voice|Rooms")
	FEEOSComponentRoomEvent OnRoomJoined;
	UPROPERTY(BlueprintAssignable, Category = "Voice|Rooms")
	FEEOSComponentRoomEvent OnRoomLeft;
	UPROPERTY(BlueprintAssignable, Category = "Voice|Rooms")
	FEEOSComponentRoomFailure OnRoomJoinFailed;
	UPROPERTY(BlueprintAssignable, Category = "Voice|Rooms")
	FEEOSComponentRoomFailure OnRoomLeaveFailed;

	/** Joins for the local user and binds this point. Custom room credentials come from a trusted server. */
	UFUNCTION(BlueprintCallable, Category = "Voice|Rooms")
	bool JoinRoom(const FString& InRoomName, const FString& ChannelCredentials = TEXT(""));
	/** Leaves a custom channel for the local user. Lobby channels must be left through lobby membership. */
	UFUNCTION(BlueprintCallable, Category = "Voice|Rooms")
	bool LeaveRoom(const FString& InRoomName);
	UFUNCTION(BlueprintCallable, Category = "Voice|Rooms")
	void AddRoomBinding(const FString& InRoomName);
	UFUNCTION(BlueprintCallable, Category = "Voice|Rooms")
	void RemoveRoomBinding(const FString& InRoomName);
	UFUNCTION(BlueprintCallable, Category = "Voice|Rooms")
	void SetFollowLobbyRoom(bool bFollow);
	UFUNCTION(BlueprintPure, Category = "Voice|Rooms")
	TArray<FString> GetJoinedRooms() const;
	UFUNCTION(BlueprintPure, Category = "Voice|Rooms")
	TArray<FString> GetRoomBindings() const;

	UFUNCTION(BlueprintCallable, Category = "Voice|Microphone")
	void SetInputMode(EEOSVoiceInputMode NewMode);
	UFUNCTION(BlueprintCallable, Category = "Voice|Microphone")
	void SetDetectionSettings(float Threshold, float ReleaseSeconds);
	/** 0..2, see MicrophoneVolume. A live local component applies it at once. */
	UFUNCTION(BlueprintCallable, Category = "Voice|Microphone")
	void SetMicrophoneVolume(float Volume);
	UFUNCTION(BlueprintCallable, Category = "Voice|Microphone")
	void SetMicrophoneMuted(bool bMuted);
	UFUNCTION(BlueprintPure, Category = "Voice|Microphone")
	bool IsMicrophoneMuted() const { return bMicrophoneMuted; }
	UFUNCTION(BlueprintCallable, Category = "Voice|Microphone")
	void SetMicrophoneTest(bool bEnabled);
	UFUNCTION(BlueprintPure, Category = "Voice|Microphone")
	bool IsTalking() const;
	UFUNCTION(BlueprintPure, Category = "Voice|Microphone")
	float GetCaptureLevel() const;
	/** The key gate is open: push-to-talk is held, or push-to-toggle is switched on. */
	UFUNCTION(BlueprintPure, Category = "Voice|Microphone")
	bool IsPushToTalkHeld() const { return bPushToTalkHeld; }
	UFUNCTION(BlueprintCallable, Category = "Voice|Microphone")
	void SetReplayOnRelease(bool bEnabled);
	/** True while a push-to-talk take is being played back locally. */
	UFUNCTION(BlueprintPure, Category = "Voice|Microphone")
	bool IsReplaying() const;
	UFUNCTION(BlueprintCallable, Category = "Voice|Microphone")
	void SetLocalCaptureTest(bool bEnabled);
	/** The local capture test's stream is open. False while the test is off, or when the microphone could not be opened. */
	UFUNCTION(BlueprintPure, Category = "Voice|Microphone")
	bool IsLocalCaptureRunning() const;
	/** Key down. Push-to-talk opens the microphone; push-to-toggle opens it, or closes it when open. */
	UFUNCTION(BlueprintCallable, Category = "Voice|Input")
	void PushToTalkPressed();
	/** Key up. Push-to-talk closes the microphone; push-to-toggle ignores it. */
	UFUNCTION(BlueprintCallable, Category = "Voice|Input")
	void PushToTalkReleased();
	/**
	 * Close the key gate in either key mode, for input that is lost rather than released (focus,
	 * a menu, possession). A toggle stays closed until the next press.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice|Input")
	void CancelPushToTalk();
	UFUNCTION(BlueprintCallable, Category = "Voice|Input")
	void SetInputSuppressed(bool bSuppressed);
	/** Installs the voice context on the local player if needed. It is never removed by this component. */
	void EnsureInputMappingRegistered();

	/**
	 * Attenuation asset — controls all spatial behavior.
	 * No falloff = 2D (everyone in the room hears at full volume).
	 * With falloff = proximity (volume decreases with distance from this component). A pawn
	 * listener with falloff hears only voices it can place: a room member whose voice point does
	 * not exist on this machine is silent.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
	TObjectPtr<USoundAttenuation> AttenuationSettings;

	/** How often to recalculate proximity volumes (seconds). Only used when attenuation has falloff. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float UpdateInterval = 0.2f;

	/**
	 * Activate the bindings on BeginPlay, and follow lobby channel changes. Turn off to call
	 * ActivateVoice yourself (e.g., interact to use a microphone).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice", meta = (DisplayName = "Auto Activate"))
	bool bVoiceAutoActivate = true;

	/**
	 * Whether the local microphone may go into this room. Gameplay permissions
	 * change it through SetTransmitAllowed; input mode is evaluated separately. Remote players' pawns never transmit on this machine.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	bool bTransmitAllowed = true;

	// ── Actions ──────────────────────────────────────────────────────────────

	/**
	 * Activate this voice point — register with the subsystem's room refcounting and, once the
	 * room's channel is confirmed live, start proximity updates and join transmit composition.
	 * With Current Lobby and no lobby room yet, the component registers when the room comes up.
	 * Called automatically on BeginPlay if Auto Join is on.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void ActivateVoice();

	/**
	 * Deactivate this voice point — unregister from the room and stop proximity updates. It stays
	 * off until ActivateVoice is called again, even with Auto Join. The last component out of a
	 * room only removes it from transmit composition: channel membership belongs to the lobby.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void DeactivateVoice();

	/**
	 * Change the room at runtime. Unregisters from the old room and registers on the new one.
	 * With Current Lobby the next lobby room replaces it; switch to Room Name to keep a fixed room.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice", meta = (DeprecatedFunction, DeprecationMessage = "Use JoinRoom/LeaveRoom, AddRoomBinding/RemoveRoomBinding and SetFollowLobbyRoom."))
	void SetRoom(const FString& NewRoomName);

	/** Change where the room name comes from. Switching to Current Lobby moves to the lobby room now. */
	UFUNCTION(BlueprintCallable, Category = "Voice", meta = (DeprecatedFunction, DeprecationMessage = "Use JoinRoom/LeaveRoom, AddRoomBinding/RemoveRoomBinding and SetFollowLobbyRoom."))
	void SetRoomSource(EEOSVoiceRoomSource NewSource);

	/** Change the role at runtime. Reactivates with the new role if already active. */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetRole(EEOSVoiceRole NewRole);

	/** Change the attenuation settings at runtime. Restarts the proximity timer if needed. */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetAttenuationSettings(USoundAttenuation* NewSettings);

	/** Allow or block the local microphone for this room. See bTransmitAllowed. */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetTransmitAllowed(bool bAllowed);

	void RefreshTransmitRegistration();

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Is this voice point active — registered AND its room channel confirmed live? */
	UFUNCTION(BlueprintPure, Category = "Voice")
	bool IsVoiceActive() const { return bIsActive; }

	/** Can this component send voice? (Source or Transceiver) */
	UFUNCTION(BlueprintPure, Category = "Voice")
	bool CanSend() const { return Role == EEOSVoiceRole::Source || Role == EEOSVoiceRole::Transceiver; }

	/** Can this component receive voice? (Listener or Transceiver) */
	UFUNCTION(BlueprintPure, Category = "Voice")
	bool CanReceive() const { return Role == EEOSVoiceRole::Listener || Role == EEOSVoiceRole::Transceiver; }

	/** Does the attenuation use distance-based falloff? If false, it's 2D. */
	UFUNCTION(BlueprintPure, Category = "Voice")
	bool HasDistanceFalloff() const;

	/** The player whose voice this component carries, found through the owner chain. */
	UFUNCTION(BlueprintPure, Category = "Voice")
	APlayerState* GetOwningPlayerState() const;

	/**
	 * Product User Id of the player whose voice this component carries. Empty until that player's
	 * PlayerState and EOS id have replicated, and for actors no player owns.
	 */
	UFUNCTION(BlueprintPure, Category = "Voice")
	FString GetOwnerUserId() const;

	/**
	 * Get all other voice chat components in the world that share the same room.
	 * Useful for finding who else is in the room, or for UI indicators.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	TArray<UEEOSVoiceChatComponent*> GetComponentsInSameRoom() const;

	/** Calculate the attenuation volume for a given distance from this component. */
	UFUNCTION(BlueprintPure, Category = "Voice")
	float GetVolumeAtDistance(float Distance) const;

protected:

	virtual void PostLoad() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:

	FTimerHandle ProximityTimerHandle;

	/** ActivateVoice was requested and DeactivateVoice has not been called since. */
	bool bWantsVoice = false;

	/** Registered with the subsystem (refcounted room interest). */
	bool bRegistered = false;

	/** Room channel confirmed live — only then is the component "active". */
	bool bIsActive = false;

	/** Exactly what was registered, so unregister mirrors it even if properties changed. */
	UPROPERTY()
	bool bLegacyRoomSettingsMigrated = false;
	TMap<FString, bool> RegisteredRooms;
	FString FollowedLobbyRoom;
	TSet<FString> PendingRoomJoins;
	TSet<FString> PendingRoomLeaves;
	TSet<FString> AnnouncedRooms;
	bool bPreviousToggleMuteActionHeld = false;
	/** Last tick's push-to-talk action value. Push-to-toggle acts on presses, not on the key being down. */
	bool bPreviousPushToTalkActionHeld = false;
	FDelegateHandle AppActiveHandle;
	bool bAppFocused = true;
	bool bInputSuppressed = false;
	bool bPushToTalkHeld = false;
	bool bMicrophoneMuted = false;
	bool bMicrophoneTest = false;
	bool bCaptureRegistered = false;
	TSharedPtr<FEEOSVoiceCaptureState, ESPMode::ThreadSafe> CaptureState;
	/** The local capture test's stream, owned here so it lives exactly as long as the test runs. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioCapture> LocalCapture;
	int32 LocalCaptureHandleId = INDEX_NONE;
	/** A microphone that failed to open is not retried until the test is switched again, so it logs once. */
	bool bLocalCaptureFailed = false;
	/** Start or stop the local stream to match bLocalCaptureTest and whether this is the local player's voice point. */
	void UpdateLocalCapture();
	void StopLocalCapture();
	/** The replay's own 2D sound. It destroys itself once stopped. */
	TWeakObjectPtr<UAudioComponent> ReplayAudio;
	FTimerHandle ReplayStopHandle;
	/** Plays the take push-to-talk just finished; drops it when replay is off or voice is going away. */
	void ReplayLastTake();
	void StopReplay();
	APlayerController* GetLocalVoiceController() const;
	void RefreshCaptureState();
	void RefreshRoomActivity();
	bool SharesActiveRoom(const UEEOSVoiceChatComponent* Other) const;
	UFUNCTION()
	void ToggleMicrophoneMute();
	UFUNCTION()
	void HandleRoomJoinFailed(const FString& InRoomName, const FString& Error);
	UFUNCTION()
	void HandleRoomLeaveFailed(const FString& InRoomName, const FString& Error);

	bool WantsTransmit() const;

	/** Player ids this component currently contributes proximity volumes for. */
	TSet<FString> ContributedPlayers;

	void Register();
	void Unregister();
	void MoveToRoom(const FString& NewRoomName);
	void FollowLobbyRoom();
	void BindRoomEvents();
	void UnbindRoomEvents();

	/** Subsystem room event handlers (follow the lobby room, confirm/unconfirm this component's room). */
	UFUNCTION()
	void HandleVoiceRoomJoined(const FString& InRoomName);

	UFUNCTION()
	void HandleVoiceRoomLeft(const FString& InRoomName);

	/** Possession decides whether this pawn's component may transmit. */
	UFUNCTION()
	void HandleOwnerControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	void ConfirmActive();
	/** Push MicrophoneVolume to the voice user, if this is the live local source. */
	void ApplyMicrophoneVolume();
	void StartProximityTimer();
	void StopProximityTimer();
	void UpdateProximityVolumes();
	float CalculateVolumeAtDistance(float Distance) const;

	UEEOSVoiceSubsystem* GetVoiceSubsystem() const;
};
