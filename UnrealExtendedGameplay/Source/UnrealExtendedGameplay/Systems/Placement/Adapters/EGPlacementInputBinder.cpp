// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementInputBinder.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTags.h"
#include "UnrealExtendedGameplay/Systems/Placement/Session/EGPlacementSessionComponent.h"

namespace EGPlacementInputPrivate
{
	UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem(const APlayerController* Player)
	{
		const ULocalPlayer* LocalPlayer = Player ? Player->GetLocalPlayer() : nullptr;
		return LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	}
}

bool UEGPlacementInputBinder::Bind(UEGPlacementSessionComponent* Session, APlayerController* Player)
{
	Unbind();
	UEnhancedInputComponent* Input = Player ? Cast<UEnhancedInputComponent>(Player->InputComponent) : nullptr;
	if (!Session || !Input)
	{
		UE_LOG(LogEGPlacement, Warning, TEXT("Placement input binder: %s."), !Session ? TEXT("no session") : TEXT("the player has no Enhanced Input component"));
		return false;
	}
	BoundSession = Session;
	BoundPlayer = Player;
	BoundInput = Input;

	auto Add = [this, Input](const UInputAction* Action, ETriggerEvent Event, void (UEGPlacementInputBinder::*Handler)(const FInputActionInstance&))
	{
		if (Action)
		{
			Handles.Add(Input->BindAction(Action, Event, this, Handler).GetHandle());
		}
	};
	Add(ConfirmAction, ETriggerEvent::Started, &UEGPlacementInputBinder::HandleConfirm);
	Add(CancelAction, ETriggerEvent::Started, &UEGPlacementInputBinder::HandleCancel);
	Add(RotateAction, ETriggerEvent::Triggered, &UEGPlacementInputBinder::HandleRotate);
	Add(AdjustDistanceAction, ETriggerEvent::Triggered, &UEGPlacementInputBinder::HandleAdjustDistance);
	Add(FreePositionAction, ETriggerEvent::Started, &UEGPlacementInputBinder::HandleFreePosition);
	Add(FreePositionAction, ETriggerEvent::Completed, &UEGPlacementInputBinder::HandleFreePositionReleased);
	Add(FreePositionAction, ETriggerEvent::Canceled, &UEGPlacementInputBinder::HandleFreePositionReleased);
	Add(FreeRotationAction, ETriggerEvent::Started, &UEGPlacementInputBinder::HandleFreeRotation);
	Add(FreeRotationAction, ETriggerEvent::Completed, &UEGPlacementInputBinder::HandleFreeRotationReleased);
	Add(FreeRotationAction, ETriggerEvent::Canceled, &UEGPlacementInputBinder::HandleFreeRotationReleased);

	if (MappingContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = EGPlacementInputPrivate::GetInputSubsystem(Player))
		{
			if (!Subsystem->HasMappingContext(MappingContext))
			{
				Subsystem->AddMappingContext(MappingContext, Priority);
				bAddedContext = true;
			}
		}
	}
	if (bUnbindWhenSessionEnds)
	{
		Session->OnSessionEnded.AddUniqueDynamic(this, &UEGPlacementInputBinder::HandleSessionEnded);
	}
	return true;
}

void UEGPlacementInputBinder::Unbind()
{
	if (UEnhancedInputComponent* Input = BoundInput.Get())
	{
		for (const uint32 Handle : Handles)
		{
			Input->RemoveBindingByHandle(Handle);
		}
	}
	Handles.Reset();
	if (bAddedContext && MappingContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = EGPlacementInputPrivate::GetInputSubsystem(BoundPlayer.Get()))
		{
			Subsystem->RemoveMappingContext(MappingContext);
		}
	}
	bAddedContext = false;
	if (UEGPlacementSessionComponent* Session = BoundSession.Get())
	{
		Session->OnSessionEnded.RemoveDynamic(this, &UEGPlacementInputBinder::HandleSessionEnded);
		Session->ClearModifiers();
	}
	BoundSession.Reset();
	BoundPlayer.Reset();
	BoundInput.Reset();
}

void UEGPlacementInputBinder::BeginDestroy()
{
	Handles.Reset();
	Super::BeginDestroy();
}

void UEGPlacementInputBinder::HandleConfirm(const FInputActionInstance& Instance)
{
	if (UEGPlacementSessionComponent* Session = BoundSession.Get())
	{
		Session->Confirm();
	}
}

void UEGPlacementInputBinder::HandleCancel(const FInputActionInstance& Instance)
{
	if (UEGPlacementSessionComponent* Session = BoundSession.Get())
	{
		Session->Cancel();
	}
}

void UEGPlacementInputBinder::HandleRotate(const FInputActionInstance& Instance)
{
	const float Value = Instance.GetValue().Get<float>();
	if (UEGPlacementSessionComponent* Session = BoundSession.Get(); Session && !FMath::IsNearlyZero(Value))
	{
		Session->Rotate(FMath::Sign(Value));
	}
}

void UEGPlacementInputBinder::HandleAdjustDistance(const FInputActionInstance& Instance)
{
	const float Value = Instance.GetValue().Get<float>();
	if (UEGPlacementSessionComponent* Session = BoundSession.Get(); Session && !FMath::IsNearlyZero(Value))
	{
		Session->AdjustDistance(Value);
	}
}

void UEGPlacementInputBinder::HandleFreePosition(const FInputActionInstance& Instance)
{
	if (UEGPlacementSessionComponent* Session = BoundSession.Get())
	{
		Session->SetFreePosition(true);
	}
}

void UEGPlacementInputBinder::HandleFreePositionReleased(const FInputActionInstance& Instance)
{
	if (UEGPlacementSessionComponent* Session = BoundSession.Get())
	{
		Session->SetFreePosition(false);
	}
}

void UEGPlacementInputBinder::HandleFreeRotation(const FInputActionInstance& Instance)
{
	if (UEGPlacementSessionComponent* Session = BoundSession.Get())
	{
		Session->SetFreeRotation(true);
	}
}

void UEGPlacementInputBinder::HandleFreeRotationReleased(const FInputActionInstance& Instance)
{
	if (UEGPlacementSessionComponent* Session = BoundSession.Get())
	{
		Session->SetFreeRotation(false);
	}
}

void UEGPlacementInputBinder::HandleSessionEnded(bool bCancelled)
{
	Unbind();
}
