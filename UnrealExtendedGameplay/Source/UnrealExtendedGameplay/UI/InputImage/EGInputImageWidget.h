// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Framework/Application/IInputProcessor.h"
#include "EGInputImageWidget.generated.h"

class UImage;

/**
 * Enum representing different input device types
 */
UENUM(BlueprintType)
enum class EEGInputDeviceType : uint8
{
	KeyboardMouse UMETA(DisplayName = "Keyboard & Mouse"),
	PlayStation UMETA(DisplayName = "PlayStation Gamepad"),
	Xbox UMETA(DisplayName = "Xbox Gamepad"),
	GenericGamepad UMETA(DisplayName = "Generic Gamepad"),
	Unknown UMETA(DisplayName = "Unknown")
};


/**
 * Widget that automatically updates its displayed image based on the current input device
 * Each widget independently tracks input and switches between keyboard/mouse,
 * PlayStation, and Xbox controller images automatically
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGInputImageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The image widget that will display the input icon */
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "Image")
	UImage* InputImage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Detection")
	bool bAutoDetectInput = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Detection")
	bool bUpdateOnConstruct = true;
	
	UFUNCTION(BlueprintPure, Category = "Input Detection")
	EEGInputDeviceType GetCurrentInputDevice() const { return CurrentInputDevice; }
	
	UFUNCTION(BlueprintCallable, Category = "Input Detection")
	void StartListening();
	
	UFUNCTION(BlueprintCallable, Category = "Input Detection")
	void StopListening();

protected:

	/** Image to display for Keyboard & Mouse input */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Images")
	UTexture2D* KeyboardMouseImage = nullptr;

	/** Image to display for PlayStation gamepad input */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Images")
	UTexture2D* PlayStationImage = nullptr;

	/** Image to display for Xbox gamepad input */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Images")
	UTexture2D* XboxImage = nullptr;

	/** Fallback image to display for generic/unknown gamepad input */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Images")
	UTexture2D* GenericGamepadImage = nullptr;


	UTexture2D* GetImageForDevice(EEGInputDeviceType DeviceType) const;
	
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;


	UFUNCTION(BlueprintCallable, Category = "Input Images")
	void UpdateDisplayedImage(EEGInputDeviceType DeviceType);
	
	void OnInputDetected(const FKey& Key);
	EEGInputDeviceType DetermineInputDeviceFromKey(const FKey& Key) const;

private:
	EEGInputDeviceType CurrentInputDevice = EEGInputDeviceType::KeyboardMouse;
	bool bIsListening = false;
	
	class FInputDeviceDetector : public IInputProcessor
	{
	public:
		FInputDeviceDetector(UEGInputImageWidget* InWidget)
			: Widget(InWidget)
		{
		}

		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> TickCursor) override {}

		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
		{
			if (Widget.IsValid())
			{
				Widget->OnInputDetected(InKeyEvent.GetKey());
			}
			return false; // Don't consume the event
		}

		virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override
		{
			if (Widget.IsValid())
			{
				Widget->OnInputDetected(InAnalogInputEvent.GetKey());
			}
			return false; // Don't consume the event
		}

		virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
		{
			if (Widget.IsValid() && MouseEvent.GetCursorDelta().SizeSquared() > 0.01f)
			{
				// Mouse movement indicates keyboard/mouse usage
				Widget->OnInputDetected(EKeys::MouseX);
			}
			return false; // Don't consume the event
		}

	private:
		TWeakObjectPtr<UEGInputImageWidget> Widget;
	};

	TSharedPtr<FInputDeviceDetector> InputDetector;
};

