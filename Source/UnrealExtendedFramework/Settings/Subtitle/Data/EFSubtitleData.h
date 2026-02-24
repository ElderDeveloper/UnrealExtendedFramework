// EFSubtitleData.h — Core data types for the EF Modular Subtitle System
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Sound/SoundBase.h"
#include "Engine/DataTable.h"
#include "EFSubtitleData.generated.h"


// ── Culture Sound Entry (RPC-safe alternative to TMap) ──

USTRUCT(BlueprintType)
struct FEFCultureSound
{
	GENERATED_BODY()

	// Culture code, e.g. "en", "fr", "tr", or full form "en-US"
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString CultureCode;

	// Sound asset for this culture
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<USoundBase> Sound;
};


// ── Enums ──

UENUM(BlueprintType)
enum class EEFSubtitleExecutionType : uint8
{
	// No spatial audio — played as 2D for all (or filtered) clients
	Boundless,

	// Audio is spatialized at a world location, subtitle shown to nearby players
	Location,

	// Audio attached to an actor, subtitle follows the actor's position
	AttachedToActor,

	// Only shown to a specific player (e.g., inner monologue, tutorial prompts)
	PlayerOnly
};


UENUM(BlueprintType)
enum class EEFSubtitleQueueMode : uint8
{
	// New subtitle replaces current one immediately
	Replace,

	// New subtitle is queued; plays after current finishes
	Queue,

	// New subtitle is queued; higher priority interrupts lower
	PriorityQueue,

	// Multiple subtitles can display simultaneously (stacked)
	Stack
};


// ── Visual Settings Structs ──

USTRUCT(BlueprintType)
struct FEFSubtitleBorderSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BorderSize = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor BorderColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BorderOpacity = 1.0f;
};


USTRUCT(BlueprintType)
struct FEFSubtitleBackgroundSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.7f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BackgroundOpacity = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D BackgroundPadding = FVector2D(10.0f, 5.0f);
};


USTRUCT(BlueprintType)
struct FEFSubtitleDurationSettings
{
	GENERATED_BODY()

	// If true, the duration will always be forced to the subtitle's Duration field
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bForceSubtitleDuration = false;

	// If true, subtitle letters will be revealed one-by-one (typewriter)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bAnimateSubtitleLetters = false;

	// Duration added per letter when auto-calculating duration
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TimePerLetter = 0.05f;

	// Additional time after the last letter appears
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TimeAfterComplete = 1.5f;
};


// ── Core Data ──

/**
 * A single subtitle entry. Can be used as a DataTable row or stored in a DataAsset.
 * Backward compatible with the old FExtendedSubtitle — adds speaker info, priority, delay.
 */
USTRUCT(BlueprintType)
struct FEFSubtitleEntry : public FTableRowBase
{
	GENERATED_BODY()

	// ── Content ──

	// The subtitle text. Supports RichTextBlock markup.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
	FText Text;

	// Optional speaker name displayed above the subtitle
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
	FText SpeakerName;

	// Speaker label color
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
	FLinearColor SpeakerColor = FLinearColor::White;

	// GameplayTag for speaker identification (icon lookup, filtering)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
	FGameplayTag SpeakerTag;

	// ── Timing ──

	// Duration in seconds. 0 = auto-calculate from text length.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
	float Duration = 0.0f;

	// Delay in seconds before showing this subtitle
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
	float Delay = 0.0f;

	// ── Audio ──

	// Default voiceover sound
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> VoiceSound;

	// Per-culture overrides: culture code → sound asset
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TArray<FEFCultureSound> CultureSounds;

	// ── Priority & Behavior ──

	// Higher priority subtitles can interrupt lower ones
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	int32 Priority = 0;

	// If true, this subtitle cannot be interrupted by equal-priority subs
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	bool bUninterruptible = false;

	FEFSubtitleEntry()
	{
		Text = FText::GetEmpty();
		SpeakerName = FText::GetEmpty();
	}
};


/**
 * A request to display a subtitle. Sent from the authority to clients.
 */
USTRUCT(BlueprintType)
struct FEFSubtitleRequest
{
	GENERATED_BODY()

	// Key to look up in the data source
	UPROPERTY(BlueprintReadWrite, Category = "Request")
	FName SubtitleKey;

	// How to execute the subtitle
	UPROPERTY(BlueprintReadWrite, Category = "Request")
	EEFSubtitleExecutionType ExecutionType = EEFSubtitleExecutionType::Boundless;

	// World location (for Location / AttachedToActor types)
	UPROPERTY(BlueprintReadWrite, Category = "Request")
	FVector WorldLocation = FVector::ZeroVector;

	// Actor to attach audio to (for AttachedToActor type)
	UPROPERTY(BlueprintReadWrite, Category = "Request")
	TWeakObjectPtr<AActor> AttachActor;

	// Max audible distance — clients beyond this won't receive the subtitle. 0 = infinite.
	UPROPERTY(BlueprintReadWrite, Category = "Request")
	float MaxDistance = 0.0f;

	// Unique ID for tracking / cancellation (assigned by subsystem)
	UPROPERTY()
	int32 RequestId = 0;
};


/**
 * An active subtitle being displayed or queued.
 */
USTRUCT(BlueprintType)
struct FEFActiveSubtitle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FEFSubtitleEntry Entry;

	UPROPERTY(BlueprintReadOnly)
	FEFSubtitleRequest Request;

	UPROPERTY(BlueprintReadOnly)
	float RemainingTime = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float ElapsedTime = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	int32 RequestId = 0;

	bool IsValid() const { return RequestId != 0; }
};


/**
 * A sequence of subtitles to play in order.
 */
USTRUCT(BlueprintType)
struct FEFSubtitleSequence
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FName> SubtitleKeys;

	// If true, wait for audio to finish before next subtitle. If false, use Duration.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bWaitForAudio = true;

	// Execution type for all subtitles in the sequence
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EEFSubtitleExecutionType ExecutionType = EEFSubtitleExecutionType::Boundless;

	// World location for Location type
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector WorldLocation = FVector::ZeroVector;

	// Max distance for spatial subtitles
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MaxDistance = 0.0f;
};
