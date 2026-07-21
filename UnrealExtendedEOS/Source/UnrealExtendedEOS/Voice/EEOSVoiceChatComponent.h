// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Sound/SoundAttenuation.h"
#include "EEOSVoiceChatComponent.generated.h"

class UEEOSVoiceSubsystem;

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
 * A modular voice point in the world.
 *
 * This component is NOT player-specific. It can be placed on any actor:
 * players, microphones, speakers, radios, intercoms, phones — anything.
 *
 * Each component represents ONE voice room with ONE role and ONE attenuation.
 * Add multiple components to an actor for multi-room support:
 *
 *   BP_Player
 *   ├── VoiceChat (Transceiver, lobby room, proximity attenuation)
 *   └── VoiceChat (Transceiver, "Team_Red", 2D attenuation)
 *
 *   BP_ShopMicrophone
 *   └── VoiceChat (Source, "ShopPA", small attenuation — stand close to talk)
 *
 *   BP_ShopSpeaker
 *   └── VoiceChat (Listener, "ShopPA", large attenuation — audio comes from here)
 *
 * ROOMS ARE LOBBY-MANAGED: voice rooms are EOS lobby RTC rooms — the engine joins/leaves them
 * with lobby membership (CreateLobby/JoinLobby with bUseVoiceChat). The component cannot create
 * a room; it registers interest in one. The room name is engine-generated — read it with
 * UEEOSVoiceSubsystem::GetLobbyVoiceRoomName() after the lobby is joined and apply it via
 * SetRoom(). Activation therefore confirms in two ways:
 *   - synchronously, if the room's channel is already live, or
 *   - later, from the subsystem's OnVoiceRoomJoined event when the lobby RTC room comes up.
 * IsVoiceActive() is true only after one of those confirmations.
 *
 * TRANSMISSION is composed at the subsystem level: the transmit set is the union of the rooms
 * of all active Source/Transceiver components. Activating one component never clobbers another
 * component's transmission, and Listener components never transmit into their room (there is
 * no per-channel input mute in IVoiceChatUser — exclusion from the explicit transmit set is
 * the mechanism, and the subsystem always uses TransmitToSpecificChannels while any component
 * is registered because the engine default is transmit-to-all).
 *
 * PROXIMITY VOLUME LIMITATION: IVoiceChatUser::SetPlayerVolume is GLOBAL per player — the
 * VoiceChat API has no per-channel receive volume (only a per-channel receive MUTE). When the
 * same player is heard through multiple components/rooms, the subsystem applies the MAXIMUM of
 * all reported volumes, so overlapping rooms cannot silence each other — but a player quiet in
 * one room and loud in another is heard at the louder value in both.
 *
 * RECEIVE LOCALITY RULE: volume contributions drive the LOCAL machine's per-player receive
 * volumes, so the receive half of a component only runs when its math is strictly the local
 * player's perspective:
 *   - Pawn owners: the receive half runs ONLY while the pawn is locally controlled (the
 *     component location is then the local player's ears; distance = source ↔ this component).
 *     Replicated proxies of REMOTE players' pawns never contribute — they would compute what
 *     the remote player hears at the remote position, and the max-wins aggregation would make
 *     two distant players standing near each other audible at full volume.
 *   - Non-pawn owners (world-placed speakers/radios): the receive half runs on every machine,
 *     but the volume is the attenuation from THIS fixture to the LOCAL listener (player 0's
 *     pawn, else camera) — identical for all sources in the room. Walk away from the speaker
 *     and it fades; the source's distance to the fixture is intentionally not a factor.
 *
 * USoundAttenuation controls all spatial behavior:
 *   - bAttenuate OFF or FalloffDistance = 0 → 2D, full volume for all in the room
 *   - bAttenuate ON + FalloffDistance > 0  → proximity, volume decreases with distance
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

	/**
	 * The voice room this component belongs to. Must be a lobby RTC room name — for the current
	 * lobby's room, read UEEOSVoiceSubsystem::GetLobbyVoiceRoomName() and apply via SetRoom().
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
	FString RoomName = TEXT("Global");

	/**
	 * Attenuation asset — controls all spatial behavior.
	 * No falloff = 2D (everyone in the room hears at full volume).
	 * With falloff = proximity (volume decreases with distance from this component).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
	TObjectPtr<USoundAttenuation> AttenuationSettings;

	/**
	 * The EOS ProductUserId associated with this voice point.
	 * Required for Source/Transceiver roles to identify whose voice to route.
	 * For Listener-only roles (speakers), this can be left empty.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
	FString OwnerUserId;

	/** How often to recalculate proximity volumes (seconds). Only used when attenuation has falloff. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float UpdateInterval = 0.2f;

	/** Whether to automatically activate on BeginPlay. Disable for manual control (e.g., interact to use a microphone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
	bool bVoiceAutoActivate = true;

	// ── Actions ──────────────────────────────────────────────────────────────

	/**
	 * Activate this voice point — register with the subsystem's room refcounting and, once the
	 * room's channel is confirmed live, start proximity updates and join transmit composition.
	 * Called automatically on BeginPlay if bVoiceAutoActivate is true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void ActivateVoice();

	/**
	 * Deactivate this voice point — unregister from the room and stop proximity updates.
	 * The last component out of a room only removes it from transmit composition: actual
	 * channel membership belongs to the lobby. Called automatically on EndPlay.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void DeactivateVoice();

	/** Change the room at runtime. Unregisters from the old room and registers on the new one. */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetRoom(const FString& NewRoomName);

	/** Change the role at runtime. Reactivates with the new role if already active. */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetRole(EEOSVoiceRole NewRole);

	/** Change the attenuation settings at runtime. Restarts the proximity timer if needed. */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetAttenuationSettings(USoundAttenuation* NewSettings);

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

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:

	FTimerHandle ProximityTimerHandle;

	/** Registered with the subsystem (refcounted room interest). */
	bool bRegistered = false;

	/** Room channel confirmed live — only then is the component "active". */
	bool bIsActive = false;

	/** Exactly what was registered, so unregister mirrors it even if properties changed. */
	FString RegisteredRoomName;
	bool bRegisteredTransmit = false;

	/** Player ids this component currently contributes proximity volumes for. */
	TSet<FString> ContributedPlayers;

	/** Subsystem room event handlers (confirm/unconfirm this component's room). */
	UFUNCTION()
	void HandleVoiceRoomJoined(const FString& InRoomName);

	UFUNCTION()
	void HandleVoiceRoomLeft(const FString& InRoomName);

	void ConfirmActive();
	void StartProximityTimer();
	void StopProximityTimer();
	void UpdateProximityVolumes();
	float CalculateVolumeAtDistance(float Distance) const;

	UEEOSVoiceSubsystem* GetVoiceSubsystem() const;
};
