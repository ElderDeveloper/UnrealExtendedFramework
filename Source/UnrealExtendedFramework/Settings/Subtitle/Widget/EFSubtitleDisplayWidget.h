// EFSubtitleDisplayWidget.h — Subtitle display widget (swappable)
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Data/EFSubtitleData.h"
#include "EFSubtitleDisplayWidget.generated.h"

class URichTextBlock;
class UTextBlock;
class UImage;
class UBorder;
class UVerticalBox;

/**
 * Base subtitle display widget. Swappable — set the subclass in UEFSubtitleProjectSettings.
 * Contains BindWidgetOptional slots for speaker label, subtitle text, background, and stacking.
 * Override in Blueprint or C++ for custom appearance.
 */
UCLASS(Blueprintable)
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleDisplayWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	// ── BindWidgets (optional — wire in your UMG Blueprint) ──

	// Main subtitle text (use RichTextBlock for markup support)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* SubtitleText;

	// Speaker name label
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* SpeakerLabel;

	// Background border behind the subtitle
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* BackgroundBorder;

	// Container for stacked subtitles (Stack mode)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UVerticalBox* StackContainer;

	// ── API ──

	// Show a subtitle
	UFUNCTION(BlueprintNativeEvent, Category = "Subtitle")
	void ShowSubtitle(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request);
	virtual void ShowSubtitle_Implementation(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request);

	// Hide the active subtitle
	UFUNCTION(BlueprintNativeEvent, Category = "Subtitle")
	void HideSubtitle();
	virtual void HideSubtitle_Implementation();

	// Update the subtitle progress (called each tick while active)
	UFUNCTION(BlueprintNativeEvent, Category = "Subtitle")
	void UpdateSubtitle(float DeltaTime, float Progress);
	virtual void UpdateSubtitle_Implementation(float DeltaTime, float Progress);

	// Apply visual settings from project settings
	UFUNCTION(BlueprintCallable, Category = "Subtitle")
	void ApplyVisualSettings();

	// ── Stack Mode API ──

	// Add a subtitle line in Stack mode
	UFUNCTION(BlueprintNativeEvent, Category = "Subtitle")
	void AddStackedSubtitle(const FEFSubtitleEntry& Entry, int32 RequestId);
	virtual void AddStackedSubtitle_Implementation(const FEFSubtitleEntry& Entry, int32 RequestId);

	// Remove a stacked subtitle by ID
	UFUNCTION(BlueprintNativeEvent, Category = "Subtitle")
	void RemoveStackedSubtitle(int32 RequestId);
	virtual void RemoveStackedSubtitle_Implementation(int32 RequestId);

protected:
	// Timer for auto-hiding
	FTimerHandle HideTimerHandle;

	// Track stacked subtitle widget entries
	UPROPERTY()
	TMap<int32, UTextBlock*> StackedEntries;
};
