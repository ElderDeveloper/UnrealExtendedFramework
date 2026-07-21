// EFSubtitleDisplayWidget.h — Subtitle display widget (swappable)
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Data/EFSubtitleData.h"
#include "EFSubtitleDisplayWidget.generated.h"

class UTextBlock;
class UBorder;
class UVerticalBox;
class UEFSubtitleStyleProfile;

/**
 * Base subtitle display widget. Swappable via UEFSubtitleProjectSettings.
 * Consumes FEFSubtitlePresentationState + optional style profile.
 */
UCLASS(Blueprintable)
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleDisplayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* SubtitleText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* SpeakerLabel;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* BackgroundBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UVerticalBox* StackContainer;

	UFUNCTION(BlueprintNativeEvent, Category = "Subtitle")
	void ShowSubtitle(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request);
	virtual void ShowSubtitle_Implementation(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request);

	UFUNCTION(BlueprintNativeEvent, Category = "Subtitle")
	void HideSubtitle();
	virtual void HideSubtitle_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Subtitle")
	void UpdateSubtitle(float DeltaTime, float Progress);
	virtual void UpdateSubtitle_Implementation(float DeltaTime, float Progress);

	/** Apply project defaults + presentation state (+ optional style profile). */
	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	void ApplyPresentationState(const FEFSubtitlePresentationState& State, UEFSubtitleStyleProfile* StyleProfile);

	/** Legacy entry point — applies project defaults with default presentation state. */
	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	void ApplyVisualSettings();

	UFUNCTION(BlueprintNativeEvent, Category = "Subtitle")
	void AddStackedSubtitle(const FEFSubtitleEntry& Entry, int32 RequestId);
	virtual void AddStackedSubtitle_Implementation(const FEFSubtitleEntry& Entry, int32 RequestId);

	UFUNCTION(BlueprintNativeEvent, Category = "Subtitle")
	void RemoveStackedSubtitle(int32 RequestId);
	virtual void RemoveStackedSubtitle_Implementation(int32 RequestId);

protected:
	UPROPERTY(Transient)
	FEFSubtitlePresentationState CachedPresentationState;

	UPROPERTY(Transient)
	TObjectPtr<UEFSubtitleStyleProfile> CachedStyleProfile;

	bool bUseBackground = true;

	FTimerHandle HideTimerHandle;

	UPROPERTY()
	TMap<int32, UTextBlock*> StackedEntries;
};
