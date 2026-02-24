// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "EFWidgetLibrary.generated.h"

class USizeBox;
class UProgressBar;
class UOverlaySlot;
class UOverlay;
class UVerticalBoxSlot;
class UVerticalBox;

/**
 * Blueprint function library for extended widget operations including
 * viewport queries, dynamic child management, texture sizing, and SizeBox control.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFWidgetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	/** Returns the current game viewport size in pixels. */
	UFUNCTION(BlueprintPure, Category="Extended Widget")
	static FVector2D GetGameViewportSize();

	/** Returns the current system resolution (ResX, ResY). */
	UFUNCTION(BlueprintPure, Category="Extended Widget")
	static FVector2D GetGameResolution();

	/**
	 * Adds a UUserWidget as a child to an Overlay with specified alignment.
	 * @return The created overlay slot, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category="Extended Widget|Overlay")
	static UOverlaySlot* AddChildToOverlay(UOverlay* Overlay, UUserWidget* Child, TEnumAsByte<EVerticalAlignment> InVerticalAlignment, TEnumAsByte<EHorizontalAlignment> InHorizontalAlignment);

	/**
	 * Adds a UUserWidget as a child to a VerticalBox with specified alignment.
	 * @return The created vertical box slot, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category="Extended Widget|VerticalBox")
	static UVerticalBoxSlot* AddChildToVerticalBox(UVerticalBox* VerticalBox, UUserWidget* Child, TEnumAsByte<EVerticalAlignment> InVerticalAlignment, TEnumAsByte<EHorizontalAlignment> InHorizontalAlignment);

	/** Returns the dimensions of a Texture2D as a Vector2D. */
	UFUNCTION(BlueprintPure, Category="Extended Widget|Texture")
	static FVector2D GetTexture2DSize(UTexture2D* Texture);

	/** Returns the dimensions of a Texture2D, each axis clamped to the given max. */
	UFUNCTION(BlueprintPure, Category="Extended Widget|Texture")
	static FVector2D GetTexture2DSizeClamped(UTexture2D* Texture, FVector2D Clamp);
	
	/** Sets the width and height overrides on a SizeBox. */
	UFUNCTION(BlueprintCallable, Category="Extended Widget|SizeBox")
	static void SetSizeBoxSize(USizeBox* SizeBox, FVector2D Size);
	
	/** Returns the width and height overrides from a SizeBox. */
	UFUNCTION(BlueprintPure, Category="Extended Widget|SizeBox")
	static FVector2D GetSizeBoxSize(USizeBox* SizeBox);
	
	/** Sets the fill image of a ProgressBar from a Texture2D. */
	UFUNCTION(BlueprintCallable, Category="Extended Widget|ProgressBar")
	static void SetProgressBarFillImage(UProgressBar* ProgressBar, UTexture2D* Texture);

	/**
	 * Returns the widget currently under the mouse cursor, or nullptr.
	 * Requires Slate to be initialized.
	 */
	UFUNCTION(BlueprintPure, Category="Extended Widget")
	static UWidget* GetWidgetUnderCursor();
};
