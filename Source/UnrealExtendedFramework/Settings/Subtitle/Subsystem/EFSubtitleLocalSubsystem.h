// EFSubtitleLocalSubsystem.h — Per-player subtitle subsystem (queue, widget, audio playback)
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Data/EFSubtitleData.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Subsystem/EFSubtitleQueue.h"
#include "EFSubtitleLocalSubsystem.generated.h"

class UEFSubtitleDisplayWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLocalSubtitleStarted, const FEFSubtitleEntry&, Entry, int32, RequestId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLocalSubtitleFinished, int32, RequestId);

/**
 * Per local-player subtitle subsystem.
 * Manages the subtitle queue, display widget lifecycle, and audio playback.
 * Each local player has their own instance.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleLocalSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	// ── Called by ReceiverComponent or directly in standalone ──

	void ReceiveSubtitle(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	void CancelSubtitle(int32 RequestId);

	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	void ClearAllSubtitles();

	// ── User Settings ──

	UFUNCTION(BlueprintPure, Category = "Subtitle")
	bool AreSubtitlesEnabled() const;

	UFUNCTION(BlueprintPure, Category = "Subtitle")
	float GetSubtitleTextScale() const;

	// ── Queue Access ──

	UFUNCTION(BlueprintPure, Category = "Subtitle")
	bool HasActiveSubtitle() const;

	// ── Delegates ──

	UPROPERTY(BlueprintAssignable)
	FOnLocalSubtitleStarted OnSubtitleStarted;

	UPROPERTY(BlueprintAssignable)
	FOnLocalSubtitleFinished OnSubtitleFinished;

private:
	// The subtitle queue
	FEFSubtitleQueue SubtitleQueue;

	// The display widget (created once, reused)
	UPROPERTY()
	UEFSubtitleDisplayWidget* DisplayWidget;

	// Timer handle for ticking the queue
	FTimerHandle QueueTickHandle;

	// Ensure the widget is created and in the viewport
	void EnsureWidgetReady();

	// Queue callbacks
	void OnActiveSubtitleChanged(const FEFActiveSubtitle& Active);
	void OnSubtitleExpired(int32 RequestId);

	// Tick function for the queue
	void TickQueue();

	// Play audio for a subtitle entry
	void PlaySubtitleAudio(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request);

	// Resolve culture-specific sound
	USoundBase* ResolveCultureSound(const FEFSubtitleEntry& Entry) const;
};
