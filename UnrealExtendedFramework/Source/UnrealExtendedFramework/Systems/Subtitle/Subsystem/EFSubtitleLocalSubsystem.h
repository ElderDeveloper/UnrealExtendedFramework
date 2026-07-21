// EFSubtitleLocalSubsystem.h — Per-player subtitle subsystem (orchestrator)
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Data/EFSubtitleData.h"
#include "EFSubtitleLocalSubsystem.generated.h"

class UEFSubtitleDisplayWidget;
class UEFSubtitleQueuePolicy;
class UEFSubtitleAudioPlayer;
class UEFSubtitleStyleProfile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLocalSubtitleStarted, const FEFSubtitleEntry&, Entry, int32, RequestId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLocalSubtitleFinished, int32, RequestId);

/**
 * Per local-player subtitle subsystem.
 * Thin orchestrator over queue policy, display widget, and audio player.
 * Presentation prefs are pushed from ModularSettings via ApplyPresentationState.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleLocalSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	void ReceiveSubtitle(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	void CancelSubtitle(int32 RequestId);

	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	void ClearAllSubtitles();

	/** Replace cached presentation state and refresh the widget. */
	UFUNCTION(BlueprintCallable, Category = "Subtitle|Settings")
	void ApplyPresentationState(const FEFSubtitlePresentationState& NewState);

	UFUNCTION(BlueprintCallable, Category = "Subtitle|Settings")
	void SetSubtitlesEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Subtitle|Settings")
	void SetSubtitleTextScale(float Scale);

	UFUNCTION(BlueprintCallable, Category = "Subtitle|Settings")
	void SetSubtitleBackgroundOpacity(float Opacity);

	UFUNCTION(BlueprintCallable, Category = "Subtitle|Settings")
	void SetShowSpeakerLabels(bool bShow);

	UFUNCTION(BlueprintCallable, Category = "Subtitle|Settings")
	void SetClosedCaptionsEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Subtitle|Settings")
	void SetStyleProfileId(FName ProfileId);

	UFUNCTION(BlueprintPure, Category = "Subtitle|Settings")
	FEFSubtitlePresentationState GetPresentationState() const { return PresentationState; }

	UFUNCTION(BlueprintPure, Category = "Subtitle")
	bool AreSubtitlesEnabled() const { return PresentationState.bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Subtitle")
	float GetSubtitleTextScale() const { return PresentationState.TextScale; }

	UFUNCTION(BlueprintPure, Category = "Subtitle")
	bool HasActiveSubtitle() const;

	UPROPERTY(BlueprintAssignable)
	FOnLocalSubtitleStarted OnSubtitleStarted;

	UPROPERTY(BlueprintAssignable)
	FOnLocalSubtitleFinished OnSubtitleFinished;

private:
	void SeedPresentationStateFromProjectSettings();
	void EnsurePoliciesReady();
	void EnsureWidgetReady();
	void RefreshWidgetPresentation();
	UEFSubtitleStyleProfile* ResolveActiveStyleProfile() const;

	void OnActiveSubtitleChanged(const FEFActiveSubtitle& Active);
	void OnSubtitleExpired(int32 RequestId);
	void TickQueue();

	UPROPERTY()
	TObjectPtr<UEFSubtitleQueuePolicy> QueuePolicy;

	UPROPERTY()
	TObjectPtr<UEFSubtitleAudioPlayer> AudioPlayer;

	UPROPERTY()
	TObjectPtr<UEFSubtitleDisplayWidget> DisplayWidget;

	FEFSubtitlePresentationState PresentationState;

	FTimerHandle QueueTickHandle;
};
