// Fill out your copyright notice in the Description page of Project Settings.

#include "EFInputImageWidget.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"

UTexture2D* UEFInputImageWidget::GetImageForDevice(EEFInputDeviceType DeviceType) const
{
	switch (DeviceType)
	{
	case EEFInputDeviceType::KeyboardMouse:
		return KeyboardMouseImage;
	case EEFInputDeviceType::PlayStation:
		return PlayStationImage ? PlayStationImage : GenericGamepadImage;
	case EEFInputDeviceType::Xbox:
		return XboxImage ? XboxImage : GenericGamepadImage;
	case EEFInputDeviceType::GenericGamepad:
		return GenericGamepadImage;
	default:
		return nullptr;
	}
}

void UEFInputImageWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Start listening if auto-detect is enabled
	if (bAutoDetectInput)
	{
		StartListening();
	}

	// Update the image immediately if requested
	if (bUpdateOnConstruct)
	{
		UpdateDisplayedImage(CurrentInputDevice);
	}
}

void UEFInputImageWidget::NativeDestruct()
{
	StopListening();
	Super::NativeDestruct();
}

void UEFInputImageWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Tick is enabled for future enhancements if needed
}

void UEFInputImageWidget::StartListening()
{
	if (bIsListening)
	{
		return;
	}

	// Register input preprocessor to detect all inputs
	if (FSlateApplication::IsInitialized())
	{
		InputDetector = MakeShared<FInputDeviceDetector>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(InputDetector);
		bIsListening = true;

		UE_LOG(LogTemp, Log, TEXT("UEFInputImageWidget: Started listening to input device changes"));
	}
}

void UEFInputImageWidget::StopListening()
{
	if (!bIsListening)
	{
		return;
	}

	// Unregister input preprocessor
	if (FSlateApplication::IsInitialized() && InputDetector.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputDetector);
	}

	InputDetector.Reset();
	bIsListening = false;

	UE_LOG(LogTemp, Log, TEXT("UEFInputImageWidget: Stopped listening to input device changes"));
}

void UEFInputImageWidget::UpdateDisplayedImage(EEFInputDeviceType DeviceType)
{
	if (!InputImage)
	{
		UE_LOG(LogTemp, Warning, TEXT("UEFInputImageWidget: InputImage is null! Make sure the widget is bound."));
		return;
	}
	
	if (UTexture2D* NewTexture = GetImageForDevice(DeviceType))
	{
		InputImage->SetBrushFromTexture(NewTexture);
		UE_LOG(LogTemp, Verbose, TEXT("UEFInputImageWidget: Updated image for device type %d"), static_cast<int32>(DeviceType));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UEFInputImageWidget: No texture found for device type %d"), static_cast<int32>(DeviceType));
	}
}

void UEFInputImageWidget::OnInputDetected(const FKey& Key)
{
	EEFInputDeviceType DetectedDevice = DetermineInputDeviceFromKey(Key);
	
	if (DetectedDevice != EEFInputDeviceType::Unknown && DetectedDevice != CurrentInputDevice)
	{
		CurrentInputDevice = DetectedDevice;
		UpdateDisplayedImage(CurrentInputDevice);
		
		UE_LOG(LogTemp, Log, TEXT("UEFInputImageWidget: Input device changed to: %d"), static_cast<int32>(CurrentInputDevice));
	}
}

EEFInputDeviceType UEFInputImageWidget::DetermineInputDeviceFromKey(const FKey& Key) const
{
	// Check if it's a mouse button
	if (Key.IsMouseButton())
	{
		return EEFInputDeviceType::KeyboardMouse;
	}

	// Check if it's a keyboard key
	if (Key == EKeys::A || Key == EKeys::W || Key == EKeys::S || Key == EKeys::D ||
		Key == EKeys::Q || Key == EKeys::E || Key == EKeys::R || Key == EKeys::F ||
		Key == EKeys::G || Key == EKeys::H || Key == EKeys::J || Key == EKeys::K ||
		Key == EKeys::L || Key == EKeys::Z || Key == EKeys::X || Key == EKeys::C ||
		Key == EKeys::V || Key == EKeys::B || Key == EKeys::N || Key == EKeys::M ||
		Key == EKeys::T || Key == EKeys::Y || Key == EKeys::U || Key == EKeys::I ||
		Key == EKeys::O || Key == EKeys::P ||
		Key == EKeys::SpaceBar || Key == EKeys::LeftShift || Key == EKeys::RightShift ||
		Key == EKeys::LeftControl || Key == EKeys::RightControl ||
		Key == EKeys::LeftAlt || Key == EKeys::RightAlt ||
		Key == EKeys::Tab || Key == EKeys::Escape || Key == EKeys::Enter ||
		Key == EKeys::BackSpace || Key == EKeys::CapsLock ||
		Key == EKeys::One || Key == EKeys::Two || Key == EKeys::Three ||
		Key == EKeys::Four || Key == EKeys::Five || Key == EKeys::Six ||
		Key == EKeys::Seven || Key == EKeys::Eight || Key == EKeys::Nine ||
		Key == EKeys::Zero ||
		Key == EKeys::Up || Key == EKeys::Down || Key == EKeys::Left || Key == EKeys::Right ||
		Key == EKeys::MouseX || Key == EKeys::MouseY)
	{
		return EEFInputDeviceType::KeyboardMouse;
	}

	// Check if it's a gamepad key
	if (Key.IsGamepadKey())
	{
		FString KeyString = Key.ToString();
		FString KeyName = Key.GetFName().ToString();

		// Check for PlayStation-specific keys
		if (KeyString.Contains(TEXT("PlayStation")) || KeyName.Contains(TEXT("PlayStation")))
		{
			return EEFInputDeviceType::PlayStation;
		}

		// Check for Xbox-specific keys
		if (KeyString.Contains(TEXT("Xbox")) || KeyName.Contains(TEXT("Xbox")))
		{
			return EEFInputDeviceType::Xbox;
		}

		// For generic gamepad keys, try to maintain the current gamepad type
		// This helps prevent switching between Xbox/PlayStation unnecessarily
		if (CurrentInputDevice == EEFInputDeviceType::PlayStation ||
			CurrentInputDevice == EEFInputDeviceType::Xbox ||
			CurrentInputDevice == EEFInputDeviceType::GenericGamepad)
		{
			return CurrentInputDevice;
		}

		// Default to Xbox for unknown gamepad input (most common on PC)
		return EEFInputDeviceType::Xbox;
	}

	return EEFInputDeviceType::Unknown;
}
