// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "EGInteractionPromptWidget.generated.h"

class UImage;
class UTextBlock;

/**
 * World interaction prompt: shows the focused interactable's text and icon, and an optional
 * hold-progress bar driven by a material scalar named "Progress".
 *
 * Every bound widget is optional, so a game's existing prompt layout keeps working whatever it
 * named its parts — override ShowPrompt/SetHoldProgress to drive a different set.
 */
UCLASS(Abstract, Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API UEGInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Show the prompt for a focused interactable. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Interaction|UI")
	void ShowPrompt(const FText& InPrompt, UTexture2D* InIcon);

	/** Hide the prompt and reset hold progress. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Interaction|UI")
	void HidePrompt();

	/** Drive the hold indicator. 0 means no hold is running. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Interaction|UI")
	void SetHoldProgress(float InProgress);

	/** Follow the mouse cursor instead of sitting where the layout put it. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction|UI")
	void SetCursorFollowMode(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction|UI")
	bool IsCursorFollowMode() const { return bCursorFollowEnabled; }

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Extended|Interaction|UI")
	TObjectPtr<UTextBlock> InteractionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Extended|Interaction|UI")
	TObjectPtr<UImage> InteractionIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Extended|Interaction|UI")
	TObjectPtr<UImage> HoldProgressImage;

	/** Offset from the cursor, so the prompt does not sit under the pointer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|UI")
	FVector2D CursorFollowOffset = FVector2D(20.0f, -30.0f);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Extended|Interaction|UI")
	FText CurrentPrompt;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Extended|Interaction|UI")
	float HoldProgress = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Extended|Interaction|UI")
	bool bHoldInteractionActive = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Extended|Interaction|UI")
	bool bCursorFollowEnabled = false;
};
