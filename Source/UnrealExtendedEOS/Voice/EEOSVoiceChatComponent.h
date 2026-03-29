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
 *   ├── VoiceChat (Transceiver, "Proximity", proximity attenuation)
 *   └── VoiceChat (Transceiver, "Team_Red", 2D attenuation)
 *
 *   BP_ShopMicrophone
 *   └── VoiceChat (Source, "ShopPA", small attenuation — stand close to talk)
 *
 *   BP_ShopSpeaker
 *   └── VoiceChat (Listener, "ShopPA", large attenuation — audio comes from here)
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

	/** The voice room this component belongs to. */
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
	 * Activate this voice point — join the room and start proximity updates.
	 * Called automatically on BeginPlay if bVoiceAutoActivate is true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void ActivateVoice();

	/**
	 * Deactivate this voice point — leave the room and stop proximity updates.
	 * Called automatically on EndPlay.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void DeactivateVoice();

	/** Change the room at runtime. Leaves the old room and joins the new one. */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetRoom(const FString& NewRoomName);

	/** Change the role at runtime. Reactivates with the new role if already active. */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetRole(EEOSVoiceRole NewRole);

	/** Change the attenuation settings at runtime. Restarts the proximity timer if needed. */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetAttenuationSettings(USoundAttenuation* NewSettings);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Is this voice point currently active (joined a room)? */
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
	bool bIsActive = false;

	void StartProximityTimer();
	void StopProximityTimer();
	void UpdateProximityVolumes();
	float CalculateVolumeAtDistance(float Distance) const;

	UEEOSVoiceSubsystem* GetVoiceSubsystem() const;
};
